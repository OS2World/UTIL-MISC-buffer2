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


// Uncomment the following line if your compiler supports partial template
// spacialization of a template function in a template class.
#define MMPartialTemplateClassTemplateFn

// Uncomment the following line if your compiler supports static data members
// in templates.
#define MMTemplateStatics

// Uncomment the following line if the following cast is side-effect free for any type T
// returned by new or new[] respectively. This does not include function pointers.
// T *x, *y;
// y = (T*)(void*)x;
// Ususally this is safe on any flat memory model.
#define MMPVoidCore

#include <string>
#include <memory>
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
inline void DM(...) {}
#endif


// internal dummy structure for uninitialized objects
extern struct uninitialized_tag {} _Uninitialized_;
extern struct static_cast_tag {} _static_cast_;
extern struct dynamic_cast_tag {} _dynamic_cast_;
extern struct const_cast_tag {} _const_cast_;


/*****************************************************************************
*
*  Pointer class which handles ownership
*
*****************************************************************************/
template <class T, T* SGL>
class sgl_own_ptr; // forward

template <class T>
class ref_ptr; // forward

template <class T>
class own_ptr
{public:
   struct ref // proxy reference for copying
   {  own_ptr<T>& Ref;  // reference to constructor argument
      // Since this class is only used as temporary object this will never
      // result in a dangling reference because the temporary object lifetime
      // is at least until the next statement.
      ref(own_ptr<T>& r)                        : Ref(r) {}
   };

 protected:
   T*          Ptr;
   bool        Own;

 protected:
   #ifdef MMSmartPointer_DEBUG
   void        DebugMessage(const char* s, ...);
   #endif
   void        Attach(T* ptr, bool own = true)  { Ptr = ptr; Own = own & ptr != NULL; DM("Attach(%p,%i)", ptr, own);  }
   void        Detach()                         { DM("Detach()"); if (Own) delete Ptr; }
   void        Reset(T* ptr, bool own = true);
               own_ptr(uninitialized_tag)         {}
 public:
               own_ptr()                        : Own(false), Ptr(NULL) { DM("own_ptr()"); }
               own_ptr(T& ref)                  : Own(false), Ptr(&ref) { DM("own_ptr(&%p)", &ref); }
               own_ptr(T* ptr, bool own = true) : Own(own & ptr != NULL), Ptr(ptr) { DM("own_ptr(%p,%i)", ptr, own); }
               own_ptr(own_ptr<T>& r)           : Own(r.Own), Ptr(r.release()) { DM("own_ptr(own_ptr&%p)", &r); }
   template <class T2>
               own_ptr(own_ptr<T2>& r)          : Own(r.own()), Ptr(r.release()) { DM("own_ptr(own_ptr&%p)", &r); }
   template <class T2>
               own_ptr(own_ptr<T2>& r, static_cast_tag) : Own(r.own()), Ptr(static_cast<T*>(r.release())) { DM("own_ptr(own_ptr&%p)", &r); }
   template <class T2>
               own_ptr(own_ptr<T2>& r, dynamic_cast_tag) : Own(r.own()), Ptr(dynamic_cast<T*>(r.release())) { DM("own_ptr(own_ptr&%p)", &r); }
   template <class T2>
               own_ptr(own_ptr<T2>& r, const_cast_tag) : Own(r.own()), Ptr(const_cast<T*>(r.release())) { DM("own_ptr(own_ptr&%p)", &r); }
               own_ptr(own_ptr<T>::ref r)       : Own(r.Ref.Own), Ptr(r.Ref.release()) { DM("own_ptr(own_ptr::ref %p{%i,%p})", &r.Ref, r.Ref.Own, r.Ref.Ptr); }
   template <class T2>
               own_ptr(typename own_ptr<T2>::ref r) : Own(r.Ref.own()), Ptr(r.Ref.release()) { DM("own_ptr(own_ptr::ref %p{%i,%p})", &r.Ref, r.Ref.Own, r.Ref.Ptr); }
               own_ptr(std::auto_ptr<T>& r)     : Own(true), Ptr(r.release()) { DM("own_ptr(auto_ptr&%p)", &r); }
   template <class T2>
               own_ptr(std::auto_ptr<T2>& r)    : Own(true), Ptr(r.release()) { DM("own_ptr(auto_ptr&%p)", &r); }
   template <class T2, T2* SGL>
               own_ptr(sgl_own_ptr<T2,SGL>& r);
   template <class T2, T2* SGL>
               own_ptr(typename sgl_own_ptr<T2,SGL>::ref r);
               ~own_ptr()                       { DM("~"); Detach(); }
   operator    ref()                            { return ref(*this); }
   T&          operator*() const                { return *Ptr; }
   T*          operator->() const               { return Ptr; }
   bool        own() const                      { return Own; }
   T*          get() const                      { return Ptr; }
   T*          release();
   void        reset()                          { Detach(); Ptr = NULL; Own = false; }
   void        reset(T* ptr, bool own = true)   { if (ptr != Ptr) Reset(ptr, own); }
   void        reset(T& o)                      { Detach(); Ptr = &o; Own = false; }
   void        reset(own_ptr<T>& r)             { if (r.Ptr != Ptr) { Reset(r.get(), r.own()); r.release(); } }
   template <class T2>
   void        reset(own_ptr<T2>& r)            { if (r.Ptr != Ptr) { Reset(r.get(), r.own()); r.release(); } }
   void        reset(ref r)                     { reset(r.Ref); }
   template <class T2>
   void        reset(typename own_ptr<T2>::ref r) { reset(r.Ref); }
   void        reset(std::auto_ptr<T>& r)       { reset(r.release()); }
   template <class T2>
   void        reset(std::auto_ptr<T2>& r)      { reset(r.release()); }
   template <class T2, T2* SGL>
   void        reset(sgl_own_ptr<T2,SGL>& r);
   template <class T2, T2* SGL>
   void        reset(typename sgl_own_ptr<T2,SGL>::ref r);
   own_ptr<T>& operator=(T* r)                  { reset(r); return *this; }
   own_ptr<T>& operator=(T& r)                  { reset(r); return *this; }
   own_ptr<T>& operator=(own_ptr<T>& r)         { reset(r); return *this; }
   template <class T2>
   own_ptr<T>& operator=(own_ptr<T2>& r)        { reset(r); return *this; }
   own_ptr<T>& operator=(ref r)                 { reset(r.Ref); return *this; }
   template <class T2>
   own_ptr<T>& operator=(typename own_ptr<T2>::ref r) { reset(r.Ref); return *this; }
   own_ptr<T>& operator=(std::auto_ptr<T>& r)   { reset(r); return *this; }
   template <class T2>
   own_ptr<T>& operator=(std::auto_ptr<T2>& r)  { reset(r); return *this; }
   template <class T2, T2* SGL>
   own_ptr<T>& operator=(sgl_own_ptr<T2,SGL>& r) { reset(r); return *this; }
   bool        operator!() const                { return !Ptr; }
   bool        operator==(const T* r) const     { return Ptr == r; }
   bool        operator!=(const T* r) const     { return Ptr != r; }
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
   fprintf(stderr, "%s(%p)::", typeid(this).name(), this);
   fprintf(stderr, s, va);
   fprintf(stderr, " {%i,%p}\n", Own, Ptr);
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

// cast traits
template <typename D, typename S>
struct MM_cast_traits<own_ptr<D>, own_ptr<S> >
{  static own_ptr<D> do_static_cast(own_ptr<S> s)
   {  return own_ptr<D>(s, _static_cast_);
   }
   static own_ptr<D> do_dynamic_cast(own_ptr<S> s)
   {  return own_ptr<D>(s, _dynamic_cast_);
   }
   static own_ptr<D> do_const_cast(own_ptr<S> s)
   {  return own_ptr<D>(s, _const_cast_);
   }
};


/*****************************************************************************
*
*  Pointer class which handles ownership and with special singleton instead of NULL
*
*****************************************************************************/
template <class T, T* SGL>
class sgl_own_ptr : protected own_ptr<T>
{public:
   struct ref // proxy reference for copying
   {  sgl_own_ptr<T,SGL>& Ref;  // reference to constructor argument
      // Since this class is only used as temporary object this will never
      // result in a dangling reference because the temporary object lifetime
      // is at least until the next statement.
      ref(sgl_own_ptr<T,SGL>& r)                        : Ref(r) {}
   };

