#include "IOinterface.h"
#include "buffer2.h"
#include "fifo.h"
#include "PerfCount.h"

#include <stdint.h>
#include <string.h>
#include <climits>
#include <cctype>
#include <string>
#include <memory>

#ifdef __OS2__
// builin threads in OS/2
#else
// use pthreads
#include <pthread.h>
#endif

using namespace std;
using namespace MM;
using namespace MM::FIFO;
using namespace MM::IPC;

int BufferSize = 65536;
int RequestSize = -1;
int PipeSize = 8192;
unsigned BufferAlignment = 1U << 14; // MUST be a power of 2

int iHighWaterMark = 0;
double dHighWaterMark = -1; // invalid
int iLowWaterMark = -1; // invalid
double dLowWaterMark = 1;

bool EnableCache = false;
#ifdef __OS2__
bool AdvantageInput = false;
bool AdvantageOutput = false;
#endif
bool EnableInputStats = false;
bool EnableOutputStats = false;
const double StatsUpdate = .3;
const size_t StatusBytes = 256*1024;

volatile const MM::FIFO::FIFO::Statistics* FIFOstat;

MM::IPC::Mutex LogMtx;

// worker base class
class Worker
{protected:
	int Result;
	Worker() : Result(0) {}
 public:
	virtual ~Worker() {}
	virtual void operator()() = 0;
	int getResult() { return Result; }
};

// input worker class
class InputWorker : public Worker
{	Drain& Dst;
	auto_ptr<IInput> Src;
 public:
	InputWorker(Drain& dst, IInput* src) : Dst(dst), Src(src) {}
	virtual void operator()();
};

void InputWorker::operator()()
{	try
	{	// initialize input
		Src->Initialize();

		auto_ptr<PerfCount> stats;
		double nextstat = StatsUpdate;
		size_t statbytes = 0;
		if (EnableInputStats)
			stats.reset(new PerfCount());

		// data transfer loop
		for(;;)
		{	void* buf;
			size_t len = RequestSize;
			//lerr << stringf("before Drain.Request(%p,%lu)", buf, len) << endl;
			Dst.RequestWrite(buf, len);
			//lerr << stringf("Drain.Request(%p,%lu)", buf, len) << endl;
			if (len == 0)
			{	lerr << "Closing input and discarding buffer because the output side stopped working." << endl;
				break;
			}
			// read from interface
			len = Src->ReadData(buf, len);
			if (len == 0)
				break;
			Dst.CommitWrite(buf, len);
			//lerr << stringf("Drain.Commit(%p,%lu)", buf, len) << endl;

			if (EnableInputStats)
			{	stats->Update(len);
				statbytes += len;
				if (statbytes > StatusBytes)
				{	statbytes = 0;
					double secs = stats->getSeconds();
					if (secs >= nextstat)
					{	nextstat = secs + StatsUpdate;
						lerr << "Input: " << stats->getBytes()/1024 << " kiB at " << stats->getBytes()/secs/1024. << " kiB/s, " << stats->getAvgBlockSize()/1024. << " kiB/blk.; "
							"Fifo " << FIFOstat->FullCount << " times full, " << FIFOstat->EmptyCount << " times empty  \r";
					}
				}
			}
		}
		if (EnableInputStats)
		{	double secs = stats->getSeconds();
			lerr << "Input: " << stats->getBytes()/1024 << " kiB at " << stats->getBytes()/secs/1024. << " kiB/s, " << stats->getAvgBlockSize()/1024. << " kiB/blk.; "
				"Fifo " << FIFOstat->FullCount << " times full, " << FIFOstat->EmptyCount << " times empty  \r";
		}
	} catch (const interrupt_exception&)
	{	// no-op
	} catch (const runtime_error& e)
	{	lerr << "Error reading data: " << e.what() << endl;
		Result = 10;
	} catch (const logic_error& e)
	{	lerr << "Error in input worker: " << e.what() << endl;
		Result = 19;
	} catch (...)
	{	lerr << "Unhandled exception in input worker." << endl;
		Result = 28;
	}
	Dst.EndWrite(); // End of input signal
	Src.reset(); // free and close input interface
}

// output worker class
class OutputWorker : public Worker
{	Source& Src;
	auto_ptr<IOutput> Dst;
 public:
	OutputWorker(Source& src, IOutput* dst) : Src(src), Dst(dst) {}
	void operator()();
};

