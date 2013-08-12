#ifndef __IOinterface_h
#define __IOinterface_h

#include <stdlib.h>


// input interface class
class IInput
{protected:
	const char* Src;
	IInput(const char* src) : Src(src) {}
 public:
	virtual ~IInput() {};
	static IInput* Factory(const char* src);
	virtual void Initialize() = 0;
	virtual size_t ReadData(void* dst, size_t len) = 0;
};

// output interface class
class IOutput
{protected:
	const char* Dst;
	IOutput(const char* dst) : Dst(dst) {}
 public:
	virtual ~IOutput() {};
	static IOutput* Factory(const char* dst);
	virtual void Initialize() = 0;
	virtual size_t WriteData(const void* dst, size_t len) = 0;
};

#endif
