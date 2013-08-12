/*****************************************************************************
*
*  Smart pointer extension
*
*  (C) 2003 Marcel Mueller, Fulda, Germany
*
*  Freeware
*
****************************************************************************/


#ifndef MMSmartPointer_h
#define MMSmartPointer_h

#include <MMCast.h>

#include <boost/smart_ptr.hpp>

#include <string>
#include <memory>
#include <iterator>
#include <stdexcept>

#ifdef _MSVC_VER
#pragma warning(push,3) // can't satisfy warning level 4 in the following classes
#endif

#ifdef MMSmartPointer_DEBUG
#include <typeinfo>
#include <stdarg.h>
#define DM DebugMessage
#else
//#define DM (void)sizeof
inline void DM(const char*, ...) {}
#endif

namespace MM {
namespace SmartPointer {
using namespace boost;

// Internals
namespace detail {
// internal dummy structure for uninitialized objects
extern struct uninitialized_tag {} _Uninitialized_;
extern struct static_cast_tag {} _static_cast_;
extern struct dynamic_cast_tag {} _dynamic_cast_;
extern struct const_cast_tag {} _const_cast_;

template <typename T>
inline void no_delete(T*)
{}

} // namspace MM::SmartPointer::detail


/*****************************************************************************
*
*  exception safe construct for auto_ptr<T>(new T(...));
*
*  Example:
*  
*  void f(auto_ptr<T> t, ...);
*  f(auto_ptr_new<T>(...), ...);
*
*  Do not use this class beyond the scope of the above case.
*  There will be no benefit. 
*
*****************************************************************************/
template <typename T>
class auto_ptr_new : public std::auto_ptr<T>
{public:
   explicit    auto_ptr_new()                   : std::auto_ptr<T>(new T()) {}
   template <typename A1>
   explicit    auto_ptr_new(A1 a1)              : std::auto_ptr<T>(new T(a1)) {}
   template <typename A1, typename A2>
               auto_ptr_new(A1 a1, A2 a2)       : std::auto_ptr<T>(new T(a1, a2)) {}
   template <typename A1, typename A2, typename A3>
               auto_ptr_new(A1 a1, A2 a2, A3 a3) : std::auto_ptr<T>(new T(a1, a2, a3)) {}
   template <typename A1, typename A2, typename A3, typename A4>
               auto_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4) : std::auto_ptr<T>(new T(a1, a2, a3, a4)) {}
   template <typename A1, typename A2, typename A3, typename A4, typename A5>
               auto_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) : std::auto_ptr<T>(new T(a1, a2, a3, a4, a5)) {}
   template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
               auto_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) : std::auto_ptr<T>(new T(a1, a2, a3, a4, a5, a6)) {}
   using       std::auto_ptr<T>::operator=;
};


/*****************************************************************************
*
*  Pointer class which handles ownership
*
*****************************************************************************/
template <class T>
class own_ptr; // forward

template <class T, T* SGL>
class sgl_own_ptr; // forward

template <class T>
class ref_ptr; // forward

template <class T>
struct own_ptr_ref // proxy reference for copying
{  T*          Ptr;  // Data
   bool        Own;  // ownership
               own_ptr_ref(T* p, bool own)       : Ptr(p), Own(own) {}
};

template <class T>
class own_ptr
{protected:
   T*          Ptr;
   bool        Own;

