#ifndef	_memory_h_
#define	_memory_h_

// Put all your #define's in memory_constants.h
#include "memory_constants.h"

extern int lastosaddress; // Defined in an assembly file

//--------------------------------------------------------
// Existing function prototypes:
//--------------------------------------------------------

int MemoryGetSize();
void MemoryModuleInit();
uint32 MemoryTranslateUserToSystem (PCB *pcb, uint32 addr);
int MemoryMoveBetweenSpaces (PCB *pcb, unsigned char *system, unsigned char *user, int n, int dir);
int MemoryCopySystemToUser (PCB *pcb, unsigned char *from, unsigned char *to, int n);
int MemoryCopyUserToSystem (PCB *pcb, unsigned char *from, unsigned char *to, int n);
int MemoryPageFaultHandler(PCB *pcb);

//---------------------------------------------------------
// Put your function prototypes here
//---------------------------------------------------------
// All function prototypes including the malloc and mfree functions go here
int MemoryAllocPageEasy(PCB *pcb);
int MemoryAllocPage(void);
void MemorySetFreemap(uint32 page);
void MemoryFreePage(uint32 page);
uint32 MemorySetupPte (uint32 page);
void MemoryFreePte (uint32 pte);

int findFreeMallocNode(PCB* pcb, int index, int order, int order_want);
uint32 addressFromOrderIndex(int order, int index);
uint32 sizeFromOrder(int order);
int partitionNode(PCB* pcb, int order, int index);
uint32 mfree_recurse(PCB* pcb, int order, int index, int cleared);
void* malloc(PCB* pcb, int memsize);
uint32 mfree(PCB* pcb, void* mem);

#endif	// _memory_h_
