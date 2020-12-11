#ifndef	_memory_constants_h_
#define	_memory_constants_h_

//------------------------------------------------
// #define's that you are given:
//------------------------------------------------

// We can read this address in I/O space to figure out how much memory
// is available on the system.
#define	DLX_MEMSIZE_ADDRESS	0xffff0000

// Return values for success and failure of functions
#define MEM_SUCCESS 1
#define MEM_FAIL -1

//--------------------------------------------------------
// Put your constant definitions related to memory here.
// Be sure to prepend any constant names with "MEM_" so
// that the grader knows they are defined in this file.
#define MEM_L1FIELD_FIRST_BITNUM	12
#define MEM_MAX_VIRTUAL_ADDRESS		0xffffc
#define MEM_MAX_SIZE			2097152
#define MEM_PTE_READONLY		0x4
#define MEM_PTE_DIRTY			0x2
#define MEM_PTE_VALID			0x1

#define MEM_PAGE_SIZE			(0x1 << MEM_L1FIELD_FIRST_BITNUM)
#define MEM_PAGESIZE			MEM_PAGE_SIZE
#define MEM_FREEMAP_SIZE		(MEM_MAX_SIZE / MEM_PAGE_SIZE / 32)
#define MEM_L1TABLE_SIZE		((MEM_MAX_VIRTUAL_ADDRESS + 4) >> MEM_L1FIELD_FIRST_BITNUM)
#define MEM_ADDRESS_OFFSET_MASK		(MEM_PAGE_SIZE - 1)
#define MEM_PTE_MASK			invert(MEM_PTE_READONLY | MEM_PTE_DIRTY | MEM_PTE_VALID)
#define MEM_MAX_32_PAGE			((0xffffffff >> MEM_L1FIELD_FIRST_BITNUM) + 1)

#define MEM_REF_COUNTER_SIZE            (MEM_MAX_SIZE >> MEM_L1FIELD_FIRST_BITNUM) 
//--------------------------------------------------------


#endif	// _memory_constants_h_
