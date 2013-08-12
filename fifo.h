#include <stdlib.h>
#include <vector>

#include <MMUtil+.h>

/*****************************************************************************
*
*  fifo.cpp - very fast fifo buffer implementation
*
*  This fifo buffer is designed for high throughput at low CPU load.
*  This is commonly required by multimedia applications. The data is not
*  copied at all. The buffers may be passed directly to DMA controllers.
*  The public functions are grouped into three interfaces. One for the fifo
*  input (a data drain), one for the fifo output (a data source) and one for
*  administrative purposes. Each interface may be used in a separate thread.
*  It is even safe to destroy the fifo object while another thread uses the
*  source or drain interface as long as you guarantee that the the interface
*  objects are no longer used after one of the interface functions throwed an
*  interupt_exception.
*
*****************************************************************************/

namespace MM {
namespace FIFO {

// ********** Interfaces

// Data drain interface (fifo writes)
// Thread safety: one instance <-> one thread.
struct Drain
{  // Request a buffer to store data.
   // The function returns a buffer where the data shold be stored.
   // Once the data is stored in the buffer you must commit this by calling
   // CommitWrite. Each call to request write requires exactly one call to
   // CommitWrite.
   // data [out] - Pointer where to store the data.
   // len [in]   - Maximum length of the returned buffer. This parameter
   //              must not be zero. You may pass INT_MAX to get a buffer as
   //              large as possible.
   // len [out]  - Returned length of the data buffer. This is less than or
   //              equal to len on input, but it will never be zero unless the
   //              reader signaled that it is no longer reading data.
   // This function will block if the fifo is full and no more buffer space
   // is available. Whether it unblocks when only a small amount of data is
   // available depends on the implementation.
   // You must not commit the buffer if the function returned with len = 0.
   virtual void RequestWrite(void*& data, size_t& len) = 0;
   // Commit the write buffer.
   // This functions commits the data to the FIFO so it might be passed to the
   // source interface. It is not allowed to access or modify the buffer data
   // after CommitWrite.
   // data [in] - The data pointer must be exactly the same than the pointer
   //             returned by RequestWrite.
   // len [in]  - The len parameter must be less than or equal to the length
   //             returned by RequestWrite. However, it is strongly
   //             recommended that len is equal to the length returned by
   //             RequestWrite except for the end of the stream.
   // Whether more than one RequestWrite call might be outstanding at a time
   // is implementation dependant. Whether the outstanding calls might be
   // committed out of order is implementation dependant as well.
   // This function will not block.
   virtual void CommitWrite(void* data, size_t len) = 0;
   // Tell the FIFO about the end of the input stream. This will cause the
   // source interface to return a length of zero when the buffer gets empty.
   // Outstanding requests will implicitly be canceled. Once you called
   // EndWrite RequestWrite will always return with a length of zero. 
   // This function will not block.
   virtual void EndWrite() = 0;
};

// Data source interface (fifo read)
// Thread safety: one instance <-> one thread.
struct Source
{  // Request data from the fifo.
   // The function returns a buffer with data. When the data is no longer
   // required this must be commited by calling CommitRead.
   // data [out] - Pointer to the data buffer.
   // len [in]   - Maximum length of the requested data. This parameter
   //              must not be zero. Pass INT_MAX to fetch as much data as
   //              possible.
   // len [out]  - Length of the data buffer. This is less than or equal to
   //              len on input, but it will never be zero unti the end of
   //              the stream has been reached.
   // If there is no data in the fifo the function blocks. Whether it unblocks
   // if only a small amount of data becomes available depends on the
   // implementation. Once the end of the stream is reached RequestRead will
   // always return immediately with len = 0. You must not commit this empty
   // buffer.
   virtual void RequestRead(void*& data, size_t& len) = 0;
   // Commit the data read by RequestRead.
   // This function commits that the data has been read from the buffer
   // returned by RequestRead. You must not access the buffer after calling
   // CommitRead.
   // data [in] - The data pointer must be exactly the same than the pointer
   //             returned by RequestRead.
   // len [in]  - The len parameter should be the same than returned by
   //             RequestWrite for this buffer. Whether returning a smaller
   //             length is defined behaviour is implementation dependant.
   // Whether there may be more than one RequestRead buffer outstanding at the
   // same time is implementation dependant. 
   // This function will not block.
   virtual void CommitRead(void* data, size_t len) = 0;
   // tell the FIFO about that the output stream is no longer read. This will
   // discard any data left in the buffer 
   // source interface to return a length of zero sooner or later.
   // Once you called EndWrite you must not use the drain interface of this
   // FIFO instance anymore.
   // This function will not block.
   virtual void EndRead() = 0;
};

// Basic administrative interface
struct FIFO
{  // Get the drain interface of the fifo (where the data is stored).
   // This function will return always the same instance for one fifo
   // instance. The instance may not be used by parallel threads.
   virtual Drain& getDrain() = 0;
   // Get the source interface of the fifo (where the data is read).
   // This function will return always the same instance for one fifo
   // instance. The instance may not be used by parallel threads.
   virtual Source& getSource() = 0;
};

// Helper class to implement write access to the FIFO by copying the data.
// Use this class only if you cannot avoid the memcopy. In general this class
// will cause the data to flow two times more through the CPU to memory
// interface. One write and one read.  
class SimpleDrain
{private:
   Drain& OriginalDrain;
 public:
   // Construct SimpleDrain from a Drain interface.
   SimpleDrain(Drain& d) : OriginalDrain(d) {}
   // Write the given data to the fifo. This function will not return normally
   // until all data have been stored into the fifo.
   // src [in] - Pointer to the (binary) data.
   // len [in] - Length of the data. If len is zero write returns immediately
   //            without an error.
   // return   - Number of bytes NOT written. This will never non-zero
   //            until the buffer is broken for some reason.
   size_t write(const void* src, size_t len);
   // See Drain::EndWrite()
   void EndWrite() { OriginalDrain.EndWrite(); }
};

// Helper class to implement read access to the FIFO by copying the data.
// Use this class only if you cannot avoid the memcopy. In general this class
// will cause the data to flow two times more through the CPU to memory
// interface. One write and one read.  
class SimpleSource
{private:
   Source& OriginalSource;
 public:
   SimpleSource(Source& src) : OriginalSource(src) {}
   // Read data from the fifo.
   // dst [in*out] - Destination buffer where the (binary) data should be
   //                stored.
   // len [in]     - Length of the destination buffer.
   // return       - Used length of the destination buffer. This will never be
   //                less than len until the end of the input stream is
   //                reached by calling Drain::EndWrite and the fifo is empty.
   // This function will block until sufficient data is available.
   size_t read(void* dst, size_t len);
};


// Simple static implemetation of the FIFO interface.

class StaticFIFO
 : public FIFO
 , private Drain
 , private Source
{private:   // internal quasi-constant objects
   typedef std::vector<char> BufferType;
   typedef BufferType::iterator BufferIterator; 
   BufferType Buffer;
   size_t LowWaterMark;
   size_t HighWaterMark;
 private:   // internal state
   BufferIterator RdPos;  // current commited read position
   BufferIterator WrPos;  // current comitted write position
   size_t volatile Level; // (commited) fill level
   size_t RdReq;          // size of last outstanding read request
   size_t WrReq;          // size of last outstanding write request
   bool volatile EOS; // end of stream flag
   bool volatile Die; // destroy-flag
 private:   // internal semaphores
   IPC::FastMutex StateLock;
   IPC::Event NotifyDrain;
   IPC::Event NotifySource;
   
 public:    // public interface
   // Constructor for a static fifo of size buffersize. 
   explicit StaticFIFO(size_t buffersize, double highwater = 0.0, double lowwater = 1.0)
    : Buffer(buffersize)
    , LowWaterMark(Part2Bytes(lowwater))
    , HighWaterMark(Part2Bytes(highwater))
    , RdPos(Buffer.begin())
    , WrPos(Buffer.begin())
    , Level(0)
    , RdReq(0)
    , WrReq(0)
    , EOS(false)
    , Die(false)
   {}

   virtual ~StaticFIFO() { Die = true; }
   
   // @see FIFO::getDrain
   Drain& getDrain()
   {  return *this;
   }
   // @see FIFO::getSource
   Source& getSource()
   {  return *this;
   }

 protected: // public interface implementations (indirect)
   void RequestWrite(void*& data, size_t& len);
   void CommitWrite(void* data, size_t len);
   void EndWrite();
   void RequestRead(void*& data, size_t& len);
   void CommitRead(void* data, size_t len);
   void EndRead();
 
   size_t Part2Bytes(double part);
   
}; 

}} // end namespace

