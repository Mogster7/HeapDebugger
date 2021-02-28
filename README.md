## Compilers  
    - [x] gcc
    - [x] clang
    - [x] msvc/Visual Studio

## Integration  
Integration in this project should be a matter of including MemoryDebugger.h/cpp in the program, and then compiling your project. 
Variants of operator new and delete are overloaded, thereby allowing the memory debugger
to take control of most instantiations by the client. The memory debugger will track client allocations and report memory leaks, as well as detect memory overflows by flagging neighboring memory as restricted.

------


## Requirements  
MSVC:

dbghelp.lib will need to be linked to the project in addition to compiling the MemoryDebugger.h/cpp as part of your program.

Clang / GCC:

No additional libraries need to be linked to function.

## Output  
A file called "DebugLog.csv" contains details about the output when a memory leak occurs, including the following headers: Message, File, Line, Bytes, Address, Additional Info .
The output includes data to match the appropriate headers above, and additional info includes the function name that made the call resulting in the output. I.E. a memory leak would report the function that called operator new.

## Additional Information
New is replaced by overloading operator new / delete, so allocations can be performs as per usual by the client.
The no-man's land is placed directly after the end of the stored data. 
One additional pages is allocated than necessary, and the data is placed directly to align with the border of the page, such that when an overflow as little as 1 byte occurs, an error occurs.
The allocations are tracked in a hash map with keys of void pointers of the client's address, and values of the Allocation struct. The Allocation struct contains a bunch of data relevant to the storage of the allocation, including client address, base address, extra overflow page address, the size, etc.
When deallocated, the allocations are placed inside of a vector, as they have no need for lookup any longer.
