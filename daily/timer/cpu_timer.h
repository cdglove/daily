// ****************************************************************************
// daily/timer/cpu_timer.h
//
// Maps section names to timer values. Handles timer reporting.
//
// Chris Glover (c.d.glover@gmail.com)
//
// Dec 13th, 2014
//
// ****************************************************************************
#pragma once
#ifndef _DAILY_TIMER_CPUTIMER_H_
#define _DAILY_TIMER_CPUTIMER_H_

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