#define INCL_BASE
#include <os2.h>
#include <climits>
#include <cctype>
#include <string>
#include <iostream>
#include <memory>

#include "fifo.h"

#define HF_STDIN 0
#define HF_STDOUT 1

using namespace std;
using namespace MM;
using namespace MM::FIFO;
using namespace MM::IPC;

Mutex LogMtx;
#define lerr (Lock(LogMtx), cerr) // lock cerr until the next syncronization point.

struct syntax_error : logic_error
{  syntax_error(const string& text) : logic_error(text) {}
};

#define APICHK(r, m) \
   if (r) \
      throw os_error(r, m);


static int BufferSize = 65536;
static int RequestSize = -1;
static int PipeSize = 8192;

static int iHighWaterMark = 0;
static double dHighWaterMark = -1; // invalid
static int iLowWaterMark = -1; // invalid
static double dLowWaterMark = 1;

static bool EnableCache = false;
static bool EnableInputStats = false;
static bool EnableOutputStats = false;
static const double StatsUpdate = .3;


// performance counter
class PerfCount
{  uint32_t Loops;
   uint64_t BytesSoFar;
   uint64_t StartTime;
   uint32_t Freq;
   uint64_t Elapsed;
 public:
   PerfCount();
   void Update(size_t bytes);
   uint64_t getBytes() const     { return BytesSoFar; }
   uint32_t getBlocks() const    { return Loops; }
   double getSeconds() const     { return (double)Elapsed / Freq; }
   double getRate() const;
   double getBlockRate() const;
   double getAvgBlockSize() const{ return (double)BytesSoFar / Loops; }
};

PerfCount::PerfCount() : Loops(0), BytesSoFar(0)
{  DosTmrQueryFreq((PULONG)&Freq);
   DosTmrQueryTime((PQWORD)&StartTime);
}

void PerfCount::Update(size_t bytes)
{  BytesSoFar += bytes;
   ++Loops;
   DosTmrQueryTime((PQWORD)&Elapsed);
   Elapsed -= StartTime;
}

double PerfCount::getRate() const
{  return (double)BytesSoFar / Elapsed * Freq;
}

double PerfCount::getBlockRate() const
{  return (double)Loops / Elapsed * Freq;
}

// worker base class
class Worker
{protected:
   int Result;
   Worker() : Result(0) {}
   bool isPipe(const char* name);
 public:
   int getResult() { return Result; }
};
          
bool Worker::isPipe(const char* name)
{  if (strnicmp(name, "\\PIPE\\", 6) == 0) // local pipe
      return true;
   if (strncmp(name, "\\\\", 2) != 0) // no remote name
      return false;
   const char* p = strchr(name+2, '\\');
   if (p != NULL && strnicmp(p+1, "PIPE\\", 5) == 0) // remote pipe
      return true;
   return false;
}

// input worker class
class InputWorker : public Worker
{  Drain& Dst;
   const char* Src;
 public:
   InputWorker(Drain& dst, const char* src) : Dst(dst), Src(src) {}
   void operator()();
};

