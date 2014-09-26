// ****************************************************************************
// daily/timer/instrument.h
//
// Macro based instrumentation system that allows fine grained control over
// what's instrumented.  Useful for profiling sections of code that are
// inlined by the compiler.
//
// Chris Glover (c.d.glover@gmail.com)
//
// Sept 17th, 2014
//
// ****************************************************************************
#pragma once
#ifndef _DAILY_TIMER_INSTRUMENT_H_
#define _DAILY_TIMER_INSTRUMENT_H_

#include <vector>
#include <chrono>
#include <tuple>
#include <boost/utility/string_ref.hpp>

namespace daily {

// ----------------------------------------------------------------------------
//
class timer_node
{
public:
	timer_node(char const* name)
		: name_(name)
		, elapsed_(std::chrono::nanoseconds(0))
	{}

	char const* name() const
	{
		return name_;
	}

	void reset()
	{
		elapsed_ = std::chrono::nanoseconds(0);
		start_ = std::chrono::system_clock::now();
	}

	void start()
	{
		start_ = std::chrono::system_clock::now();
	}

	void stop()
	{
		elapsed_ += std::chrono::system_clock::now() - start_;
	}

	float elapsed() const
	{
		return std::chrono::duration_cast<std::chrono::duration<float>>(elapsed_).count();
	}

private:

	// No copying.
	timer_node(timer_node const&);
	timer_node& operator=(timer_node);

	std::chrono::time_point<std::chrono::system_clock> start_;
	char const* name_;
	std::chrono::nanoseconds elapsed_;
};

namespace detail 
{
	template<typename Tuple, int Idx>
	struct tuple_less_comparer
	{
		bool operator()(Tuple const& a, Tuple const& b)
		{
			using std::get;
			return get<Idx>(a) < get<Idx>(b);
		}
	};	

	template<typename Tuple, int Idx>
	struct tuple_greater_comparer
	{
		bool operator()(Tuple const& a, Tuple const& b)
		{
			using std::get;
			return get<Idx>(a) > get<Idx>(b);
		}
	};	

	template<typename Tuple, int Idx>
	struct tuple_eq_comparer
	{
		bool operator()(Tuple const& a, Tuple const& b)
		{
			using std::get;
			return get<Idx>(a) == get<Idx>(b);
		}
	};	

	template<int Idx, typename Tuple>
	tuple_less_comparer<Tuple, Idx> compare_tuple_less(Tuple const&)
	{
		return tuple_less_comparer<Tuple, Idx>();
	}

	template<int Idx, typename Tuple>
	tuple_greater_comparer<Tuple, Idx> compare_tuple_greater(Tuple const&)
	{
		return tuple_greater_comparer<Tuple, Idx>();
	}

	template<int Idx, typename Tuple>
	tuple_eq_comparer<Tuple, Idx> compare_tuple_eq(Tuple const&)
	{
		return tuple_eq_comparer<Tuple, Idx>();
	}
}

// ----------------------------------------------------------------------------
//
class timer_map
{
public:

	timer_map()
	{}	

	static timer_map& get_default()
	{
		static timer_map map_;
		return map_;
	}

	void register_node(timer_node& node)
	{
		nodes_.push_back(&node);
	}

	void report(std::ostream& to) const
	{
		typedef std::tuple<boost::string_ref, float> result_type;
		using std::get;
		
		std::vector<result_type> result;

		for(auto i = nodes_.begin(); i != nodes_.end(); ++i)
        {
			result.push_back(std::make_tuple((*i)->name(), (*i)->elapsed()));
		}

		if(result.size() > 0)
		{
			// Sort by name so we can accumulate results.
			std::sort(result.begin(), result.end(), detail::compare_tuple_less<0>(result[0]));

			auto curr = result.begin();
			auto end = result.end();

			while(curr != end)
			{
				// Accumulate adjacent results with the same name.
				auto range = std::equal_range(curr, end, *curr, detail::compare_tuple_less<0>(result[0]));
				float total = std::accumulate(range.first, range.second, 0.f, [](float& accumulator, result_type const& node)
				{
					return accumulator + get<1>(node);
				});

                get<1>(*range.first++) = total;

				while(range.first != range.second)
				{
                    get<1>(*range.first++) = 0.f;
				}
				
				curr = range.second;				
			}

			result.erase(std::remove_if(result.begin(), result.end(), [](result_type const& r)
			{
				return get<1>(r) == 0.f;
			}), result.end());

			std::sort(result.begin(), result.end(), detail::compare_tuple_greater<1>(result[0]));

			for(auto i = result.begin(); i != result.end(); ++i)
			{
				to << get<0>(*i) << " : " << get<1>(*i) << '\n';
			}
		}
	}

	void reset_all()
	{
		for(auto i = nodes_.begin(); i != nodes_.end(); ++i)
		{
			(*i)->reset();
		}
	}

private:

	// No copying.
	timer_map(timer_map const&);
	timer_map& operator=(timer_map);

	std::vector<timer_node*> nodes_;
};

// ----------------------------------------------------------------------------
//
class auto_timer_scope
{
public:
	auto_timer_scope(timer_node& source)
		: source_node_(source)
	{
        source_node_.start();
	}

	~auto_timer_scope()
	{
		source_node_.stop();
	}

private:

	// No copying.
	auto_timer_scope(auto_timer_scope const&);
	auto_timer_scope& operator=(auto_timer_scope);

	timer_node& source_node_;
};

// ----------------------------------------------------------------------------
//
class register_timer
{
public:

	register_timer(timer_node& timer, timer_map& map)
	{
		map.register_node(timer);
	}
};

#if DAILY_ENABLE_INSTRUMENTATION
#  define DAILY_AUTO_INSTRUMENT_NODE(name) \
	static daily::timer_node name ## _timer(#name); \
	static daily::register_timer name ## _register(name ## _timer, daily::timer_map::get_default()); \
	daily::auto_timer_scope name ## _auto_timer_scope(name ## _timer)

#  define DAILY_DECLARE_INSTRUMENT_NODE(name) \
	static daily::timer_node name ## _timer(#name)

#  define DAILY_START_INSTRUMENT_NODE(name) \
	name ## _timer.start()

#  define DAILY_STOP_INSTRUMENT_NODE(name) \
	name ## _timer.stop()

#  define DAILY_RESET_INSTRUMENT_NODE(name) \
	name ## _timer.reset()
#else
#  define DAILY_AUTO_INSTRUMENT_NODE(name)
#  define DAILY_DECLARE_INSTRUMENT_NODE(name) 
#  define DAILY_START_INSTRUMENT_NODE(name)
#  define DAILY_STOP_INSTRUMENT_NODE(name)
#  define DAILY_RESET_INSTRUMENT_NODE(name)
#endif
}
#endif // _DAILY_TIMER_INSTRUMENT_H_
