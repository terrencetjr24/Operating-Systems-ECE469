//
//	memory.c
//
//	Routines for dealing with memory management.

//static char rcsid[] = "$Id: memory.c,v 1.1 2000/09/20 01:50:19 elm Exp elm $";

#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "memory.h"
#include "memory_constants.h"
#include "queue.h"

// num_pages = size_of_memory / size_of_one_page
static uint32 freemap[MEM_FREEMAP_SIZE];
static uint32 pagestart;
static int nfreepages;
static int freemapmax;

//----------------------------------------------------------------------
//
//	This silliness is required because the compiler believes that
//	it can invert a number by subtracting it from zero and subtracting
//	an additional 1.  This works unless you try to negate 0x80000000,
//	which causes an overflow when subtracted from 0.  Simply
//	trying to do an XOR with 0xffffffff results in the same code
//	being emitted.
//
//----------------------------------------------------------------------
static int negativeone = 0xFFFFFFFF;
static inline uint32 invert (uint32 n) {
  return (n ^ negativeone);
}

//----------------------------------------------------------------------
//
//	MemoryGetSize
//
//	Return the total size of memory in the simulator.  This is
//	available by reading a special location.
//
//----------------------------------------------------------------------
int MemoryGetSize() {
  return (*((int *)DLX_MEMSIZE_ADDRESS));
}


//----------------------------------------------------------------------
//
//	MemoryModuleInit
//
//	Initialize the memory module of the operating system.
//      Basically just need to setup the freemap for pages, and mark
//      the ones in use by the operating system as "VALID", and mark
//      all the rest as not in use.
//
//      0 bit in freemap array means page is in use - "VALID" ?
//      1 bit means not in use
//
//----------------------------------------------------------------------
void MemoryModuleInit() {
  int i;
  pagestart = lastosaddress / MEM_PAGE_SIZE;
  freemapmax = MemoryGetSize() / MEM_PAGE_SIZE;
  nfreepages = freemapmax - pagestart;

  if (lastosaddress % MEM_PAGE_SIZE != 0) pagestart++;

  for (i = 0; i < MEM_FREEMAP_SIZE; i++) {
    freemap[i] = 0;
  }

  for (i = pagestart; i < freemapmax; i++) {
    MemorySetFreemap(i);
  }
  // printf("MemoryModuleInit: nfreepages %d\n", nfreepages);
}


//----------------------------------------------------------------------
//
// MemoryTranslateUserToSystem
//
//	Translate a user address (in the process referenced by pcb)
//	into an OS (physical) address.  Return the physical address.
//
//----------------------------------------------------------------------
uint32 MemoryTranslateUserToSystem (PCB *pcb, uint32 addr) {
  uint32 pagenum = addr >> MEM_L1FIELD_FIRST_BITNUM;
  uint32 offset = addr & MEM_ADDRESS_OFFSET_MASK;

  if (addr > MEM_MAX_VIRTUAL_ADDRESS) {
    printf("PID: %d addr is larger than possible virtual address\n", GetPidFromAddress(pcb));
    printf("  killing PID: %d\n", GetCurrentPid());
    ProcessKill();
  }

  if ((pcb->pagetable[pagenum] & MEM_PTE_VALID) != MEM_PTE_VALID) {
    pcb->currentSavedFrame[PROCESS_STACK_FAULT] = addr;
    MemoryPageFaultHandler(pcb);
  }

  //printf("MemoryTranslate: %x\n", (pcb->pagetable[pagenum] & MEM_PTE_MASK) | offset);
  return (pcb->pagetable[pagenum] & MEM_PTE_MASK) | offset;
}