void InputWorker::operator()()
{  try
   {  // initialize input
      HFILE hfi;
      if (strcmp(Src, "-") == 0)
         hfi = HF_STDIN;
       else if (isPipe(Src))
      {  // named pipe
         // first try to open an existing instance
         ULONG action;
         ULONG rc = DosOpen((PUCHAR)Src, &hfi, &action, 0, 0, OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
            OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYREAD|OPEN_ACCESS_READONLY, NULL);
         if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND)
         {  // try to create the pipe
            APICHK(DosCreateNPipe((PUCHAR)Src, &hfi, NP_ACCESS_INBOUND, NP_WAIT|NP_TYPE_BYTE|1, 0, PipeSize, 0)
             , stringf("Failed to create named inbound pipe %s.", Src));
            APICHK(DosConnectNPipe(hfi), stringf("Failed to connect named inbound pipe %s.", Src));
         } else
            APICHK(rc, stringf("Failed to open named inbound pipe %s.", Src));
      } else
      {  // ordinary file
         ULONG action;
         APICHK(DosOpen((PUCHAR)Src, &hfi, &action, 0, 0, OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
          EnableCache ? OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYNONE|OPEN_ACCESS_READONLY : OPEN_FLAGS_NO_CACHE|OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYNONE|OPEN_ACCESS_READONLY, NULL)
          , stringf("Failed to open %s for input.", Src));
      }

      auto_ptr<PerfCount> stats;
      double nextstat = StatsUpdate;
      if (EnableInputStats)
         stats.reset(new PerfCount());

      // data transfer loop
      for(;;)
      {  void* buf;
         size_t len = RequestSize;
         //lerr << stringf("before Drain.Request(%p,%lu)", buf, len) << endl;
         Dst.RequestWrite(buf, len);
         //lerr << stringf("Drain.Request(%p,%lu)", buf, len) << endl;
         if (len == 0)
         {  lerr << "Closing input and discarding buffer because the output side stopped working." << endl;
            break;
         }
         ULONG rlen;
         APICHK(DosRead(hfi, buf, len, &rlen), "Failed to read from input stream.");
         len = rlen;
         if (len == 0)
            break;
         Dst.CommitWrite(buf, len);
         //lerr << stringf("Drain.Commit(%p,%lu)", buf, len) << endl;

         if (EnableInputStats)
         {  stats->Update(len);
            double secs = stats->getSeconds();
            if (secs >= nextstat)
            {  nextstat = secs + StatsUpdate; 
               lerr << "Input: " << stats->getBytes()/1024 << " kiB so far at " << stats->getBytes()/secs/1024. << " kiB/s, avg. block size " << stats->getAvgBlockSize()/1024. << " kiB  \r";
            }
         } 
      }
   } catch (const interrupt_exception&)
   {  // no-op
   } catch (const runtime_error& e)
   {  lerr << "Error reading data: " << e.what() << endl;
      Result = 10;
   } catch (const logic_error& e)
   {  lerr << "Error in input worker: " << e.what() << endl;
      Result = 19;
   } catch (...)
   {  lerr << "Unhandled exception in input worker." << endl;
      Result = 28;
   }
   Dst.EndWrite(); // End of input signal
}

// output worker class
class OutputWorker : public Worker
{  Source& Src;
   const char* Dst;
 public:
   OutputWorker(Source& src, const char* dst) : Src(src), Dst(dst) {}
   void operator()();
};

void OutputWorker::operator()()
{  try
   {  // initialize output
      HFILE hfo;
      if (strcmp(Dst, "-") == 0)
         hfo = HF_STDOUT;
       else if (isPipe(Dst))
      {  // named pipe
         // first try to open an existing instance
         ULONG action;
         ULONG rc = DosOpen((PUCHAR)Dst, &hfo, &action, 0, 0, OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
            OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYWRITE|OPEN_ACCESS_WRITEONLY, NULL);
         if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND)
         {  // try to create the pipe
            APICHK(DosCreateNPipe((PUCHAR)Dst, &hfo, NP_ACCESS_OUTBOUND, NP_WAIT|NP_TYPE_BYTE|1, PipeSize, 0, 0)
             , stringf("Failed to create outbound pipe %s.", Dst));
            APICHK(DosConnectNPipe(hfo), stringf("Failed to connect outbound pipe %s.", Dst));
         } else
            APICHK(rc, stringf("Failed to open outbound pipe %s.", Dst));
      } else
      {  // ordinary file
         ULONG action;
         APICHK(DosOpen((PUCHAR)Dst, &hfo, &action, 0, 0, OPEN_ACTION_CREATE_IF_NEW|OPEN_ACTION_REPLACE_IF_EXISTS,
          EnableCache ? OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYWRITE|OPEN_ACCESS_WRITEONLY : OPEN_FLAGS_NO_CACHE|OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYWRITE|OPEN_ACCESS_WRITEONLY, NULL)
          , stringf("Failed to open %s for output.", Dst));
      }

      auto_ptr<PerfCount> stats;
      double nextstat = StatsUpdate;
      if (EnableOutputStats)
         stats.reset(new PerfCount());

      // data transfer loop
      for(;;)
      {  void* buf;
         size_t len = RequestSize;
         //lerr << stringf("before Source.Request(%p,%lu)", buf, len) << endl;
         Src.RequestRead(buf, len);
         //lerr << stringf("Source.Request(%p,%lu)", buf, len) << endl;
         if (len == 0)
            break;
         ULONG rlen;
         APICHK(DosWrite(hfo, buf, len, &rlen), "Failed to write to output stream.");
         len = rlen;
         if (len == 0)
            throw runtime_error("Failed to write to the output stream because the destination does not accept more data.");
         Src.CommitRead(buf, len);
         //lerr << stringf("Source.Commit(%p,%lu)", buf, len) << endl;
         if (EnableOutputStats)
         {  stats->Update(len);
            double secs = stats->getSeconds();
            if (secs >= nextstat)
            {  nextstat = secs + StatsUpdate; 
               lerr << "Output: " << stats->getBytes()/1024 << " kiB so far at " << stats->getBytes()/secs/1024. << " kiB/s, avg. block size " << stats->getAvgBlockSize()/1024. << " kiB  \r";
            }
         } 
      }
   } catch (const interrupt_exception&)
   {  // no-op
   } catch (const runtime_error& e)
   {  lerr << "Error writing data: " << e.what() << endl;
      Result = 11;
   } catch (const logic_error& e)
   {  lerr << "Error in output worker: " << e.what() << endl;
      Result = 19;
   } catch (...)
   {  lerr << "Unhandled exception in output worker." << endl;
      Result = 28;
   }  
   Src.EndRead(); // End of input signal
}


