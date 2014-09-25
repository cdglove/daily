// ****************************************************************************
// daily/timer/timer.h
//
// Simple timer class that can be used for timing sections of code etc.
//
// Chris Glover (c.d.glover@gmail.com)
//
// June 8th, 2014
//
// ****************************************************************************
#pragma once
#ifndef _DAILY_TIMER_TIMER_H_
#define _DAILY_TIMER_TIMER_H_

#include <chrono>

namespace daily {
	
// ----------------------------------------------------------------------------
//
class timer
{
public:
	timer()
	{
		reset();
	}

	float elapsed()
	{
		auto e = std::chrono::system_clock::now() - start_;
		return std::chrono::duration_cast<std::chrono::duration<float>>(e).count();
	}

	void reset()
	{
		start_ = std::chrono::system_clock::now();
	}

private:

	std::chrono::time_point<std::chrono::system_clock> start_;
};

}

#endif // _DAILY_TIMER_TIMER_H_