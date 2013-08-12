/*****************************************************************************
*
*  MMCast.h - generic cast expressions
*
*  (C) 2003-2006 Marcel Mueller, Fulda, Germany
*
*  Licence: LGPL
*
*  Bugs:
*  - There is a known problem that cast expressions do no longer preserve the
*    compile-time constant attribute. E.g.:
*      static const int i = static_cast<unsigned char>(-1);
*    is no longer valid. C-style casts are not affected.
*    The automatic redirecting is only enabled if you define MM_CAST_REDIRECT.
*
****************************************************************************/


#ifndef MMCast_h
#define MMCast_h


namespace MM {

/*****************************************************************************
*
*  cast traits class
*
*****************************************************************************/

template <typename D, typename S>
struct MM_cast_traits
{  static D do_static_cast(S s)
   {  return static_cast<D>(s);
   }
   static D do_dynamic_cast(S s)
   {  return dynamic_cast<D>(s);
   }
   static D do_const_cast(S s)
   {  return const_cast<D>(s);
   }
   // we will never overload the reinterpret_cast!
};


/*****************************************************************************
*
*  redirect standard cast expressions
*
*****************************************************************************/

// wee need wrappers here
template <typename D, typename S>
inline D MM_static_cast(S s)
{  return MM_cast_traits<D,S>::do_static_cast(s);
}
template <typename D, typename S>
inline D MM_dynamic_cast(S s)
{  return MM_cast_traits<D,S>::do_dynamic_cast(s);
}
template <typename D, typename S>
inline D MM_const_cast(S s)
{  return MM_cast_traits<D,S>::do_const_cast(s);
}

#ifdef MM_CAST_REDIRECT
#define static_cast ::MM::MM_static_cast
#define dynamic_cast ::MM::MM_dynamic_cast
#define const_cast ::MM::MM_const_cast
#endif


/*****************************************************************************
*
*  You may now specialize MM_cast_traits to add your own functionality
*
*****************************************************************************/

} // namespace

#endif
