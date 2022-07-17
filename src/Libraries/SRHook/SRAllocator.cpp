#include "SRAllocator.h"
#include "SRBaseHook.h"
#include <algorithm>
#include <cstring>
#include <cstdarg>
#ifdef _WIN32
#	include <windows.h>
#else
#	include <sys/mman.h>
#	include <unistd.h>
#	if __has_include( <asm/cachectl.h>)
#		include <asm/cachectl.h>
#	endif
#endif

bool SRHook::ptr_t::flush() const noexcept {
#ifdef _WIN32
	return FlushInstructionCache( GetCurrentProcess(), ptr, size ) != 0;
#else
#	if __has_include( <asm/cachectl.h>)
	return cacheflush( ptr, size, ICACHE ) == 0;
#	else
	return false;
#	endif
#endif
}

SRHook::ptr_t SRHook::allocator_t::page_t::allocate( std::size_t size ) noexcept {
	dbg( "TRACE page allocate %d", size );
	// Поиск наименьшего подходящего участка
	ptr_t free_ptr{ nullptr, 0 };
	for ( auto &&ptr : free )
		if ( ptr.size >= size ) free_ptr = ptr;
	if ( !free_ptr.ptr ) {
		free_ptr.size = 0;
		dbg( "Can't find free ptr" );
		return free_ptr;
	}
	// Итератор на найденный участок
	auto it = std::find( free.begin(), free.end(), free_ptr );
	// Если размер найденного участка полностью совпадает с запрошеным, то отдаем его
	if ( free_ptr.size == size ) {
		free.erase( it );
		dbg( "Get all free space from finded ptr" );
		return free_ptr;
	}
	// иначе изменяем размер найденного участка
	it->ptr += size;
	it->size -= size;
	dbg( "Return ptr{0x%X, %d}", free_ptr.ptr, size );
	return ptr_t{ free_ptr.ptr, size };
}

bool SRHook::allocator_t::page_t::deallocate( const SRHook::ptr_t &_ptr ) noexcept {
	dbg( "TRACE page deallocate {0x%X, %d}", _ptr.ptr, _ptr.size );
	if ( _ptr.ptr < this->ptr.ptr ) {
		dbg( "ptr not from this page" );
		return false;
	}
	if ( _ptr.ptr + _ptr.size > this->ptr.ptr + this->ptr.size ) {
		dbg( "ptr not from this page" );
		return false;
	}

	// Если список свободных участков пуст, то просто сохраняем в него деаллоцированную память
	if ( free.empty() ) {
		free.push_back( _ptr );
		dbg( "add free ptr {0x%X, %d} to page", _ptr.ptr, _ptr.size );
		return true;
	}
	// иначе ищем соседний участок для расширения
	for ( auto &&ptr : free ) {
		if ( ptr.ptr == _ptr.ptr + _ptr.size ) {
			ptr.ptr -= _ptr.size;
			ptr.size += _ptr.size;
			dbg( "expand free ptr", ptr.ptr, ptr.size );
			return true;
		} else if ( ptr.ptr + ptr.size == _ptr.ptr ) {
			ptr.size += _ptr.size;
			dbg( "expand free ptr", ptr.ptr, ptr.size );
			return true;
		}
	}
	// Раз соседние участки не нашлись, то просто добавляем участок в список свободных
	free.push_back( _ptr );
	dbg( "add free ptr {0x%X, %d} to page", _ptr.ptr, _ptr.size );
	return true;
}

