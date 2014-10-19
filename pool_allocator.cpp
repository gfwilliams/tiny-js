/*
 * 42TinyJS
 *
 * A fork of TinyJS with the goal to makes a more JavaScript/ECMA compliant engine
 *
 * Authored By Armin Diedering <armin@diedering.de>
 *
 * Copyright (C) 2010-2014 ardisoft
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "pool_allocator.h"
#include <vector>
#include <algorithm>
#include <stdio.h>

struct block {
	block* next;
};
struct block_head {
	block_head* next;
};

static void set_next(void* p, void* next) {
	static_cast<block*>(p)->next = static_cast<block*>(next);
}

static void* get_next(void* p) {
	return static_cast<block*>(p)->next;
}

fixed_size_allocator::fixed_size_allocator( size_t numObjects, size_t objectSize, const char *for_class )
{
	num_objects = numObjects;
	object_size = objectSize >= sizeof(block) ? objectSize : sizeof(block);

	head_of_free_list = head = 0;

#ifdef DEBUG_POOL_ALLOCATOR
	if(for_class) name = for_class;
	allocs=
	frees=
	max =
	current=
	blocks =
#endif
	refs = 0;
}

fixed_size_allocator::~fixed_size_allocator()
{
	while(head) {
		char *p = (char*)head;
		head = head->next;
		delete [] p;
	}
#ifdef DEBUG_POOL_ALLOCATOR
#	ifndef LOG_POOL_ALLOCATOR_MEMORY_USAGE
	if(refs) {
#	endif
	fprintf(stderr, "allocator [%s](%d) destroyed\n", name.c_str(), object_size);
	fprintf(stderr, "  allocs:%i, ", allocs);
	fprintf(stderr, "frees:%i, ", frees);
	fprintf(stderr, "max:%i, ", max);
	fprintf(stderr, "blocks:%i\n", blocks);
	if(refs) fprintf(stderr, "************ %i x not freed ************\n", refs);
	fprintf(stderr, "\n");
#	ifndef LOG_POOL_ALLOCATOR_MEMORY_USAGE
	}
#	endif
#endif
}

void* fixed_size_allocator::_alloc( size_t ) {
	refs++;
#ifdef DEBUG_POOL_ALLOCATOR
	allocs++;current++;
	if(current>max)max=current;
#endif
	void* p = head_of_free_list;
	if(p)  {
		head_of_free_list = get_next(p);
	} else {

		char* new_block = new char[sizeof(block_head) + num_objects * object_size];
		((block_head*)new_block)->next = head;
		head = (block_head*)new_block;
		new_block += sizeof(block_head);
		for(std::size_t i = object_size; i < (num_objects - 1) * object_size; i += object_size) {
			set_next(&new_block[i], &new_block[i + object_size]);
		}
		set_next(&new_block[(num_objects - 1) * object_size], 0);
		p = new_block;
		head_of_free_list = &new_block[object_size];

#ifdef DEBUG_POOL_ALLOCATOR
		blocks++;
#endif
	}
	return p;
}
#include <assert.h>
#ifndef ASSERT
#	define ASSERT(X) assert(X)
#endif

bool fixed_size_allocator::_free( void* p, size_t ) {
	if(p == 0) return refs==0;
	refs--;
#ifdef DEBUG_POOL_ALLOCATOR
	ASSERT(refs>=0);
	frees++;current--;
#endif
	block* dead_object = static_cast<block*>(p);

	dead_object->next = static_cast<block*>(head_of_free_list);
	head_of_free_list = dead_object;
	return refs==0;
}
typedef std::vector<fixed_size_allocator*> allocator_pool_t;
typedef allocator_pool_t::iterator allocator_pool_it;

static bool compare_allocator_pool(fixed_size_allocator *allocator, size_t Size) {
	return allocator->objectSize() < Size;
}

static class _allocator_pool
{
public:
	_allocator_pool() : allocator_pool(0) {
#ifdef LOG_POOL_ALLOCATOR_MEMORY_USAGE
	last_ok = last_access = 0;
#endif
	}
	~_allocator_pool() {
		if(allocator_pool && !allocator_pool->empty())
			for(allocator_pool_it it = allocator_pool->begin(); it!=allocator_pool->end(); it++)
				delete *it;
		delete allocator_pool;
#ifdef LOG_POOL_ALLOCATOR_MEMORY_USAGE
		if(last_access) fprintf(stderr, "last_ok:%i(%i)=%i%%\n", last_ok, last_access, (last_ok*100)/last_access);
#endif
	}
	allocator_pool_it *findAllocator(size_t size, allocator_pool_it &it) {
		if(!allocator_pool) return 0;
		it = lower_bound(allocator_pool->begin(), allocator_pool->end(), size, compare_allocator_pool);
		if(it != allocator_pool->end() && (*it)->objectSize() == size)
			return &it;
		return 0;
	}
	fixed_size_allocator *checkLastAllocator(size_t size) {
#ifdef LOG_POOL_ALLOCATOR_MEMORY_USAGE
		last_access++;
#endif
		if(last_allocate_allocator && last_allocate_allocator->objectSize()==size) {
#ifdef LOG_POOL_ALLOCATOR_MEMORY_USAGE
			last_ok++;
#endif
			return last_allocate_allocator;
		}
		else if(last_free_allocator && last_free_allocator->objectSize()==size) {
#ifdef LOG_POOL_ALLOCATOR_MEMORY_USAGE
			last_ok++;
#endif
			return last_free_allocator;
		} else
			return 0;
	}
	void removeAllocator(allocator_pool_it it) {
		if(last_allocate_allocator == *it) last_allocate_allocator = 0;
		if(last_free_allocator == *it) last_free_allocator = 0;
		delete *it; allocator_pool->erase(it);
		if(allocator_pool->empty()) { 
			delete allocator_pool; allocator_pool=0; 
		}
	}
	allocator_pool_t *allocator_pool;
	fixed_size_allocator *last_allocate_allocator;
	fixed_size_allocator *last_free_allocator;
#ifdef LOG_POOL_ALLOCATOR_MEMORY_USAGE
	int last_ok;
	int last_access;
#endif
}allocator_pool;

//#define WITH_TIME_LOGGER
#include "time_logger.h"

TimeLoggerCreate(alloc, false);
TimeLoggerCreate(free, false);
#ifdef NO_THREADING
#	define LOCK do{}while(0)
#else
CScriptMutex fixed_size_allocator::locker;
class lock_help {
public:
	lock_help() { fixed_size_allocator::locker.lock(); }
	~lock_help() { fixed_size_allocator::locker.unlock(); }
};
#define LOCK lock_help lock
#endif
void* fixed_size_allocator::alloc(size_t size, const char *for_class) {
	TimeLoggerHelper(alloc);
	LOCK;
	if(!allocator_pool.allocator_pool) {
		allocator_pool.allocator_pool = new allocator_pool_t();
		allocator_pool.last_allocate_allocator = allocator_pool.last_free_allocator = 0;
	}
	fixed_size_allocator *last = allocator_pool.checkLastAllocator(size);
	if(last) 
		return last->_alloc(size);
	else {
		allocator_pool_it it; if(allocator_pool.findAllocator(size, it))
			return (allocator_pool.last_allocate_allocator = *(it))->_alloc(size);
		else {
			return (allocator_pool.last_allocate_allocator = (*allocator_pool.allocator_pool->insert(it, new fixed_size_allocator(64, size, for_class))))->_alloc(size);
		}
	}
}
void fixed_size_allocator::free(void *p, size_t size) {
	TimeLoggerHelper(free);
	LOCK;
	if(!allocator_pool.allocator_pool) {
		ASSERT(0/* free called but not allocator defined*/);
		return;
	}
	fixed_size_allocator *last = allocator_pool.checkLastAllocator(size);
	if(last) { 
		if( last->_free(p, size) ) {
			allocator_pool_it it; if(allocator_pool.findAllocator(size, it))
				allocator_pool.removeAllocator(it);
		}
	} else {
		allocator_pool_it it; if(allocator_pool.findAllocator(size, it)) {
			if( (allocator_pool.last_free_allocator = *it)->_free(p, size) )
				allocator_pool.removeAllocator(it);
		} else
			ASSERT(0/* free called but not allocator defined*/);
	}
}