 protected:
   void        Attach(T* ptr, bool own = true)
   {  if (ptr)
      {  Ptr = ptr;
         Own = own;
      } else
      {  Ptr = SGL;
         Own = false;
   }  }
   void        Reset(T* ptr, bool own = true);
 public:
               sgl_own_ptr()                    : own_ptr<T>(*SGL) {}
               sgl_own_ptr(T& ref)              : own_ptr<T>(ref) {}
               sgl_own_ptr(T* ptr, bool own = true) : own_ptr<T>(_Uninitialized_) { Attach(ptr, own); }
               sgl_own_ptr(std::auto_ptr<T>& r) : own_ptr<T>(r.release(), true) { DM("sgl_own_ptr(auto_ptr&%p)", &r); }
   template <class T2>
               sgl_own_ptr(std::auto_ptr<T2>& r) : own_ptr<T>(r.release(), true) { DM("sgl_own_ptr(auto_ptr&%p)", &r); }
               sgl_own_ptr(own_ptr<T>& r)       : own_ptr<T>(_Uninitialized_) { Attach(r.get(), r.own()); r.release(); }
   template <class T2>
               sgl_own_ptr(own_ptr<T2>& r)      : own_ptr<T>(_Uninitialized_) { Attach(r.get(), r.own()); r.release(); }
               sgl_own_ptr(typename own_ptr<T>::ref r) : own_ptr<T>(_Uninitialized_) { Attach(r.Ref.get(), r.Ref.own()); r.Ref.release(); }
   template <class T2>
               sgl_own_ptr(typename own_ptr<T2>::ref r) : own_ptr<T>(_Uninitialized_) { Attach(r.Ref.get(), r.Ref.own()); r.Ref.release(); }
               sgl_own_ptr(sgl_own_ptr<T,SGL>& r) : own_ptr<T>(r.Ptr, r.Own) { r.release(); }
   template <class T2, T2* SGL2>
               sgl_own_ptr(sgl_own_ptr<T2,SGL2>& r) : own_ptr<T>(_Uninitialized_) { Attach(r.get_raw(), r.own()); r.release(); }
   #ifdef MMPartialTemplateClassTemplateFn
   template <class T2>
               sgl_own_ptr(sgl_own_ptr<T2,SGL>& r) : own_ptr<T>(r.get(), r.own()) { r.release(); } // faster if same singleton
   #endif
               sgl_own_ptr(ref r)               : own_ptr<T>(r.Ref.Ptr, r.Ref.Own) { r.Ref.release(); }
   template <class T2, T2* SGL2>
               sgl_own_ptr(typename sgl_own_ptr<T2,SGL2>::ref r) : own_ptr<T>(_Uninitialized_) { Attach(r.Ref.get_raw(), r.Ref.own()); r.Ref.release(); }
   template <class T2, T2* SGL2>
               sgl_own_ptr(typename sgl_own_ptr<T2,SGL2>::ref r, static_cast_tag) : own_ptr<T>(_Uninitialized_) { Attach(r.Ref.get_raw(), static_cast<T*>(r.Ref.own())); r.Ref.release(); }
   template <class T2, T2* SGL2>
               sgl_own_ptr(typename sgl_own_ptr<T2,SGL2>::ref r, dynamic_cast_tag) : own_ptr<T>(_Uninitialized_) { Attach(r.Ref.get_raw(), dynamic_cast<T*>(r.Ref.own())); r.Ref.release(); }
   template <class T2, T2* SGL2>
               sgl_own_ptr(typename sgl_own_ptr<T2,SGL2>::ref r, const_cast_tag) : own_ptr<T>(_Uninitialized_) { Attach(r.Ref.get_raw(), const_cast<T*>(r.Ref.own())); r.Ref.release(); }
   operator    ref()                            { return ref(*this); }
   using       own_ptr<T>::operator*;
   using       own_ptr<T>::operator->;
   using       own_ptr<T>::own;
   using       own_ptr<T>::get;
   T*          get_raw() const                  { return Ptr == SGL ? NULL : Ptr; }
   T*          release();
   void        reset()                          { Detach(); Ptr = SGL; Own = false; }
   void        reset(T* ptr, bool own = true)   { if (ptr != Ptr) Reset(ptr, own); }
   void        reset(T& ref)                    { Detach(); Ptr = &ref; Own = false; }
   void        reset(sgl_own_ptr<T,SGL>& r)     { if (r.Ptr != Ptr) { Reset(r.get_raw(), r.Own); r.release(); } }
   template <class T2, T2* SGL2>
   void        reset(sgl_own_ptr<T2,SGL2>& r)   { if (r.Ptr != Ptr) { Reset(r.get_raw(), r.own()); r.release(); } }
   void        reset(ref r)                     { reset(r.Ref); }
   template <class T2, T2* SGL2>
   void        reset(typename sgl_own_ptr<T2,SGL2>::ref r) { reset(r.Ref); }
   void        reset(own_ptr<T>& r)             { if (r.Ptr != Ptr) { Reset(r.get(), r.own()); r.release(); } }
   template <class T2>
   void        reset(own_ptr<T2>& r)            { if (r.Ptr != Ptr) { Reset(r.get(), r.own()); r.release(); } }
   void        reset(typename own_ptr<T>::ref r) { reset(r.Ref); }
   template <class T2>
   void        reset(typename own_ptr<T2>::ref r) { reset(r.Ref); }
   void        reset(std::auto_ptr<T>& r)       { reset(r.release()); }
   template <class T2>
   void        reset(std::auto_ptr<T2>& r)      { reset(r.release()); }
   sgl_own_ptr<T,SGL>& operator=(T* r)          { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(T& r)          { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(sgl_own_ptr<T,SGL>& r) { reset(r); return *this; }
   template <class T2, T2* SGL2>
   sgl_own_ptr<T,SGL>& operator=(sgl_own_ptr<T2,SGL2>& r) { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(ref r)         { reset(r.Ref); return *this; }
   template <class T2, T2* SGL2>
   sgl_own_ptr<T,SGL>& operator=(typename sgl_own_ptr<T2,SGL2>::ref r) { reset(r.Ref); return *this; }
   sgl_own_ptr<T,SGL>& operator=(own_ptr<T>& r) { reset(r); return *this; }
   template <class T2>
   sgl_own_ptr<T,SGL>& operator=(own_ptr<T2>& r) { reset(r); return *this; }
   sgl_own_ptr<T,SGL>& operator=(typename own_ptr<T>::ref r) { reset(r.Ref); return *this; }
   template <class T2>
   sgl_own_ptr<T,SGL>& operator=(typename own_ptr<T2>::ref r) { reset(r.Ref); return *this; }
   sgl_own_ptr<T,SGL>& operator=(std::auto_ptr<T>& r) { reset(r.release()); return *this; }
   template <class T2>
   sgl_own_ptr<T,SGL>& operator=(std::auto_ptr<T2>& r) { reset(r.release()); return *this; }
   bool        operator!() const                { return Ptr == SGL; }
   bool        operator==(const T* r) const     { return get_raw() == r; }
   bool        operator!=(const T* r) const     { return get_raw() != r; }
};

// template implementation
template <class T, T* SGL>
void sgl_own_ptr<T, SGL>::Reset(T* ptr, bool own) // same as in base class, but for efficiency issues Attach and Detach are not virtual.
{  Detach();
   Attach(ptr, own);
}

template <class T, T* SGL>
T* sgl_own_ptr<T, SGL>::release()
{  T* ptr = get_raw();
   Ptr = SGL;
   Own = false;
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

// cast traits
template <typename D, typename S, D* DSGL, S* SSGL>
struct MM_cast_traits<sgl_own_ptr<D, DSGL>, sgl_own_ptr<S, SSGL> >
{  static sgl_own_ptr<D, DSGL> do_static_cast(sgl_own_ptr<S, SSGL> s)
   {  return sgl_own_ptr<D, DSGL>(s, _static_cast_);
   }
   static sgl_own_ptr<D, DSGL> do_dynamic_cast(sgl_own_ptr<S, SSGL> s)
   {  return sgl_own_ptr<D, DSGL>(s, _dynamic_cast_);
   }
   static sgl_own_ptr<D, DSGL> do_const_cast(sgl_own_ptr<S, SSGL> s)
   {  return sgl_own_ptr<D, DSGL>(s, _const_cast_);
   }
};


/*****************************************************************************
*
*  Reference counted pointer class with optional ownership
*
*****************************************************************************/
class ref_ptr_base // non-template base class to ref_ptr
// This class is not thread-safe so far.
{protected:
   struct internal
   {  volatile size_t Cnt;
   };
 protected:
   static internal NullRef;
   internal*   Ref;
 protected:
               ref_ptr_base()                   {} // protect class
   void        Attach(internal* ref)            { ++ref->Cnt; Ref = ref; }
   size_t      Detach()                         { return --Ref->Cnt; }
 public:
   size_t      ref_count() const                { return Ref->Cnt; }
};

template <class T>
class ref_ptr : public ref_ptr_base
{protected:
   struct internal : public ref_ptr_base::internal
   {  T*       Ptr;
      internal(T* ptr)                          : Ptr(ptr) { Cnt = 1; }
   };

 protected:
   T*          Ptr;

 protected:
   #ifdef MMSmartPointer_DEBUG
   void        DebugMessage(const char* s, ...);
   #endif
   void        Attach()                         { ref_ptr_base::Attach(&NullRef); Ptr = NULL; DM("Attach()"); }
   void        Attach(T& ref)                   { ref_ptr_base::Attach(&NullRef); Ptr = &ref; DM("Attach(&%p)", &ref); }
   void        Attach(T* ptr)                   { if (ptr) Ref = new internal(Ptr = ptr); else Attach(); DM("Attach(%p)", ptr); }
   void        Attach(T* ptr, bool own);
   void        Attach(const ref_ptr_base& ref, T* ptr) { ref_ptr_base::Attach(((ref_ptr<T>&)ref).Ref); Ptr = ptr; DM("Attach(%p{%lu},%p)", &((ref_ptr<T>&)ref).Ref, ref.ref_count(), ptr); }
   void        Detach();
 public:
               ref_ptr()                        { Attach(); DM("ref_ptr()"); }
               ref_ptr(T* ptr)                  { Attach(ptr); DM("ref_ptr(%p)", ptr); }
               ref_ptr(T* ptr, bool own)        { Attach(ptr, own); DM("ref_ptr(%p,%i)", ptr, own); }
               ref_ptr(T& ref)                  { Attach(ref); DM("ref_ptr(&%p)", &ref); }
               ref_ptr(std::auto_ptr<T>& r)     { Attach(r.release()); DM("ref_ptr(auto_ptr&%p)", &r); }
   template <class T2>
               ref_ptr(std::auto_ptr<T2>& r)    { Attach(r.release()); DM("ref_ptr(auto_ptr&%p)", &r); }
               ref_ptr(own_ptr<T>& r)           { Attach(r.get(), r.own()); r.release(); }
   template <class T2>
               ref_ptr(own_ptr<T2>& r)          { Attach(r.get(), r.own()); r.release(); }
               ref_ptr(typename own_ptr<T>::ref r) { Attach(r.Ref.get(), r.Ref.own()); r.Ref.release(); }
   template <class T2>
               ref_ptr(typename own_ptr<T2>::ref r) { Attach(r.Ref.get(), r.Ref.own()); r.Ref.release(); }
   template <class T2, T2* SGL2>
               ref_ptr(sgl_own_ptr<T2,SGL2>& r) { Attach(r.get_raw(), r.own()); r.release(); }
   template <class T2, T2* SGL2>
               ref_ptr(typename sgl_own_ptr<T2,SGL2>::ref r) { Attach(r.Ref.get_raw(), r.Ref.own()); r.Ref.release(); }
               ref_ptr(const ref_ptr<T>& r)     { Attach(r, r.Ptr);
                                                  DM("ref_ptr(&%p{%p{%i},%p})", &r, r.Ref, r.Ref->Cnt, Ptr); }
   template <class T2>
               ref_ptr(const ref_ptr<T2>& r)    { Attach(r, r.get()); DM("ref_ptr<T2>(&%p{%p{%i},%p})", &r, ((const ref_ptr<T>*)&r)->Ref, r.ref_count(), r.get()); }
   template <class T2>
               ref_ptr(const ref_ptr<T2>& r, static_cast_tag) { Attach(r, static_cast<T*>(r.get())); DM("ref_ptr<T2>(&%p{%p{%i},%p})", &r, ((const ref_ptr<T>*)&r)->Ref, r.ref_count(), r.get()); }
   template <class T2>
               ref_ptr(const ref_ptr<T2>& r, dynamic_cast_tag) { Attach(r, dynamic_cast<T*>(r.get())); DM("ref_ptr<T2>(&%p{%p{%i},%p})", &r, ((const ref_ptr<T>*)&r)->Ref, r.ref_count(), r.get()); }
   template <class T2>
               ref_ptr(const ref_ptr<T2>& r, const_cast_tag) { Attach(r, const_cast<T*>(r.get())); DM("ref_ptr<T2>(&%p{%p{%i},%p})", &r, ((const ref_ptr<T>*)&r)->Ref, r.ref_count(), r.get()); }
               ~ref_ptr()                       { DM("~"); Detach(); }
   T&          operator*() const                { return *get(); }
   T*          operator->() const               { return get(); }
   T*          get() const                      { return Ptr; }
   bool        own() const                      { return Ref != &NullRef; }
   void        reset()                          { Detach(); Attach(); }
   void        reset(T* ptr, bool own)          { Detach(); Attach(ptr, own); }
   void        reset(T* ptr)                    { Detach(); Attach(ptr); }
   void        reset(T& ref)                    { Detach(); Attach(ref); }
   void        reset(std::auto_ptr<T>& r)       { reset(r.release()); }
   template <class T2>
   void        reset(std::auto_ptr<T2>& r)      { reset(r.release()); }
   void        reset(own_ptr<T>& r)             { Attach(r.get(), r.own()); r.release(); }
   template <class T2>
   void        reset(own_ptr<T2>& r)            { Attach(r.get(), r.own()); r.release(); }
   void        reset(typename own_ptr<T>::ref r) { reset(r.Ref); }
   template <class T2>
   void        reset(typename own_ptr<T2>::ref r) { reset(r.Ref); }
   template <class T2, T2* SGL2>
   void        reset(sgl_own_ptr<T2,SGL2>& r)   { Attach(r.get_raw(), r.own()); r.release(); }
   template <class T2, T2* SGL2>
   void        reset(typename sgl_own_ptr<T2,SGL2>::ref r) { reset(r.Ref); }
   void        reset(const ref_ptr<T>& r)       { if (Ptr != r.Ptr) { Detach(); Attach(r, r.Ptr); } }
   template <class T2>
   void        reset(const ref_ptr<T2>& r)      { if (Ptr != r.get()) { Detach(); Attach(r, r.get()); } }
   ref_ptr<T>& operator=(T* r)                  { reset(r); return *this; }
   ref_ptr<T>& operator=(T& r)                  { reset(r); return *this; }
   ref_ptr<T>& operator=(std::auto_ptr<T>& r)   { reset(r); return *this; }
   template <class T2>
   ref_ptr<T>& operator=(std::auto_ptr<T2>& r)  { reset(r); return *this; }
   ref_ptr<T>& operator=(own_ptr<T>& r)         { reset(r); return *this; }
   template <class T2>
   ref_ptr<T>& operator=(own_ptr<T2>& r)        { reset(r); return *this; }
   ref_ptr<T>& operator=(typename own_ptr<T>::ref r) { reset(r.Ref); return *this; }
   template <class T2>
   ref_ptr<T>& operator=(typename own_ptr<T2>::ref r) { reset(r.Ref); return *this; }
   template <class T2, T2* SGL2>
   ref_ptr<T>& operator=(sgl_own_ptr<T2,SGL2>& r) { reset(r); return *this; }
   template <class T2, T2* SGL2>
   ref_ptr<T>& operator=(typename sgl_own_ptr<T2,SGL2>::ref r) { reset(r.Ref); return *this; }
   ref_ptr<T>& operator=(const ref_ptr<T>& r)   { reset(r); return *this; }
   template <class T2>
   ref_ptr<T>& operator=(const ref_ptr<T2>& r)  { reset(r); return *this; }
   bool        operator!() const                { return !get(); }
   bool        operator==(const T* r) const     { return get() == r; }
   bool        operator!=(const T* r) const     { return get() != r; }
};

// template implementation
#ifdef MMSmartPointer_DEBUG
template <class T>
void ref_ptr<T>::DebugMessage(const char* s, ...)
{  va_list va;
   va_start(va, s);
   fprintf(stderr, "%s(%p)::", typeid(this).name(), this);
   fprintf(stderr, s, va);
   fprintf(stderr, " {%p->{%lu,%p}}\n", Ref, Ref->Cnt, *(void**)(Ref+1));
   va_end(va);
}
#endif

template <class T>
void ref_ptr<T>::Attach(T* ptr, bool own)
{  if (ptr && own)
      Ref = new internal(Ptr = ptr);
    else
      Attach(*ptr);
   DM("Attach(%p,%i)", ptr, cnt);
}

template <class T>
void ref_ptr<T>::Detach()
{  DM("Detach()");
   if (ref_ptr_base::Detach() == 0)
   {  //delete Ptr;
      delete ((internal*)Ref)->Ptr; // delete original object instead of current (will also wor with non-virtual destructors)
      delete Ref;
      #ifdef MMSmartPointer_DEBUG
      Ptr = (T*)0xbbbbbbbb;
      Ref = (internal*)0xbbbbbbbb;
      #endif
}  }

// global operators
template <class T, class T2>
inline bool operator==(const ref_ptr<T>& l, const ref_ptr<T2>& r)
{  return l.get() == r.get();
}

template <class T, class T2>
inline bool operator!=(const ref_ptr<T>& l, const ref_ptr<T2>& r)
{  return l.get() != r.get();
}

// cast traits
template <typename D, typename S>
struct MM_cast_traits<ref_ptr<D>, ref_ptr<S> >
{  static ref_ptr<D> do_static_cast(ref_ptr<S> s)
   {  return ref_ptr<D>(s, _static_cast_);
   }
   static ref_ptr<D> do_dynamic_cast(ref_ptr<S> s)
   {  return ref_ptr<D>(s, _dynamic_cast_);
   }
   static ref_ptr<D> do_const_cast(ref_ptr<S> s)
   {  return ref_ptr<D>(s, _const_cast_);
   }
};


/*****************************************************************************
*
*  late implementations (dependencies)
*
*****************************************************************************/

template <class T>
template <class T2, T2* SGL>
inline own_ptr<T>::own_ptr(sgl_own_ptr<T2,SGL>& r)
 : Own(r.own())
 , Ptr(r.release())
{  DM("own_ptr(own_ptr&%p)", &r);
}
template <class T>
template <class T2, T2* SGL>
inline own_ptr<T>::own_ptr(typename sgl_own_ptr<T2,SGL>::ref r)
 : Own(r.Ref.own())
 , Ptr(r.Ref.release())
{  DM("own_ptr(own_ptr::ref %p{%i,%p})", &r.Ref, r.Ref.Own, r.Ref.Ptr);
}

template <class T>
template <class T2, T2* SGL>
inline void own_ptr<T>::reset(sgl_own_ptr<T2,SGL>& r)
{  Reset(r.get(), r.own());
   r.release();
   DM("own_ptr(own_ptr&%p)", &r);
}
template <class T>
template <class T2, T2* SGL>
inline void own_ptr<T>::reset(typename sgl_own_ptr<T2,SGL>::ref r)
{  reset(r.Ref);
}

/*****************************************************************************
*
*  Array base calss
*
*****************************************************************************/
template <class T>
class auto_array; // forward
template <class T>
class own_array; // forward
template <class T>
class ref_array; // forward

template <class T>
class array_base
{protected:
   size_t      Size;
   T*          Ptr;
 protected:
               array_base()                     {} // protect class
   void        Copy(const T* src);
   void        Fill(const T& obj);
 public:
   T*          get() const                      { return Ptr; } // can't return array ref
   size_t      size() const                     { return Size; }
   T*          begin() const                    { return Ptr; } // STL container like interface
   T*          end() const                      { return Ptr + Size; }
   #ifdef _DEBUG
   T&          operator[](size_t i) const       { if (i >= Size) throw class runtime_error(stringf("Array index out of bounds (%u/%u)", i, Size)); return Ptr[i]; }
   #else
   T&          operator[](size_t i) const       { return Ptr[i]; }
   #endif
   bool        operator!() const                { return !Ptr; }
   /*bool        operator==(T const r[]) const    { return Ptr == r; }
   bool        operator!=(T const r[]) const    { return Ptr != r; }*/
};

// template implementation
template <class T>
void array_base<T>::Copy(const T* src)
{  T* dst = Ptr;
   T* de = dst + Size;
   while (dst != de)
      *dst++ = *src++;
}

template <class T>
void array_base<T>::Fill(const T& obj)
{  T* dst = Ptr;
   T* de = dst + Size;
   while (dst != de)
      *dst++ = obj;
}

// global operators
template <class T>
inline bool operator==(const array_base<T>& l, const array_base<T>& r)
{  return l.get() == r.get();
}
template <class T>
inline bool operator!=(const array_base<T>& l, const array_base<T>& r)
{  return l.get() != r.get();
}

/*****************************************************************************
*
*  Simple, unmanaged array class
*
*****************************************************************************/
template <class T>
class array : public array_base<T>
{protected:
               array(uninitialized_tag)         {}
 public:
               array()                          { Size = 0; Ptr = NULL; }
               array(T* ptr, size_t size)       { Size = size; Ptr = ptr; }
               array(const array_base<T>& r)    { Size = r.Size; Ptr = r.Ptr; }
   template <size_t n>
               array(T (&r)[n])                 { Size = n; Ptr = r; }
/*               //array(const auto_array<T>& r);
               array(const own_array<T>& r);
               array(const ref_array<T>& r);*/
   void        reset()                          { Size = 0; Ptr = NULL; }
   void        reset(T* ptr, size_t size)       { Size = size; Ptr = ptr; }
   template <size_t n>
   void        reset(T (&r)[n])                 { Size = n; Ptr = r; return *this; }
   void        reset(const array_base<T>& r)    { Size = r.size(); Ptr = r.get(); }
/*   void        reset(const auto_array<T>& r);
   void        reset(const own_array<T>& r);
   void        reset(const ref_array<T>& r);*/
   template <size_t n>
   array<T>&   operator=(T (&r)[n])             { reset(r); return *this; }
   array<T>&   operator=(const array<T>& r)     { reset(r); return *this; }
   array<T>&   operator=(const auto_array<T>& r) { reset(r); return *this; }
   array<T>&   operator=(const own_array<T>& r) { reset(r); return *this; }
   array<T>&   operator=(const ref_array<T>& r) { reset(r); return *this; }
   T*          release();                       // only for convenience
};

// Template implementation
template <class T>
T* array<T>::release()
{  T* ptr = Ptr;
   Ptr = NULL;
   Size = 0;
   return ptr;
}

/*****************************************************************************
*
*  Managed array pointer class
*
*****************************************************************************/
template <class T>
class auto_array : public array<T>
{  friend class own_array<T>;
   friend class ref_array<T>;

 public:
   struct ref // proxy reference for copying
   {  auto_array<T>& Ref;  // reference to constructor argument
      // Since this class is only used as temporary object this will never
      // result in a dangling reference because the temporary object lifetime
      // is at least until the next statement.
      ref(auto_array<T>& r) : Ref(r)            {}
   };

 protected:
   void        Reset(T* ptr, size_t size);
               auto_array(uninitialized_tag)    : array<T>(_Uninitialized_) {}
 public:
               auto_array()                     {}
               auto_array(T* ptr, size_t size)  : array<T>(ptr, size) {}
   template <size_t S>
               auto_array(T (*r)[S])            : array<T>(*r, S) {}
               auto_array(auto_array<T>& r)     : array<T>(r.Ptr, r.Size) { r.release(); }
               auto_array(ref r)                : array<T>(r.Ref.Ptr, r.Ref.Size) { r.Ref.release(); }
   explicit    auto_array(size_t size)          : array<T>(new T[size], size) {}
               auto_array(size_t size, const T* src) : array<T>(new T[size], size) { Copy(src); }
               auto_array(size_t size, const T& obj) : array<T>(new T[size], size) { Fill(obj); }
               ~auto_array()                    { delete[] Ptr; }
   operator    ref()                            { return ref(*this); }
   using       array_base<T>::get;
   using       array_base<T>::size;
   using       array_base<T>::operator[];
   using       array_base<T>::operator!;
   /*using       array_base<T>::operator==;
   using       array_base<T>::operator!=;*/
   void        reset()                          { Reset(NULL, 0); }
   void        reset(T* ptr, size_t size)       { if (ptr != Ptr) Reset(ptr, size); }
   template <size_t S>
   void        reset(T (*r)[S])                 { Reset(*r, S); }
   void        reset(auto_array<T>& r)          { if (r.Ptr != Ptr) { Reset(r.get(), r.size()); r.release(); } }
   void        reset(ref r)                     { reset(r.Ref); }
   void        reset(size_t size)               { Reset(new T[size], size); }
   void        reset(size_t size, const T* ptr) { reset(size); Copy(ptr); }
   void        reset(size_t size, const T& obj) { reset(size); Fill(obj); }
   template <size_t S>
   auto_array<T>& operator=(T (*r)[S])          { reset(r); return *this; }
   auto_array<T>& operator=(auto_array<T>& r)   { reset(r); return *this; }
   auto_array<T>& operator=(ref r)              { reset(r.Ref); return *this; }
   /*using       array_base<T>::operator==;
   using       array_base<T>::operator!=;*/
};

// Template implementation
template <class T>
void auto_array<T>::Reset(T* ptr, size_t size)
{  delete[] Ptr;
   Ptr = ptr;
   Size = size;
}


/*****************************************************************************
*
*  Array pointer class which handles ownership
*
*****************************************************************************/
template <class T>
class own_array : protected array<T>
{public:
   struct ref // proxy reference for copying
   {  own_array<T>& Ref;  // reference to constructor argument
      // Since this class is only used as temporary object this will never
      // result in a dangling reference because the temporary object lifetime
      // is at least until the next statement.
      ref(own_array<T>& r) : Ref(r)             {}
   };

 protected:
   bool        Own;

 protected:
   void        Attach(T* ptr, size_t size, bool own);
   void        Attach(size_t size)              { Own = true; Size = size; Ptr = new T[size]; }
   void        Detach()                         { if (Own) delete[] Ptr; }
   void        Reset(T* ptr, size_t size, bool own) { if (ptr != Ptr) { Detach(); Attach(ptr, size, own); } }
               own_array(uninitialized_tag)     : array<T>(_Uninitialized_) {}
 public:
               own_array()                      : Own(false) {}
               own_array(T* ptr, size_t size, bool own = true) : auto_array<T>(_Uninitialized_) { Attach(ptr, size, own); }
               own_array(T& ref, size_t size)   : array<T>(&ref, size), Own(false) {}
   template <size_t S>
               own_array(T (*r)[S], bool own = true) : array<T>(_Uninitialized_) { Attach(*r, S, own); }
   template <size_t S>
               own_array(T (&r)[S])             : array<T>(r, S), Own(false) {}
               own_array(own_array<T>& r)       : array<T>(r.Ptr, r.Size), Own(r.Own) { r.release(); }
               own_array(ref r)                 : array<T>(r.Ref.Ptr, r.Ref.Size), Own(r.Ref.Own) { r.Ref.release(); }
               own_array(const array<T>& r)     : array<T>(r.get(), r.size()), Own(false) {}
               own_array(auto_array<T>& r)      : array<T>(_Uninitialized_) { Attach(r.get(), r.size(), true); r.release(); }
               own_array(typename auto_array<T>::ref r) : array<T>(_Uninitialized_) { Attach(r.Ref.get(), r.Ref.size(), true); r.Ref.release(); }
   explicit    own_array(size_t size)           : array<T>(new T[size], size), Own(true) {}
               own_array(size_t size, const T* src) : array<T>(new T[size], size), Own(true) { Copy(src); }
               own_array(size_t size, const T& obj) : array<T>(new T[size], size), Own(true) { Fill(obj); }
               ~own_array()                     { Detach(); }
   operator    ref()                            { return ref(*this); }
   using       array_base<T>::get;
   using       array_base<T>::size;
   using       array_base<T>::begin;
   using       array_base<T>::end;
   using       array_base<T>::operator[];
   using       array_base<T>::operator!;
   /*using       array_base<T>::operator==;
   using       array_base<T>::operator!=;*/
   bool        own() const                      { return Own; }
   T*          release();
   void        reset()                          { Detach(); Own = false; Size = 0; Ptr = NULL; }
   void        reset(T* ptr, size_t size, bool own = true) { Reset(ptr, size, own); }
   void        reset(T& ref, size_t size)       { Detach(); Own = false; Size = size; Ptr = &ref; }
   template <size_t S>
   void        reset(T (*r)[S], bool own = true) { Reset(*r, S, own); }
   template <size_t S>
   void        reset(T (&r)[S])                 { Detach(); Own = false; Size = S; Ptr = r; }
   void        reset(const array<T>& r)         { Detach(); Own = false; Size = r.size(); Ptr = r.get(); }
   void        reset(own_array<T>& r)           { Detach(); Own = r.Own; Size = r.Size; Ptr = r.release(); }
   void        reset(ref r)                     { reset(r.Ref); }
   void        reset(auto_array<T>& r)          { Detach(); Attach(r.get(), r.size(), true); r.release(); }
   void        reset(typename auto_array<T>::ref r) { reset(r.Ref); }
   void        reset(size_t size)               { Detach(); Attach(size); }
   void        reset(size_t size, const T* ptr) { reset(size); Copy(ptr); }
   void        reset(size_t size, const T& obj) { reset(size); Fill(obj); }
   template <size_t S>
   own_array<T>& operator=(T (*r)[S])           { reset(r); return *this; }
   template <size_t S>
   own_array<T>& operator=(T (&r)[S])           { reset(r); return *this; }
   own_array<T>& operator=(const array<T>& r)   { reset(r); return *this; }
   own_array<T>& operator=(own_array<T>& r)     { reset(r); return *this; }
   own_array<T>& operator=(ref r)               { reset(r.Ref); return *this; }
   own_array<T>& operator=(auto_array<T>& r)    { reset(r); return *this; }
   own_array<T>& operator=(typename auto_array<T>::ref r) { reset(r); return *this; }
};

// Template implementation
template <class T>
void own_array<T>::Attach(T* ptr, size_t size, bool own)
{  if ((Ptr = ptr) != NULL)
   {  Size = size;
      Own = own;
   } else
   {  Size = 0;
      Own = false;
}  }

template <class T>
T* own_array<T>::release()
{  T* ptr = Ptr;
   Ptr = NULL;
   Size = 0;
   Own = false;
   return ptr;
}


/*****************************************************************************
*
*  Reference counted array pointer class with optional ownership
*
*****************************************************************************/
class ref_array_base // non-template base class to ref_array
// This class is not thread-safe so far.
{protected:
   struct internal
   {  volatile size_t Cnt;
      size_t   Size;
   };

 protected:
   internal*   Ref;

 protected:
               ref_array_base()                 {} // protect class
   void        Attach(internal* ref)            { ++ref->Cnt; Ref = ref; }
   size_t      Detach()                         { return --Ref->Cnt; }
 public:
   size_t      size() const                     { return Ref->Size; }
   size_t      ref_count() const                { return Ref->Cnt; }
};

template <class T>
class ref_array : public ref_array_base
{protected:
   struct internal : public ref_array_base::internal
   {  T*       Ptr;
      internal(T* ptr, size_t size, size_t cnt) : Ptr(ptr) { Size = size, Cnt = cnt; }
   };

   #ifdef MMTemplateStatics
 private:
   static internal NullRef;
   #endif

 protected:
   #ifdef MMSmartPointer_DEBUG
   void        DebugMessage(const char* s, ...);
   #endif
   void        Attach(ref_array_base::internal* ref);
   void        Attach(T* ptr, size_t size, size_t cnt = 1);
   #ifdef MMTemplateStatics
   void        Attach()                         { ref_array_base::Attach(&NullRef); }
   #else
   void        Attach()                         { Attach(NULL, 0); }
   #endif
   void        Attach(size_t size)              { Ref = new internal(size ? new T[size] : NULL, size, 1); }
   void        Detach();
   void        Copy(const T* src);
   void        Fill(const T& obj);
               ref_array(uninitialized_tag)     {}
 public:
               ref_array()                      { Attach(); }
               ref_array(T* ptr, size_t size, bool own = true) { Attach(ptr, size, cnt); }
               ref_array(T& ref, size_t size)   { Ref = new internal(&ref, size, 1); }
   template <size_t S>
               ref_array(T (*r)[S], bool own = true) { Attach(ptr, size, own); }
   template <size_t S>
               ref_array(T (&r)[S])             { Attach(&ref, size, false); }
               ref_array(const array<T>& r)     { Attach(r.get(), r.size(), false); }
               ref_array(auto_array<T>& r)      { Attach(r.get(), r.size()); r.release(); }
               ref_array(typename auto_array<T>::ref r) { Attach(r.Ref.get(), r.Ref.size()); r.Ref.release(); }
               ref_array(own_array<T>& r)       { Attach(r.get(), r.size(), r.own()); r.release(); }
               ref_array(typename own_array<T>::ref r) { Attach(r.Ref.get(), r.Ref.size(), r.own()); r.Ref.release(); }
               ref_array(const ref_array<T>& r) { Attach(r.Ref); }
   explicit    ref_array(size_t size)           { Attach(size); }
               ref_array(size_t size, const T* src) { Attach(size); Copy(src); }
               ref_array(size_t size, const T& obj) { Attach(size); Fill(obj); }
               ~ref_array()                     { DM("~"); Detach(); }
   T*          get() const                      { return ((internal*)Ref)->Ptr; }
   T*          begin() const                    { return get(); } // STL container like interface
   T*          end() const                      { return get() + size(); }
   bool        own() const                      { return true; }
   //operator    T(*)[]() const                     { return get(); }
   #ifdef _DEBUG
   T&          operator[](size_t i) const       { if (i >= Ref->Size) throw class runtime_error(stringf("Array index out of bounds (%u/%u)", i, Ref->Size)); return get()[i]; }
   #else
   T&          operator[](size_t i) const       { return get()[i]; }
   #endif
   void        reset()                          { Detach(); Attach(); }
   void        reset(T* ptr, size_t size, bool own = true) { Detach(); Attach(ptr, size, own); }
   template <size_t S>
   void        reset(T (*r)[S], bool own = true) { reset(*r, S, own); }
   template <size_t S>
   void        reset(T (&r)[S])                 { Detach(); Ref = new internal(&ref, size, 0); }
   void        reset(const array<T>& r)         { Detach(); Attach(r.get(), r.size(), false); }
   void        reset(auto_array<T>& r)          { Detach(); Attach(r.get(), r.size()); r.release(); }
   void        reset(typename auto_array<T>::ref r) { reset(r.Ref); }
   void        reset(own_array<T>& r)           { Detach(); Attach(r.get(), r.size(), r.own()); r.release(); }
   void        reset(typename own_array<T>::ref r) { reset(r.Ref); }
   void        reset(const ref_array<T>& r)     { if (Ref != r.Ref) { Detach(); Attach(r.Ref); } }
   void        reset(size_t size)               { Detach(); Attach(size); }
   void        reset(size_t size, const T* ptr) { reset(size); Copy(ptr); }
   void        reset(size_t size, const T& obj) { reset(size); Fill(obj); }
   template <size_t S>
   ref_array<T>& operator=(T (*r)[S])           { reset(r); return *this; }
   template <size_t S>
   ref_array<T>& operator=(T (&r)[S])           { reset(r); return *this; }
   ref_array<T>& operator=(const array<T>& r)   { reset(r); return *this; }
   ref_array<T>& operator=(auto_array<T>& r)    { reset(r); return *this; }
   ref_array<T>& operator=(typename auto_array<T>::ref r) { reset(r.Ref); return *this; }
   ref_array<T>& operator=(own_array<T>& r)     { reset(r); return *this; }
   ref_array<T>& operator=(typename own_array<T>::ref r) { reset(r.Ref); return *this; }
   ref_array<T>& operator=(const ref_array<T>& r) { reset(r); return *this; }
   bool        operator!() const                { return !get(); }
   bool        operator==(T const r[]) const    { return get() == r; }
   bool        operator!=(T const r[]) const    { return get() != r; }
};

#ifdef MMTemplateStatics
// template statics
template <class T>
typename ref_array<T>::internal ref_array<T>::NullRef(NULL, 1); // counter will never return to zero
#endif

// template implementation
#ifdef MMSmartPointer_DEBUG
template <class T>
void ref_array<T>::DebugMessage(const char* s, ...)
{  va_list va;
   va_start(va, s);
   fprintf(stderr, "%s(%p)::", typeid(this).name(), this);
   fprintf(stderr, s, va);
   fprintf(stderr, " {%p->{%lu,%lu,%p}}\n", Ref, Ref->Cnt, Ref->Size, ((internal*)Ref)->Ptr);
   va_end(va);
}
#endif

template <class T>
void ref_array<T>::Attach(ref_array_base::internal* r)
{  if (r->Cnt)
      ref_array_base::Attach(r);
    else
      Ref = new internal(((internal*)r)->Ptr, r->Size, 0);
   DM("Attach({%lu, %lu, %p})", r->Cnt, r->Size, ((internal*)r)->Ptr);
}
template <class T>
void ref_array<T>::Attach(T* ptr, size_t size, size_t cnt)
{  if (ptr)
      Ref = new internal(ptr, size, cnt);
    else
      Attach();
   DM("Attach(%p, %lu, %lu)", ptr, size, cnt);
}

template <class T>
void ref_array<T>::Detach()
{  DM("Detach()");
   if (Ref->Cnt)
      if (ref_array_base::Detach() == 0)
         delete[] get();
       else
         return;
   delete Ref;
}

template <class T>
void ref_array<T>::Copy(const T* src)
{  T* dst = get();
   T* de = dst + Ref->Size;
   while (dst != de)
      *dst++ = *src++;
}

template <class T>
void ref_array<T>::Fill(const T& obj)
{  T* dst = get();
   T* de = dst + Ref->Size;
   while (dst != de)
      *dst++ = obj;
}

// global operators
template <class T>
inline bool operator==(const ref_array<T>& l, const ref_array<T>& r)
{  return l.get() == r.get();
}
template <class T>
inline bool operator!=(const ref_array<T>& l, const ref_array<T>& r)
{  return l.get() != r.get();
}


/*****************************************************************************
*
*  late implementations (dependencies)
*
*****************************************************************************/

/*template <class T>
inline array<T>::array(const auto_array<T>& r)
{  Size = r.size();
   Ptr = r.get();
}*/
/*template <class T>
inline array<T>::array(const own_array<T>& r)
{  Size = r.size();
   Ptr = r.get();
}
template <class T>
inline array<T>::array(const ref_array<T>& r)
{  Size = r.size();
   Ptr = r.get();
}

template <class T>
inline void array<T>::reset(const auto_array<T>& r)
{  Size = r.size();
   Ptr = r.get();
}
template <class T>
inline void array<T>::reset(const own_array<T>& r)
{  Size = r.size();
   Ptr = r.get();
}
template <class T>
inline void array<T>::reset(const ref_array<T>& r)
{  Size = r.size();
   Ptr = r.get();
}*/


#undef DM

#ifdef _MSVC_VER
#pragma warning(pop)
#endif

#endif
