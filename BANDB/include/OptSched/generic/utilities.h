/*******************************************************************************
Description:  Contains a few generic utility functions.
Author:       Ghassan Shobaki
Created:      Oct. 1997
Last Update:  Mar. 2011
*******************************************************************************/

#ifndef OPTSCHED_GENERIC_UTILITIES_H
#define OPTSCHED_GENERIC_UTILITIES_H

#ifdef WIN32
  // For struct timeb, ftime().
  #include <sys/timeb.h>
#else
  // For struct tms, times().
  #include <sys/times.h>
  // For sysconf().
  #include <unistd.h>
#endif
#include "llvm/CodeGen/OptSched/generic/defines.h"

namespace opt_sched {

namespace Utilities {
  // Calculates the minimum number of bits that can hold a given integer value.
  uint16_t clcltBitsNeededToHoldNum(uint64_t value);
  // Returns the time that has passed since the start of the process, in
  // milliseconds.
  Milliseconds GetProcessorTime();
}

inline uint16_t Utilities::clcltBitsNeededToHoldNum(uint64_t value) {
  uint16_t bitsNeeded = 0;

  while (value) {
    value >>= 1;
    bitsNeeded++;
  }

  return bitsNeeded;
}

inline Milliseconds Utilities::GetProcessorTime() {
  Milliseconds currentTime;

  // Unfortunately clock() from <ctime> was not reliable enough.
  #ifdef WIN32
    timeb timeBuf;
    ftime(&timeBuf);
    currentTime = timeBuf.time * 1000 + timeBuf.millitm;
    static Milliseconds startTime = currentTime;
  #else
    const Milliseconds startTime = 0;
    tms t;
    times(&t);
    int64_t ticks = t.tms_utime;
    long ticksPerSecond = sysconf(_SC_CLK_TCK);
    currentTime = (ticks * 1000) / ticksPerSecond;
  #endif

  return currentTime - startTime;
}

} // end namespace opt_sched

#endif
