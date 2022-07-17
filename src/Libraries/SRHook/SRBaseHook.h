#ifndef SRBASEHOOK_H
#define SRBASEHOOK_H

#include <cstdint>
#include <deque>
#include <functional>
#include <list>
#include <mutex>
#include <tuple>
#include <variant>

namespace SRHook {
	constexpr bool debug = false;
	void dbg_out( bool crit, const char *fmt, ... );

	/// Тип вызова
	enum class call_t : uint8_t
	{
		/// Соглашение cdecl, стек чистит вызывающая сторона
		ccall,
		/// Соглашение stdcall, стек чистит вызываемая сторона
		stdcall,
		/// Соглашение thiscall, стек чистит вызываемая сторона. В ECX/RCX передается указатель на объект
		thiscall
	};

#ifndef CPURX
#	define CPURX( reg ) \
		union { \
			union { \
				struct { \
					std::uint8_t reg##L; \
					std::uint8_t reg##H; \
				}; \
				std::uint16_t reg##X; \
			}; \
			std::size_t E##reg##X; \
		};
#endif
#ifndef CPUR
#	define CPUR( reg ) \
		union { \
			std::uint16_t reg; \
			std::size_t E##reg; \
		};
#endif
	struct CPU {
		CPURX( A )
		CPURX( C )
		CPURX( D )
		CPURX( B )
		CPUR( SP )
		CPUR( BP )
		CPUR( SI )
		CPUR( DI )
		union {
			struct {
				union {
					struct {
						uint8_t CF		  : 1;
						uint8_t RESERVE1  : 1;
						uint8_t PF		  : 1;
						uint8_t RESERVE3  : 1;
						uint8_t AF		  : 1;
						uint8_t RESERVE5  : 1;
						uint8_t ZF		  : 1;
						uint8_t SF		  : 1;

						uint8_t TF		  : 1;
						uint8_t IF		  : 1;
						uint8_t DF		  : 1;
						uint8_t OF		  : 1;
						uint8_t IOPL	  : 2;
						uint8_t NT		  : 1;
						uint8_t RESERVE15 : 1;
					};
					uint16_t FLAGS;
				};

				uint8_t RF		  : 1;
				uint8_t VM		  : 1;
				uint8_t AC		  : 1;
				uint8_t VIF		  : 1;
				uint8_t VIP		  : 1;
				uint8_t ID		  : 1;
				uint8_t RESERVE22 : 2;

				uint8_t RESERVE24;
			};
			std::size_t EFLAGS;
		};

		bool operator==( const CPU &r ) {
			return EAX == r.EAX && ECX == r.ECX && EDX == r.EDX && EBX == r.EBX && ESP == r.ESP && EBP == r.EBP && ESI == r.ESI &&
				   EDI == r.EDI && EFLAGS == r.EFLAGS;
		}
		bool operator!=( const CPU &r ) { return !operator==( r ); }
	};

	struct Info {
		CPU &cpu;
		std::size_t &retAddr;
		bool skipOriginal;

		Info( CPU *cpu, std::size_t *retAddr ) : cpu( *cpu ), retAddr( *retAddr ) { skipOriginal = false; }
		Info() = delete;
	};
#ifdef CPURX
#	undef CPURX
#endif
#ifdef CPUR
#	undef CPUR
#endif