//----------------------------------------------------------------------
//
//	MemoryMoveBetweenSpaces
//
//	Copy data between user and system spaces.  This is done page by
//	page by:
//	* Translating the user address into system space.
//	* Copying all of the data in that page
//	* Repeating until all of the data is copied.
//	A positive direction means the copy goes from system to user
//	space; negative direction means the copy goes from user to system
//	space.
//
//	This routine returns the number of bytes copied.  Note that this
//	may be less than the number requested if there were unmapped pages
//	in the user range.  If this happens, the copy stops at the
//	first unmapped address.
//
//----------------------------------------------------------------------
int MemoryMoveBetweenSpaces (PCB *pcb, unsigned char *system, unsigned char *user, int n, int dir) {
  unsigned char *curUser;         // Holds current physical address representing user-space virtual address
  int		bytesCopied = 0;  // Running counter
  int		bytesToCopy;      // Used to compute number of bytes left in page to be copied

  while (n > 0) {
    // Translate current user page to system address.  If this fails, return
    // the number of bytes copied so far.
    curUser = (unsigned char *)MemoryTranslateUserToSystem (pcb, (uint32)user);

    // If we could not translate address, exit now
    if (curUser == (unsigned char *)0) break;

    // Calculate the number of bytes to copy this time.  If we have more bytes
    // to copy than there are left in the current page, we'll have to just copy to the
    // end of the page and then go through the loop again with the next page.
    // In other words, "bytesToCopy" is the minimum of the bytes left on this page
    // and the total number of bytes left to copy ("n").

    // First, compute number of bytes left in this page.  This is just
    // the total size of a page minus the current offset part of the physical
    // address.  MEM_PAGESIZE should be the size (in bytes) of 1 page of memory.
    // MEM_ADDRESS_OFFSET_MASK should be the bit mask required to get just the
    // "offset" portion of an address.
    bytesToCopy = MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK);

    // Now find minimum of bytes in this page vs. total bytes left to copy
    if (bytesToCopy > n) {
      bytesToCopy = n;
    }

    // Perform the copy.
    if (dir >= 0) {
      bcopy (system, curUser, bytesToCopy);
    } else {
      bcopy (curUser, system, bytesToCopy);
    }

    // Keep track of bytes copied and adjust addresses appropriately.
    n -= bytesToCopy;           // Total number of bytes left to copy
    bytesCopied += bytesToCopy; // Total number of bytes copied thus far
    system += bytesToCopy;      // Current address in system space to copy next bytes from/into
    user += bytesToCopy;        // Current virtual address in user space to copy next bytes from/into
  }
  return (bytesCopied);
}

//----------------------------------------------------------------------
//
//	These two routines copy data between user and system spaces.
//	They call a common routine to do the copying; the only difference
//	between the calls is the actual call to do the copying.  Everything
//	else is identical.
//
//----------------------------------------------------------------------
int MemoryCopySystemToUser (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, from, to, n, 1));
}

int MemoryCopyUserToSystem (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, to, from, n, -1));
}

//---------------------------------------------------------------------
// MemoryPageFaultHandler is called in traps.c whenever a page fault
// (better known as a "seg fault" occurs.  If the address that was
// being accessed is on the stack, we need to allocate a new page
// for the stack.  If it is not on the stack, then this is a legitimate
// seg fault and we should kill the process.  Returns MEM_SUCCESS
// on success, and kills the current process on failure.  Note that
// fault_address is the beginning of the page of the virtual address that
// caused the page fault, i.e. it is the vaddr with the offset zero-ed
// out.
//
// Note: The existing code is incomplete and only for reference.
// Feel free to edit.
//---------------------------------------------------------------------
int MemoryPageFaultHandler(PCB *pcb) {
  uint32 npg;
  uint32 fault_addr = pcb->currentSavedFrame[PROCESS_STACK_FAULT];
  uint32 usr_stack_addr = pcb->currentSavedFrame[PROCESS_STACK_USER_STACKPOINTER];
  uint32 fault_pagenum = fault_addr >> MEM_L1FIELD_FIRST_BITNUM;
  uint32 stack_pagenum = usr_stack_addr >> MEM_L1FIELD_FIRST_BITNUM;

  printf("MEMFaultHandler %x %x \n", fault_addr, usr_stack_addr);
  if (fault_pagenum >= MEM_L1TABLE_SIZE) {
    printf("PID: %d: 0x%x address exceeds maximum virtual address\n", fault_addr, GetPidFromAddress(pcb));
    printf("  killing PID: %d\n", GetCurrentPid());
    ProcessKill();
    return MEM_FAIL;
  }

  if (fault_pagenum < stack_pagenum) {
    printf("PID: %d SEGFAULT\n", GetPidFromAddress(pcb));
    printf("  killing PID: %d\n", GetCurrentPid());
    ProcessKill();
    return MEM_FAIL;
  }
  else {
    printf("MemoryPageFaultHandler: Growing call stack of PID: %d\n", GetPidFromAddress(pcb));
    npg = MemoryAllocPageEasy(pcb);
    pcb->pagetable[fault_pagenum] = MemorySetupPte(npg);
    pcb->npages++;
    return MEM_SUCCESS;
  }
}


