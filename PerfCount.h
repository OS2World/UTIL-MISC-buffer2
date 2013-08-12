#ifndef __PERFCOUNT_H
#define __PERFCOUNT_H
// performance counter

#include <stdint.h>
#include <stdlib.h>

class PerfCount
{	uint32_t Loops;
	uint64_t BytesSoFar;
	uint64_t StartTime;
	int32_t Freq;
	mutable uint64_t Elapsed;
	mutable bool tvalid;
	void TimeUpdate() const;
 public:
	PerfCount();
	void Update(size_t bytes);
	void TimeCheck() const        { if (!tvalid) TimeUpdate(); }
	uint64_t getBytes() const     { return BytesSoFar; }
	uint32_t getBlocks() const    { return Loops; }
	double getSeconds() const     { TimeCheck(); return (double)Elapsed / Freq; }
	double getRate() const;
	double getBlockRate() const;
	double getAvgBlockSize() const{ return (double)BytesSoFar / Loops; }
};

#endif

