#ifndef SRHOOK_HPP
#define SRHOOK_HPP

// C++17
//#include <atomic>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>
#include <cstdarg>
// This project
#include "SRAllocator.h"
#include "SRBaseHook.h"
#include "fn2void.hpp"
#include "memsafe.h" // winonly!

#ifdef _MSC_VER
#	ifdef _WIN64
typedef __int64 ssize_t;
#	else
typedef int ssize_t;
#	endif
#endif

#if !defined( _WIN32 )
#	include <dlfcn.h>
#	if __has_include( <asm/cachectl.h>)
#		include <asm/cachectl.h>
#	endif
#endif

namespace SRHook {
	template<typename... Args> class Hook {
		enum class CallingStage
		{
			wait = 0,
			before,
			after,
			install,
			remove
		};

		std::size_t addr;
		ssize_t size;
		std::string _module;

		bool debugOverride;
		void dbg( const char *fmt, ... ) {
			if ( debugOverride ) {
				if ( fmt == nullptr ) return;
				static char buf[4096]{ 0 };
				va_list argptr;
				va_start( argptr, fmt );
				vsprintf( buf, fmt, argptr );
				va_end( argptr );
				if ( _module.empty() )
					dbg_out( true, "{0x%X, %d}: %s", addr, size, buf );
				else
					dbg_out( true, "{0x%X, %d, %s}: %s", addr, size, _module.c_str(), buf );
			}
		}

	public:
		Hook( std::size_t addr, ssize_t size, std::string_view _module = "", bool debugOverride = debug ) :
			addr( addr ), size( size ), _module( _module ), debugOverride( debugOverride ) {}
		Hook( std::size_t addr, std::string_view _module = "", bool debugOverride = debug ) : Hook( addr, -1, _module, debugOverride ) {}
		virtual ~Hook() {
			while ( stage != CallingStage::wait )
				;
			remove();
			if ( originalCode ) {
				delete[] originalCode;
				originalCode = nullptr;
			}
			if ( !hooked && code_ptr.ptr ) Allocator().deallocate( code_ptr );
			dbg( nullptr );
		}

		virtual bool isHooked() { return hooked; }

		virtual bool changeHook( std::size_t addr, ssize_t size, std::string_view _module = "" ) {
			if ( hooked ) return false;
			if ( originalCode ) {
				delete[] originalCode;
				originalCode = nullptr;
			}
			if ( code_ptr.ptr ) Allocator().deallocate( code_ptr );
			this->addr = addr;
			this->size = size;
			this->_module = _module;
			return true;
		}
		virtual bool changeHook( std::size_t addr, std::string_view _module = "" ) { return changeHook( addr, -1, _module ); }

		virtual bool changeAddr( std::size_t addr ) { return changeHook( addr, size, _module ); }
		virtual bool changeSize( std::size_t size ) { return changeHook( addr, size, _module ); }
		virtual bool changeModule( std::string_view _module ) { return changeHook( addr, size, _module ); }

		virtual std::size_t getAddr() const { return addr; }
		virtual std::size_t getSize() const { return size; }
		virtual const std::string_view getModule() const { return _module; }

		Callback<Hook<Args...>, Args &...> onBefore;
		Callback<Hook<Args...>, Args &...> onAfter;

		virtual bool skipOriginal() {
			if ( stage != CallingStage::before ) return false;
			skip = true;
			return true;
		}

		virtual void changeRetAddr( std::size_t out ) { retAddr = out; }

		CPU cpu;

		virtual bool
			install( ssize_t stackOffsetBefore = 0, ssize_t stackOffsetAfter = 0, bool collectFlags = true, ptr_t stack = ptr_t() ) {
			if ( stage != CallingStage::wait ) return false;
			stage = CallingStage::install;
			codeLength = 0;

			// Копирование оригинального кода в класс
			if ( !originalCode ) {
				originalAddr = addr;
				if ( !_module.empty() ) {
#ifdef _WIN32
					auto mod = GetModuleHandleA( _module.data() );
					if ( mod == nullptr || mod == INVALID_HANDLE_VALUE ) {
#else
					auto mod = dlopen( _module.data(), RTLD_NOW ); // NOTE: Это не указатель на начало либы!!!
					if ( mod == nullptr ) {
#endif
						Allocator().deallocate( code_ptr );
						stage = CallingStage::wait;
						return false;
					}
					originalAddr += (std::size_t)mod;
				}
				if ( size == -1 ) size = tryToDetectSize( originalAddr );
				if ( !size ) {
					Allocator().deallocate( code_ptr );
					stage = CallingStage::wait;
					return false;
				}
				originalCode = new std::uint8_t[size];
				memsafe::copy( originalCode, (void *)originalAddr, size );
			}

			// Указатель на вершину стека
			void *stack_ptr = (void *)( std::size_t( stack.ptr ) + stack.size );

			// Копирование EAX в класс
			pusha<std::uint8_t>( 0xA3, &cpu.EAX );
			// Перемещение адреса возврата в класс (обязательно до копирования ESP)
			pusha<std::uint8_t, std::uint8_t>( 0x58, 0xA3, &retAddr );
			// Копирование остальных регистров в класс
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x0D, &cpu.ECX );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x15, &cpu.EDX );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x1D, &cpu.EBX );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x25, &cpu.ESP );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x2D, &cpu.EBP );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x35, &cpu.ESI );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x3D, &cpu.EDI );
			// Копирование флагов в класс
			if ( collectFlags ) pusha<std::uint8_t, std::uint8_t, std::uint8_t>( 0x9C, 0x58, 0xA3, &cpu.EFLAGS );

			// Замена стека
			if ( stack_ptr ) pusha<std::uint8_t>( 0xBC, stack_ptr );
			// Копирование аргументов со стека
			for ( int i = sizeof...( Args ) - 1; i >= 0; --i ) {
				pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x15, &cpu.ESP ); // mov edx, esp
				pusha<std::uint8_t, std::uint8_t>( 0x81, 0xC2,
												   stackOffsetBefore + i * 4 ); // add edx, offset
				push<std::uint8_t>( 0x52 ); // push edx
			}
			// Вызов обработчика перед оригинальным кодом
