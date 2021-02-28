// Jonathan Bourim, Project 2 - Part 2
#include "MemoryDebugger.h"
#include <new>
#include <type_traits>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <errno.h>
#include <algorithm>
#include <execinfo.h>

// Symbol info for reading line/function data of where error was found
struct SymbolInfo
{
    Basic_String fileName = "";
    Basic_String functionName = "";
    int lineNumber = 0;
};

constexpr size_t SIZE_PAGE = 4096;
constexpr size_t BUFFER_SIZE = 2048;

// Allocator initialization assistance
static int nurgleCounter;
static std::aligned_storage<sizeof(Nurgle), alignof(Nurgle)>::type nurgleBuf;
Nurgle& nurgle = reinterpret_cast<Nurgle&>(nurgleBuf);


// Logging constants
constexpr auto DEBUG_LOG_FILE = "DebugLog.csv";
constexpr auto DEBUG_LOG_HEADER = "Message, File, Line, Bytes, Address, Additional Info";


Nurgle::Nurgle()
{
}


void GetSymbolsFromAddress(void* address, SymbolInfo& symRef)
{
    // Get name of the file trace
    char** strings = backtrace_symbols(&address, 1);
    if (strings == nullptr) return;
    Basic_String traceInfo(strings[0]);
    free(strings);

    // Find the offset bytes that is inside traceInfo
    char offset[BUFFER_SIZE] = {0};

    // Wrap this in an ifdef because backtrace_symbols returns differently for GCC/Clang
#ifdef __clang__
    char delimiterStart = '[';
    char delimiterEnd = ']';
#else
    char delimiterStart = '(';
    char delimiterEnd = ')';
#endif
    size_t offsetStart = traceInfo.find(delimiterStart);
    size_t offsetEnd = traceInfo.find(delimiterEnd, offsetStart);
    memcpy(offset, &traceInfo[offsetStart + 1], offsetEnd - offsetStart - 1);

    int p = 0;
    while(traceInfo[p] != '(' && traceInfo[p] != ' ' && traceInfo[p] != 0) {
        ++p;
    }
    char syscom[BUFFER_SIZE * 2];

    // Get all of the associated data from the addr2line
    sprintf(syscom,"addr2line -f -a -C -e %.*s %s", p, traceInfo.c_str(), offset);

    FILE* fp = popen(syscom, "r");
    if (!fp) {
        return;
    }

    Basic_String info;
    char path[BUFFER_SIZE] = {0};

    /* Read the output a line at a time. */
    while (fgets(path, BUFFER_SIZE, fp) != NULL) {
        info += Basic_String(path);
        memset(path, 0, BUFFER_SIZE * sizeof(char));
    }

    // Prepare for reading in and delimiting by newlines
    char infoBuf[BUFFER_SIZE];
    sprintf(infoBuf, "%s", info.c_str());

    // First token is function address
    char* token = strtok(infoBuf, "\n");

    // Second is function name
    token = strtok(NULL, "\n");
    symRef.functionName = token;
    // Third is file path in its entirety
    token = strtok(NULL, "\n");

    // Find second to last slash for one directory up, as in the MSVC version
    char* lastSlash = strrchr(token, '/');
    char temp = lastSlash[0];
    lastSlash[0] = '\0';
    char* secondToLastSlash = strrchr(token, '/');
    lastSlash[0] = temp;

    char* end = token +  strlen(token);
    char* colon = std::find(token, end, ':');
    // Split by the colon
    colon[0] = '\0';
    // Last slash to colon is file name
    symRef.fileName = secondToLastSlash + 1;
    char* lineNum = colon + 1;
    // past that its line number
    symRef.lineNumber = atoi(lineNum);

    /* close */
    pclose(fp);
}

// Log all of the leaks found on destruction
void Nurgle::LogLeaks()
{
	FILE* fp = fopen(DEBUG_LOG_FILE, "w+");
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
		fprintf(fp, "%zu, ", alloc.second.size);
		// Allocation address
		fprintf(fp, "%p, ", alloc.second.address);
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
		munmap(deallocation.pageBase, deallocation.pageBaseSize);
		munmap(deallocation.pageOverflow, SIZE_PAGE);
	}
	mDeallocated.clear();
}


void* Nurgle::Allocate(size_t size, Allocation::TYPE allocationType = Allocation::TYPE::SCALAR, bool throwException = false)
{
    // Match signed version of size_t
	if (size >= std::numeric_limits<size_t>::max() / 2)
	{
		// Size greater than acceptable allocation limit
		if (throwException)
			throw std::bad_alloc();

		return nullptr;
	}

	// Fit the number of pages to the requested size, ceiling of the fractional value
	const size_t numPages = (size / SIZE_PAGE) + 1llu;

	void* basePtr = mmap(0, numPages * (SIZE_PAGE + 1),
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (basePtr == (void*)(-1))
    {
	    printf("mmap error: %s \n", strerror(errno));
	    fflush(stdout);
    }
	char* byteAddr = static_cast<char*>(basePtr);
	// Allocate single extra page for overflow
	void* overflowPtr = static_cast<void*>(byteAddr + (numPages * SIZE_PAGE));
    // Protect the last page to be non read-write
    mprotect(overflowPtr, SIZE_PAGE, PROT_NONE);
	void* clientPtr = byteAddr + (numPages * SIZE_PAGE) - size;

	Allocation alloc;
	alloc.address = clientPtr;
	alloc.size = size;
	alloc.pageBase = basePtr;
	alloc.pageBaseSize = numPages * SIZE_PAGE;
	alloc.pageOverflow = overflowPtr;
	alloc.allocationType = allocationType;

	// Backtrack up by 1 and get the return address of that as the caller
	alloc.callingFunctionAddress = __builtin_return_address(1);

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
	munmap(allocation.pageBase, allocation.pageBaseSize);
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