	template<class F, typename... Args> class Callback {
		friend F;

		using cb_empty_t = std::function<void( void )>;
		using cb_default_t = std::function<void( Args... )>;
		using cb_cpu_t = std::function<void( CPU &, Args... )>;
		using cb_info_t = std::function<void( Info &, Args... )>;
		using cb_expand_t = std::function<void( F &, Args... )>;

		using callback_small_t = std::variant<cb_default_t, cb_cpu_t, cb_info_t, cb_expand_t>;
		using callback_full_t = std::variant<cb_empty_t, cb_default_t, cb_cpu_t, cb_info_t, cb_expand_t>;

		using callback_t = std::conditional_t<sizeof...( Args ) != 0, callback_full_t, callback_small_t>;

		std::deque<callback_t> _callbacks;
		std::list<std::size_t> _empty;
		std::mutex mtx;

	public:
		template<typename = std::enable_if<sizeof...( Args ) != 0>> std::size_t operator+=( const std::function<void( void )> &func ) {
			return connect( func );
		}
		std::size_t operator+=( const std::function<void( Args... )> &func ) { return connect( func ); }
		std::size_t operator+=( const std::function<void( CPU &, Args... )> &func ) { return connect( func ); }
		std::size_t operator+=( const std::function<void( Info &, Args... )> &func ) { return connect( func ); }
		std::size_t operator+=( const std::function<void( F &, Args... )> &func ) { return connect( func ); }

		//		template<typename = std::enable_if<sizeof...( Args ) != 0>, class C>
		//		size_t operator+=( const std::tuple<C *, void ( C::* )( void )> &func ) {
		//			return connect_method( func );
		//		}
		template<class C> std::size_t operator+=( const std::tuple<C *, void ( C::* )( Args... )> &method ) {
			auto [obj, func] = method;
			return connect_method( obj, func );
		}
		template<class C> std::size_t operator+=( const std::tuple<C *, void ( C::* )( CPU &, Args... )> &method ) {
			auto [obj, func] = method;
			return connect_method( obj, func );
		}
		template<class C> std::size_t operator+=( const std::tuple<C *, void ( C::* )( Info &, Args... )> &method ) {
			auto [obj, func] = method;
			return connect_method( obj, func );
		}
		template<class C> std::size_t operator+=( const std::tuple<C *, void ( C::* )( F &, Args... )> &method ) {
			auto [obj, func] = method;
			return connect_method( obj, func );
		}

		void operator-=( std::size_t id ) {
			std::lock_guard lock( mtx );
			// Если id числится в пустых слотах, то не удаляем его
			if ( std::find( _empty.begin(), _empty.end(), id ) != _empty.end() ) return;
			// Если это id последнего коллбека, то полностью удаляем его из очереди
			if ( id == _callbacks.size() - 1 ) {
				_callbacks.pop_back();
				return;
			}
			// Отсечка не валидных id
			if ( id >= _callbacks.size() ) return;
			// Заменяем коллбек пустой заглушкой
			_callbacks[id] = [] {
			};
			_empty.push_back( id );
		}

	protected:
		void operator()( F &f, Info &info, Args... args ) {
			for ( auto &cb : _callbacks ) {
				if constexpr ( sizeof...( Args ) != 0 ) {
					if ( std::get_if<cb_empty_t>( &cb ) ) {
						std::get<cb_empty_t>( cb )();
						continue;
					}
				}
				if ( std::get_if<cb_default_t>( &cb ) )
					std::get<cb_default_t>( cb )( args... );
				else if ( std::get_if<cb_cpu_t>( &cb ) )
					std::get<cb_cpu_t>( cb )( info.cpu, args... );
				else if ( std::get_if<cb_info_t>( &cb ) )
					std::get<cb_info_t>( cb )( info, args... );
				else if ( std::get_if<cb_expand_t>( &cb ) )
					std::get<cb_expand_t>( cb )( f, args... );
				else
					dbg_out( true, "Skip callback - unknown type" );
			}
		}

	private:
		template<typename... Args_cb> std::size_t connect( const std::function<void( Args_cb... )> &func ) {
			std::lock_guard lock( mtx );
			// Если есть пустые коллбеки, то используем их
			if ( !_empty.empty() ) {
				auto id = _empty.front();
				_empty.pop_front();
				_callbacks[id] = func;
				return id;
			}
			// Иначе добавляем коллбек в конец
			_callbacks.push_back( func );
			return _callbacks.size() - 1;
		}
		template<class C, typename... Args_cb> std::size_t connect_method( C *obj, void ( C::*func )( Args_cb... ) ) {
			std::lock_guard lock( mtx );
			auto lambda = [obj, func]( Args_cb... args ) {
				( obj->*func )( args... );
			};
			// Если есть пустые коллбеки, то используем их
			if ( !_empty.empty() ) {
				auto id = _empty.front();
				_empty.pop_front();
				_callbacks[id] = lambda;
				return id;
			}
			// Иначе добавляем коллбек в конец
			_callbacks.push_back( lambda );
			return _callbacks.size() - 1;
		}
	};
} // namespace SRHook

#endif // SRBASEHOOK_H
