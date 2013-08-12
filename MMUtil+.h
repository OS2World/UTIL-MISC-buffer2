/*****************************************************************************
*
*  MMUtil+.h - Utility functions
*
*  (C) 2003 Marcel Mueller, Fulda, Germany
*
*  Freeware
*
****************************************************************************/


#ifndef __MMUtlis_h
#define __MMUtlis_h

#include <string>
#include <stdexcept>
#include <ctype.h>

#include <MMSmartPointer.h>

namespace MM {

/*****************************************************************************
*
*  overload global toupper with string extension
*
*****************************************************************************/
std::string toupper(std::string s);

/*****************************************************************************
*
*  string abbrevation test
*
*****************************************************************************/
bool starts_with(const std::string& s, const std::string& w);

/*****************************************************************************
*
*  sprintf to a string object, buffer overflow safe
*
*****************************************************************************/
std::string vstringf(const char* fmt, va_list ap);
std::string stringf(const char* fmt, ...);

// some more exceptions
// signal an unexpected interrupt condition, e.g. when an object where you are
// currently waiting for is destroyed.
class interrupt_exception : std::logic_error
{public:
   explicit interrupt_exception(const std::string& msg) : logic_error(msg) {}
};

// signal an unhandled error from the underlying operating system  
class os_error : public std::runtime_error
{  long RC;
 public:
   explicit os_error(long rc, const std::string& msg);
   long rc() const throw() { return RC; }
};


// Well, the standard still does not define objects like these...
namespace IPC {

#if defined(_WIN32) || defined(__OS2__)
#ifdef _WIN32
#include <windows.h>
#elif defined(__OS2__)
#define INCL_BASE
#include <os2.h>
#ifdef __EMX__
#include <sys/builtin.h>
#include <sys/smutex.h>
#endif
#endif

/*****************************************************************************
*
*  mutual exclusive section abstract base class
*
*  usage:
*  - Create Mutex object at a shared memory location, e.g. as class
*    member, or pass the object's reference. A Mutex object is any class
*    derived from MutexBase
*      Mutex cs;
*  - To enter a critical section simply create an instance of
*    MutexBase::Lock. The constructor will wait until the resource
*    becomes available. The destrurctor will release the semaphore when the
*    Lock object goes out of scope. This is particularly useful when
*    exceptions are cought outside the scope of the critical section.
*      { MutexBase::Lock lo(cs); // aquire semaphore
*        ...                     // critical code section
*      }                         // release semaphore
*  - To check if the resource is successfully aquired after performing a timed
*    wait, simply query lo as boolean.
*      if (lo) ...
*  - If the scope dependency of the Mutex::Lock Object is not
*    adequate to you, you can use the Wait and Release functions of the
*    Mutex object directly.
*  - It is permitted to destroy the critical section object while other
*    threads are waiting for the resource to become ready if all of these
*    threads check the return value of the Wait function, even if they don't
*    use the timeout feature. This is guaranteed for the Mutex::Lock
*    class. Of course, other threads must not access the Mutex
*    Object after the error condition anymore.
*    You shoud be owner of the critical section when you use this feature to
*    ensure that no other thread is inside the critical section and tries
*    to release the resource afterwards.
*
*****************************************************************************/
class MutexBase
{public:
   class Lock
   {  MutexBase*  CS;   // critical section handle, not owned
      bool        Own;
    public:
      explicit    Lock(int ms = -1);
      explicit    Lock(MutexBase& cs, int ms = -1) : CS(&cs), Own(cs.Wait(ms)) {}
                  ~Lock()     { Release(); }
      bool Wait(int ms = -1);
      bool Release();
      operator bool() const   { return Own; }
   };
 public:
   virtual ~MutexBase() {}
   // Aquire Semaphore
   // If it takes more than ms milliseconds the funtion will return false.
   virtual bool Wait(int ms = -1) = 0;
   // Release Semaphore
   // On error, e.g. if you are not owner, the funtion returns false.
   virtual bool Release() = 0;
};

typedef MutexBase::Lock Lock; // Well, no everybody likes nested classes


/*****************************************************************************
*
*  process mutual exclusive section class
*
*  This is a singleton MutexBase implementation which temporarily disables
*  thread switching for the current process.
*  This will prevent _all_ other threads from working, even if they don't know
*  about the Interlock object. However, on some platforms (e.g. Windows) this
*  can't be really ensured.
*
*  usage:
*    { MutexBase::Lock();     // disable thread switching
*      ...                    // critical code section
*    }                        // release semaphore
*  this is equivalent to
*    { MutexBase::Lock(Interlock::Instance);
*      ...
*    }
*
*  Normally there is no need to access the Interlock singleton directly.
*
*****************************************************************************/
class Interlock : public MutexBase
{public:
   static Interlock Instance; // singleton
 private:
   Interlock() {}
 public:
   virtual bool Wait(int ms = -1);
   virtual bool Release();
};

/*****************************************************************************
*
*  generic mutual exclusive section class
*
*  This is general mutex semaphore for use inside and outside the current
*  process. The Mutex can be inherited by fork() or in case of a named mutex
*  by simply using the same name.
*
*  usage:
*    Mutex mu;
*    ...
*    { MutexBase::Lock(mu);   // disable thread switching
*      ...                    // critical code section
*    }                        // release semaphore
*
*  named Mutex:
*    Mutex mu("MySpecialMutex");
*    ...
*    { MutexBase::Lock(mu);   // disable thread switching
*      ...                    // critical code section
*    }                        // release semaphore
*
*****************************************************************************/
class Mutex : public MutexBase
{private:
   #ifdef _WIN32
   typedef HMUTEX handle_t;
   #elif defined(__OS2__)
   typedef HMTX handle_t;
   #endif
 private:
   handle_t Handle;
 public:
               Mutex(bool share = true);// constructor can throw runtime_error if the mutex can't be initialized for some reason
   explicit    Mutex(const char* name);// Creates a _named_ mutex.
   virtual     ~Mutex();
   // Aquire Mutex object
   // If it takes more than ms milliseconds the funtion will return false.
   virtual bool Wait(int ms = -1);
   // Release the Mutex object
   // On error, e.g. if you are not owner, the funtion returns false.
   virtual bool Release();
};

/*****************************************************************************
*
*  fast mutual exclusive section class
*
*  This is a fast Mutex semaphore for use inside the same process.
*
*  usage:
*    FastMutex fmu;
*    ...
*    { MutexBase::Lock(fmu);  // disable thread switching
*      ...                    // critical code section
*    }                        // release semaphore
*
*****************************************************************************/
#if defined(__OS2__) && !defined(__EMX__)
typedef Mutex FastMutex; // there is no generic equivalence in OS/2 => wrap to Mutex
#else
class FastMutex : public MutexBase
{private:
   #if defined(_WIN32)
   CRITICAL_SECTION CS;
   #elif defined(__OS2__)
   volatile _smutex CS;
   #endif
 public:
               FastMutex();
   #ifndef __OS2__
   virtual     ~FastMutex();
   #endif
   virtual bool Wait(int ms = -1);
   virtual bool Release();
};
#endif

/*****************************************************************************
*
*  event semaphore class
*
*  usage:
*  - Create Event object at a shared memory location, e.g. as class
*    member, or pass the object's reference.
*      Event ev;
*  - To wait for a signal call ev.Wait().
*  - To stop the Wait function from blocking call ev.Set().
*  - To case the Wait function to block in future call ev.Reset().
*  - All Event functions except for the destructor are thread safe.
*  - It is permitted to destroy the Notification object while other
*    threads are waiting for the resource to become ready if all of these
*    threads can handle the interrupt_exception correctly.
*
*****************************************************************************/
class Event
{protected:
   #ifdef _WIN32
   typedef HANDLE handle_t;
   #elif defined(__OS2__)
   typedef HEV handle_t;
   #endif
 private:
   handle_t Handle;
 public:
               Event();
   explicit    Event(const char* name);
   virtual     ~Event();
   // Wait for a notify call.
   // If it takes more than ms milliseconds the funtion will return false.
   // If the object is destroyed while a thread is waiting wait will throw an
   // interrupt_exception. The caller must not access the Event object after
   // this exception.
   virtual bool Wait(int ms = -1);
   // Set the event to the signalled state causing all waiting threads to
   // resume.
   virtual void Set();
   // Reset the semaphore
   // The wait function will block in future.
   virtual void Reset();
};

/*****************************************************************************
*
*  notification class
*
*  usage:
*  - Create Notification object at a shared memory location, e.g. as class
*    member, or pass the object's reference.
*      Notification nt;
*  - To wait for a signal call nt.Wait()
*  - To signal exactly one waiting thread call nt.Notify()
*  - It is permitted to destroy the Notification object while other
*    threads are waiting for the resource to become ready if all of these
*    threads check the return value of the Wait function, even if they don't
*    use the timeout feature.
*
*****************************************************************************/
class Notification
 : public Mutex
 , private Event
{public:
               Notification();
   explicit    Notification(const char* name);
               ~Notification();
   // Wait for a notify call.
   // If it takes more than ms milliseconds the funtion will return false.
   virtual bool Wait(int ms = -1);
   // Submit a notification
   // If the semaphore is alredy posted the function returns false.
   virtual bool Notify();
   // Submit a notification to all threads
   // If the semaphore is alredy posted the function returns false.
   virtual bool NotifyAll();
};
#endif // platform specific semaphores

}} // end namespace

#endif