 protected:
   #ifdef MMSmartPointer_DEBUG
   void        DebugMessage(const char* s, ...);
   #endif
   void        Attach(T* ptr, bool own = true)  { Ptr = ptr; Own = own & ptr != NULL; DM("Attach(%p,%i)", ptr, own);  }
   void        Detach()                         { DM("Detach()"); if (Own) delete Ptr; }
   void        Reset(T* ptr, bool own = true);
               own_ptr(detail::uninitialized_tag) {}
 public:
               own_ptr()                        : Ptr(NULL), Own(false) { DM("own_ptr()"); }
               own_ptr(T& r)                    : Ptr(&r), Own(false) { DM("own_ptr(&%p)", &r); }
               own_ptr(T* p)                    : Ptr(p), Own(p != NULL) { DM("own_ptr(%p)", p); }
               own_ptr(T* p, bool own)          : Ptr(p), Own(own & p != NULL) { DM("own_ptr(%p,%i)", p, own); }
               own_ptr(own_ptr<T>& r)           : Ptr(r.get()), Own(r.Own) { r.release(); DM("own_ptr(own_ptr&%p)", &r); }
   template <class T2>
               own_ptr(own_ptr<T2>& r, detail::static_cast_tag) : Ptr(static_cast<T*>(r.get())), Own(r.own()) { r.release(); DM("own_ptr(own_ptr&%p)", &r); }
   template <class T2>
               own_ptr(own_ptr<T2>& r, detail::dynamic_cast_tag) : Ptr(dynamic_cast<T*>(r.get())), Own(r.own()) { if (Ptr) r.release(); else Own = false; DM("own_ptr(own_ptr&%p)", &r); }
   template <class T2>
               own_ptr(own_ptr<T2>& r, detail::const_cast_tag) : Ptr(const_cast<T*>(r.get())), Own(r.own()) { r.release(); DM("own_ptr(own_ptr&%p)", &r); }
               own_ptr(own_ptr_ref<T> r)        : Ptr(r.Ptr), Own(r.Own) { DM("own_ptr(own_ptr_ref %p{%p,%i})", &r, r.Ptr, r.Own); }
   template <class T2>
               own_ptr(std::auto_ptr<T2> r)     : Ptr(r.release()), Own(true) { DM("own_ptr(auto_ptr<T2>&%p)", &r); }
   template <T* SGL>
               own_ptr(sgl_own_ptr<T,SGL> r);
               ~own_ptr()                       { DM("~"); Detach(); }
   template <class T2>
   operator    own_ptr<T2>()                    { DM("operator own_ptr<T2>()"); own_ptr<T2> r(Ptr, Own); release(); return r; }
   operator    own_ptr_ref<T>()                 { DM("operator own_ptr_ref()"); own_ptr_ref<T> r(Ptr, Own); release(); return r; }
   template <class T2>
   operator    own_ptr_ref<T2>()                { DM("operator own_ptr_ref<T2>()"); own_ptr_ref<T2> r(Ptr, Own); release(); return r; }
   T&          operator*() const                { return *Ptr; }
   T*          operator->() const               { return Ptr; }
   bool        own() const                      { return Own; }
   T*          get() const                      { return Ptr; }
   T*          release();
   void        reset()                          { Detach(); Ptr = NULL; Own = false; }
   void        reset(T* ptr, bool own = true)   { if (ptr != Ptr) Reset(ptr, own); }
   void        reset(T& o)                      { Detach(); Ptr = &o; Own = false; }
   void        reset(own_ptr<T>& r)             { if (r.Ptr != Ptr) { Reset(r.get(), r.own()); r.release(); } }
   void        reset(own_ptr_ref<T> r)          { reset(r.Ptr, r.Own); }
   void        reset(std::auto_ptr<T>& r)       { reset(r.release()); }
   template <T* SGL>
   void        reset(sgl_own_ptr<T,SGL>& r);
   own_ptr<T>& operator=(T* r)                  { reset(r); return *this; }
   own_ptr<T>& operator=(T& r)                  { reset(r); return *this; }
   own_ptr<T>& operator=(own_ptr<T>& r)         { reset(r); return *this; }
   template <class T2>
   own_ptr<T>& operator=(own_ptr<T2>& r)        { reset(r); return *this; }
   own_ptr<T>& operator=(own_ptr_ref<T> r)      { reset(r); return *this; }
   own_ptr<T>& operator=(std::auto_ptr<T>& r)   { reset(r); return *this; }
   template <T* SGL>
   own_ptr<T>& operator=(sgl_own_ptr<T,SGL>& r) { reset(r); return *this; }
   typedef T*  own_ptr<T>::*unspecified_bool_type;
   operator    unspecified_bool_type() const    { return &own_ptr<T>::Ptr; }   
   bool        operator!() const                { return !Ptr; }
};

