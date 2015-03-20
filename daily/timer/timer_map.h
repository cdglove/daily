// ****************************************************************************
// daily/timer/timer_map.h
//
// Maps section names to timer values. Handles timer reporting.
//
// Chris Glover (c.d.glover@gmail.com)
//
// Dec 13th, 2014
//
// ****************************************************************************
#pragma once
#ifndef _DAILY_TIMER_TIMERMAP_H_
#define _DAILY_TIMER_TIMERMAP_H_

#include <numeric>
#include <deque>
#include <vector>
#include <chrono>
#include <tuple>
#include <iostream>
#include <mutex>
#include <iomanip>
#include <daily/timer/cpu_timer.h>
#include <boost/utility/string_ref.hpp>

// ----------------------------------------------------------------------------
//
namespace daily {

// ----------------------------------------------------------------------------
//
class timer_map
{
public:

	struct result_type
	{
		result_type(boost::string_ref name_, double time_)
			: name(name_)
			, time(time_)
		{}

		boost::string_ref name;
		double time;
	};

	timer_map()
	{}	

	static timer_map& get_default()
	{
		static timer_map map_;
		return map_;
	}

	cpu_timer& create_node(char const* name)
	{
		std::lock_guard<std::mutex> lock(node_lock_);

		nodes_.emplace_back(name);
		return nodes_.back();
	}

	template<typename OutputIter>
	void gather_report(OutputIter out) const
	{
		return report_impl( 
			[&out](result_type r)
			{
				*out++ = std::move(r);
			}
		);
	}

	template<typename Handler>
	void emit_report(Handler h) const
	{
		return report_impl(out, h);
	}

	void reset_all()
	{
		std::lock_guard<std::mutex> lock(node_lock_);
		for(auto i = nodes_.begin(); i != nodes_.end(); ++i)
		{
			i->start(); i->stop();
		}
	}

	bool empty()
	{
		std::lock_guard<std::mutex> lock(node_lock_);
		return nodes_.empty();
	}

private:

	struct compare_name_less
	{
		bool operator()(result_type const& a, result_type const& b)
		{
			return a.name < b.name;
		}
	};

	struct compare_time_greater
	{
		bool operator()(result_type const& a, result_type const& b)
		{
			return a.time > b.time;
		}
	};

	template<typename Handler>
	void report_impl(Handler handler) const
	{
		SScopeMutexLock lock(node_lock_);

		std::vector<result_type> result;
		std::transform(
			nodes_.begin(), 
			nodes_.end(), 
			std::back_inserter(result),
			[](cpu_timer const& node)
			{
				return timer_map::result_type(node.name(), node.elapsed());
			}
		);

		if(result.size() > 0)
		{
			// Sort by name so we can accumulate results.
			std::sort(result.begin(), result.end(), compare_name_less());

			auto curr = result.begin();
			auto end = result.end();

			while(curr != end)
			{
				// Accumulate adjacent results with the same name.
				auto range = std::equal_range(
					curr, end, *curr, 
					compare_name_less()
				);

				double total = std::accumulate(
					range.first, range.second, 0.0, 
					[](double accumulator, result_type const& node)
					{
						return accumulator + node.time;
					}
				);

				handler(result_type(range.first->name, total));
				curr = range.second;	
			}		
		}
	}

	// No copying.
	timer_map(timer_map const&);
	timer_map& operator=(timer_map);

	mutable std::mutex node_lock_;
	std::deque<cpu_timer> nodes_;
};

} // namespace daily

#endif // _DAILY_TIMER_TIMERMAP_H_