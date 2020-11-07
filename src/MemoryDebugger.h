// Jonathan Bourim, Project 2
#ifndef MEMORY_DEBUGGER
#define MEMORY_DEBUGGER

#include "Common.h"
#include <vector>
#include <unordered_map>

// i named my memory debugger after that stinky god in warhammer that makes you immortal
struct Nurgle
{
	Nurgle();
	~Nurgle();


	struct Allocation
	{
		enum class TYPE { SCALAR = 0, VECTOR };

		void* address = nullptr;
		size_t size = 0;
		size_t callingFunctionAddress = 0;
		void* pageBase = nullptr;
		size_t pageBaseSize = 0;
		void* pageOverflow = nullptr;
		TYPE allocationType = TYPE::SCALAR;
	};


	void* Allocate(size_t size, Allocation::TYPE allocationType, bool throwException);
	void Deallocate(void* address, Allocation::TYPE allocationType);

private:
	void LogLeaks();

	template <typename T1, typename T2>
	using AllocationMap = std::unordered_map<T1, T2, std::hash<T1>, std::equal_to<>, Mallocator<std::pair<const T1, T2>>>;

	AllocationMap<void*, Allocation> mAllocated;
	std::vector<Allocation, Mallocator<Allocation>> mDeallocated;
};
extern Nurgle& nurgle;

static struct NurgleInitializer
{
	NurgleInitializer();
	~NurgleInitializer();
} nurgleInitializer;


void* operator new(size_t size);
void* operator new(size_t size, const std::nothrow_t&) noexcept;

void* operator new[](size_t size);
void* operator new[](size_t size, const std::nothrow_t&) noexcept;

void operator delete(void* address) noexcept;
void operator delete(void* address, size_t) noexcept;
void operator delete(void* address, const std::nothrow_t&) noexcept;

void operator delete[](void* address) noexcept;
void operator delete[](void* address, size_t) noexcept;
void operator delete[](void* address, const std::nothrow_t&) noexcept;

#endif