// Template implementation
template <class T>
void own_ptr<T>::Reset(T* ptr, bool own)
{  Detach();
    Attach(ptr, own);
   DM("Reset(%p,%i)", ptr, own);
}

template <class T>
T* own_ptr<T>::release()
{  T* ptr = Ptr;
   Ptr = NULL;
   Own = false;
   return ptr;
}

#ifdef MMSmartPointer_DEBUG
template <class T>
void own_ptr<T>::DebugMessage(const char* s, ...)
{  va_list va;
   va_start(va, s);
   printf("%s(%p)::", typeid(this).name(), this);
   vprintf(s, va);
   printf(" {%p,%i}\n", Ptr, Own);
   va_end(va);
}
#endif

// global operators
template <class T, class T2>
bool operator==(const own_ptr<T>& l, const own_ptr<T2>& r)
{  return l.get() == r.get();
}

template <class T, class T2>
bool operator!=(const own_ptr<T>& l, const own_ptr<T2>& r)
{  return l.get() != r.get();
}


/*****************************************************************************
*
*  exception safe construct for own_ptr<T>(new T(...));
*
*  Example:
*  
*  void f(own_ptr<T> t, ...);
*  f(own_ptr_new<T>(...), ...);
*
*  or
*
*  own_ptr_new<T> p(...);
*
*  Do not use this class beyond the scope of the above case.
*  There will be no benefit. 
*
*****************************************************************************/
template <typename T>
class own_ptr_new : public own_ptr<T>
{public:
   explicit    own_ptr_new()                    : own_ptr<T>(new T()) {}
   template <typename A1>
   explicit    own_ptr_new(A1 a1)               : own_ptr<T>(new T(a1)) {}
   template <typename A1, typename A2>
               own_ptr_new(A1 a1, A2 a2)        : own_ptr<T>(new T(a1, a2)) {}
   template <typename A1, typename A2, typename A3>
               own_ptr_new(A1 a1, A2 a2, A3 a3) : own_ptr<T>(new T(a1, a2, a3)) {}
   template <typename A1, typename A2, typename A3, typename A4>
               own_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4) : own_ptr<T>(new T(a1, a2, a3, a4)) {}
   template <typename A1, typename A2, typename A3, typename A4, typename A5>
               own_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) : own_ptr<T>(new T(a1, a2, a3, a4, a5)) {}
   template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
               own_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) : own_ptr<T>(new T(a1, a2, a3, a4, a5, a6)) {}
};


/*****************************************************************************
*
*  Pointer class which handles ownership and with special singleton instead of NULL
*
*****************************************************************************/
template <class T, T* SGL>
struct sgl_own_ptr_ref // proxy reference for copying
{  T*          Ptr;  // Data
   bool        Own;  // ownership
               sgl_own_ptr_ref(T* p, bool own)       : Ptr(p), Own(own) {}
};

