
#ifndef pool_allocator_h__
#define pool_allocator_h__

#include <typeinfo>
#include <stdint.h>
#include <string>

struct block_head;
class fixed_size_allocator {
public:
	~fixed_size_allocator();
	void* alloc(size_t);
	void free(void* p, size_t);
	static fixed_size_allocator *get(size_t,bool,const char* for_class=0);
private:
	fixed_size_allocator(size_t num_objects, size_t object_size, const char* for_class); 
	fixed_size_allocator(const fixed_size_allocator&);
	fixed_size_allocator& operator=(const fixed_size_allocator&);
	size_t num_objects;
	size_t object_size;
	void *head_of_free_list;
	block_head *head;
	// Debug
	std::string name;
	int allocs;
	int frees;
	int current;
	int max;
	int blocks;
};
//**************************************************************************************
template<typename T, int num_objects=64>
class fixed_size_object {
public:
	static void* operator new(size_t size) {
		return fixed_size_allocator::get(size, true, typeid(T).name())->alloc(size);
	}
	static void* operator new(size_t size, void* p) {
		return p;
	}
	static void operator delete(void* p, size_t size) {
		fixed_size_allocator::get(size, false)->free(p, size);
	}
private:
};
#if 0 // under construction
template<typename T>
class block_allocator_stl {
public : 
	//    typedefs

	typedef T value_type;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;
public : 
	//    convert an allocator<T> to allocator<U>

	template<typename U>
	struct rebind {
		typedef block_allocator_stl<U> other;
	};

	inline explicit block_allocator_stl() {}
	inline ~block_allocator_stl() {}
	inline block_allocator_stl(block_allocator_stl const&) {}
	template<typename U>
	inline block_allocator_stl(block_allocator_stl<U> const&) {}
	inline block_allocator_stl<T> &operator=(block_allocator_stl<T> const&) {}
	template<typename U>
	inline block_allocator_stl<T> &operator=(block_allocator_stl<U> const&) {}

	//    address

	inline pointer address(reference r) { return &r; }
	inline const_pointer address(const_reference r) { return &r; }

	//    memory allocation
	inline pointer allocate(size_type cnt, const void*) { 
			return reinterpret_cast<pointer>(fixed_size_allocator::get(cnt * sizeof (T), true, typeid(T).name())->alloc(cnt * sizeof (T)));
			//			return reinterpret_cast<pointer>(::operator new(cnt * sizeof (T))); 
	}
	inline void deallocate(pointer p, size_type cnt) { 
		fixed_size_allocator::get(cnt * sizeof (T), false)->free(p, cnt * sizeof (T));
		//		::operator delete(p); 
	}

	//    size

	inline size_type max_size() const { 
		return SIZE_MAX / sizeof(T);
	}

	inline void construct(pointer _Ptr, value_type& _Val) {
		::new ((void*)_Ptr) value_type(_Val);
	}
	inline void destroy(pointer _Ptr) {
		_Ptr->~value_type();
	}
};
#endif

#endif // pool_allocator_h__