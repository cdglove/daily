// ****************************************************************************
// daily/memory/boost_pool_allocator.h
//
// STL adapter interface to a boost.pool instance.
//
// Chris Glover (c.d.glover@gmail.com)
//
// Nov 30th, 2014
//
// ****************************************************************************
#pragma once
#ifndef _DAILY_MEMORY_BOOSTPOOLALLOCATOR_H_
#define _DAILY_MEMORY_BOOSTPOOLALLOCATOR_H_

#include <boost/pool/pool.hpp>
#include <boost/assert.hpp>

// ----------------------------------------------------------------------------
//
namespace daily {

// ----------------------------------------------------------------------------
//
template<typename T, typename UserAllocator = boost::default_user_allocator_new_delete>
class boost_pool_allocator
{
public:

	typedef T value_type;
	typedef UserAllocator user_alloc;

	boost_pool_allocator(boost::pool<UserAllocator>& pool)
		: pool_(pool)
	{}

	template <typename U> 
	boost_pool_allocator(boost_pool_allocator<U> const& other)
		: pool_(other.pool_)
	{}

	T* allocate(std::size_t n)
	{
		BOOST_ASSERT(n == 1);
		BOOST_ASSERT(sizeof(T) <= pool_.get_requested_size());
		return static_cast<T*>(pool_.malloc());
	}

	void deallocate(T* p, std::size_t n)
	{
		BOOST_ASSERT(n == 1);
		pool_.free(p);
	}
	// Allocate shared on MSVC isn't using allocator traits
#if _MSC_VER < 1900
	template<typename U>
	struct rebind 
	{ 
		typedef boost_pool_allocator<U, UserAllocator> other;
	};

	void destroy(T* p)
	{
		p->~T();
	}
#endif

private:

	template <typename Other, typename OtherUserAlloc> 
	friend class boost_pool_allocator;

	template <typename U, typename V>
	friend bool operator==(boost_pool_allocator<U> const& a, boost_pool_allocator<V> const& b);

	template <typename U, typename V>
	friend bool operator!=(boost_pool_allocator<U> const& a, boost_pool_allocator<V> const& b);

	boost::pool<UserAllocator>& pool_;
};


// ----------------------------------------------------------------------------
//
template <typename U, typename V>
bool operator==(boost_pool_allocator<U> const& a, boost_pool_allocator<V> const& b)
{
	return &a.pool_ == &b.pool_;
}

template <typename U, typename V>
bool operator!=(boost_pool_allocator<U> const& a, boost_pool_allocator<V> const& b)
{
	return &a.pool_ != &b.pool_;
}

}	// namespace daily

#endif // _DAILY_MEMORY_BOOSTPOOLALLOCATOR_H_