bool SRHook::allocator_t::page_t::reallocate( SRHook::ptr_t &_ptr, std::size_t size ) noexcept {
	dbg( "TRACE page reallocate {0x%X, %d -> %d}", _ptr.ptr, _ptr.size, size );
	if ( _ptr.ptr < this->ptr.ptr ) {
		dbg( "ptr not from this page" );
		return false;
	}
	if ( _ptr.ptr + _ptr.size > this->ptr.ptr + this->ptr.size ) {
		dbg( "ptr not from this page" );
		return false;
	}

	auto it = std::find_if( free.begin(), free.end(), [&]( const SRHook::ptr_t &ptr ) {
		return ptr.ptr == _ptr.ptr + _ptr.size && ptr.size + _ptr.size >= size;
	} );
	if ( it == free.end() ) {
		dbg( "no space left on this page" );
		return false;
	}
	if ( it->size + _ptr.size == size )
		free.erase( it );
	else {
		it->ptr = _ptr.ptr + size;
		it->size = size - ( it->size + _ptr.size );
	}
	_ptr.size = size;
	dbg( "expand ptr to %d", size );
	return true;
}

void SRHook::allocator_t::page_t::dbg( const char *fmt, ... ) {
	if ( fmt == nullptr ) return;
	static char buf[4096]{ 0 };
	va_list argptr;
	va_start( argptr, fmt );
	vsprintf( buf, fmt, argptr );
	va_end( argptr );
	std::size_t free_space = 0;
	for ( auto &&ptr : free ) free_space += ptr.size;
	dbg_out( false, "Page{0x%X, %d/%d}: %s", ptr.ptr, free_space, ptr.size, buf );
}

SRHook::ptr_t SRHook::allocator_t::allocate( std::size_t size ) noexcept {
	std::lock_guard lock( mtx );
	dbg( "TRACE allocate %d", size );
	// Ищем свободное место в уже выделенных страницах
	for ( auto &&page : pages ) {
		auto ptr = page.allocate( size );
		if ( ptr.ptr && ptr.size ) {
			dbg( "Allocate ptr{0x%X, %d} in page{0x%X, %d}", ptr.ptr, ptr.size, page.get_ptr().ptr, page.get_ptr().size );
			return ptr;
		}
	}
	// Вычисляем размер аллоцируемой страницы
	auto pagesz = page_size();
	if ( size > page_size() && size % page_size() ) pagesz = size + ( page_size() - ( size % page_size() ) );
		// Раз в выделенных страницах памяти не нашлось, аллоцируем новую
#ifdef _WIN32
	auto mem = (uint8_t *)VirtualAlloc( nullptr, pagesz, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE );
#else
	auto mem = (uint8_t *)mmap( nullptr, pagesz, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0 );
#endif
	ptr_t page_ptr{ mem, pagesz };
	if ( !page_ptr.ptr ) {
		page_ptr.size = 0;
		dbg( "Can't allocate new page" );
		return page_ptr;
	}
	if ( unprotect( page_ptr ) ) {
		std::memset( page_ptr.ptr, 0xCC, page_ptr.size ); // заполняем память брейкпоинтами
		pages.push_back( page_t{ page_ptr } );
		auto ptr = pages.back().allocate( size );
		dbg( "Allocate ptr{0x%X, %d} in new page{0x%X, %d}", ptr.ptr, ptr.size, pages.back().get_ptr().ptr, pages.back().get_ptr().size );
		return ptr;
	}
#ifdef _WIN32
	VirtualFree( page_ptr.ptr, page_ptr.size, 0 );
#else
	munmap( page_ptr.ptr, page_ptr.size );
#endif
	dbg( "Can't allocate new ptr" );
	return ptr_t{ nullptr, 0 };
}

bool SRHook::allocator_t::deallocate( SRHook::ptr_t &_ptr ) noexcept {
	std::lock_guard lock( mtx );
	dbg( "TRACE deallocate {0x%X, %d}", _ptr.ptr, _ptr.size );
	// Ищем страницу с указателем и удаляем его
	auto it = std::find_if( pages.begin(), pages.end(), [&]( page_t &page ) {
		if ( page.get_ptr().ptr > _ptr.ptr ) return false;
		if ( page.get_ptr().ptr + page.get_ptr().size < _ptr.ptr ) return false;
		auto hasDeallocated = page.deallocate( _ptr );
		if ( hasDeallocated ) dbg( "deallocate from page{0x%X, %d}", page.get_ptr().ptr, page.get_ptr().size );
		return hasDeallocated;
	} );
	if ( it == pages.end() ) {
		dbg( "ptr not from this allocator" );
		return false;
	}
	// Если страница пустая, то удаляем и ее
	if ( it->empty() ) {
#ifdef _WIN32
		VirtualFree( it->get_ptr().ptr, it->get_ptr().size, 0 );
#else
		munmap( it->get_ptr().ptr, it->get_ptr().size );
#endif
		pages.erase( it );
		dbg( "remove empty page" );
	} else
		std::memset( _ptr.ptr, 0xCC, _ptr.size ); // заполняем память брейкпоинтами
	_ptr = { nullptr, 0 };
	dbg( "remove ptr{0x%X, %d}", _ptr.ptr, _ptr.size );
	return true;
}

