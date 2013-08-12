/*****************************************************************************
*
*  Utils.CPP - Utility functions
*
*  (C) 2002 comspe AG, Fulda, Germany
*      Marcel Müller
*
****************************************************************************/


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>

#include "MMUtil+.h"

namespace MM {

using namespace std;


/*****************************************************************************
*
*  overload global toupper with string extension
*
*****************************************************************************/
using ::toupper;

static inline void mytouppercore(char& c)
{  c = toupper(c);
}

string toupper(string s)
{  for_each(s.begin(), s.end(), &mytouppercore);
   return s;
}

/*****************************************************************************
*
*  string abbrevation test
*
*****************************************************************************/
bool starts_with(const std::string& s, const std::string& w)
{  return s.size() >= w.size()
    && memcmp(s.data(), w.data(), w.length() * sizeof(char)) == 0;
}

/*****************************************************************************
*
*  sprintf to a string object, buffer overflow safe
*
*****************************************************************************/
#if defined(_MSVC_VER)
string vstringf(const char* fmt, va_list ap)
{  size_t len = _vscprintf(fmt, ap);
   char* cp = new char[len +1];
   vsprintf(cp, fmt, ap);
   string ret(cp, len);
   delete[] cp;
   return ret;
}

#elif defined(__GNUC__)
string vstringf(const char* fmt, va_list va)
{  // BSD and compatible environments only:
   size_t len = vsnprintf(NULL, 0, fmt, va);
   char* cp = new char[len+1]; // political correct (do not write the internal string data structures directly)
   vsnprintf(cp, len+1, fmt, va);
   string s(cp, len);
   delete[] cp;
   return s;
}
#else
#error unsupported platform
#endif

string stringf(const char* fmt, ...)
{  va_list ap;
   va_start(ap, fmt);
   string ret = vstringf(fmt, ap);
   va_end(ap);
   return ret;
}

} // end namespace
