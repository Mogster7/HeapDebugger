// Jonathan Bourim, Project 2
#include "MemoryDebugger.h"
#include <new>
#include <type_traits>

#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <dbghelp.h>

#pragma region ContextGetter
#pragma comment( lib, "dbghelp" )

#if defined(_M_AMD64)
// x64
#define GET_CONTEXT(c) \
	do { \
		RtlCaptureContext(&(c)); \
	} while(0)

void FillStackFrame(STACKFRAME64& stack_frame, const CONTEXT& context) {
	stack_frame.AddrPC.Mode = AddrModeFlat;
	stack_frame.AddrPC.Offset = context.Rip;
	stack_frame.AddrStack.Mode = AddrModeFlat;
	stack_frame.AddrStack.Offset = context.Rsp;
	stack_frame.AddrFrame.Mode = AddrModeFlat;
	stack_frame.AddrFrame.Offset = context.Rbp;
}

#define IMAGE_FILE_MACHINE_CURRENT IMAGE_FILE_MACHINE_AMD64
// x64
#elif defined(_M_IX86)
// This only works for x86
// For x64, use RtlCaptureContext()
// See: http://jpassing.wordpress.com/2008/03/12/walking-the-stack-of-the-current-thread/
__declspec(naked) DWORD _stdcall GetEIP() {
	_asm {
		mov eax, dword ptr[esp]
		ret
	};
}

__declspec(naked) DWORD _stdcall GetESP() {
	_asm {
		mov eax, esp
		ret
	};
}

__declspec(naked) DWORD _stdcall GetEBP() {
	_asm {
		mov eax, ebp
		ret
	};
}

// Capture the context at the current location for the current thread
// This is a macro because we want the CURRENT function - not a sub-function
#define GET_CONTEXT(c) \
	do { \
		ZeroMemory(&c, sizeof(c)); \
		c.ContextFlags = CONTEXT_CONTROL; \
		c.Eip = GetEIP(); \
		c.Esp = GetESP(); \
		c.Ebp = GetEBP(); \
	} while(0)

void FillStackFrame(STACKFRAME64& stack_frame, const CONTEXT& context) {
	stack_frame.AddrPC.Mode = AddrModeFlat;
	stack_frame.AddrPC.Offset = context.Eip;
	stack_frame.AddrStack.Mode = AddrModeFlat;
	stack_frame.AddrStack.Offset = context.Esp;
	stack_frame.AddrFrame.Mode = AddrModeFlat;
	stack_frame.AddrFrame.Offset = context.Ebp;
}

#define IMAGE_FILE_MACHINE_CURRENT IMAGE_FILE_MACHINE_I386
// x86
#else
#error Unsupported Architecture
#endif

#pragma endregion ContextGetter


// Allocator initialization assistance
static int nurgleCounter;
static std::aligned_storage<sizeof(Nurgle), alignof(Nurgle)>::type nurgleBuf;
Nurgle& nurgle = reinterpret_cast<Nurgle&>(nurgleBuf);

// Logging constants
constexpr auto DEBUG_LOG_FILE = "DebugLog.csv";
constexpr auto DEBUG_LOG_HEADER = "Message, File, Line, Bytes, Address, Additional Info";

Nurgle::Nurgle()
{
	// Initialize symbol read options
	SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES);
	SymInitialize(GetCurrentProcess(), nullptr, true);
}

// Symbol info for reading line/function data of where error was found
struct SymbolInfo
{
	Basic_String fileName;
	Basic_String functionName;
	int lineNumber = 0;
};