bool SRHook::allocator_t::reallocate( SRHook::ptr_t &_ptr, std::size_t size ) noexcept {
	std::lock_guard lock( mtx );
	dbg( "TRACE page reallocate {0x%X, %d -> %d}", _ptr.ptr, _ptr.size, size );
	// Ищем страницу с указанной памятью
	auto it = std::find_if( pages.begin(), pages.end(), [&]( const page_t &page ) {
		return _ptr.ptr >= page.get_ptr().ptr && _ptr.ptr < page.get_ptr().ptr + page.get_ptr().size;
	} );
	// Если такой страницы нет, то реаллоцировать не получится
	if ( it == pages.end() ) {
		dbg( "ptr not from this allocator" );
		return false;
	}
	// Пробуем растянуть блок на свободные ячейки без реаллокаций
	if ( it->reallocate( _ptr, size ) ) {
		dbg( "expand ptr in current page" );
		return true;
	}
	// иначе перемещаем данные в новую память
	auto ptr = allocate( size );
	if ( !ptr.ptr || !ptr.size ) {
		dbg( "Can't allocate new page" );
		return false;
	}
	std::memcpy( ptr.ptr, _ptr.ptr, _ptr.size );
	deallocate( _ptr );
	_ptr = ptr;
	dbg( "Move data to new ptr{0x%X, %d}", _ptr.ptr, _ptr.size );
	return true;
}

bool SRHook::allocator_t::unprotect( const SRHook::ptr_t &ptr ) const noexcept {
#ifdef _WIN32
	size_t address = (size_t)ptr.ptr;
	size_t size = (size_t)ptr.size;
	do {
		MEMORY_BASIC_INFORMATION mbi;
		if ( !::VirtualQuery( reinterpret_cast<PVOID>( address ), &mbi, sizeof( mbi ) ) ) return false;
		if ( size > mbi.RegionSize )
			size -= mbi.RegionSize;
		else
			size = 0;
		DWORD oldp;
		if ( !::VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldp ) ) return false;
		if ( reinterpret_cast<size_t>( mbi.BaseAddress ) + mbi.RegionSize < address + size )
			address = reinterpret_cast<size_t>( mbi.BaseAddress ) + mbi.RegionSize;
	} while ( size );
#else
	if ( ::mprotect( (void *)ptr.ptr, ptr.size, PROT_READ | PROT_WRITE | PROT_EXEC ) ) return false;
#endif
	return true;
}

std::size_t SRHook::allocator_t::page_size() const noexcept {
#ifdef _WIN32
	SYSTEM_INFO sysInfo;
	GetSystemInfo( &sysInfo );
	return sysInfo.dwPageSize;
#else
	return sysconf( _SC_PAGESIZE );
#endif
}

void SRHook::allocator_t::dbg( const char *fmt, ... ) {
	if ( fmt == nullptr ) return;
	static char buf[4096]{ 0 };
	va_list argptr;
	va_start( argptr, fmt );
	vsprintf( buf, fmt, argptr );
	va_end( argptr );
	dbg_out( false, "Allocator{0x%X, %d}: %s", this, pages.size(), buf );
}

SRHook::allocator_t &SRHook::Allocator() {
	static allocator_t _allocator;
	return _allocator;
}
