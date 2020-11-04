// DO NOT PLACE REQUIRED MODIFICATIONS IN THIS FILE, IT WILL BE REPLACED WHEN GRADING
/***********************************************************************************/
// Scenarios

#include <iostream>
#include <cstring>
#include <new>

#include "Common.h"
#include "TestScenarios.h"

#if defined (__clang__)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-new-delete"

#endif

#if INTPTR_MAX == INT64_MAX
//~9 Exabytes (~9223372 TB)
#define UNSERVICEABLE_VALUE 0x7FFFFFFFFFFFFFFF
#else
//4GB
#define UNSERVICEABLE_VALUE 0xF0000000
#endif

#if DEBUG
#define FAILED_TEST() \
	std::cout << "Failed Test: " << GET_FUNCTION_INFO() << " in " << GET_FILE_LINE_INFO() << std::endl; \
	DEBUG_BREAKPOINT();
#else
#define FAILED_TEST() \
	std::cout << "Test End: " << GET_FUNCTION_INFO() << " in " << GET_FILE_LINE_INFO() << std::endl;
#endif


/*****************************************************************************/
/* Memory Leaks */
double* project2_leaks_helper()
{
	return new double;
}

void project2_leaks()
{
	int* ints[10] = { nullptr };
	// 10 allocations - 9 deletes
	for (int i = 0; i < 10; i++)
	{
		ints[i] = new int;
	}
	for (int i = 0; i < 9; i++)
	{
		delete ints[i];
		ints[i] = nullptr;
	}

	// 10 vector allocations - 9 vector deletes
	for (int i = 0; i < 10; i++)
	{
		ints[i] = new int[i + 1];
	}

	for (int i = 0; i < 9; i++)
	{
		delete[] ints[i];
		ints[i] = nullptr;
	}

	// 1 allocation through helper function
	double* d = project2_leaks_helper();
	UNUSED(d);

	// All total - 3 leaks
}

/*****************************************************************************/
/* Overflows */

void project2_writeoverflow()
{
	int* ints = new int[10];
	// Valid Access
	for (int i = 0; i < 10; i++)
	{
		ints[i] = i;
	}

	// Write overflow
	for (int i = 1; i <= 10; i++)
	{
		ints[i] = ints[i - 1];
	}

	FAILED_TEST();

	delete[] ints;
}

void project2_readoverflow()
{
	int* ints = new int[10];
	// Valid Access
	for (int i = 0; i < 10; i++)
	{
		ints[i] = i;
	}

	// Read overflow
	for (int i = 0; i <= 10; i++)
	{
		std::cout << "ints " << i << " is " << ints[i] << std::endl;
	}

	FAILED_TEST();

	delete[] ints;
}

/*****************************************************************************/
/* Deleted Memory Access */

void project2_deletedmemorywrite()
{
	int* i = new int;
	*i = 42;
	delete i;

	// Write Access to Deleted Pointer
	*i = 43;

	FAILED_TEST();
}

void project2_deletedmemoryread()
{
	int* i = new int;
	*i = 42;
	delete i;

	// Read Access to Deleted Pointer
	std::cout << "Deleted i is: " << *i << std::endl;

	FAILED_TEST();
}

/*****************************************************************************/
/* Double Delete */

void project2_doubledelete()
{
	int* i = new int;
	delete i;

	// Double Delete
	delete i;

	FAILED_TEST();
}

void project2_doublevectordelete()
{
	int* i = new int[2];
	delete[] i;

	// Double Delete
	delete[] i;

	FAILED_TEST();
}

/*****************************************************************************/
/* Mismatched new/delete[] and new[]/delete */

void project2_vectornew_scalardelete()
{
	int* ints = new int[2];

	// Mismatched new[]/delete
	delete ints;

	FAILED_TEST();
}

void project2_scalarnew_vectordelete()
{
	int* ints = new int;

	// Mismatched new/delete[]
	delete[] ints;

	FAILED_TEST();
}

/*****************************************************************************/
/* Random Pointer Delete */

void project2_randompointer1()
{
	int local_int = 0;
	int* i = &local_int;

	// Cannot delete a stack variable
	delete i;

	FAILED_TEST();
}

void project2_randompointer2()
{
	int* i = new int;
	if (i) std::cout << *i << std::endl;

	// Move the pointer backward by 1 byte - no longer i. Forward would trigger an AccessViolation.
	int* bad_i = (int*)((char*)i - 1);

	if (bad_i) std::cout << *bad_i << std::endl;

	// Cannot delete a pointer that was not returned from new
	delete bad_i;

	FAILED_TEST();
}

/*****************************************************************************/
/* GOOD Usage - There is nothing wrong with this test */

void project2_good()
{
	// Simple Allocation Test
	int* i = new int;
	*i = 42;
	delete i;
	i = nullptr;

	// Simple Vector Allocation Test
	i = new int[2];
	i[0] = 0;
	i[1] = i[0] + 1;
	delete[] i;
	i = nullptr;

	// delete NULL test
	i = nullptr;
	delete i;

	// Large array test - ~16K
	i = new int[8000];
	std::memset(i, 0x00, 8000 * sizeof(int));
	std::memset(i, 0xFF, 8000 * sizeof(int));
	delete[] i;
	i = nullptr;

	// Very large array test - ~4MB
	i = new int[1024 * 1024];
	std::memset(i, 0x00, 1024 * 1024 * sizeof(int));
	std::memset(i, 0xFF, 1024 * 1024 * sizeof(int));
	delete[] i;
	i = nullptr;

	// Zero byte allocation must be non-NULL and Unique
	i = new int[0];
	int* i2 = new int[0];
	// If this breakpoint triggers, you will fail Scenario 0
	if (i == i2)
	{
		FAILED_TEST();
	}
	delete[] i;
	delete[] i2;

	// bad_alloc test
	bool bad_alloc_caught = false;
	try
	{
		// a 4GB (x86) or 1TB (x64) request is simply not serviceable
		size_t s = static_cast<size_t>(UNSERVICEABLE_VALUE);
		char* c = new char[s];
		UNUSED(s);
		UNUSED(c);
	}
	catch (std::bad_alloc&)
	{
		bad_alloc_caught = true;
	}
	// If this breakpoint triggers, you will fail Scenario 0
	if (!bad_alloc_caught)
	{
		FAILED_TEST();
	}

	// nothrow test
	{
		bad_alloc_caught = false;
		try
		{
			// a 4GB (x86) or 1TB (x64) request is simply not serviceable
			size_t s = static_cast<size_t>(UNSERVICEABLE_VALUE);
			char* c = new (std::nothrow) char[s];
			if (c != nullptr)
			{
				FAILED_TEST();
			}
		}
		catch (std::bad_alloc&)
		{
			bad_alloc_caught = true;
		}
		// If this breakpoint triggers, you will fail Scenario 0
		if (bad_alloc_caught)
		{
			FAILED_TEST();
		}
	}
}

#if defined (__clang__)

#pragma clang diagnostic pop

#endif

/***********************************************************************************/
// DO NOT PLACE REQUIRED MODIFICATIONS IN THIS FILE, IT WILL BE REPLACED WHEN GRADING