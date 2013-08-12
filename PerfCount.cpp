#include "PerfCount.h"

#ifdef __OS2__
#define INCL_BASE
#include <os2.h>

#else
#include <time.h>
#endif


#ifdef __OS2__
PerfCount::PerfCount() : Loops(0), BytesSoFar(0)
{	DosTmrQueryFreq((PULONG)&Freq);
	DosTmrQueryTime((PQWORD)&StartTime);
	tvalid = true;
}

void PerfCount::TimeUpdate() const
{	DosTmrQueryTime((PQWORD)&Elapsed);
	Elapsed -= StartTime;
	tvalid = true;
}

#else
inline static void gettime(uint64_t& dst)
{	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	dst = (uint64_t)1000000000 * ts.tv_sec + ts.tv_nsec;
}

PerfCount::PerfCount() : Loops(0), BytesSoFar(0), Freq(1000000000)
{	gettime(StartTime);
	tvalid = true;
}

void PerfCount::TimeUpdate() const
{	gettime(Elapsed);
	Elapsed -= StartTime;
	tvalid = true;
}

#endif

void PerfCount::Update(size_t bytes)
{	BytesSoFar += bytes;
	++Loops;
	tvalid = false;
}

double PerfCount::getRate() const
{	TimeCheck(); 
	return (double)BytesSoFar / Elapsed * Freq;
}

double PerfCount::getBlockRate() const
{	TimeCheck(); 
	return (double)Loops / Elapsed * Freq;
}

