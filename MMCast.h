/*****************************************************************************
*
*  MMCast.h - generic cast expressions
*
*  (C) 2003 Marcel Mueller, Fulda, Germany
*
*  Freeware
*
****************************************************************************/


#ifndef MMCast_h
#define MMCast_h



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

#define static_cast ::MM_static_cast
#define dynamic_cast ::MM_dynamic_cast
#define const_cast ::MM_const_cast


/*****************************************************************************
*
*  You may now specialize MM_cast_traits to add your own functionality
*
*****************************************************************************/


#endif
