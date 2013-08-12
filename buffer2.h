#ifndef __buffer2_h
#define __buffer2_h

#include <iostream>
#include <MMUtil+.h>

extern MM::IPC::Mutex LogMtx;
#define lerr (MM::IPC::Lock(LogMtx), cerr) // lock cerr until the next syncronization point.

struct syntax_error : std::logic_error
{  syntax_error(const std::string& text) : logic_error(text) {}
};

// configuration
extern int BufferSize;
extern int RequestSize;
extern int PipeSize;
extern unsigned BufferAlignment;

extern int iHighWaterMark;
extern double dHighWaterMark;
extern int iLowWaterMark;
extern double dLowWaterMark;

extern bool EnableCache;
extern bool EnableInputStats;
extern bool EnableOutputStats;
extern const double StatsUpdate;


#endif
