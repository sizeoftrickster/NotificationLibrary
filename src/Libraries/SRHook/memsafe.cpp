#include "memsafe.h"
#include <stdexcept>
#include <cstring>
#if !defined( _WIN32 )
#	if __has_include( <asm/cachectl.h>)
#		include <asm/cachectl.h>
#	endif
#	include <unistd.h>
#endif

void *memsafe::copy( void *dest, const void *src, std::size_t stLen ) {
	if ( dest == nullptr || src == nullptr || stLen == 0 ) return nullptr;

#ifdef _WIN32
	MEMORY_BASIC_INFORMATION mbi;
	VirtualQuery( dest, &mbi, sizeof( mbi ) );
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect );
#else
	Unprotect( dest, stLen );
#endif

	void *pvRetn = std::memcpy( dest, src, stLen );
#ifdef _WIN32
	VirtualProtect( mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect );
	FlushInstructionCache( GetCurrentProcess(), dest, stLen );
#else
#	if __has_include( <asm/cachectl.h>)
	cacheflush( dest, stLen, BCACHE ) == 0;
#	endif
#endif
	return pvRetn;
}

void *memsafe::copy( void *dest, const std::vector<unsigned char> &src ) {
	return copy( dest, src.data(), src.size() );
}
void *memsafe::copy( std::size_t dest, const std::size_t src, std::size_t stLen ) {
	return copy( (void *)dest, (void *)src, stLen );
}

void *memsafe::copy( std::size_t dest, const std::vector<unsigned char> &src ) {
	return copy( (void *)dest, src );
}

int memsafe::compare( const void *_s1, const void *_s2, uint32_t len ) {
	const uint8_t *s1 = static_cast<const uint8_t *>( _s1 );
	const uint8_t *s2 = static_cast<const uint8_t *>( _s2 );
	uint8_t buf[4096];

	for ( ;; ) {
		if ( len > 4096 ) {
			if ( !copy( buf, s1, 4096 ) ) return 0;
			if ( std::memcmp( buf, s2, 4096 ) ) return 0;
			s1 += 4096;
			s2 += 4096;
			len -= 4096;
		} else {
			if ( !copy( buf, s1, len ) ) return 0;
			if ( std::memcmp( buf, s2, len ) ) return 0;
			break;
		}
	}

	return 1;
}

int memsafe::compare( const void *_s1, const std::vector<unsigned char> &_s2 ) {
	return compare( _s1, _s2.data(), _s2.size() );
}

int memsafe::compare( const std::size_t _s1, const std::size_t _s2, uint32_t len ) {
	return compare( (void *)_s1, (void *)_s2, len );
}

int memsafe::compare( const std::size_t _s1, const std::vector<unsigned char> &_s2 ) {
	return compare( (void *)_s1, _s2.data(), _s2.size() );
}

int memsafe::set( void *_dest, int c, std::size_t len ) {
	std::uint8_t *dest = static_cast<std::uint8_t *>( _dest );
	std::uint8_t buf[4096];
	std::memset( buf, c, ( len > 4096 ) ? 4096 : len );
	for ( ;; ) {
		if ( len > 4096 ) {
			if ( !copy( dest, buf, 4096 ) ) return 0;
			dest += 4096;
			len -= 4096;
		} else {
			if ( !copy( dest, buf, len ) ) return 0;
			break;
		}
	}
	return 1;
}

int memsafe::set( std::size_t _dest, int c, std::size_t len ) {
	return set( (void *)_dest, c, len );
}

std::vector<unsigned char> memsafe::nop( void *_dest, std::size_t len ) {
	std::vector<unsigned char> result( len, 0 );
	copy( result.data(), _dest, len );
	set( _dest, 0x90, len );
	return result;
}

std::vector<unsigned char> memsafe::nop( std::size_t _dest, std::size_t len ) {
	return nop( (void *)_dest, len );
}

void memsafe::Unprotect( std::size_t address, std::size_t size ) {
#ifdef _WIN32
	do {
		MEMORY_BASIC_INFORMATION mbi;
		if ( !VirtualQuery( PVOID( address ), &mbi, sizeof( mbi ) ) ) throw std::runtime_error( "virtual query error" );
		if ( size > mbi.RegionSize )
			size -= mbi.RegionSize;
		else
			size = 0;
		DWORD oldp;
		if ( !VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldp ) )
			throw std::runtime_error( "virtual protect error" );
		if ( DWORD( mbi.BaseAddress ) + mbi.RegionSize < address + size ) address = DWORD( mbi.BaseAddress ) + mbi.RegionSize;
	} while ( size );
#else
	auto pageSize = sysconf( _SC_PAGESIZE );
	auto pageMask = makeMask( pageSize );
	for ( std::size_t i = address & pageMask; i <= ( ( address + size ) & pageMask ); i += pageSize )
		mprotect( (void *)i, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC );
#endif
}

void memsafe::Unprotect( const void *address, std::size_t size ) {
	return Unprotect( std::size_t( address ), size );
}