//---------------------------------------------------------------------
// You may need to implement the following functions and access them from process.c
// Feel free to edit/remove them
//---------------------------------------------------------------------

int MemoryAllocPageEasy(PCB *pcb) {
  uint32 pagenum;
  if ((pagenum = MemoryAllocPage()) == 0) {
    // printf("MemoryAllocPageEasy: killing pagenum: %d\n", pagenum);
    // printf("MemoryAllocPageEasy: killing nfreepages: %d\n", nfreepages);
    printf("MemoryAllocPageEasy: PID: %d no more pages to allocate\n", GetPidFromAddress(pcb));
    printf("  killing PID: %d\n", GetCurrentPid());
    ProcessKill();
  }
  return pagenum;
}

int MemoryAllocPage(void) {
  int i;
  int j;
  uint32 tmp;
  if (nfreepages == 0) {
    // printf("MemoryAllocPage: returning zero nfreepags: %d\n", nfreepages);
    return 0;
  }

  for (i = 0; i < MEM_FREEMAP_SIZE; i++) {
    if (freemap[i] != 0) {
      tmp = freemap[i];
      for (j = 0; j < 32; j++) {
        if ((tmp & 0x1) == 1) {
          nfreepages--;
	  // printf("MemoryAllocPage: nfreepags: %d\n", nfreepages);
	  freemap[i] &= invert(0x1 << j);
          return (i*32 + j);
        }
        tmp = tmp >> 1;
      }
    }
  }
  return 0;
}

void MemorySetFreemap(uint32 page) {
  freemap[page / 32] |= 1 << (page % 32);
}

void MemoryFreePage(uint32 page) {
  MemorySetFreemap(page);
  nfreepages++;
}

void MemoryFreePte(uint32 pte) {
  MemoryFreePage((pte & MEM_PTE_MASK) >> MEM_L1FIELD_FIRST_BITNUM);
}

uint32 MemorySetupPte (uint32 page) {
  //printf("MemorySetupPte: %x\n", (page << MEM_L1FIELD_FIRST_BITNUM) | MEM_PTE_VALID);
  return (page << MEM_L1FIELD_FIRST_BITNUM) | MEM_PTE_VALID;
}

int findFreeMallocNode(PCB* pcb, int index, int order, int order_want) {
  int ret1;
  int ret2;
  
  if (order == order_want && pcb->malloc_meta[index] == 0) {
    return MEM_FINDFREE_MALLOC_STATUS | (order << 8) | index;
  }
  else if ((pcb->malloc_meta[index] & MEM_MALLOC_TAKEN) == MEM_MALLOC_TAKEN) {
    return MEM_MALLOC_FIND_TAKEN;
  }
  else if ((pcb->malloc_meta[index] & MEM_MALLOC_PARTITIONED) != MEM_MALLOC_PARTITIONED) {
    return (order << 8) | index;
  }

  ret1 = findFreeMallocNode(pcb, 2*index + 1, order - 1, order_want);
  if ((ret1 & MEM_FINDFREE_MALLOC_STATUS) == MEM_FINDFREE_MALLOC_STATUS) return ret1;
  ret2 = findFreeMallocNode(pcb, 2*index + 2, order - 1, order_want);
  if ((ret2 & MEM_FINDFREE_MALLOC_STATUS) == MEM_FINDFREE_MALLOC_STATUS) return ret2;

  if (ret1 == MEM_MALLOC_FIND_TAKEN) return ret2;
  if (ret2 == MEM_MALLOC_FIND_TAKEN) return ret1;

  if ((ret1 & MEM_MALLOC_FIND_ORDER_MASK) > (ret2 & MEM_MALLOC_FIND_ORDER_MASK)) {
    return ret2;
  } 
  else { 
    return ret1;
  }
}

uint32 addressFromOrderIndex(int order, int index) {
  order = 7 - order;
  return (index - ((1 << order) - 1)) * sizeFromOrder(7 - order);
}

