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

#include "daily/timer/timer_map.h"

#if DAILY_ENABLE_INSTRUMENTATION
#  define DAILY_AUTO_INSTRUMENT_NODE(name) \
	static daily::cpu_timer& name ## _timer = daily::timer_map::get_default().create_node(#name); \
	daily::cpu_timer_scope name ## _auto_timer_scope(name ## _timer)

#  define DAILY_DECLARE_INSTRUMENT_NODE(name) \
	static daily::cpu_node name ## _timer(#name)

#  define DAILY_START_INSTRUMENT_NODE(name) \
	name ## _timer.start()

#  define DAILY_STOP_INSTRUMENT_NODE(name) \
	name ## _timer.stop()

#  define DAILY_RESUME_INSTRUMENT_NODE(name) \
	name ## _timer.resume()
#else
#  define DAILY_AUTO_INSTRUMENT_NODE(name)
#  define DAILY_DECLARE_INSTRUMENT_NODE(name) 
#  define DAILY_START_INSTRUMENT_NODE(name)
#  define DAILY_STOP_INSTRUMENT_NODE(name)
#  define DAILY_RESUME_INSTRUMENT_NODE(name)
#endif

#endif // _DAILY_TIMER_INSTRUMENT_H_
