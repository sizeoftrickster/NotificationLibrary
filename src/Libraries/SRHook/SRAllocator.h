#ifndef SRALLOCATOR_H
#define SRALLOCATOR_H

#include <cstdint>
#include <deque>
#include <mutex>

namespace SRHook {
	struct ptr_t {
		uint8_t *ptr;
		std::size_t	 size;
		ptr_t( uint8_t *ptr = nullptr, std::size_t size = 0 ) : ptr( ptr ), size( size ) {}
		ptr_t( const ptr_t &copy ) { operator=( copy ); }

		bool operator<( const ptr_t &rhs ) const noexcept { return ptr < rhs.ptr; }
		bool operator==( const ptr_t &rhs ) const noexcept { return ptr == rhs.ptr && size == rhs.size; }
		void operator=( const ptr_t &rhs ) noexcept {
			ptr	 = rhs.ptr;
			size = rhs.size;
		}

		bool flush() const noexcept;
	};

	class allocator_t {
		friend allocator_t &Allocator();
		allocator_t() {}
		~allocator_t() {}

		struct page_t {
			page_t( const ptr_t &ptr ) : ptr( ptr ) { free.push_back( ptr ); }

			ptr_t allocate( std::size_t size ) noexcept;
			bool  deallocate( const ptr_t &_ptr ) noexcept;
			bool  reallocate( ptr_t &_ptr, std::size_t size ) noexcept;

			bool empty() const noexcept {
				return free.size() == 1 && free.front().ptr == ptr.ptr && free.front().size == ptr.size;
			}
			const ptr_t &get_ptr() const { return ptr; }

			bool operator<( const page_t &rhs ) const noexcept { return ptr < rhs.ptr; }
			bool operator==( const page_t &rhs ) const noexcept { return ptr == rhs.ptr && free == rhs.free; }

		private:
			ptr_t			  ptr;
			std::deque<ptr_t> free;

			void dbg( const char *fmt, ... );
		};

		std::deque<page_t> pages;
		std::mutex		   mtx;

	public:
		ptr_t allocate( std::size_t size ) noexcept;
		bool  deallocate( ptr_t &_ptr ) noexcept;
		bool  reallocate( ptr_t &_ptr, std::size_t size ) noexcept;

	protected:
		bool   unprotect( const ptr_t &ptr ) const noexcept;
		std::size_t page_size() const noexcept;

		void dbg( const char *fmt, ... );
	};
	allocator_t &Allocator();
} // namespace SRHook

#endif // SRALLOCATOR_H