#ifdef _WIN32
			pusha<std::uint8_t>( 0xB9, (std::size_t)this ); // mov ecx, this
#else
			pusha<std::uint8_t>( 0x68, (std::size_t)this ); // push this
#endif
			auto relAddr = getRelAddr( (std::size_t)code_ptr.ptr + codeLength, (std::size_t)fn2void( &Hook<Args...>::before ) );
			pusha<std::uint8_t>( 0xE8, relAddr ); // call before
#ifndef _WIN32
			pusha<std::uint8_t, std::uint8_t, std::uint8_t>( 0x83, 0xC4, sizeof(std::size_t) * 2 + sizeof...( Args ) ); // add esp, N (retAddr + this + args)
#endif

			// Пропуск оригинального кода
			pusha<std::uint8_t, std::uint8_t>( 0x85, 0xC0 ); // test eax, eax
			if ( collectFlags )
				pusha<std::uint8_t, std::uint8_t>( 0x0F, 0x85, 108 + size ); // jnz j_after
			else
				pusha<std::uint8_t, std::uint8_t>( 0x0F, 0x85, 94 + size ); // jnz j_after

			// Восстановление флагов из класса
			if ( collectFlags ) {
				pusha<std::uint8_t>( 0xA1, &cpu.EFLAGS );
				pusha<std::uint8_t, std::uint8_t>( 0x50, 0x9D );
			}
			// Восстановление регистров из класса
			pusha<std::uint8_t>( 0xA1, &cpu.EAX );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x0D, &cpu.ECX );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x15, &cpu.EDX );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x1D, &cpu.EBX );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x25, &cpu.ESP );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x2D, &cpu.EBP );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x35, &cpu.ESI );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x3D, &cpu.EDI );

			// оригинальный код
			if ( !pushOriginal() ) {
				stage = CallingStage::wait;
				return false;
			}

			// Копирование регистров в класс
			pusha<std::uint8_t>( 0xA3, &cpu.EAX );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x0D, &cpu.ECX );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x15, &cpu.EDX );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x1D, &cpu.EBX );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x25, &cpu.ESP );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x2D, &cpu.EBP );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x35, &cpu.ESI );
			pusha<std::uint8_t, std::uint8_t>( 0x89, 0x3D, &cpu.EDI );
			// Копирование флагов в класс
			if ( collectFlags ) pusha<std::uint8_t, std::uint8_t, std::uint8_t>( 0x9C, 0x58, 0xA3, &cpu.EFLAGS );

			// j_after
			// Замена стека
			if ( stack_ptr ) pusha<std::uint8_t>( 0xBC, stack_ptr );
			// Копирование аргументов со стека
			for ( int i = sizeof...( Args ) - 1; i >= 0; --i ) {
				pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x15, &cpu.ESP ); // mov edx, esp
				pusha<std::uint8_t, std::uint8_t>( 0x81, 0xC2,
												   stackOffsetAfter + i * 4 ); // add edx, offset
				push<std::uint8_t>( 0x52 ); // push edx
			}
			// Вызов обработчика после оригинального кода
