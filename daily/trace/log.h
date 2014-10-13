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

#ifndef DAILY_ENABLE_FAST_LOG
#  define DAILY_ENABLE_FAST_LOG 0
#endif

#if DAILY_ENABLE_FAST_LOG
#  include <daily/fast_iostream/fast_ostream.h>
#endif
// ----------------------------------------------------------------------------
//
namespace daily
{
	class logger
	{
	public:
	
	#if DAILY_ENABLE_FAST_LOG
		typedef daily::fast_ostream<std::ostream> sink_impl;
	#else
		typedef std::ostream sink_impl;
	#endif
		

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
		  #if DAILY_ENABLE_FAST_LOG
			sink_.stream() << f;
		  #else
			sink_ << f;
		  #endif
		#endif
			return *this;
		}

		sink_impl& sink() 
		{
			return sink_;
		}

	private:

		sink_impl& sink_;
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