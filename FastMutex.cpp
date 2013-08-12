/*****************************************************************************
*
*  FastMutex.cpp - platform specific wrapper of mutex semaphore class for
*                  internal use of the current process
*
*  (C) 2003 comspe AG, Fulda, Germany
*      Marcel MÅller
*
****************************************************************************/


#include "MMUtil+.h"


namespace MM {
namespace IPC {

#if defined(_WIN32)
/*****************************************************************************
*
*  high speed mutual exclusive section class
*  Wrapper to WIN32 CriticalSection.
*
*****************************************************************************/
//#include <windows.h> already in header

FastMutex::FastMutex()
{  InitializeCriticalSection(&CS);
}

FastMutex::~FastMutex()
{  DeleteCriticalSection(&CS);
}

bool FastMutex::Wait(int) // timeouts are not supported
{  EnterCriticalSection(&CS);
   return true;
}

bool FastMutex::Release()
{  LeaveCriticalSection(&CS);
   return true;
}

#elif defined(__OS2__)
#if defined(__EMX__)

/*****************************************************************************
*
*  mutual exclusive section class
*  Wrapper to WIN32 Mutex.
*
*****************************************************************************/
//#include <os2.h> already in header

FastMutex::FastMutex()
{  memset((void*)&CS, 0, sizeof CS);
}

bool FastMutex::Wait(int)
{  //fprintf(stderr, "FastMutex(%p)::Wait()\n", this);
   _smutex_request(&CS);
   return true;
}

bool FastMutex::Release()
{  //fprintf(stderr, "FastMutex(%p)::Release()\n", this);
   _smutex_release(&CS);
   return true;
}

#endif
#else

#error unsupported platform

#endif

}} // end namespace
