// ****************************************************************************
// debugbreak.h
//
// Macros to perform a debug break on various platforms.
//
// Chris Glover (c.d.glover@gmail.com)
//
// Oct 21th, 2012
//
// ****************************************************************************
#pragma once
#ifndef _DAILY_DEBUG_BREAK_H_
#define _DAILY_DEBUG_BREAK_H_

#if _MSC_VER
#	define DEBUG_BREAK() __debugbreak()
#elif defined(SIGTRAP)
#	include <signal.h>
#	define DEBUG_BREAK() raise(SIGTRAP)
#elif WIN32
#	include <windows.h>
#	define DEBUG_BREAK() DebugBreak()
#else
#   include <cassert>
#	define DEBUG_BREAK() assert(false)
#endif

#endif // _DAILY_DEBUG_BREAK_H_