uint32 sizeFromOrder(int order) {
  return (1 << order) * MEM_MALLOC_MIN_SIZE;
}

int partitionNode(PCB* pcb, int order, int index) {
  pcb->malloc_meta[index] |= MEM_MALLOC_PARTITIONED;
  printf("Created a right child node (order = %d, addr = %x, size = %d) of parent (order = %d, addr = %x, size = %d\n", order - 1, addressFromOrderIndex(order - 1, 2*index + 2), sizeFromOrder(order - 1), order, addressFromOrderIndex(order, index), sizeFromOrder(order));
  printf("Created a left child node (order = %d, addr = %x, size = %d) of parent (order = %d, addr = %x, size = %d\n", order - 1, addressFromOrderIndex(order - 1, 2*index + 1), sizeFromOrder(order - 1), order, addressFromOrderIndex(order, index), sizeFromOrder(order));
  return MEM_SUCCESS; 
}

void* malloc(PCB* pcb, int memsize) {
  int tmpmul = 32;
  int order = 0;
  int findval = 0;
  uint32 addressToReturn;
  while (memsize > tmpmul) {
    order++;
    tmpmul *= 2;
  }
  if (order > 7) return NULL;
  if (memsize < 0) return NULL;
  
  findval = findFreeMallocNode(pcb, 0, 7, order);
  while ((findval & MEM_FINDFREE_MALLOC_STATUS) != MEM_FINDFREE_MALLOC_STATUS) {
    if (findval == MEM_MALLOC_FIND_TAKEN) return NULL; 
    partitionNode(pcb, (findval & MEM_MALLOC_FIND_ORDER_MASK) >> 8, findval & MEM_MALLOC_FIND_INDEX_MASK);
    findval = findFreeMallocNode(pcb, 0, 7, order);
  }
  currentPCB->malloc_meta[findval & MEM_MALLOC_FIND_INDEX_MASK] |= MEM_MALLOC_TAKEN;

  addressToReturn = (uint32) pcb->heap_base | addressFromOrderIndex((findval & MEM_MALLOC_FIND_ORDER_MASK) >> 8, findval & MEM_MALLOC_FIND_INDEX_MASK);
  return (void*) addressToReturn;
}

uint32 mfree_recurse(PCB* pcb, int order, int index, int cleared) {
  // If no block of this size has been allocated, then mfree should return a fail (-1)
  if (order > 7)
    return MEM_FAIL;

  if (cleared == 0) { 
    if (pcb->malloc_meta[index] == MEM_MALLOC_TAKEN) {
      printf("Freed the block: order = %d, addr = %d, size = %d\n", order, addressFromOrderIndex(order, index), sizeFromOrder(order));
      pcb->malloc_meta[index] = 0;
      mfree_recurse(pcb, order + 1, (index - 1)/2, 1);
      return sizeFromOrder(order);
    }
    return mfree_recurse(pcb, order + 1, (index - 1)/2, 0);
  }
  else {
    if (pcb->malloc_meta[2*index+1] == 0 && pcb->malloc_meta[2*index+2] == 0) {
      printf("Coalesced buddy nodes (order = %d, addr = %d, size = %d) & (order = %d, addr = %d, size = %d)\n", order - 1, addressFromOrderIndex(order - 1, 2*index + 1), sizeFromOrder(order - 1), order - 1, addressFromOrderIndex(order - 1, 2*index + 2), sizeFromOrder(order - 1));
      printf("into the parent node (order = %d, addr = %d, size = %d)\n", order, addressFromOrderIndex(order, index), sizeFromOrder(order));
      pcb->malloc_meta[index] = 0;
      return mfree_recurse(pcb, order + 1, (index - 1)/2, 1);
    }
    return MEM_SUCCESS;
  }
}

uint32 mfree(PCB* pcb, void* mem) {
  int index;
  uint32 mem2 = (uint32) mem;
  if (mem2 < (uint32) pcb->heap_base || mem2 > ((uint32) pcb->heap_base + 4092) || mem == NULL) return MEM_FAIL;
  mem2 = mem2 & 0xfff;
  index = ((uint32) (mem2))/32 + (255 - 128); 
  return mfree_recurse(pcb, 0, index, 0); 
}