#ifdef _WIN32
			pusha<std::uint8_t>( 0xB9, (std::size_t)this ); // mov ecx, this
#else
			pusha<std::uint8_t>( 0x68, (std::size_t)this ); // push this
#endif
			relAddr = getRelAddr( (std::size_t)code_ptr.ptr + codeLength, (std::size_t)fn2void( &Hook<Args...>::after ) );
			pusha<std::uint8_t>( 0xE8, relAddr ); // call after
#ifndef _WIN32
			pusha<std::uint8_t, std::uint8_t, std::uint8_t>( 0x83, 0xC4, sizeof(std::size_t) * 2 + sizeof...( Args ) ); // add esp, N (retAddr + this + args)
#endif

			// Восстановление флагов из класса
			if ( collectFlags ) {
				pusha<std::uint8_t>( 0xA1, &cpu.EFLAGS );
				pusha<std::uint8_t, std::uint8_t>( 0x50, 0x9D );
			}
			// Восстановление регистров из класса
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x3D, &cpu.EDI );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x35, &cpu.ESI );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x2D, &cpu.EBP );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x25, &cpu.ESP );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x1D, &cpu.EBX );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x15, &cpu.EDX );
			pusha<std::uint8_t, std::uint8_t>( 0x8B, 0x0D, &cpu.ECX );
			// Восстановление адреса возврата из класса
			pusha<std::uint8_t>( 0xA1, &retAddr ); // mov eax, retAddr
			push<std::uint8_t>( 0x50 ); // push eax
			// Восстановление EAX из класса
			pusha<std::uint8_t>( 0xA1, &cpu.EAX );

			// Выход из хука
			push<std::uint8_t>( 0xC3 ); // ret
			code_ptr.flush();

			// Вход в хук
			if ( !hooked ) {
				auto relAddr = getRelAddr( originalAddr, (std::size_t)code_ptr.ptr );
				memsafe::write<std::uint8_t>( (void *)originalAddr, 0xE8 );
				memsafe::write<std::size_t>( (void *)( originalAddr + 1 ), relAddr );
				if ( size > 5 ) memsafe::set( (void *)( originalAddr + 5 ), 0x90, size - 5 );
#ifdef _WIN32
				FlushInstructionCache( GetCurrentProcess(), (void *)originalAddr, size );
#else
#	if __has_include( <asm/cachectl.h>)
				cacheflush( (void *)originalAddr, size, ICACHE );
#	endif
#endif
				hooked = true;
			}

			stage = CallingStage::wait;
			dbg( "install to %08X", originalAddr );
			return true;
		}

		virtual bool remove() {
			for ( int i = 0; stage == CallingStage::before || stage == CallingStage::after; ++i ) {
				std::this_thread::yield();
				if ( i > 1000 ) return false;
				;
			}
			if ( stage != CallingStage::wait ) return false;
			if ( !hooked ) return false;
			if ( !originalCode ) return false;
			if ( !code_ptr.ptr ) return false;
			stage = CallingStage::remove;

			if ( memsafe::read<std::uint8_t>( (void *)originalAddr ) == 0xE8 ) {
				auto rel = memsafe::read<std::size_t>( (void *)( originalAddr + 1 ) );
				auto dest = getDestAddr( originalAddr, rel );
				if ( dest == (std::size_t)code_ptr.ptr ) {
					memsafe::copy( (void *)originalAddr, originalCode, size );
#ifdef _WIN32
					FlushInstructionCache( GetCurrentProcess(), (void *)originalAddr, size );
#else
#	if __has_include( <asm/cachectl.h>)
					cacheflush( (void *)originalAddr, size, ICACHE );
#	endif
#endif
					hooked = false;
					if ( remove_EAX ) {
						delete remove_EAX;
						remove_EAX = nullptr;
					}
					if ( remove_raddr ) {
						delete remove_raddr;
						remove_raddr = nullptr;
					}
					stage = CallingStage::wait;
					dbg( "remove from %08X", originalAddr );
					return true;
				} else {
					remove_EAX = new std::size_t;
					remove_raddr = new std::size_t;
				}
			} else {
				remove_EAX = new std::size_t;
				remove_raddr = new std::size_t;
			}

			codeLength = 0;

			// Копирование EAX в класс
			pusha<std::uint8_t>( 0xA3, remove_EAX );
			// Перемещение адреса возврата в класс (обязательно до копирования ESP)
			pusha<std::uint8_t, std::uint8_t>( 0x58, 0xA3, remove_raddr );
			// Восстановление EAX из класса
			pusha<std::uint8_t>( 0xA1, remove_EAX );

			// оригинальный код
			if ( !pushOriginal() ) {
				stage = CallingStage::wait;
				return false;
			}

			// Копирование EAX в класс
			pusha<std::uint8_t>( 0xA3, remove_EAX );
			// Восстановление адреса возврата из класса
			pusha<std::uint8_t>( 0xA1, remove_raddr ); // mov eax, retAddr
			push<std::uint8_t>( 0x50 ); // push eax
			// Восстановление EAX из класса
			pusha<std::uint8_t>( 0xA1, remove_EAX );

			push<std::uint8_t>( 0xC3 ); // ret
			code_ptr.flush();

			stage = CallingStage::wait;
			dbg( "remove from %08X (compability layer)", originalAddr );
			return true;
		}

		/**
		 * @brief Автоматическое определение размера хука
		 * @details Решение основывается только на основе первого опкода по указанному адресу. Если размер
		 * данного опкодаменее 5 байт, или опкод не задан в функции, результат будет 0
		 * @param address Адрес хука
		 * @return Размер хука
		 */
		static std::size_t tryToDetectSize( std::size_t address ) {
			auto pCode = reinterpret_cast<std::uint8_t *>( address );
			switch ( *pCode ) {
				case 0xE8:
					[[fallthrough]];
				case 0xE9:
					[[fallthrough]];
				case 0xA3:
					[[fallthrough]];
				case 0xA1:
					[[fallthrough]];
				case 0xA0:
					[[fallthrough]];
				case 0xA2:
					[[fallthrough]];
				case 0xA9:
					[[fallthrough]];
				case 0xB8:
					[[fallthrough]];
				case 0xB9:
					[[fallthrough]];
				case 0xBA:
					[[fallthrough]];
				case 0xBB:
					[[fallthrough]];
				case 0xBD:
					[[fallthrough]];
				case 0xBE:
					[[fallthrough]];
				case 0xBF:
					[[fallthrough]];
				case 0xBC:
					[[fallthrough]];
				case 0x35:
					[[fallthrough]];
				case 0x3D:
					[[fallthrough]];
				case 0x0D:
					[[fallthrough]];
				case 0x15:
					[[fallthrough]];
				case 0x1D:
					[[fallthrough]];
				case 0x25:
					[[fallthrough]];
				case 0x2D:
					[[fallthrough]];
				case 0x68:
					[[fallthrough]];
				case 0x05:
					return 5;
				case 0x89:
					[[fallthrough]];
				case 0x8B:
					[[fallthrough]];
				case 0x69:
					[[fallthrough]];
				case 0x81:
					[[fallthrough]];
				case 0xC7:
					[[fallthrough]];
				case 0xF7:
					[[fallthrough]];
				case 0x0F:
					return 6;
				case 0xEA:
					[[fallthrough]];
				case 0x9A:
					return 7;
				default:
					return 0;
			}
		}
		static ssize_t getRelAddr( ssize_t from, ssize_t to, std::size_t opLen = 5 ) { return to - ( from + opLen ); }
		static ssize_t getDestAddr( ssize_t from, ssize_t relAddr, std::size_t opLen = 5 ) { return relAddr + ( from + opLen ); }

	protected:
		bool skip;
		/*std::atomic<*/ CallingStage /*>*/ stage = CallingStage::wait; // on x86 int is atomic

		std::size_t *remove_EAX = nullptr;
		std::size_t *remove_raddr = nullptr;

		std::size_t originalAddr;
		std::uint8_t *originalCode = nullptr;

		std::size_t codeLength = 0;
		ptr_t code_ptr{ nullptr, 0 };

		std::size_t retAddr;

		bool hooked = false;

		bool before( Args &...args ) {
			stage = CallingStage::before;
			dbg( "call before" );
			skip = false;
			Info info( &cpu, &retAddr );
			onBefore( *this, info, args... );
			if ( info.skipOriginal ) skip = true;
#if defined( _GLIBCXX_STRING ) || defined( _MSC_VER )
			[[maybe_unused]] std::function<void()> gcc10_cruch = [] {
			};
#endif
			if ( skip ) dbg( "skip original code" );
			return skip;
		}
		void after( Args &...args ) {
			stage = CallingStage::after;
			dbg( "call after" );
			Info info( &cpu, &retAddr );
			onAfter( *this, info, args... );
#if defined( _GLIBCXX_STRING ) || defined( _MSC_VER )
			[[maybe_unused]] std::function<void()> gcc10_cruch = [] {
			};
#endif
			dbg( "return to %08X", retAddr );
			stage = CallingStage::wait;
		}

	private:
		void push( std::uint8_t *data, std::size_t length ) {
			if ( length + codeLength >= code_ptr.size ) alloc();
			for ( std::size_t i = 0; i < length; ++i ) code_ptr.ptr[codeLength++] = data[i];
		}

		template<typename T> void push( const T &value ) {
			if ( sizeof( T ) + codeLength >= code_ptr.size ) alloc();
			if constexpr ( sizeof( T ) > 1 ) {
				union _ {
					_( T value ) : value( value ) {}
					T value;
					std::uint8_t bytes[sizeof( T )];
				} dec( value );
				for ( int i = 0; i < sizeof( T ); ++i ) code_ptr.ptr[codeLength++] = dec.bytes[i];
			} else
				code_ptr.ptr[codeLength++] = (std::uint8_t)value;
		}

		template<typename T, typename... Ts> void pusha( T value, Ts... values ) {
			push( value );
			if constexpr ( sizeof...( Ts ) > 1 )
				pusha( values... );
			else
				push( values... );
		}

		bool pushOriginal() {
			// Вставка оригинального кода
			push( originalCode, size );
			// Модификация оригинального кода
			auto firstOpcode = originalCode[0]; // NOTE: Только первый опкод, потому что в последним может
			// быть не опкод, а операнд одного из опкодов
			if ( firstOpcode == 0xE8 || firstOpcode == 0xE9 || firstOpcode == 0x0F ) {
				if ( firstOpcode == 0x0F ) {
					firstOpcode = originalCode[1];
					if ( firstOpcode >= 0x81 && firstOpcode <= 0x8F ) {
						auto dest = getDestAddr( originalAddr, *(std::size_t *)&originalCode[2], 6 );
						auto rel = getRelAddr( (std::size_t)&code_ptr.ptr[codeLength - size], dest, 6 );
						*(std::size_t *)&code_ptr.ptr[codeLength - ( size - 2 )] = rel;
					}
				} else {
					auto dest = getDestAddr( originalAddr, *(std::size_t *)&originalCode[1] );
					auto rel = getRelAddr( (std::size_t)&code_ptr.ptr[codeLength - size], dest );
					*(std::size_t *)&code_ptr.ptr[codeLength - ( size - 1 )] = rel;
				}
			}
			return true;
		}

		void alloc() {
			if ( !code_ptr.ptr ) {
#ifndef _WIN32
				code_ptr = Allocator().allocate( /*258*/ 268 + size + sizeof...( Args ) * /*18*/ 32 );
#else
				code_ptr = Allocator().allocate( /*258*/ 268 + size + sizeof...( Args ) * /*18*/ 26 );
#endif
				if ( !code_ptr.ptr ) dbg( "Can't allocate memory for code" );
			} else {
				if ( !Allocator().reallocate( code_ptr, code_ptr.size * 2 ) ) dbg( "Can't reallocate memory for code" );
			}
		}
	};

	template<typename... Args> void make_for( Hook<Args...> *&hook, std::size_t addr, std::size_t size, std::string_view _module = "" ) {
		hook = new Hook<Args...>( addr, size, _module );
	}
	template<typename... Args> void make_for( Hook<Args...> *&hook, std::size_t addr, std::string_view _module = "" ) {
		hook = new Hook<Args...>( addr, _module );
	}
} // namespace SRHook

#endif // SRHOOK_HPP
