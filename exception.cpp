/*****************************************************************************
*
*  exception.cpp - exception classes
*
*****************************************************************************/


#include "MMUtil+.h"
#include <stdexcept>
using namespace std;

namespace MM {

os_error::os_error(long rc, const std::string& msg)
 : runtime_error(msg+stringf("\nError code = %li",rc)), RC(rc)
{}

} // end namespace