void GetSymbolsFromAddress(DWORD64 address, SymbolInfo& symRef)
{
	// Get line from calling address
	DWORD displacement;
	IMAGEHLP_LINE line = { 0 };
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE);
	BOOL result = SymGetLineFromAddr(GetCurrentProcess(), address, &displacement, &line);

	if (result)
	{
		// Update symbol
		symRef.lineNumber = line.LineNumber;
		symRef.fileName = line.FileName;
	}
	
	// Find the symbol name
	// This weird structure allows the SYMBOL_INFO to store a string of a user
	// defined maximum length without requiring any allocations or additional pointers
	struct {
		SYMBOL_INFO symbol_info;
		char buffer[MAX_PATH];
	} symbol = { 0 };


	// Maximum size of the path length of the file
	constexpr unsigned MAX_PATH_LENGTH = 260;
	
	symbol.symbol_info.SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol.symbol_info.MaxNameLen = MAX_PATH_LENGTH;
	DWORD64 symbol_offset = 0;
	result = SymFromAddr(GetCurrentProcess(), address, &symbol_offset, &symbol.symbol_info);

	if (result)
	{
		symRef.functionName = symbol.symbol_info.Name;
	}
}

// Log all of the leaks found on destruction
void Nurgle::LogLeaks()
{
	FILE* fp = nullptr;
	fopen_s(&fp, DEBUG_LOG_FILE, "w+");
	if (fp == nullptr) return;

	fprintf(fp, DEBUG_LOG_HEADER);
	fprintf(fp, "\n");
	SymbolInfo symbolInfo;

	for (const auto& alloc : mAllocated)
	{
		GetSymbolsFromAddress(alloc.second.callingFunctionAddress, symbolInfo);
		Basic_String fileName = symbolInfo.fileName;
		Basic_String strippedFileName = fileName.substr(fileName.substr(0, fileName.find_last_of("\\")).find_last_of("\\") + 1);
		
		// Type of error
		fprintf(fp, "%s, ", "Memory leak found");
		// File name
		fprintf(fp, "%s, ", strippedFileName.c_str());
		// Line number
		fprintf(fp, "%d, ", symbolInfo.lineNumber);
		// Allocation size
		fprintf(fp, "%llu, ", alloc.second.size);
		// Allocation address
		fprintf(fp, "0x%p, ", alloc.second.address);
		// Function name
		fprintf(fp, "Function Name: %s", symbolInfo.functionName.c_str());
		fprintf(fp, "\n");
	}
}

Nurgle::~Nurgle()
{
	if (mAllocated.empty()) return;

	LogLeaks();

	// Place all allocated data into deallocated set for freeing
	for(auto& allocation : mAllocated)
		mDeallocated.emplace_back(allocation.second);
	mAllocated.clear();

	// Free all data
	for (const auto& deallocation : mDeallocated)
	{
		VirtualFree(deallocation.pageBase, 0, MEM_RELEASE);
		VirtualFree(deallocation.pageOverflow, 0, MEM_RELEASE);
	}
	mDeallocated.clear();
}


size_t GetCallingFunctionAddress(unsigned backtraceDepth)
{
	// Get context and fill stack frame
	CONTEXT context;
	GET_CONTEXT(context);

	STACKFRAME64 stack_frame = { 0 };
	FillStackFrame(stack_frame, context);

	// Find calling function by walking up the stack by depth amount
	for (unsigned i = 0; i < backtraceDepth; ++i)
	{
		StackWalk64(
			IMAGE_FILE_MACHINE_CURRENT, // IMAGE_FILE_MACHINE_I386 or IMAGE_FILE_MACHINE_AMD64
			GetCurrentProcess(), // Process
			GetCurrentThread(), // Thread
			&stack_frame, // Stack Frame Information
			(PVOID)&context, // Thread Context Information
			NULL, // Read Memory Call Back (Not Used)
			SymFunctionTableAccess64, // Function Table Accessor
			SymGetModuleBase64, // Module Base Accessor
			NULL // Address Translator (Not Used)
		);
	}

	return static_cast<size_t>(stack_frame.AddrPC.Offset);
}


