/*****************************************************************************
*
*  MutexBase.cpp - basic mutex functions
*
*  (C) 2002 comspe AG, Fulda, Germany
*      Marcel M�ller
*
****************************************************************************/


#include "MMUtil+.h"

namespace MM {
namespace IPC {

MutexBase::Lock::Lock(int ms)
 : CS(&Interlock::Instance)
 , Own(Interlock::Instance.Wait(ms))
{}

bool MutexBase::Lock::Wait(int ms)
{  if (Own)
      return true;
   return Own = CS->Wait(ms);
}

bool MutexBase::Lock::Release()
{  if (!Own)
      return false;
   Own = false;
   return CS->Release();
}


Interlock Interlock::Instance; // singleton


#if defined(_WIN32)
/*****************************************************************************
*
*  disable thread switching
*  Windows cannot disable the thread switching for the current process.
*  The dirty hack is to temporarily increase the priority of the current thread.
*  This will fail on SMP machines !!!
*
*****************************************************************************/
//#include <windows.h> already in header

static volatile LONG CSset = 0;
static int CSLastPriority;

bool Interlock::Wait(int) // the wait parameter does not make any sense here
{  if (InterlockedIncrement(&CSset) == 1)
   {  CSLastPriority = GetThreadPriority();
      SetThreadPriority(THREAD_PRIORITY_TIME_CRITICAL);
   }
   return true;
}

bool Interlock::Release()
{  if (CSset == 0)
      return false;
   if (--CSset == 0)
      SetThreadPriority(CSLastPriority);
   return true;
}

#elif defined(__OS2__)

/*****************************************************************************
*
*  disable thread switching
*  Wrapper to DosEnter/ExitCritSec
*
*****************************************************************************/
//#include <os2.h> already in header

bool Interlock::Wait(int) // the wait parameter does not make any sense here
{  return DosEnterCritSec() == 0;
}

bool Interlock::Release()
{  return DosExitCritSec() == 0;
}

#else

#error unsupported platform

#endif

}} // end namespace