void OutputWorker::operator()()
{	try
	{	// initialize output
		Dst->Initialize();
		
		auto_ptr<PerfCount> stats;
		double nextstat = StatsUpdate;
		size_t statbytes = 0;
		if (EnableOutputStats)
			stats.reset(new PerfCount());

		// data transfer loop
		for(;;)
		{	void* buf;
			size_t len = RequestSize;
			//lerr << stringf("before Source.Request(%p,%lu)", buf, len) << endl;
			Src.RequestRead(buf, len);
			//lerr << stringf("Source.Request(%p,%lu)", buf, len) << endl;
			if (len == 0)
				break;
			len = Dst->WriteData(buf, len);
			if (len == 0)
				throw runtime_error("Failed to write to the output stream because the destination does not accept more data.");
			Src.CommitRead(buf, len);
			//lerr << stringf("Source.Commit(%p,%lu)", buf, len) << endl;
			if (EnableOutputStats)
			{	stats->Update(len);
				statbytes += len;
				if (statbytes > StatusBytes)
				{	statbytes = 0;
					double secs = stats->getSeconds();
					if (secs >= nextstat)
					{	nextstat = secs + StatsUpdate;
						lerr << "Output: " << stats->getBytes()/1024 << " kiB at " << stats->getBytes()/secs/1024. << " kiB/s, " << stats->getAvgBlockSize()/1024. << " kiB/blk.; "
							"Fifo " << FIFOstat->FullCount << " times full, " << FIFOstat->EmptyCount << " times empty  \r";
			}	}	}
		}
		if (EnableOutputStats)
		{	double secs = stats->getSeconds();
			lerr << "Output: " << stats->getBytes()/1024 << " kiB at " << stats->getBytes()/secs/1024. << " kiB/s, " << stats->getAvgBlockSize()/1024. << " kiB/blk.; "
				"Fifo " << FIFOstat->FullCount << " times full, " << FIFOstat->EmptyCount << " times empty  \r";
		}
	} catch (const interrupt_exception&)
	{	// no-op
	} catch (const runtime_error& e)
	{	lerr << "Error writing data: " << e.what() << endl;
		Result = 11;
	} catch (const logic_error& e)
	{	lerr << "Error in output worker: " << e.what() << endl;
		Result = 19;
	} catch (...)
	{	lerr << "Unhandled exception in output worker." << endl;
		Result = 28;
	}
	Src.EndRead(); // End of output signal
}

#ifdef __OS2__
static void runInputWorker(void* param)
{	(*reinterpret_cast<InputWorker*>(param))();
}
#else
// pthreads wont a return value
static void* runInputWorker(void* param)
{	(*reinterpret_cast<InputWorker*>(param))();
	return NULL;
}
#endif

static long parseint(const char* src)
{	long ret;
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
{	double ret;
	size_t l = (size_t)-1;
	if (sscanf(src, "=%lf%n", &ret, &l) == 0 || l != strlen(src))
		throw syntax_error(stringf("'=' followed by an floating-point value expected. Found '%s'", src));
	return ret;
}

static void parseoption(char* cp)
{	switch (tolower(cp[1]))
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
		{	cp[1+strlen(cp+2)] = 0;
			dHighWaterMark = parsedouble(cp+2) / 100;
			if (dHighWaterMark < 0 || dHighWaterMark > 100)
				throw syntax_error(stringf("The relative buffer level %s%% is not in the range 0-100%.", cp+3));
		} else
		{	iHighWaterMark = parseint(cp+2);
			dHighWaterMark = -1;
		}
		return;
	 case 'l':
		if (cp[1+strlen(cp+2)] == '%')
		{	cp[1+strlen(cp+2)] = 0;
			dLowWaterMark = parsedouble(cp+2) / 100;
			if (dLowWaterMark < 0 || dLowWaterMark > 100)
				throw syntax_error(stringf("The relative buffer level %s%% is not in the range 0-100%.", cp+3));
		} else
		{	iLowWaterMark = parseint(cp+2);
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
		 case 0:
			EnableInputStats = true;
			EnableOutputStats = true;
			return;
		}
		break;
	#ifdef __OS2__
	 case 'a':
		switch (tolower(cp[2]))
		{case 'i':
			AdvantageInput = true;
			return;
		 case 'o':
			AdvantageOutput = true;
			return;
		}
		break;
	#endif
	}
	throw syntax_error(stringf("Invalid option %s.", cp));
}

#if defined(__OS2__) || defined (_WIN32)
static void slash2backslash(char* cp)
{	while (*cp)
	{	if (*cp == '/')
			*cp = '\\';
		++cp;
	}
}
#else
#define slash2backslash(x)
#endif

