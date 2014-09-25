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

// ----------------------------------------------------------------------------
//
inline std::ostream& LOG(unsigned int verbosity = 0)
{
	extern unsigned int g_log_level;
	extern bool			g_use_buffered_log;

	if(g_log_level >= verbosity)
	{
		return g_use_buffered_log ? std::clog : std::cerr;
	}
	else
	{
		static boost::onullstream cnull;
		return cnull;
	}
}

// ----------------------------------------------------------------------------
//
void SetLogVerbosity(unsigned int v);
void SetLogUseBufferedOutput(bool buffer);

#endif // _DAILY_LOG_H_