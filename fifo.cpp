/*****************************************************************************
*
*  FIFO buffer implementation.
*  Attension! This implememtation relies on implicit atomic read/write
*  operations to integers of size_t type and booleans. 
*  
*****************************************************************************/

#include <iostream>

#include "fifo.h"
#include <stdexcept>
#include <climits>
#include <memory.h>

namespace MM {
namespace FIFO {

using namespace MM::IPC;

// class SimpleDrain
size_t SimpleDrain::write(const void* src, size_t len)
{  while (len != 0)
   {  void* dst;
      size_t blen = len;
      OriginalDrain.RequestWrite(dst, blen);
      if (!blen)
         return len;
      memcpy(dst, src, blen);
      OriginalDrain.CommitWrite(dst, blen);
      (const char*&)src += blen;
      len -= blen;
   }
   return 0;
}

// class SimpleSource
size_t SimpleSource::read(void* dst, size_t len)
{  size_t olen = len;
   while (len != 0)
   {  void* src;
      size_t blen = len;
      OriginalSource.RequestRead(src, blen);
      if (!blen)
         return olen - len;
      memcpy(dst, src, blen);
      OriginalSource.CommitRead(src, blen);
      (char*&)dst += blen;
      len -= blen;
   }
   return olen;
}

// class StaticFIFO
void StaticFIFO::RequestWrite(void*& data, size_t& len)
{  if (WrReq != 0)
      throw std::logic_error("The StaticFIFO class does not support two buffer resquests without commit in between.");
   do
   {  Lock(StateLock);
      NotifyDrain.Reset();
      if (EOS)
      {  len = 0;
         return;
      }
      size_t rem = Buffer.size() - Level;
      if (rem > 0)
      {  if (len > rem)
            len = rem;
         rem = Buffer.end() - WrPos;
         if (len > rem)
            len = rem;
         data = &*WrPos;
         WrReq = len;
         return;
      }
   } while (NotifyDrain.Wait());
}

void StaticFIFO::CommitWrite(void* data, size_t len)
{  Lock(StateLock);
   if (data != &*WrPos)
      throw std::logic_error("Cannot commit a buffer that is not requested before.");
   if (len > WrReq)
      throw std::logic_error("Cannot commit a larger buffer than requested.");
   WrReq = 0;
   WrPos += len;
   if (WrPos == Buffer.end())
      WrPos = Buffer.begin();
   if ((Level += len) >= HighWaterMark)
      NotifySource.Set();
}

void StaticFIFO::EndWrite()
{  //Lock(StateLock); implicitly atomic
   EOS = true; // end of stream marker
   WrReq = 0; // cancel outstanding requests
   NotifySource.Set(); // notify the other side regardless of the high water mark.
}

void StaticFIFO::RequestRead(void*& data, size_t& len)
{  if (RdReq != 0)
      throw std::logic_error("The StaticFIFO class does not support two buffer resquests without commit in between.");
   do
   {  Lock(StateLock);
      NotifySource.Reset();
      if (Level > 0)
      {  if (len > Level)
            len = Level;
         size_t rem = Buffer.end() - RdPos;
         if (len > rem)
            len = rem;
         data = &*RdPos;
         RdReq = len;
         return;
      }
      if (EOS)
      {  len = 0;
         return;
      }
   } while (NotifySource.Wait());
}

void StaticFIFO::CommitRead(void* data, size_t len)
{  Lock(StateLock);
   if (data != &*RdPos)
      throw std::logic_error("Cannot commit a buffer that is not requested before.");
   if (len > RdReq)
      throw std::logic_error("Cannot commit a larger buffer than requested.");
   RdReq = 0;
   RdPos += len;
   if (RdPos == Buffer.end())
      RdPos = Buffer.begin();
   if ((Level -= len) <= LowWaterMark)
      NotifyDrain.Set();
}

void StaticFIFO::EndRead()
{  //Lock(StateLock); implicitly atomic
   EOS = true; // end of stream marker
   RdReq = 0; // cancel outstanding requests
   NotifyDrain.Set(); // notify the other side regardless of the low water mark.
}

size_t StaticFIFO::Part2Bytes(double part)
{  if (part < 0.0 || part > 1.0)
      throw std::invalid_argument("The fractional part of the fifo buffer is not in the range [0,1]."); 
   return (size_t)(Buffer.size() * part +.5); // rounding is still a dark chapther of the C language
}

}} // end namespace
