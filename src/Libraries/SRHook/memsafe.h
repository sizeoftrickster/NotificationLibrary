#ifndef MEMSAFE_H
#define MEMSAFE_H

#include <cstdint>
#include <vector>
#if __has_include( <array>)
#	include <array>
#endif
#ifdef _WIN32
#	include <windows.h>
#else
#	include <sys/mman.h>
#endif

namespace memsafe {
	void *copy( void *dest, const void *src, std::size_t stLen );
	void *copy( void *dest, const std::vector<unsigned char> &src );
#if __has_include( <array>)
	template<std::size_t stLen> void *copy( void *dest, const std::array<unsigned char, stLen> &src ) {
		return copy( dest, (void*)src.data(), src.size() );
	}
#endif
	void *copy( std::size_t dest, const std::size_t src, std::size_t stLen );
	void *copy( std::size_t dest, const std::vector<unsigned char> &src );
#if __has_include( <array>)
	template<std::size_t stLen> void *copy( std::size_t dest, const std::array<unsigned char, stLen> &src ) {
		return copy( (void*)dest, (void*)src.data(), src.size() );
	}
#endif
	int compare( const void *_s1, const void *_s2, uint32_t len );
	int compare( const void *_s1, const std::vector<unsigned char> &_s2 );
#if __has_include( <array>)
	template<std::size_t len> int compare( const void *_s1, const std::array<unsigned char, len> &_s2 ) {
		return compare( _s1, (void*)_s2.data(), _s2.size() );
	}
#endif
	int compare( const std::size_t _s1, const std::size_t _s2, uint32_t len );
	int compare( const std::size_t _s1, const std::vector<unsigned char> &_s2 );
#if __has_include( <array>)
	template<std::size_t len> int compare( const std::size_t _s1, const std::array<unsigned char, len> &_s2 ) {
		return compare( (void*)_s1, (void*)_s2.data(), _s2.size() );
	}
#endif
	int set( void *_dest, int c, std::size_t len );
	int set( std::size_t _dest, int c, std::size_t len );
	std::vector<unsigned char> nop( void *_dest, std::size_t len );
#if __has_include( <array>)
	template<std::size_t len> std::array<unsigned char, len> nop( void *_dest ) {
		std::array<unsigned char, len> result;
		copy( result.data(), _dest, len );
		set( _dest, 0x90, len );
		return result;
	}
#endif
	std::vector<unsigned char> nop( std::size_t _dest, std::size_t len );
#if __has_include( <array>)
	template<std::size_t len> std::array<unsigned char, len> nop( std::size_t _dest ) { return nop<len>( (void *)_dest ); }
#endif

	void Unprotect( std::size_t address, std::size_t size );
	void Unprotect( const void *address, std::size_t size );

	template<typename T> void write( void *address, T value ) {
#ifdef _WIN32
		DWORD oldVP;
#endif
		std::size_t len = sizeof( T );

#ifdef _WIN32
		VirtualProtect( (void *)address, len, PAGE_EXECUTE_READWRITE, &oldVP );
#else
		Unprotect( address, len );
#endif
		*(T *)address = value;
#ifdef _WIN32
		VirtualProtect( (void *)address, len, oldVP, &oldVP );
#endif
	}
	template<typename T> void write( std::size_t address, T value ) { return write<T>( (void *)address, value ); }
	template<typename T> T read( void *address ) {
#ifdef _WIN32
		DWORD oldVP;
#endif
		std::size_t len = sizeof( T );

#ifdef _WIN32
		VirtualProtect( (void *)address, len, PAGE_EXECUTE_READWRITE, &oldVP );
#else
		Unprotect( address, len );
#endif
		T value = *(T *)address;
#ifdef _WIN32
		VirtualProtect( (void *)address, len, oldVP, &oldVP );
#endif

		return value;
	}
	template<typename T> T read( std::size_t address ) { return read<T>( (void *)address ); }

	template<std::size_t bits = 8> constexpr std::size_t makeMask( std::size_t value ) {
		struct test {
			char bitField : bits;
		};
		static_assert( sizeof( test ) * sizeof( std::size_t ) == sizeof( std::size_t ), "Invalid count of bits in byte" );

		std::size_t result = ~value;
		std::size_t maskLen = 0;
		for ( std::size_t i = 0; i < sizeof( value ) * bits; ++i )
			if ( value & ( 0x1 << i ) ) maskLen = i;
		for ( std::size_t i = maskLen + 1; i < sizeof( result ) * bits; ++i ) result = ( result ^ ( 1 << i ) );
		return static_cast<std::size_t>( -1 ) ^ result;
	}
}; // namespace memsafe

#endif // MEMSAFE_H
