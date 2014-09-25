// io.cpp 
// compile
// cl.exe io.cpp /Ox /Zi /I%BOOST_ROOT% /EHsc
// or
// g++ io.cpp -std=gnu++11 -O3 -I$BOOST_ROOT


#include <iostream>
#include <chrono>
#include <stdio.h>
#include <vector>
#include <sstream>
#include <string>
#include <array>
#include "daily/fast_iostream/fast_ostream.h"
#include "daily/fast_iostream/fast_istream.h"
#include "daily/fast_iostream/fast_locale.h"

// ----------------------------------------------------------------------------
//
namespace boost
{
#ifdef BOOST_NO_EXCEPTIONS
    void throw_exception( std::exception const & e )
	{
	
	}
#endif
}

// ----------------------------------------------------------------------------
//
class Timer
{
public:
	Timer()
	{
		Reset();
	}

	float Elapsed()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::system_clock::now() - mStartTime).count() / 1000.f;
	}

	void Reset()
	{
		mStartTime = std::chrono::system_clock::now();
	}

private:

	std::chrono::time_point<std::chrono::system_clock> mStartTime;
};

int kIterations = 100000;

int main()
{
	Timer t;	
	
	std::ostringstream sout;
		
	t.Reset();
	
	for(int i = 0; i < kIterations; ++i)
	{
		sout << i << " " << t.Elapsed() << "\n";
	}
	
	float sout_time = t.Elapsed();
	
	std::istringstream sin(sout.str());
	
	t.Reset();	
	
	int r = 0;
	float f = 0;
	for(int i = 0; i < kIterations; ++i)
	{
		sin >> r >> f;
	}
		
	float sin_time = t.Elapsed();
	
	sout.imbue(daily::generate_fast_locale(""));
	sin.imbue(daily::generate_fast_locale(""));
	
	t.Reset();
	
	for(int i = 0; i < kIterations; ++i)
	{
		sout << i << " " << t.Elapsed() << "\n";
	}
	
	float new_sout_time = t.Elapsed();

	sin.str(sout.str());
		
	t.Reset();	
	
	for(int i = 0; i < kIterations; ++i)
	{
		sin >> r >> f;
	}
	
	float new_sin_time = t.Elapsed();
	
	std::ostringstream fsout_;
	daily::fast_ostream<std::ostringstream> fsout(fsout_);
		
	t.Reset();
	
	for(int i = 0; i < kIterations; ++i)
	{
		fsout << i << " " << t.Elapsed() << "\n";
	}
	
	float fsout_time = t.Elapsed();
	
	std::istringstream fsin_(fsout_.str());
	daily::fast_istream<std::istringstream> fsin(fsin_);
	
	t.Reset();	
	
	for(int i = 0; i < kIterations; ++i)
	{
		fsin >> r >> f;
	}
		
	float fsin_time = t.Elapsed();
	
	std::cout << "sout took " << sout_time << " seconds" << std::endl;
	std::cout << "sin took " << sin_time << " seconds" << std::endl;
	std::cout << "new sout took " << new_sout_time << " seconds" << std::endl;
	std::cout << "new sin took " << new_sin_time << " seconds" << std::endl;
	std::cout << "fsout took " << fsout_time << " seconds" << std::endl;
	std::cout << "fsin took " << fsin_time << " seconds" << std::endl;

	return 0;
}