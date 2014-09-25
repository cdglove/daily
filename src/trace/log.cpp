// ****************************************************************************
// log.cpp
//
// Basic logging facility for daily.
//
// Chris Glover (c.d.glover@gmail.com)
//
// Oct 21th, 2012
//
// ****************************************************************************

// ----------------------------------------------------------------------------
//
unsigned int g_log_level = 0;
bool		 g_use_buffered_log = true;

// ----------------------------------------------------------------------------
//
void SetLogVerbosity(unsigned int v)
{
	g_log_level = v;
}

// ----------------------------------------------------------------------------
//
void SetLogUseBufferedOutput(bool buffer)
{
	g_use_buffered_log = buffer;
}