void* Nurgle::Allocate(size_t size, Allocation::TYPE allocationType = Allocation::TYPE::SCALAR, bool throwException = false)
{
	if (size >= static_cast<size_t>(std::numeric_limits<long long>::max()))
	{
		// Size greater than acceptable allocation limit
		if (throwException)
			throw std::bad_alloc();

		return nullptr;
	}

	constexpr size_t SIZE_PAGE = 4096;
	// Fit the number of pages to the requested size, ceiling of the fractional value
	const size_t numPages = (size / SIZE_PAGE) + 1llu;

	void* basePtr = VirtualAlloc(nullptr, numPages * SIZE_PAGE, 
		MEM_COMMIT, PAGE_READWRITE);
	char* byteAddr = static_cast<char*>(basePtr);
	// Allocate single extra page for overflow
	void* overflowPtr = VirtualAlloc(static_cast<void*>(byteAddr + (numPages * SIZE_PAGE)),
		SIZE_PAGE, MEM_RESERVE, PAGE_NOACCESS);
	void* clientPtr = byteAddr + (numPages * SIZE_PAGE) - size;

	Allocation alloc;
	alloc.address = clientPtr;
	alloc.size = size;
	alloc.pageBase = basePtr;
	alloc.pageBaseSize = numPages * SIZE_PAGE;
	alloc.pageOverflow = overflowPtr;
	alloc.allocationType = allocationType;

	// number of hops up the call stack to where it was originally called
	constexpr auto BACKTRACE_DEPTH = 4u;
	alloc.callingFunctionAddress = GetCallingFunctionAddress(BACKTRACE_DEPTH);

	nurgle.mAllocated[clientPtr] = alloc;
	return clientPtr;
}

void Nurgle::Deallocate(void* address, Allocation::TYPE allocationType = Allocation::TYPE::SCALAR)
{
	if (address == nullptr) return;

	const auto allocIter = mAllocated.find(address);

	if (allocIter == mAllocated.end())
	{
		// DOUBLE OR INVALID DELETE OF ALLOCATED DATA
		DEBUG_BREAKPOINT();
		return;
	}
	if (allocationType != (*allocIter).second.allocationType)
	{
		// MISMATCHED ALLOCATION TYPE (SCALAR VS. VECTOR)
		DEBUG_BREAKPOINT();
		return;
	}

	Allocation allocation = (*allocIter).second;

	// Decommit the allocated pages (not the overflow page)
	VirtualFree(allocation.pageBase, 0, MEM_DECOMMIT);
	mDeallocated.emplace_back(allocation);
	mAllocated.erase(address);
}


NurgleInitializer::NurgleInitializer()
{
	if (nurgleCounter++ == 0) new (&nurgle) Nurgle();
}

NurgleInitializer::~NurgleInitializer()
{
	if (--nurgleCounter == 0) (&nurgle)->~Nurgle();
}

void *operator new(size_t size)
{
	return nurgle.Allocate(size, Nurgle::Allocation::TYPE::SCALAR, true);
}

void *operator new(size_t size, const std::nothrow_t &) noexcept
{
	return nurgle.Allocate(size);
}

void *operator new[](size_t size) 
{
	return nurgle.Allocate(size, Nurgle::Allocation::TYPE::VECTOR, true);
}
void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
	return nurgle.Allocate(size, Nurgle::Allocation::TYPE::VECTOR);
}

void operator delete(void *address) noexcept
{
	nurgle.Deallocate(address);
}
void operator delete(void *address, size_t) noexcept
{
	nurgle.Deallocate(address);
}

void operator delete(void *address, const std::nothrow_t &) noexcept
{
	nurgle.Deallocate(address);
}

void operator delete[](void *address) noexcept
{
	nurgle.Deallocate(address, Nurgle::Allocation::TYPE::VECTOR);
	
}
void operator delete[](void *address, size_t) noexcept
{
	nurgle.Deallocate(address, Nurgle::Allocation::TYPE::VECTOR);
}

void operator delete[](void *address, const std::nothrow_t &) noexcept
{
	nurgle.Deallocate(address, Nurgle::Allocation::TYPE::VECTOR);
}
