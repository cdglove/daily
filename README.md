daily
=====

Daily is a collection of things I find useful in most code I write

- simple timers
  Start, stop, elapsed time in seconds.  Very easy to use.

- simple program options
  When boost is more than you need.  This is small and header only.

- simple profiling
  Macro based instrumentation that reports where time is going.  Not a replacement for a sampling profiler, but can be better in some situations where inlining prevents symbols from showing in the sample and micro optimizations are needed.
- simple logging
  Verbosity based logging system that can be completely complied out of a shipping app if necessary.

- fast_iostream

  std::iostream wrappers that are significantly faster at number conversion.
  
  The std::iostreams are famously slow compared to say printf or scanf. Many will claim this is due to all of the virtual dispatch involved in iostreams customization.  However, it turns out most of it is due to formatting through std::locale.
  
  Here we present three classes that can eliminate all of the trouble (at the cost of loc formatting) if speed is really an issue for you.
  
  If you're able to fully replace your stream objects then you can use fast_istream and fast_ostream in place of std::istream and std::ostream, or if you can't, simply imbue the streams with the fast_locale object and get most of the benefit while being able to reuse existing iostream based code.
  
  
  I'm adding to it all of the time.  

