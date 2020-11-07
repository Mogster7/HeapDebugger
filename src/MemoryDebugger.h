// Jonathan Bourim, Project 2
#pragma once

#include "Common.h"
#include <vector>
#include <basetsd.h>
#include <numeric>
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
		DWORD64 callingFunctionAddress = 0;
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
void *operator new(size_t size, const std::nothrow_t &) noexcept;

void *operator new[](size_t size);
void *operator new[](size_t size, const std::nothrow_t &) noexcept;

void operator delete(void* ptr) noexcept;
void operator delete(void *ptr, size_t) noexcept;
void operator delete(void *ptr, const std::nothrow_t &) noexcept;

void operator delete[](void *ptr) noexcept;
void operator delete[](void *ptr, size_t) noexcept;
void operator delete[](void *ptr, const std::nothrow_t &) noexcept;