template <class T, T* SGL>
class sgl_own_ptr : protected own_ptr<T>
{public:
 protected:
   void        Attach(T* ptr, bool own)
   {  if (ptr)
      {  this->Ptr = ptr;
         this->Own = own;
      } else
      {  this->Ptr = SGL;
         this->Own = false;
   }  }
   void        Reset(T* ptr, bool own = true);
 public:
               sgl_own_ptr()                    : own_ptr<T>(*SGL) {}
               sgl_own_ptr(T& ref)              : own_ptr<T>(ref) {}
               sgl_own_ptr(T* ptr, bool own = true) : own_ptr<T>(detail::_Uninitialized_) { Attach(ptr, own); }
               sgl_own_ptr(std::auto_ptr<T>& r) : own_ptr<T>(r.release(), true) { DM("sgl_own_ptr(auto_ptr&%p)", &r); }
   template <class T2>
               sgl_own_ptr(std::auto_ptr<T2>& r) : own_ptr<T>(r.release(), true) { DM("sgl_own_ptr(auto_ptr&%p)", &r); }
               sgl_own_ptr(own_ptr<T>& r)       : own_ptr<T>(detail::_Uninitialized_) { Attach(r.get(), r.own()); r.release(); }
   template <class T2>
               sgl_own_ptr(own_ptr<T2>& r)      : own_ptr<T>(detail::_Uninitialized_) { Attach(r.get(), r.own()); r.release(); }
               sgl_own_ptr(own_ptr_ref<T> r)    : own_ptr<T>(detail::_Uninitialized_) { Attach(r.Ptr, r.Own); }
               sgl_own_ptr(sgl_own_ptr<T,SGL>& r) : own_ptr<T>(r.Ptr, r.Own) { r.release(); }
   template <class T2, T2* SGL2>
               sgl_own_ptr(sgl_own_ptr<T2,SGL2>& r) : own_ptr<T>(detail::_Uninitialized_) { Attach(r.get_raw(), r.own()); r.release(); }
   template <class T2>
               sgl_own_ptr(sgl_own_ptr<T2,SGL>& r) : own_ptr<T>(r.get(), r.own()) { r.release(); } // faster if same singleton
               sgl_own_ptr(sgl_own_ptr_ref<T,SGL> r) : own_ptr<T>(r.Ptr, r.Own) {}
               sgl_own_ptr(sgl_own_ptr_ref<T,SGL> r, detail::static_cast_tag) : own_ptr<T>(detail::_Uninitialized_) { Attach(static_cast<T*>(r.Ptr), r.Own); }
               sgl_own_ptr(sgl_own_ptr_ref<T,SGL> r, detail::dynamic_cast_tag) : own_ptr<T>(detail::_Uninitialized_) { Attach(dynamic_cast<T*>(r.Ptr), r.Own); }
               sgl_own_ptr(sgl_own_ptr_ref<T,SGL> r, detail::const_cast_tag) : own_ptr<T>(detail::_Uninitialized_) { Attach(const_cast<T*>(r.Ptr), r.Own); }
   operator    sgl_own_ptr_ref<T,SGL>()         { DM("operator sgl_own_ptr_ref<T>()"); sgl_own_ptr_ref<T,SGL> r(this->Ptr, this->Own); release(); return r; }
   template <class T2, T2* SGL2>
   operator    sgl_own_ptr_ref<T2,SGL2>()       { DM("operator own_ptr_ref<T2>()"); sgl_own_ptr_ref<T2,SGL2> r(this->Ptr == SGL ? SGL2 : this->Ptr, this->Own); release(); return r; }
   template <class T2>
   operator    own_ptr<T2>()                    { DM("operator own_ptr()"); own_ptr<T2> r(this->Ptr, this->Own); release(); return r; }
   using       own_ptr<T>::operator*;
   using       own_ptr<T>::operator->;
   using       own_ptr<T>::own;
   using       own_ptr<T>::get;
   T*          get_raw() const                  { return this->Ptr == SGL ? NULL : this->Ptr; }
   T*          release();
   void        reset()                          { this->Detach(); this->Ptr = SGL; this->Own = false; }
   void        reset(T* ptr, bool own = true)   { if (ptr != this->Ptr) this->Reset(ptr, own); }
   void        reset(T& ref)                    { this->Detach(); this->Ptr = &ref; this->Own = false; }
   void        reset(sgl_own_ptr<T,SGL>& r)     { if (r.Ptr != this->Ptr) { this->Reset(r.get_raw(), r.Own); r.release(); } }
   template <class T2, T2* SGL2>
   void        reset(sgl_own_ptr<T2,SGL2>& r)   { if (r.Ptr != this->Ptr) { this->Reset(r.get_raw(), r.own()); r.release(); } }
   void        reset(sgl_own_ptr_ref<T,SGL> r)  { reset(r.Ptr, r.Own); }
   void        reset(own_ptr<T>& r)             { if (r.Ptr != this->Ptr) { this->Reset(r.get(), r.own()); r.release(); } }
   template <class T2>
   void        reset(own_ptr<T2>& r)            { if (r.Ptr != this->Ptr) { this->Reset(r.get(), r.own()); r.release(); } }
   void        reset(own_ptr_ref<T> r)          { reset(r.Ptr, r.Own); }
   void        reset(std::auto_ptr<T>& r)       { reset(r.release()); }
   template <class T2>
   void        reset(std::auto_ptr<T2>& r)      { reset(r.release()); }
   sgl_own_ptr<T,SGL>& operator=(T* r)          { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(T& r)          { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(sgl_own_ptr<T,SGL>& r) { reset(r); return *this; }
   template <class T2, T2* SGL2>
   sgl_own_ptr<T,SGL>& operator=(sgl_own_ptr<T2,SGL2>& r) { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(sgl_own_ptr_ref<T,SGL> r) { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(own_ptr<T>& r) { reset(r); return *this; }
   template <class T2>
   sgl_own_ptr<T,SGL>& operator=(own_ptr<T2>& r) { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(own_ptr_ref<T> r) { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(std::auto_ptr<T>& r) { reset(r.release()); return *this; }
   template <class T2>
   sgl_own_ptr<T,SGL>& operator=(std::auto_ptr<T2>& r) { reset(r.release()); return *this; }
   using       own_ptr<T>::operator typename own_ptr<T>::unspecified_bool_type;
   bool        operator!() const                { return false; }
// bool        operator==(const T* r) const     { return get_raw() == r; }
// bool        operator!=(const T* r) const     { return get_raw() != r; }
};

// template implementation
template <class T, T* SGL>
void sgl_own_ptr<T, SGL>::Reset(T* ptr, bool own) // same as in base class, but for efficiency issues Attach and Detach are not virtual.
{  this->Detach();
   this->Attach(ptr, own);
}

template <class T, T* SGL>
T* sgl_own_ptr<T, SGL>::release()
{  T* ptr = get_raw();
   this->Ptr = SGL;
   this->Own = false;
   return ptr;
}

// global operators
template <class T, T* SGL, class T2, T2* SGL2>
inline bool operator==(const sgl_own_ptr<T,SGL>& l, const sgl_own_ptr<T2,SGL2>& r)
{  return l.get_raw() == r.get_raw();
}

template <class T, T* SGL, class T2, T2* SGL2>
inline bool operator!=(const sgl_own_ptr<T,SGL>& l, const sgl_own_ptr<T2,SGL2>& r)
{  return l.get_raw() != r.get_raw();
}

template <class T, T* SGL, class T2>
inline bool operator==(const sgl_own_ptr<T,SGL>& l, const sgl_own_ptr<T2,SGL>& r)
{  return l.get() == r.get(); // faster if same singleton
}

template <class T, T* SGL, class T2>
inline bool operator!=(const sgl_own_ptr<T,SGL>& l, const sgl_own_ptr<T2,SGL>& r)
{  return l.get() != r.get(); // faster if same singleton
}

template <class T, T* SGL, class T2>
inline bool operator==(const sgl_own_ptr<T,SGL>& x, const own_ptr<T2>& y)
{  return x.get_raw() == y.get();
}
template <class T, T* SGL, class T2>
inline bool operator==(const own_ptr<T2>& y, const sgl_own_ptr<T,SGL>& x)
{  return x.get_raw() == y.get();
}

template <class T, T* SGL, class T2>
bool operator!=(const sgl_own_ptr<T,SGL>& x, const own_ptr<T2>& y)
{  return x.get_raw() != y.get();
}
template <class T, T* SGL, class T2>
bool operator!=(const own_ptr<T2>& y, const sgl_own_ptr<T,SGL>& x)
{  return x.get_raw() != y.get();
}


/*****************************************************************************
*
*  Reference counted pointer class with optional ownership
*
*****************************************************************************/

template <typename T>
class ref_ptr : public ::boost::shared_ptr<T>
{protected:
   #ifdef MMSmartPointer_DEBUG
   void        DebugMessage(const char* s, ...);
   #endif
 private:
  	inline static void (*make_deleter(bool own))(T*)
 	{	return own
	 	 ? (void (*)(T*))&checked_delete<T>
	 	 :	(void (*)(T*))&detail::no_delete<T>;
 	}
 
 public:
               ref_ptr()                        : shared_ptr<T>() { DM("ref_ptr()"); }
               ref_ptr(T* p)                    : shared_ptr<T>(p) { DM("ref_ptr(%p)", p); }
               ref_ptr(T* p, bool own)          : shared_ptr<T>(p, make_deleter(own)) { DM("ref_ptr(%p,%i)", p, own); }
               ref_ptr(T& r)                    : shared_ptr<T>(&r, (void (*)(T*))&detail::no_delete<T>) { DM("ref_ptr(&%p)", &r); }
               ref_ptr(std::auto_ptr<T>& r)     : shared_ptr<T>(r.release()) { DM("ref_ptr(auto_ptr&%p)", this->get()); }
   template <class T2>
               ref_ptr(std::auto_ptr<T2>& r)    : shared_ptr<T>(r.release()) { DM("ref_ptr<T2>(auto_ptr&%p)", this->get()); }
               ref_ptr(own_ptr<T>& r)           : shared_ptr<T>(r.get(), make_deleter(r.own())) { r.release(); }
   template <class T2>
               ref_ptr(own_ptr<T2>& r)          : shared_ptr<T>(r.get(), make_deleter(r.own())) { r.release(); }
//             ref_ptr(typename own_ptr<T>::ref r) : shared_ptr<T>(r.Ref.get(), make_deleter(r.Ref.own())) { r.Ref.release(); }
// template <class T2>
//             ref_ptr(typename own_ptr<T2>::ref r) : shared_ptr<T>(r.Ref.get(), make_deleter(r.Ref.own())) { r.Ref.release(); }
   template <class T2, T2* SGL2>
               ref_ptr(sgl_own_ptr<T2,SGL2>& r) : shared_ptr<T>(r.get(), make_deleter(r.own())) { r.release(); }
   template <class T2, T2* SGL2>
               ref_ptr(typename sgl_own_ptr<T2,SGL2>::ref r) : shared_ptr<T>(r.Ref.get(), make_deleter(r.Ref.own())) { r.Ref.release(); }
               ref_ptr(const shared_ptr<T>& r)  : shared_ptr<T>(r) {}
   template <class T2>
               ref_ptr(const shared_ptr<T2>& r) : shared_ptr<T>(r) {}
   template <class T2>
               ref_ptr(const shared_ptr<T2>& r, detail::static_cast_tag) : shared_ptr<T>(::boost::static_pointer_cast<T>(r)) {}
   template <class T2>
               ref_ptr(const shared_ptr<T2>& r, detail::dynamic_cast_tag) : shared_ptr<T>(::boost::dynamic_pointer_cast<T>(r)) {}
//   template <class T2>
//               ref_ptr(const shared_ptr<T2>& r, detail::const_cast_tag) : shared_ptr<T>(::boost::const_pointer_cast<T>(r)) {}
   bool        own() const                      { return get_deleter<void (*)(T*)>(*this) != &detail::no_delete<T>; }
   using       shared_ptr<T>::reset;
   void        reset(T* p)                      { shared_ptr<T>::reset(p); }
   void        reset(T* p, bool own)            { shared_ptr<T>::reset(p, make_deleter(own)); }
   void        reset(T& r)                      { shared_ptr<T>::reset(&r, make_deleter(false)); }
// void        reset(std::auto_ptr<T>& r)       { shared_ptr<T>::reset(r); }
// template <class T2>
// void        reset(std::auto_ptr<T2>& r)      { shared_ptr<T>::reset(r); }
   void        reset(own_ptr<T>& r)             { shared_ptr<T>::reset(r.get(), make_deleter(r.own)); r.release(); }
   template <class T2>
   void        reset(own_ptr<T2>& r)            { shared_ptr<T>::reset(r.get(), make_deleter(r.own)); r.release(); }
// void        reset(typename own_ptr<T>::ref r) { reset(r.Ref); }
// template <class T2>
// void        reset(typename own_ptr<T2>::ref r) { reset(r.Ref); }
   template <class T2, T2* SGL2>
   void        reset(sgl_own_ptr<T2,SGL2>& r)   { shared_ptr<T>::reset(r.get(), make_deleter(r.own)); r.release(); }
   template <class T2, T2* SGL2>
   void        reset(typename sgl_own_ptr<T2,SGL2>::ref r) { shared_ptr<T>::reset(r.get(), make_deleter(r.own)); r.release(); }
   void        reset(const ref_ptr<T>& r)       { shared_ptr<T>::reset(r); }
   template <class T2>
   void        reset(const ref_ptr<T2>& r)      { shared_ptr<T>::reset(r); }
   using       shared_ptr<T>::operator=;
   ref_ptr<T>& operator=(T* r)                  { reset(r); return *this; }
   ref_ptr<T>& operator=(T& r)                  { reset(r); return *this; }
   ref_ptr<T>& operator=(own_ptr<T>& r)         { reset(r); return *this; }
   template <class T2>
   ref_ptr<T>& operator=(own_ptr<T2>& r)        { reset(r); return *this; }
// ref_ptr<T>& operator=(typename own_ptr<T>::ref r) { reset(r.Ref); return *this; }
// template <class T2>
// ref_ptr<T>& operator=(typename own_ptr<T2>::ref r) { reset(r.Ref); return *this; }
   template <class T2, T2* SGL2>
   ref_ptr<T>& operator=(sgl_own_ptr<T2,SGL2>& r) { reset(r); return *this; }
   template <class T2, T2* SGL2>
   ref_ptr<T>& operator=(typename sgl_own_ptr<T2,SGL2>::ref r) { reset(r.Ref); return *this; }
};

// template implementation
#ifdef MMSmartPointer_DEBUG
template <class T>
void ref_ptr<T>::DebugMessage(const char* s, ...)
{  va_list va;
   va_start(va, s);
   printf("%s(%p)::", typeid(this).name(), this);
   vprintf(s, va);
   //printf(" {%p->{%lu,%p}}\n", Ref, Ref->Cnt, *(void**)(Ref+1));
   va_end(va);
}
#endif

/*****************************************************************************
*
*  exception safe construct for ref_ptr<T>(new T(...));
*
*  Example:
*  
*  void f(ref_ptr<T> t, ...);
*  f(ref_ptr_new<T>(...), ...);
*
*  Do not use this class beyond the scope of the above case.
*  There will be no benefit. 
*
*****************************************************************************/
template <typename T>
class ref_ptr_new : public ref_ptr<T>
{public:
   explicit    ref_ptr_new()                    : ref_ptr<T>(new T()) {}
   template <typename A1>
   explicit    ref_ptr_new(A1 a1)               : ref_ptr<T>(new T(a1)) {}
   template <typename A1, typename A2>
               ref_ptr_new(A1 a1, A2 a2)        : ref_ptr<T>(new T(a1, a2)) {}
   template <typename A1, typename A2, typename A3>
               ref_ptr_new(A1 a1, A2 a2, A3 a3) : ref_ptr<T>(new T(a1, a2, a3)) {}
   template <typename A1, typename A2, typename A3, typename A4>
               ref_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4) : ref_ptr<T>(new T(a1, a2, a3, a4)) {}
   template <typename A1, typename A2, typename A3, typename A4, typename A5>
               ref_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) : ref_ptr<T>(new T(a1, a2, a3, a4, a5)) {}
   template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
               ref_ptr_new(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) : ref_ptr<T>(new T(a1, a2, a3, a4, a5, a6)) {}
};


/*****************************************************************************
*
*  late implementations (dependencies)
*
*****************************************************************************/

template <class T>
template <T* SGL>
inline own_ptr<T>::own_ptr(sgl_own_ptr<T,SGL> r)
 : Ptr(r.get())
 , Own(r.own())
{  r.release();
   DM("own_ptr(own_ptr&%p)", &r);
}
/*template <class T>
template <T* SGL>
inline own_ptr<T>::own_ptr(typename sgl_own_ptr<T,SGL>::ref r)
 : Own(r.Ref.own())
 , Ptr(r.Ref.release())
{  DM("own_ptr(own_ptr::ref %p{%i,%p})", &r.Ref, r.Ref.Own, r.Ref.Ptr);
}*/

template <class T>
template <T* SGL>
inline void own_ptr<T>::reset(sgl_own_ptr<T,SGL>& r)
{  Reset(r.get(), r.own());
   r.release();
   DM("own_ptr(own_ptr&%p)", &r);
}
/*template <class T>
template <T* SGL>
inline void own_ptr<T>::reset(typename sgl_own_ptr<T,SGL>::ref r)
{  reset(r.Ref);
}*/


#undef DM

#ifdef _MSVC_VER
#pragma warning(pop)
#endif

} // end namespace SmartPointer


// cast traits
template <typename D, typename S>
struct MM_cast_traits<MM::SmartPointer::own_ptr<D>, MM::SmartPointer::own_ptr<S> >
{  static MM::SmartPointer::own_ptr<D> do_static_cast(MM::SmartPointer::own_ptr<S> s)
   {  return MM::SmartPointer::own_ptr<D>(s, MM::SmartPointer::detail::_static_cast_);
   }
   static MM::SmartPointer::own_ptr<D> do_dynamic_cast(MM::SmartPointer::own_ptr<S> s)
   {  return MM::SmartPointer::own_ptr<D>(s, MM::SmartPointer::detail::_dynamic_cast_);
   }
   static MM::SmartPointer::own_ptr<D> do_const_cast(MM::SmartPointer::own_ptr<S> s)
   {  return MM::SmartPointer::own_ptr<D>(s, MM::SmartPointer::detail::_const_cast_);
   }
};

template <typename D, typename S, D* DSGL, S* SSGL>
struct MM_cast_traits<MM::SmartPointer::sgl_own_ptr<D, DSGL>, MM::SmartPointer::sgl_own_ptr<S, SSGL> >
{  static MM::SmartPointer::sgl_own_ptr<D, DSGL> do_static_cast(MM::SmartPointer::sgl_own_ptr<S, SSGL> s)
   {  return MM::SmartPointer::sgl_own_ptr<D, DSGL>(s, MM::SmartPointer::detail::_static_cast_);
   }
   static MM::SmartPointer::sgl_own_ptr<D, DSGL> do_dynamic_cast(MM::SmartPointer::sgl_own_ptr<S, SSGL> s)
   {  return MM::SmartPointer::sgl_own_ptr<D, DSGL>(s, MM::SmartPointer::detail::_dynamic_cast_);
   }
   static MM::SmartPointer::sgl_own_ptr<D, DSGL> do_const_cast(MM::SmartPointer::sgl_own_ptr<S, SSGL> s)
   {  return MM::SmartPointer::sgl_own_ptr<D, DSGL>(s, MM::SmartPointer::detail::_const_cast_);
   }
};

template <typename D, typename S>
struct MM_cast_traits<MM::SmartPointer::ref_ptr<D>, MM::SmartPointer::ref_ptr<S> >
{  static MM::SmartPointer::ref_ptr<D> do_static_cast(MM::SmartPointer::ref_ptr<S> s)
   {  return MM::SmartPointer::ref_ptr<D>(s, MM::SmartPointer::detail::_static_cast_);
   }
   static MM::SmartPointer::ref_ptr<D> do_dynamic_cast(MM::SmartPointer::ref_ptr<S> s)
   {  return MM::SmartPointer::ref_ptr<D>(s, MM::SmartPointer::detail::_dynamic_cast_);
   }
   static MM::SmartPointer::ref_ptr<D> do_const_cast(MM::SmartPointer::ref_ptr<S> s)
   {  return MM::SmartPointer::ref_ptr<D>(s, MM::SmartPointer::detail::_const_cast_);
   }
};

} // end namespace MM

#endif
