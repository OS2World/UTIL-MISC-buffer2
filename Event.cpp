/*****************************************************************************
*
*  Event.cpp - platform specific wrapper of event semaphore
*
*****************************************************************************/


#include "MMUtil+.h"
#include <stdexcept>
using namespace std;

namespace MM {
namespace IPC {


#if defined(_WIN32)
/*****************************************************************************
*
*  Event class
*  Wrapper to WIN32 Event.
*
*****************************************************************************/
//#include <windows.h> already in header

Event::Event()
{  Handle = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (Handle == NULL)
      throw runtime_error("Cannot initialize Event object."); // can't help it, better than continue
}

Event::Event(const char* name)
{  Handle = CreateEvent(NULL, FALSE, FALSE, name);
   if (Handle == NULL)
      throw runtime_error("Cannot initialize Event object."); // can't help it, better than continue
}

Event::~Event()
{  CloseHandle(Handle);
}

bool Event::Wait(long ms)
{  return WaitForSingleObject(Handle, ms) == WAIT_OBJECT_0; // The mapping from ms == -1 to INFINITE is implicitely OK.
}

void Event::Set()
{  SetEvent(Handle);
}

void Event::Reset()
{  ResetEvent(Handle);
}

#elif defined(__OS2__)

/*****************************************************************************
*
*  Event class
*  Wrapper to OS2 EventSem.
*
*****************************************************************************/
//#include <os2.h> already in header

Event::Event()
{  APIRET rc = DosCreateEventSem(NULL, &Handle, 0, FALSE);
   if (rc != 0)
      throw os_error(rc, "Cannot initialize Event object."); // can't help it, better than continue
}

Event::Event(const char* name)
{  char* cp = new char[strlen(name)+8];
   strcpy(cp, "\\SEM32\\");
   strcpy(cp+7, name);
   APIRET rc;
   do
   {  rc = DosCreateEventSem((UCHAR*)name, &Handle, DC_SEM_SHARED, FALSE);
      if (rc != ERROR_DUPLICATE_NAME)
         break;
      Handle = 0;
      rc = DosOpenEventSem((UCHAR*)name, &Handle);
   } while (rc == ERROR_SEM_NOT_FOUND);
   delete[] cp;
   if (rc != 0)
      throw os_error(rc, "Cannot initialize Event object."); // can't help it, better than continue
}

Event::~Event()
{  DosCloseEventSem(Handle); // can't handle errors here
}

bool Event::Wait(long ms)
{  APIRET rc = DosWaitEventSem(Handle, ms); // The mapping from ms == -1 to INFINITE is implicitely OK.
   switch (rc)
   {case 0:
      return true;
    case ERROR_TIMEOUT:
      return false;
    case ERROR_INTERRUPT:
      throw interrupt_exception("Event semaphore destroyed while waiting for it to be posted.");
    default:
      throw os_error(rc, "Failed to wait for an event semaphore.");
   }
}

void Event::Set()
{  ULONG cnt;
   APIRET rc = DosQueryEventSem(Handle, &cnt);
   if (rc != 0)
      throw os_error(rc, "Failed to query event semaphore.");
   if (cnt == 0)
   {  rc = DosPostEventSem(Handle);
      if (rc != 0)
         throw os_error(rc, "Failed to post event semaphore.");
   }
}

void Event::Reset()
{  ULONG cnt;
   APIRET rc = DosResetEventSem(Handle, &cnt);
   if (rc != 0 && rc != ERROR_ALREADY_RESET)
      throw os_error(rc, "Failed to reset event semaphore.");
}

#else

// use pthreads
Event::Event() : State(false)
{	pthread_cond_init(&Handle, NULL);
}

Event::~Event()
{	pthread_cond_destroy(&Handle);
}

bool Event::Wait(long ms)
{	if (State) // fast double check
		return true;
	Lock lck(*this);
	while (!State)
		if (!Notification::Wait(ms))
			return false;
	return true;
}

void Event::Set()
{	if (State) // fast double check
		return;
	Lock lck(*this);
	State = true;
	NotifyAll();
}

void Event::Reset()
{	Lock lck(*this);
	State = false;
}
#endif

}} // end namespace