static void runInputWorker(void* param)
{  (*reinterpret_cast<InputWorker*>(param))();
}


static long parseint(const char* src)
{  long ret;
   size_t l = (size_t)-1;
   char unit[2] = "";
   if (sscanf(src, "=%li%n%1s%n", &ret, &l, unit, &l) == 0 || l != strlen(src))  
      throw syntax_error(stringf("'=' followed by an integer value expected. Found '%s'", src));
   switch (toupper(unit[0]))
   {default:
      throw syntax_error(stringf("The unit '%c' is invalid at the integer constant '%s'", unit[0], src+1));
    case 'K':
      ret *= 1024; break;
    case 'M':
      ret *= 1024*1024; break;
    case 'G':
      ret *= 1024*1024*1024; break;
    case 0:;      
   }
   return ret;
}

static double parsedouble(const char* src)
{  double ret;
   size_t l = (size_t)-1;
   if (sscanf(src, "=%lf%n", &ret, &l) == 0 || l != strlen(src))  
      throw syntax_error(stringf("'=' followed by an floating-point value expected. Found '%s'", src));
   return ret;
} 

static void parseoption(char* cp)
{  switch (tolower(cp[1]))
   {case 'b':
      BufferSize = parseint(cp+2);
      if (BufferSize < 1)
         throw syntax_error("The buffer size must be positive."); 
      return;
    case 'r':
      RequestSize = parseint(cp+2);
      if (RequestSize < 1)
         throw syntax_error("The request size must be positive."); 
      return;
    case 'p':
      PipeSize = parseint(cp+2);
      if (PipeSize < 1)
         throw syntax_error("The pipe buffer size must be positive."); 
      return;
    case 'h':
      if (cp[1+strlen(cp+2)] == '%')
      {  cp[1+strlen(cp+2)] = 0;
         dHighWaterMark = parsedouble(cp+2) / 100;
         if (dHighWaterMark < 0 || dHighWaterMark > 100)
            throw syntax_error(stringf("The relative buffer level %s%% is not in the range 0-100%.", cp+3));
      } else
      {  iHighWaterMark = parseint(cp+2);
         dHighWaterMark = -1;
      }
      return;
    case 'l':
      if (cp[1+strlen(cp+2)] == '%')
      {  cp[1+strlen(cp+2)] = 0;
         dLowWaterMark = parsedouble(cp+2) / 100;
         if (dLowWaterMark < 0 || dLowWaterMark > 100)
            throw syntax_error(stringf("The relative buffer level %s%% is not in the range 0-100%.", cp+3));
      } else
      {  iLowWaterMark = parseint(cp+2);
         dLowWaterMark = -1;
      }
      return;
    case 'c':
      EnableCache = true;
      return;
    case 's':
      switch (tolower(cp[2]))
      {case 'i':
         EnableInputStats = true;
         return;
       case 'o':
         EnableOutputStats = true;
         return;
      }
      break;
   }
   throw syntax_error(stringf("Invalid option %s.", cp));
}

