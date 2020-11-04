#pragma once

#include "mallocator.h"

typedef std::basic_string<char, std::char_traits<char>, Mallocator<char>> Basic_String;

#define _STR(X) #X
#define STR(X) _STR(X)

#define UNUSED(expr) do { (void)(expr); } while (0)

#define GET_FILE_LINE_INFO() __FILE__ "(" STR(__LINE__) ")"


// if we are compiling with MSVC/Windows
#if defined (_MSC_VER)

#include <intrin.h>  // for _ReturnAddress()
#pragma intrinsic(_ReturnAddress)

#define GET_RETURN_ADDR() _ReturnAddress()
#define GET_FILE_INFO() __FILE__
#define GET_FUNCTION_INFO() __FUNCTION__
#define GET_LINE_INFO() __LINE__

#if DEBUG
#define DEBUG_BREAKPOINT() __debugbreak()
#else
#define DEBUG_BREAKPOINT() 
#endif

// else - we are NOT compiling with MSVC/Windows
#else

#include <csignal>    // for std::raise(SIGTRAP)

//pass the depth you want to return
#define GET_RETURN_ADDR() __builtin_return_address(0)
#define GET_FILE_INFO() __builtin_FILE()
#define GET_FUNCTION_INFO() __builtin_FUNCTION()
#define GET_LINE_INFO() __builtin_LINE()

#if DEBUG
#define DEBUG_BREAKPOINT() std::raise(SIGTRAP) // __builtin_trap() 
#else
#define DEBUG_BREAKPOINT() 
#endif

#endif
