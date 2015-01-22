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
#include <boost/utility/string_ref.hpp>

#if DAILY_INSTRUMENTATION_USE_BOOST_TIMER
#include <boost/timer/timer.hpp>
namespace daily
{
	// ----------------------------------------------------------------------------
	//
	class cpu_timer
	{
	public:
		cpu_timer(char const* name)
			: name_(name)
		{}

		char const* name() const
		{
			return name_;
		}

		void start()
		{
			timer_.start();
		}

		void stop()
		{
			timer_.stop();
		}

		void resume()
		{
			timer_.resume();
		}

		float elapsed() const
		{
			return boost::chrono::duration_cast<
				boost::chrono::duration<float>
			>(boost::chrono::nanoseconds(timer_.elapsed().user)).count();
		}

	private:

		// No copying.
		cpu_timer(cpu_timer const&);
		cpu_timer& operator=(cpu_timer);

		char const* name_;
		boost::timer::cpu_timer timer_;
	};

	std::ostream& operator <<(std::ostream& out, cpu_timer const& node )
	{
		float elapsed = node.elapsed();
		out << node.name() << " : " << elapsed;
		return out;
	}
}
#else
namespace daily
{
	// ----------------------------------------------------------------------------
	//
	class cpu_timer
	{
	public:
		cpu_timer(char const* name)
			: name_(name)
			, elapsed_(std::chrono::nanoseconds(0))
		{}

		char const* name() const
		{
			return name_;
		}

		void start()
		{
			elapsed_ = std::chrono::nanoseconds(0);
			start_ = std::chrono::system_clock::now();
		}

		void stop()
		{
			elapsed_ += std::chrono::system_clock::now() - start_;
		}

		void resume()
		{
			start_ = std::chrono::system_clock::now();
		}

		float elapsed() const
		{
			return std::chrono::duration_cast<std::chrono::duration<float>>(elapsed_).count();
		}

	private:

		// No copying.
		cpu_timer(cpu_timer const&);
		cpu_timer& operator=(cpu_timer);

		std::chrono::time_point<std::chrono::system_clock> start_;
		char const* name_;
		std::chrono::nanoseconds elapsed_;
	};

	std::ostream& operator <<(std::ostream& out, cpu_timer const& node )
	{
		float elapsed = node.elapsed();
		out << node.name() << " : " << elapsed;
		return out;
	}
}
#endif 

// ----------------------------------------------------------------------------
//
namespace daily {

// ----------------------------------------------------------------------------
//
class auto_cpu_timer : public cpu_timer
{
public:

	auto_cpu_timer(char const* name)
		: cpu_timer(name)
		, out_(std::cout)
	{}

	auto_cpu_timer(char const* name, std::ostream& out)
		: cpu_timer(name)
		, out_(std::cout)
	{}

	~auto_cpu_timer()
	{
		out_ << *this << '\n';
	}

private:

	std::ostream& out_;
};

// ----------------------------------------------------------------------------
//
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

	cpu_timer& create_node(char const* name)
	{
		std::lock_guard<std::mutex> lock(node_lock_);

		nodes_.emplace_back(name);
		return nodes_.back();
	}

	static const struct keep_zeroes_tag {} keep_zeroes;
	static const struct discard_zeroes_tag {} discard_zeroes;
	static const struct sort_time_tag {} sort_time;
	static const struct sort_alpha_tag {} sort_alpha;

	void report(std::ostream& to) const
	{
		return report_impl(to, get_filter(discard_zeroes), get_sorter(sort_time));;
	}

	template<typename FilterTag, typename SorterTag>
	void report(std::ostream& to, FilterTag f, SorterTag s) const
	{
		report_impl(to, get_filter(f), get_sorter(s));
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

	struct filter_none_t
	{
		bool operator()(double val) const
		{
			return false;
		}
	};

	struct filter_zeroes_t
	{
		bool operator()(double val) const
		{
			return val == 0;
		}
	};

	struct sort_time_t
	{
		template<typename T>
		void operator()(T& t) const
		{
			std::sort(t.begin(), t.end(), detail::compare_tuple_greater<1>(t[0]));
		}
	};

	struct sort_alpha_t
	{
		template<typename T>
		void operator()(T& t) const
		{
			std::sort(t.begin(), t.end(), detail::compare_tuple_greater<0>(t[0]));
		}
	};

	filter_none_t get_filter(keep_zeroes_tag) const { return filter_none_t(); }
	filter_zeroes_t get_filter(discard_zeroes_tag) const { return filter_zeroes_t(); }
	sort_time_t get_sorter(sort_time_tag) const { return sort_time_t(); }
	sort_alpha_t get_sorter(sort_alpha_tag) const { return sort_alpha_t(); }

	template<typename Filter, typename Sorter>
	void report_impl(std::ostream& to, Filter f, Sorter s) const
	{
		std::lock_guard<std::mutex> lock(node_lock_);

		typedef std::tuple<boost::string_ref, double> result_type;
		using std::get;

		std::vector<result_type> result;

		for(auto i = nodes_.begin(); i != nodes_.end(); ++i)
		{
			result.push_back(std::make_tuple(i->name(), i->elapsed()));
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
				double total = std::accumulate(range.first, range.second, 0.0, [](double& accumulator, result_type const& node)
				{
					return accumulator + get<1>(node);
				});

				get<1>(*range.first++) = total;

				while(range.first != range.second)
				{
					get<1>(*range.first++) = 0.0;
				}

				curr = range.second;				
			}

			result.erase(std::remove_if(result.begin(), result.end(), [f](result_type const& r)
			{
				return f(get<1>(r));
			}), result.end());

			if(!result.empty())
			{
				s(result);

				for(auto i = result.begin(); i != result.end(); ++i)
				{
					std::ios_base::fmtflags flags = to.flags();
					to << std::fixed << std::setprecision( 6 ) << std::setfill( '0' );
					to << get<0>(*i) << " : " << get<1>(*i) << '\n';
					to.flags(flags);
				}
			}
		}
	}

	// No copying.
	timer_map(timer_map const&);
	timer_map& operator=(timer_map);

	mutable std::mutex node_lock_;
	std::deque<cpu_timer> nodes_;
};

// ----------------------------------------------------------------------------
//
class cpu_timer_scope
{
public:
	cpu_timer_scope(cpu_timer& source)
		: source_node_(source)
	{
		source_node_.resume();
	}

	~cpu_timer_scope()
	{
		source_node_.stop();
	}

private:

	// No copying.
	cpu_timer_scope(cpu_timer_scope const&);
	cpu_timer_scope& operator=(cpu_timer_scope);

	cpu_timer& source_node_;
};

} // namespace daily

#endif // _DAILY_TIMER_TIMERMAP_H_