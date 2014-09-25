// ****************************************************************************
// boost_assert.h
//
// Adapters for using boost assert within daily
//
// Chris Glover (c.d.glover@gmail.com)
//
// Oct 21th, 2012
//
// ****************************************************************************

#include "daily/trace/debugbreak.h"
#include "daily/trace/log.h"

// ----------------------------------------------------------------------------
// Required when linking against a boost with boost assert enabled.
namespace boost
{
	void assertion_failed(char const * expr, char const * function, char const * file, long line)
	{
		LOG()
			<< "***** Internal Program Error - assertion (" << expr << ") failed in "
			<< function << ":\n"
			<< file << '(' << line << ")" << std::endl
		;

		DEBUG_BREAK();
      
	}

	void assertion_failed_msg(char const * expr, char const * msg, char const * function, char const * file, long line)
	{
		LOG()
			<< "***** Internal Program Error - assertion (" << expr << ") failed in "
			<< function << ":\n"
			<< file << '(' << line << "): " << msg << std::endl
		;

		DEBUG_BREAK();
      
	}

} // namespace boost