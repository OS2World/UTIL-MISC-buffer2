/*****************************************************************************
*
*  Mutex.cpp - platfor specific wrappe of mutex semaphore class
*
*  (C) 2002 comspe AG, Fulda, Germany
*      Marcel MÅller
*
****************************************************************************/


#include "MMUtil+.h"
#include <stdexcept>
using namespace std;

namespace MM {
namespace IPC {

#if defined(_WIN32)
/*****************************************************************************
*
*  mutual exclusive section class
*  Wrapper to WIN32 Mutex.
*
*****************************************************************************/
//#include <windows.h> already in header

Mutex::Mutex(bool share)
{  static const SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
   Handle = CreateMutex(share ? &sa : NULL, FALSE, NULL);
   if (Handle == NULL)
      throw runtime_error("Cannot initialize Mutex object."); // can't help it, better than continue
}

Mutex::Mutex(const char* name)
{  Handle = CreateMutex(NULL, FALSE, name);
   if (Handle == NULL)
      throw runtime_error("Cannot initialize Mutex object."); // can't help it, better than continue
}

Mutex::~Mutex()
{  CloseHandle(Handle);
}

bool Mutex::Wait(int ms)
{  return WaitForSingleObject(Handle, ms) == WAIT_OBJECT_0; // The mapping from ms == -1 to INFINITE is implicitely OK.
}

bool Mutex::Release()
{  return !!ReleaseMutex(Handle);
}

#elif defined(__OS2__)

/*****************************************************************************
*
*  mutual exclusive section class
*  Wrapper to WIN32 Mutex.
*
*****************************************************************************/
//#include <os2.h> already in header

Mutex::Mutex(bool share)
{  APIRET rc = DosCreateMutexSem(NULL, &Handle, share * DC_SEM_SHARED, FALSE);
   if (rc != 0)
      throw runtime_error("Cannot initialize Mutex object."); // can't help it, better than continue
}

Mutex::Mutex(const char* name)
{  char* cp = new char[strlen(name)+8];
   strcpy(cp, "\\SEM32\\");
   strcpy(cp+7, name);
   APIRET rc;
   do
   {  rc = DosCreateMutexSem((UCHAR*)name, &Handle, DC_SEM_SHARED, FALSE);
      if (rc != ERROR_DUPLICATE_NAME)
         break;
      Handle = 0;
      rc = DosOpenMutexSem((UCHAR*)name, &Handle);
   } while (rc == ERROR_SEM_NOT_FOUND);
   delete[] cp;
   if (rc != 0)
      throw runtime_error("Cannot initialize Mutex object."); // can't help it, better than continue
}

Mutex::~Mutex()
{  DosCloseMutexSem(Handle);
}

bool Mutex::Wait(int ms)
{  return DosRequestMutexSem(Handle, ms) == 0; // The mapping from ms == -1 to SEM_INDEFINITE_WAIT is implicitely OK.
}

bool Mutex::Release()
{  return DosReleaseMutexSem(Handle) == 0;
}

#else

#error unsupported platform

#endif

}} // end namespace
