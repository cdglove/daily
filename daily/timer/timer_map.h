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
#include <unordered_map>
#include <ostream>

#include <iomanip>
#include <daily/timer/cpu_timer.h>
#include <boost/utility/string_ref.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/functional/hash.hpp>

// ----------------------------------------------------------------------------
// Workaround lack of hash support for boost::string_ref
namespace std
{
	template<>
	struct hash<boost::string_ref>
	{
		size_t operator()(boost::string_ref const& sr) const 
		{
			return boost::hash_range(sr.begin(), sr.end());
		}
	};
}

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
		result_type(
			boost::string_ref name_, 
			double latest_time_,
			double average_time_,
			double max_time_,
			double min_time_)
			: name(name_)
			, latest_time(latest_time_)
			, average_time(average_time_)
			, max_time(max_time_)
			, min_time(min_time_)
		{}

		boost::string_ref name;
		double latest_time;
		double average_time;
		double max_time;
		double min_time;
	};

	timer_map(std::size_t history_length = 100)
		: history_length_(history_length)
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
		sample_history_[name] = boost::circular_buffer<double>(history_length_);

		return nodes_.back();
	}

	void maybe_create_node(char const* name, cpu_timer*& timer_node)
	{
		if(timer_node)
			return;

		std::lock_guard<std::mutex> lock(node_lock_);

		// Double checked locking pattern.
		if(timer_node)
			return;

		nodes_.emplace_back(name);
		sample_history_[name] = boost::circular_buffer<double>(history_length_);
		timer_node = &nodes_.back();
	}

	template<typename OutputIter>
	void gather_report(OutputIter out)
	{
		return report_impl( 
			[&out](result_type r)
			{
				*out++ = std::move(r);
			}
		);
	}

	template<typename Handler>
	void emit_report(Handler h)
	{
		return report_impl(h);
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

	struct sample_type
	{
		sample_type(boost::string_ref name_, double time_)
			: name(name_)
			, time(time_)
		{}

		boost::string_ref name;
		double			  time;
	};

	struct compare_name_less
	{
		bool operator()(sample_type const& a, sample_type const& b)
		{
			return a.name < b.name;
		}
	};

	struct compare_time_greater
	{
		bool operator()(sample_type const& a, sample_type const& b)
		{
			return a.time > b.time;
		}
	};

	template<typename Handler>
	void report_impl(Handler handler)
	{
		std::lock_guard<std::mutex> lock(node_lock_);

		std::vector<sample_type> result;
		std::transform(
			nodes_.begin(), 
			nodes_.end(), 
			std::back_inserter(result),
			[](cpu_timer const& node)
			{
				return timer_map::sample_type(node.name(), node.elapsed());
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

				double current_total = std::accumulate(
					range.first, range.second, 0.0, 
					[](double accumulator, sample_type const& node)
					{
						return accumulator + node.time;
					}
				);

				// Aggregate some history			
				auto&& this_history = sample_history_.at(range.first->name);
				this_history.push_back(current_total);
				
				// Find the min and max from history
				auto min_max_sample = std::minmax_element(this_history.begin(), this_history.end());

				// Find the average.
				double history_avg 
					= std::accumulate(this_history.begin(), this_history.end(), 0.0) 
					/ this_history.size()
				;

				handler(result_type(
					range.first->name, 
					current_total,
					history_avg,
					*min_max_sample.first,
					*min_max_sample.second
				));
				
				curr = range.second;	
			}		
		}
	}

	// No copying.
	timer_map(timer_map const&);
	timer_map& operator=(timer_map);

	mutable std::mutex node_lock_;
	std::deque<cpu_timer> nodes_;
	std::unordered_map<
		boost::string_ref, boost::circular_buffer<double>
	> sample_history_;

	std::size_t history_length_;
};

std::ostream& operator <<(std::ostream& out, timer_map::result_type const& r)
{
	out << std::setw(32) << r.name
		<< std::setw(10) << r.latest_time
		<< std::setw(10) << r.average_time
		<< std::setw(10) << r.max_time
		<< std::setw(10) << r.min_time
	;

	return out;
}

} // namespace daily

#endif // _DAILY_TIMER_TIMERMAP_H_