int main(int argc, char** argv)
{	const char* input = NULL;
	const char* output = NULL;

	try
	{	char** ap = argv;
		while (*++ap) // skip arg[0]
		{	if ((*ap)[0] == '-' && (*ap)[1] != 0)
				// option
				parseoption(*ap);
			 else if (input == NULL)
			{	slash2backslash(*ap);
				input = *ap;
			} else if (output == NULL)
			{	slash2backslash(*ap);
				output = *ap;
			} else
				throw syntax_error(stringf("More than two I/O objects in the command line (at %s).", *ap));
		}

		// check if we have source & destination
		if (output == NULL)
		{	cerr << "Buffer2 Version 0.12\n\n"
				"usage " << argv[0] << " <input> <output> [options]\n\n"
				"<input>: Input stream. This is one of\n"
				"         Filename - an ordinary file which is read until EOF,\n"
				#ifdef __OS2__
				"         Pipe - a named pipe e.g. \\PIPE\\MyPipe,\n"
				#endif
				"         Device - any character device like \"COM1:\" or \"/dev/st0\",\n"
				"         Socket - a TCP/IP port tcpip://[hostname]:port or\n"
				"         \"-\" - stdin\n"
				"<output>: Output stream. This is one of\n"
				"          Filename - an ordinary file which is APPENDED,\n"
				#ifdef __OS2__
				"          Pipe - a named pipe e.g. \\PIPE\\MyPipe,\n"
				#endif
				"          Device - any character device like \"LPT1:\" or \"/dev/st0\",\n"
				"          Socket - a TCP/IP port tcpip://[hostname]:port or\n"
				"          \"-\" - stdout.\n\n"
				#ifdef __OS2__
				"Remarks: If the pipe does not exist so far it is created.\n"
				#endif
				"The Hostname may be an IP address or a DNS name. If hostname is omitted a local\n"
				"socket is created in listening mode accepting exactly one connection.\n\n"
				"options:\n"
				" -b=<size>  Internal fifo buffer size. 64kiB by default. If the number is\n"
				"            followed directly by the letter `k', `m' or `g' the size is\n"
				"            multiplied by 1024 to the power of 1, 2 or 3.\n"
				" -r=<size>  I/O-request size. As much as possible by default. The request size\n"
				"            should neither exceed the fifo size nor the pipe buffer size.\n"
				"            Larger values have no effect.\n"
				" -p=<size>  Pipe buffer size, only if a pipe is created. If the number is\n"
				"            followed directly by the letter `k' the size is multiplied by 1024.\n"
				#ifdef __OS2__
				"            Note that OS/2 does not accept pipe buffers beyond 64kiB.\n"
				#endif
				" -h=<level> High water mark. If the output thread is stopped because of an\n"
				"            empty buffer it will not resume until the buffer is filled up to\n"
				"            the high water mark. The level must be less than or equal to the\n"
				"            buffer size. If level ends with % the size is relative to the\n"
				"            buffer size in percent. The default value of 0 causes the output\n"
				"            thread never to stop unless the buffer is empty.\n"
				" -l=<level> Low water mark. If the input thread is stopped because the buffer\n"
				"            is full it will not resume until the buffer is emptied at least to\n"
				"            the low water mark. The level must be be less than or equal to the\n"
				"            buffer size. If level ends with % the size is relative to the\n"
				"            buffer size in percent. The default value of 100% causes the input\n"
				"            thread never to stop unless the buffer is completly full.\n"
				" -c         Enable file system cache.\n"
				#ifdef __OS2__
				" -ai        Prefer input. This raises the priority of the input thread.\n"
				" -ao        Prefer output. This raises the priority of the output thread.\n"
				#endif
				" -si        Print input statistics to stderr.\n"
				" -so        Print output statistics to stderr.\n";
			return 48;
		}

		// some calculations
		if (dHighWaterMark < 0)
		{	if (iHighWaterMark > BufferSize)
				throw syntax_error("The high water mark is larger than the buffer size.");
			dHighWaterMark = (double)iHighWaterMark / BufferSize;
		}
		if (dLowWaterMark < 0)
		{	if (iLowWaterMark > BufferSize)
				throw syntax_error("The low water mark is larger than the buffer size.");
			dLowWaterMark = (double)iLowWaterMark / BufferSize;
		}
		if (RequestSize < 0)
		{	// Auto Calculate
			RequestSize = BufferSize >= 1024*256 ? BufferSize / 8 : BufferSize / 4;
		}
		
		// initialize buffer and Workers
		StaticFIFO fifo(BufferSize, dHighWaterMark, dLowWaterMark, BufferAlignment);
		FIFOstat = &fifo.getStatistics();
		InputWorker iwrk(fifo.getDrain(), IInput::Factory(input));
		OutputWorker owrk(fifo.getSource(), IOutput::Factory(output));

		// start reader thread
		#ifdef __OS2__
		int iwtid = _beginthread(runInputWorker, NULL, 65536, &iwrk);
		if (iwtid == -1)
			throw runtime_error("Failed to start input worker thread.");
		if (AdvantageInput)
			DosSetPriority(PRTYS_THREAD, PRTYC_NOCHANGE, 1, iwtid);
		if (AdvantageOutput)
			DosSetPriority(PRTYS_THREAD, PRTYC_NOCHANGE, 1, 0);
		#else
		pthread_t iwth;
		int rc = pthread_create(&iwth, NULL, runInputWorker, &iwrk);
		if (rc != 0)
			throw os_error(rc, "Failed to start input worker thread.");
		#endif

		// execute output worker in main thread
		owrk();
		
		#ifdef __OS2__
		// join the input worker thread
		TID tid = iwtid;
		DosWaitThread(&tid, DCWW_WAIT);
		#else
		pthread_join(iwth, NULL);
		#endif

		if (EnableInputStats | EnableOutputStats)
			lerr << endl;

		return iwrk.getResult() != 0 ? iwrk.getResult() : owrk.getResult();

	} catch (const syntax_error& e)
	{	lerr << e.what();
		return 49;
	} catch (const runtime_error& e)
	{	lerr << e.what();
		return 29;
	} catch (const logic_error& e)
	{	lerr << e.what();
		return 19;
	} catch (...)
	{	lerr << "Unhandled exception." << endl;
		return 28;
	}
}

