// basic.cpp 
// compile
// cl.exe io.cpp /Ox /Zi /I%BOOST_ROOT% /EHsc
// or
// g++ io.cpp -std=gnu++11 -O3 -I$BOOST_ROOT


#include <iostream>
#include "daily/trace/log.h"
#include "daily/trace/debugbreak.h"

int main()
{
	// Make sure LOG compiles.
	LOG() << "Testing " << 1 << ", " << 2 << "..." << std::endl;
}