int main(int argc, char**argv)
{  const char* input = NULL;
   const char* output = NULL;

   try
   {  while (*++argv) // skip arg[0]
      {  if ((*argv)[0] == '/')
            // option
            parseoption(*argv);
          else if (input == NULL)
            input = *argv;
          else if (output == NULL)
            output = *argv;
          else
            throw syntax_error(stringf("More than two I/O objects in the command line (at %s).", *argv));
      }

      // check if we have source & destination
      if (output == NULL)
      {  cerr <<
            "usage " << argv[0] << " <input> <output> [options]\n\n"
            "<input>: Input stream. This is one of\n"
            "         Filename - an ordinary file which is read until EOF,\n"
            "         Pipe - a named pipe e.g. \\PIPE\\MyPipe (The pipe is created if it does\n"
            "                not exist.) or\n"
            "         \"-\" - stdin\n"
            "<output>: Output stream. This is one of\n"
            "          Filename - an ordinary file which is APPENDED,\n"
            "          Pipe - a named pipe e.g. \\PIPE\\MyPipe (The Pipe is created if it does\n"
            "                 not exist.),\n"
            "          Device - any character device like \"LPT1\" or\n"
            "          \"-\" - stdout.\n"
            "options:\n"   
            " /b=<size>  Internal fifo buffer size. 64kiB by default. If the number is\n"
            "            followed directly by the letter `k', `m' or `g' the size is\n"
            "            multiplied by 1024 to the power of 1, 2 or 3.\n"
            " /p=<size>  Pipe buffer size, only if a pipe is created. If the number is\n"
            "            followed directly by the letter `k' the size is multiplied by 1024.\n"
            "            Note that OS/2 does not accept pipe buffers beyond 64kiB.\n"
            " /r=<size>  I/O-request size. As much as possible by default. The request size\n"
            "            should neither exceed the fifo size nor the pipe buffer size.\n"
            "            Larger values have no effect.\n"
            " /h=<level> High water mark. If the output thread is stopped because of an\n"
            "            empty buffer it will not resume until the buffer is filled up to\n"
            "            the high water mark. The level must be less than or equal to the\n"
            "            buffer size. If level ends with % the size is relative to the\n"
            "            buffer size in percent. The default value of 0 causes the output\n"
            "            thread never to stop unless the buffer is empty.\n"
            " /l=<level> Low water mark. If the input thread is stopped because the buffer\n"
            "            is full it will not resume until the buffer is emptied at least to\n"
            "            the low water mark. The level must be be less than or equal to the\n"
            "            buffer size. If level ends wit % the size is relative to the\n"
            "            buffer size in percent. The default value of 100% causes the input\n"
            "            thread never to stop unless the buffer is completly full.\n"
            " /c         Enable file system cache.\n"
            " /s         Print statistics to stderr.\n";
         return 48;
      }

      // some calculations
      if (dHighWaterMark < 0)
      {  if (iHighWaterMark > BufferSize)
            throw syntax_error("The high water mark is larger than the buffer size.");
         dHighWaterMark = (double)iHighWaterMark / BufferSize;
      }
      if (dLowWaterMark < 0)
      {  if (iLowWaterMark > BufferSize)
            throw syntax_error("The low water mark is larger than the buffer size.");
         dLowWaterMark = (double)iLowWaterMark / BufferSize;
      }
      if (RequestSize < 0)
      {  // Auto Calculate
         RequestSize = BufferSize >= 1024*256 ? BufferSize / 8 : BufferSize / 4;
      } 
      
      // initialize buffer and Workers
      StaticFIFO fifo(BufferSize, dHighWaterMark, dLowWaterMark);
      InputWorker iwrk(fifo.getDrain(), input);
      OutputWorker owrk(fifo.getSource(), output);

      // start reader thread
      int iwtid = _beginthread(runInputWorker, NULL, 65536, &iwrk);
      if (iwtid == -1)
         throw runtime_error("Failed to start input worker thread.");

      // execute output worker in main thread
      owrk();
      
      // join the input worker thread
      TID tid = iwtid;
      DosWaitThread(&tid, DCWW_WAIT);

      return iwrk.getResult() != 0 ? iwrk.getResult() : owrk.getResult();

   } catch (const syntax_error& e)
   {  lerr << e.what();
      return 49;
   } catch (const runtime_error& e)
   {  lerr << e.what();
      return 29;
   } catch (const logic_error& e)
   {  lerr << e.what();
      return 19;
   } catch (...)
   {  lerr << "Unhandled excaption." << endl;
      return 28;
   }
}

