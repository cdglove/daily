// ****************************************************************************
// log.h
//
// Basic logging facility for daily.
//
// Chris Glover (c.d.glover@gmail.com)
//
// Oct 21th, 2012
//
// ****************************************************************************
#pragma once
#ifndef _DAILY_LOG_H_
#define _DAILY_LOG_H_

#include <iostream>
#include <boost/test/utils/nullstream.hpp>

#ifndef DAILY_ENABLE_LOGGING
#  define DAILY_ENABLE_LOGGING 1
#endif

// ----------------------------------------------------------------------------
//
namespace daily
{
	class logger
	{
	public:

		logger(std::ostream& sink)
			: sink_(sink)
		{}

		template<typename T>
		logger& operator<<(T&& t)
		{
		#if DAILY_ENABLE_LOGGING
			sink_ << std::forward<T>(t);
		#endif
			return *this;
		}

		// Allow io-maniplulators.
		logger& operator<<(std::ostream&(*f)(std::ostream&))
		{
		#if DAILY_ENABLE_LOGGING
			sink_ << f;
		#endif
			return *this;
		}

		std::ostream& sink() 
		{
			return sink_;
		}

	private:

		std::ostream& sink_;
	};
}

// ----------------------------------------------------------------------------
//
inline daily::logger LOG(unsigned int verbosity = 0)
{
	extern unsigned int g_log_level;
	extern bool			g_use_buffered_log;

	if(g_log_level >= verbosity)
	{
		return g_use_buffered_log ? daily::logger(std::clog) : daily::logger(std::cerr);
	}
	else
	{
		static boost::onullstream cnull;
		return daily::logger(cnull);
	}
}

// ----------------------------------------------------------------------------
//
void SetLogVerbosity(unsigned int v);
void SetLogUseBufferedOutput(bool buffer);

#endif // _DAILY_LOG_H_