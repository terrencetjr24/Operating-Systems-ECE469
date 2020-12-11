//
//	process.c
//
//	This file defines routines for dealing with processes.  This
//	includes the "main" routine for the OS, which creates a process
//	for the initial thread of execution.  It also includes
//	code to create and delete processes, as well as context switch
//	code.  Note, however, that the actual context switching is
//	done in assembly language elsewhere.

#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "synch.h"
#include "memory.h"
#include "filesys.h"
#include "share_memory.h"
#include "mbox.h"
#include "clock.h"

// next decay time, 10 quanta after previous decay time
int		nextEstcpuDecay;

// Pointer to the current PCB.  This is used by the assembly language
// routines for context switches.
PCB		*currentPCB;

// Pointer to idle process
PCB		*idlePCB;

// List of free PCBs.
static Queue	freepcbs;

// List of processes that are ready to run (ie, not waiting for something
// to happen).
static Queue	runQueue[NUM_PRIORITY_QUEUES];

// List of processes that are waiting for something to happen.  There's no
// reason why this must be a single list; there could be many lists for many
// different conditions.
static Queue	waitQueue;

// List of processes waiting to be deleted.  See below for a description of
// the reason that we need a separate queue for processes about to die.
static Queue	zombieQueue;

// Static area for all process control blocks.  This is necessary because
// we can't use malloc() inside the OS.
static PCB	pcbs[PROCESS_MAX_PROCS];

// String listing debugging options to print out.
char	debugstr[200];

int ProcessGetCodeInfo(const char *file, uint32 *startAddr, uint32 *codeStart, uint32 *codeSize,
                       uint32 *dataStart, uint32 *dataSize);
int ProcessGetFromFile(int fd, unsigned char *buf, uint32 *addr, int max);
uint32 get_argument(char *string);



//----------------------------------------------------------------------
//
//	ProcessModuleInit
//
//	Initialize the process module.  This involves initializing all
//	of the process control blocks to appropriate values (ie, free
//	and available).  We also need to initialize all of the queues.
//
//----------------------------------------------------------------------
void ProcessModuleInit () {
  int		i;

  dbprintf ('p', "ProcessModuleInit: function started\n");
  AQueueInit (&freepcbs);
  for (i = 0; i < NUM_PRIORITY_QUEUES; i++) {
    AQueueInit(&runQueue[i]);
  }
  AQueueInit (&waitQueue);
  AQueueInit (&zombieQueue);
  // For each PCB slot in the global pcbs array:
  for (i = 0; i < PROCESS_MAX_PROCS; i++) {
    dbprintf ('p', "Initializing PCB %d @ 0x%x.\n", i, (int)&(pcbs[i]));
    // First, set the internal PCB link pointer to a newly allocated link
    if ((pcbs[i].l = AQueueAllocLink(&pcbs[i])) == NULL) {
      printf("FATAL ERROR: could not allocate link in ProcessModuleInit!\n");
      exitsim();
    }
    // Next, set the pcb to be available
    pcbs[i].flags = PROCESS_STATUS_FREE;
    // Finally, insert the link into the queue
    if (AQueueInsertFirst(&freepcbs, pcbs[i].l) != QUEUE_SUCCESS) {
      printf("FATAL ERROR: could not insert PCB link into queue in ProcessModuleInit!\n");
      exitsim();
    }
  }
  nextEstcpuDecay = NUM_JIFFIES_DECAY;
  // There are no processes running at this point, so currentPCB=NULL
  currentPCB = NULL;
  dbprintf ('p', "ProcessModuleInit: function complete\n");
}

//----------------------------------------------------------------------
//
//	ProcessSetStatus
//
//	Set the status of a process.
//
//----------------------------------------------------------------------
void ProcessSetStatus (PCB *pcb, int status) {
  pcb->flags &= ~PROCESS_STATUS_MASK;
  pcb->flags |= status;
}

//----------------------------------------------------------------------
//
//	ProcessFreeResources
//
//	Free the resources associated with a process.  This assumes the
//	process isn't currently on any queue.
//
//----------------------------------------------------------------------
void ProcessFreeResources (PCB *pcb) {
  int i = 0;
  int npages = 0;

  dbprintf ('p', "ProcessFreeResources: function started\n");


  //-----------------------------------------------------
  // Your code for closing any open mailbox connections
  // that a dying process might have goes here.
  //-----------------------------------------------------
  if ( MboxCloseAllByPid(pcb - pcbs) == MBOX_FAIL){
    printf("FATAL ERROR: could not close mailboxes in ProcessFreeResources!\n");
    exitsim();
  }

  // Allocate a new link for this pcb on the freepcbs queue
  if ((pcb->l = AQueueAllocLink(pcb)) == NULL) {
    printf("FATAL ERROR: could not get Queue Link in ProcessFreeResources!\n");
    exitsim();
  }
  // Set the pcb's status to available
  pcb->flags = PROCESS_STATUS_FREE;
  // Insert the link into the freepcbs queue
  if (AQueueInsertLast(&freepcbs, pcb->l) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not insert PCB link into freepcbs queue in ProcessFreeResources!\n");
    exitsim();
  }

  // Free the process's memory.  This is easy with a one-level page
  // table, but could get more complex with two-level page tables.
  // Free the shared pages first, since the standard MemoryFreePte was
  // not modified to recognize shared pages, therefore it could really
  // screw up.
  npages = pcb->npages;
  for (i=0; i<npages; i++) {
    MemoryFreeSharedPte(pcb, i);
  }
  // Next free the non-shared pages
  for (i = 0; i < pcb->npages; i++) {
    MemoryFreePte (pcb->pagetable[i]);
  }
  // Free the page allocated for the system stack
  MemoryFreePage (pcb->sysStackArea / MEMORY_PAGE_SIZE);
  ProcessSetStatus (pcb, PROCESS_STATUS_FREE);
  dbprintf ('p', "ProcessFreeResources: function complete\n");
}

//----------------------------------------------------------------------
//
//	ProcessSetResult
//
//	Set the result returned to a process.  This is done by storing
//	the value into the current register save area for r1.  When the
//	context is restored, r1 will contain the return value.  This
//	routine should only be called from a trap.  Calling it at other
//	times (such as an interrupt handler) will cause unpredictable
//	results.
//
//----------------------------------------------------------------------
void ProcessSetResult (PCB * pcb, uint32 result) {
  pcb->currentSavedFrame[PROCESS_STACK_IREG+1] = result;
}


//----------------------------------------------------------------------
//
//	ProcessSchedule
//
//	Schedule the next process to run.  If there are no processes to
//	run, exit.  This means that there should be an idle loop process
//	if you want to allow the system to "run" when there's no real
//	work to be done.
//
//	NOTE: the scheduler should only be called from a trap or interrupt
//	handler.  This way, interrupts are disabled.  Also, it must be
//	called consistently, and because it might be called from an interrupt
//	handler (the timer interrupt), it must ALWAYS be called from a trap
//	or interrupt handler.
//
//	Note that this procedure doesn't actually START the next process.
//	It only changes the currentPCB and other variables so the next
//	return from interrupt will restore a different context from that
//	which was saved.
//
//----------------------------------------------------------------------
void ProcessSchedule () {
  PCB *pcb=NULL;
  //int i=0;
  //Link *l=NULL;

  dbprintf ('p', "Now entering ProcessSchedule (cur=0x%x, %d ready)\n",
	    (int)currentPCB, AQueueLength (&runQueue[WhichQueue(currentPCB)]));

  // update current PCB runtime and print if pinfo
  if (currentPCB->switchedtime != 0) {
    currentPCB->runtime += ClkGetCurJiffies() - currentPCB->switchedtime; 
  }
  if (currentPCB->pinfo == 1) {
    printf(PROCESS_CPUSTATS_FORMAT, GetPidFromAddress(currentPCB), currentPCB->runtime, currentPCB->priority);
  }

  // The OS exits if there's no runnable process.  This is a feature, not a
  // bug.  An easy solution to allowing no runnable "user" processes is to
  // have an "idle" process that's simply an infinite loop.
//  if (AQueueEmpty(&runQueue)) {
//    if (!AQueueEmpty(&waitQueue)) {
//      printf("FATAL ERROR: no runnable processes, but there are sleeping processes waiting!\n");
//      l = AQueueFirst(&waitQueue);
//      while (l != NULL) {
//        pcb = AQueueObject(l);
//        printf("Sleeping process %d: ", i++); printf("PID = %d\n", (int)(pcb - pcbs));
//        l = AQueueNext(l);
//      }
//      exitsim();
//    }
//    printf ("No runnable processes - exiting!\n");
//    exitsim ();	// NEVER RETURNS
//  }

  pcb = ProcessFindHighestPriorityPCB();
  if (pcb == idlePCB) {
    if (ProcessCountAutowake() == 0) {
      printf("No runnable processes - exiting!\n");
      exitsim();
    }
  }
  if (pcb == NULL) printf("highest pcb is null\n");

  // update estimated cpu time if process used entire window
  if (currentPCB->yieldflag == 0) { 
    if (ClkGetCurJiffies() - currentPCB->switchedtime >= PROCESS_QUANTUM_JIFFIES) {
      currentPCB->estcpu++;
      currentPCB->num_quanta++;
      ProcessRecalcPriority(currentPCB);
    }
  }
  else {
    currentPCB->yieldflag = 0;
  }
//  printf("PID: %d quanta: %d diff: %d", GetPidFromAddress(currentPCB), currentPCB->num_quanta, ClkGetCurJiffies() - currentPCB->switchedtime);

  if (ClkGetCurJiffies() > nextEstcpuDecay) {
    //printf("before decay: \n");
    //ProcessPrintRunQueues();
    ProcessDecayAllEstcpus();
    ProcessFixRunQueues();
    nextEstcpuDecay = ClkGetCurJiffies() + NUM_JIFFIES_DECAY;
  }

  WakeupSleepingProcesses();

  // Move the front of the queue to the end.  The running process was the one in front.
  pcb = ProcessFindHighestPriorityPCB();
  if (pcb == currentPCB) {
    if (AQueueRemove(&pcb->l) == QUEUE_FAIL) {
      printf("can't remove pcb from queue\n");
      exitsim();
    }
    if (ProcessInsertRunning(pcb) == QUEUE_FAIL) {
      printf("process insert running failed\n");
      exitsim();
    }
    pcb = ProcessFindHighestPriorityPCB();
  }
  
  //printf("idle process PID %d, current PID %d\n", GetPidFromAddress(idlePCB), GetPidFromAddress(currentPCB));
  //printf("BEFORE fixing:\n");
  //ProcessPrintRunQueues();
  //ProcessPrintWaitQueues();
  currentPCB = pcb;
  dbprintf ('p',"About to switch to PCB 0x%x,flags=0x%x @ 0x%x\n",
	    (int)pcb, pcb->flags, (int)(pcb->sysStackPtr[PROCESS_STACK_IAR]));

  //saved time switched in
  currentPCB->switchedtime = ClkGetCurJiffies();

  //This is the change made (3/24/20)
    if (currentPCB->flags & PROCESS_STATUS_RUNNABLE) {
    if (AQueueRemove(&currentPCB->l) == QUEUE_FAIL) {
      printf("can't remove pcb from queue\n");
      exitsim();
    }
    if (ProcessInsertRunning(currentPCB) == QUEUE_FAIL) {
      printf("process insert running failed\n");
      exitsim();
    }
  } 
    //printf("AFTER fixing: \n");
    //ProcessPrintRunQueues();  
  // Clean up zombie processes here.  This is done at interrupt time
  // because it can't be done while the process might still be running
  while (!AQueueEmpty(&zombieQueue)) {
    pcb = (PCB *)AQueueObject(AQueueFirst(&zombieQueue));
    dbprintf ('p', "Freeing zombie PCB 0x%x.\n", (int)pcb);
    if (AQueueRemove(&(pcb->l)) != QUEUE_SUCCESS) {
      printf("FATAL ERROR: could not remove zombie process from zombieQueue in ProcessSchedule!\n");
      exitsim();
    }
    ProcessFreeResources(pcb);
  }
  dbprintf ('p', "Leaving ProcessSchedule (cur=0x%x)\n", (int)currentPCB);
}

//----------------------------------------------------------------------
//
//	ProcessSuspend
//
//	Place a process in suspended animation until it's
//	awakened by ProcessAwaken.
//
//	NOTE: This must only be called from an interrupt or trap.  It
//	should be immediately followed by ProcessSchedule().
//
//----------------------------------------------------------------------
void ProcessSuspend (PCB *suspend) {
  // Make sure it's already a runnable process.
  dbprintf ('p', "ProcessSuspend (%d): function started\n", GetCurrentPid());
  ASSERT (suspend->flags & PROCESS_STATUS_RUNNABLE, "Trying to suspend a non-running process!\n");
  ProcessSetStatus (suspend, PROCESS_STATUS_WAITING);

  if (AQueueRemove(&(suspend->l)) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not remove process from run Queue in ProcessSuspend!\n");
    exitsim();
  }
  if ((suspend->l = AQueueAllocLink(suspend)) == NULL) {
    printf("FATAL ERROR: could not get Queue Link in ProcessSuspend!\n");
    exitsim();
  }
  if (AQueueInsertLast(&waitQueue, suspend->l) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not insert suspend PCB into waitQueue!\n");
    exitsim();
  }
  dbprintf ('p', "ProcessSuspend (%d): function complete\n", GetCurrentPid());

  //printf("suspending %d\n", GetPidFromAddress(suspend));
  //ProcessPrintRunQueues();
}

//----------------------------------------------------------------------
//
//	ProcessWakeup
//
//	Wake up a process from its slumber.  This only involves putting
//	it on the run queue; it's not guaranteed to be the next one to
//	run.
//
//	NOTE: This must only be called from an interrupt or trap.  It
//	need not be followed immediately by ProcessSchedule() because
//	the currently running process is unaffected.
//
//----------------------------------------------------------------------
void ProcessWakeup (PCB *wakeup) {
  dbprintf ('p',"Waking up PID %d.\n", (int)(wakeup - pcbs));
  // Make sure it's not yet a runnable process.
  ASSERT (wakeup->flags & PROCESS_STATUS_WAITING, "Trying to wake up a non-sleeping process!\n");
  ProcessSetStatus (wakeup, PROCESS_STATUS_RUNNABLE);
  if (AQueueRemove(&(wakeup->l)) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not remove wakeup PCB from waitQueue in ProcessWakeup!\n");
    exitsim();
  }

  wakeup->autowake = 0;
  ProcessDecayEstcpuSleep(wakeup, ClkGetCurJiffies() - wakeup->sleeptime);
  ProcessRecalcPriority(wakeup);
    
  ProcessInsertRunning(wakeup);
}


//----------------------------------------------------------------------
//
//	ProcessDestroy
//
//	Destroy a process by setting its status to zombie and putting it
//	on the zombie queue.  The next time the scheduler is called, this
//	process will be marked as free.  We can't necessarily do it now
//	because we might be the currently running process.
//
//	NOTE: This must only be called from an interrupt or trap.  However,
//	it need not be followed immediately by a ProcessSchedule() because
//	the process can continue running.
//
//----------------------------------------------------------------------
void ProcessDestroy (PCB *pcb) {
  dbprintf ('p', "ProcessDestroy (%d): function started\n", GetCurrentPid());
  ProcessSetStatus (pcb, PROCESS_STATUS_ZOMBIE);
  if (AQueueRemove(&(pcb->l)) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not remove link from queue in ProcessDestroy!\n");
    exitsim();
  }
  if ((pcb->l = AQueueAllocLink(pcb)) == NULL) {
    printf("FATAL ERROR: could not get link for zombie PCB in ProcessDestroy!\n");
    exitsim();
  }
  if (AQueueInsertFirst(&zombieQueue, pcb->l) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not insert link into runQueue in ProcessWakeup!\n");
    exitsim();
  }
  dbprintf ('p', "ProcessDestroy (%d): function complete\n", GetCurrentPid());
}

//----------------------------------------------------------------------
//
//	ProcessExit
//
//	This routine is called to exit from a system process.  It simply
//	calls an exit trap, which will be caught to exit the process.
//
//----------------------------------------------------------------------
static void ProcessExit () {
  exit ();
}


//----------------------------------------------------------------------
//
//	ProcessFork
//
//	Create a new process and make it runnable.  This involves the
//	following steps:
//	* Allocate resources for the process (PCB, memory, etc.)
//	* Initialize the resources
//	* Place the PCB on the runnable queue
//
//	NOTE: This code has been tested for system processes, but not
//	for user processes.
//
//----------------------------------------------------------------------
int ProcessFork (VoidFunc func, uint32 param, int pnice, int pinfo,char *name, int isUser) {
  int		fd, n;
  int		start, codeS, codeL, dataS, dataL;
  uint32	*stackframe;
  int		newPage;
  PCB		*pcb;
  int	addr = 0;
  int		intrs;
  unsigned char buf[100];
  uint32 dum[MAX_ARGS+8], count, offset;
  char *str;

  dbprintf ('p', "ProcessFork (%d): function started\n", GetCurrentPid());
  intrs = DisableIntrs ();
  dbprintf ('I', "Old interrupt value was 0x%x.\n", intrs);
  dbprintf ('p', "Entering ProcessFork args=0x%x 0x%x %s %d\n", (int)func,
	    param, name, isUser);
  // Get a free PCB for the new process
  if (AQueueEmpty(&freepcbs)) {
    printf ("FATAL error: no free processes!\n");
    exitsim ();	// NEVER RETURNS!
  }
  pcb = (PCB *)AQueueObject(AQueueFirst (&freepcbs));
  dbprintf ('p', "Got a link @ 0x%x\n", (int)(pcb->l));
  if (AQueueRemove (&(pcb->l)) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not remove link from freepcbsQueue in ProcessFork!\n");
    exitsim();
  }
  // This prevents someone else from grabbing this process
  ProcessSetStatus (pcb, PROCESS_STATUS_RUNNABLE);

  // At this point, the PCB is allocated and nobody else can get it.
  // However, it's not in the run queue, so it won't be run.  Thus, we
  // can turn on interrupts here.
  dbprintf ('I', "Before restore interrupt value is 0x%x.\n", (int)CurrentIntrs());
  RestoreIntrs (intrs);
  dbprintf ('I', "New interrupt value is 0x%x.\n", (int)CurrentIntrs());

  // Copy the process name into the PCB.
  dbprintf('p', "ProcessFork: Copying process name (%s) to pcb\n", name);
  dstrcpy(pcb->name, name);

  //----------------------------------------------------------------------
  // This section initializes the memory for this process
  //----------------------------------------------------------------------
  // For now, we'll use one user page and a page for the system stack.
  // For system processes, though, all pages must be contiguous.
  // Of course, system processes probably need just a single page for
  // their stack, and don't need any code or data pages allocated for them.
  pcb->npages = 1;
  newPage = MemoryAllocPage ();
  if (newPage == 0) {
    printf ("aFATAL: couldn't allocate memory - no free pages!\n");
    exitsim ();	// NEVER RETURNS!
  }
  pcb->pagetable[0] = MemorySetupPte (newPage);
  newPage = MemoryAllocPage ();
  if (newPage == 0) {
    printf ("bFATAL: couldn't allocate system stack - no free pages!\n");
    exitsim ();	// NEVER RETURNS!
  }
  pcb->sysStackArea = newPage * MEMORY_PAGE_SIZE;

  //----------------------------------------------------------------------
  // Stacks grow down from the top.  The current system stack pointer has
  // to be set to the bottom of the interrupt stack frame, which is at the
  // high end (address-wise) of the system stack.
  stackframe = ((uint32 *)(pcb->sysStackArea + MEMORY_PAGE_SIZE)) -
    (PROCESS_STACK_FRAME_SIZE + 8);
  // The system stack pointer is set to the base of the current interrupt
  // stack frame.
  pcb->sysStackPtr = stackframe;
  // The current stack frame pointer is set to the same thing.
  pcb->currentSavedFrame = stackframe;

  dbprintf ('p',
	    "Setting up PCB @ 0x%x (sys stack=0x%x, mem=0x%x, size=0x%x)\n",
	    (int)pcb, pcb->sysStackArea, pcb->pagetable[0],
	    pcb->npages * MEMORY_PAGE_SIZE);

  //----------------------------------------------------------------------
  // This section sets up the stack frame for the process.  This is done
  // so that the frame looks to the interrupt handler like the process
  // was "suspended" right before it began execution.  The standard
  // mechanism of swapping in the registers and returning to the place
  // where it was "interrupted" will then work.
  //----------------------------------------------------------------------

  // The previous stack frame pointer is set to 0, meaning there is no
  // previous frame.
  stackframe[PROCESS_STACK_PREV_FRAME] = 0;

  // Set the base of the level 1 page table.  If there's only one page
  // table level, this is it.  For 2-level page tables, put the address
  // of the level 1 page table here.  For 2-level page tables, we'll also
  // have to build up the necessary tables....
  stackframe[PROCESS_STACK_PTBASE] = (uint32)&(pcb->pagetable[0]);

  // Set the size (maximum number of entries) of the level 1 page table.
  // In our case, it's just one page, but it could be larger.
  stackframe[PROCESS_STACK_PTSIZE] = pcb->npages;

  // Set the number of bits for both the level 1 and level 2 page tables.
  // This can be changed on a per-process basis if desired.  For now,
  // though, it's fixed.
  stackframe[PROCESS_STACK_PTBITS] = (MEMORY_L1_PAGE_SIZE_BITS
					  + (MEMORY_L2_PAGE_SIZE_BITS << 16));


  if (isUser) {
    dbprintf ('p', "About to load %s\n", name);
    fd = ProcessGetCodeInfo (name, &start, &codeS, &codeL, &dataS, &dataL);
    if (fd < 0) {
      // Free newpage and pcb so we don't run out...
      ProcessFreeResources (pcb);
      return (-1);
    }
    dbprintf ('p', "File %s -> start=0x%08x\n", name, start);
    dbprintf ('p', "File %s -> code @ 0x%08x (size=0x%08x)\n", name, codeS,
	      codeL);
    dbprintf ('p', "File %s -> data @ 0x%08x (size=0x%08x)\n", name, dataS,
	      dataL);
    while ((n = ProcessGetFromFile (fd, buf, &addr, sizeof (buf))) > 0) {
      dbprintf ('p', "Placing %d bytes at vaddr %08x.\n", n, addr - n);
      // Copy the data to user memory.  Note that the user memory needs to
      // have enough space so that this copy will succeed!
      MemoryCopySystemToUser (pcb, buf, addr - n, n);
    }
    FsClose (fd);
    stackframe[PROCESS_STACK_ISR] = PROCESS_INIT_ISR_USER;
    // Set the initial stack pointer correctly.  Currently, it's just set
    // to the top of the (single) user address space allocated to this
    // process.
    str = (char *)param;
    stackframe[PROCESS_STACK_IREG+29] = MEMORY_PAGE_SIZE - SIZE_ARG_BUFF;
    // Copy the initial parameter to the top of stack
    MemoryCopySystemToUser (pcb, (char *)str,
			    (char *)stackframe[PROCESS_STACK_IREG+29],
			    SIZE_ARG_BUFF-32);
    offset = get_argument((char *)param);

    dum[2] = MEMORY_PAGE_SIZE - SIZE_ARG_BUFF + offset; 
    for(count=3;;count++)
    {
      offset=get_argument(NULL);
      dum[count] = MEMORY_PAGE_SIZE - SIZE_ARG_BUFF + offset;
      if(offset==0)
      {
        break;
      }
    }
    dum[0] = count-2;
    dum[1] = MEMORY_PAGE_SIZE - SIZE_ARG_BUFF - (count-2)*4;
    MemoryCopySystemToUser (pcb, (char *)dum,
			    (char *)(stackframe[PROCESS_STACK_IREG+29]-count*4),
			    (count)*sizeof(uint32));
    stackframe[PROCESS_STACK_IREG+29] -= 4*count;
    // Set the correct address at which to execute a user process.
    stackframe[PROCESS_STACK_IAR] = (uint32)start;
    pcb->flags |= PROCESS_TYPE_USER;
  } else {
    // Set r31 to ProcessExit().  This will only be called for a system
    // process; user processes do an exit() trap.
    stackframe[PROCESS_STACK_IREG+31] = (uint32)ProcessExit;

    // Set the stack register to the base of the system stack.
    stackframe[PROCESS_STACK_IREG+29]=pcb->sysStackArea + MEMORY_PAGE_SIZE-32;

    // Set the initial parameter properly by placing it on the stack frame
    // at the location pointed to by the "saved" stack pointer (r29).
    *((uint32 *)(stackframe[PROCESS_STACK_IREG+29])) = param;

    // Set up the initial address at which to execute.  This is done by
    // placing the address into the IAR slot of the stack frame.
    stackframe[PROCESS_STACK_IAR] = (uint32)func;

    // Set the initial value for the interrupt status register
    stackframe[PROCESS_STACK_ISR] = PROCESS_INIT_ISR_SYS;

    // Mark this as a system process.
    pcb->flags |= PROCESS_TYPE_SYSTEM;
  }

  // initialize added pcb struct attributes
  pcb->pinfo = pinfo;
  pcb->pnice = pnice;

  pcb->runtime = 0;
  pcb->switchedtime = 0;

  pcb->priority = 0;
  pcb->base_priority = BASE_PRIORITY;
  pcb->estcpu = 0;
  pcb->num_quanta = 0;

  pcb->autowake = 0;
  pcb->sleeptime = 0;
  pcb->wakeuptime = 0;


  if (func == ProcessIdle) {
    pcb->base_priority = MAX_PRIORITY; 
    idlePCB = pcb;
  }

  ProcessRecalcPriority(pcb);

  // Place PCB onto run queue
  intrs = DisableIntrs ();
  if ((pcb->l = AQueueAllocLink(pcb)) == NULL) {
    printf("FATAL ERROR: could not get link for forked PCB in ProcessFork!\n");
    exitsim();
  }
  if (ProcessInsertRunning(pcb) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not insert link into runQueue in ProcessFork!\n");
    exitsim();
  }
  RestoreIntrs (intrs);

  // If this is the first process, make it the current one
  if (currentPCB == NULL) {
    dbprintf ('p', "Setting currentPCB=0x%x, stackframe=0x%x\n", (int)pcb, (int)(pcb->currentSavedFrame));
    currentPCB = pcb;
  }

  dbprintf ('p', "Leaving ProcessFork (%s)\n", name);
  // Return the process number (found by subtracting the PCB number
  // from the base of the PCB array).
  dbprintf ('p', "ProcessFork (%d): function complete\n", GetCurrentPid());

  return (pcb - pcbs);
}

//----------------------------------------------------------------------
//
//	getxvalue
//
//	Convert a hex digit into an actual value.
//
//----------------------------------------------------------------------
static
inline
int
getxvalue (int x)
{
  if ((x >= '0') && (x <= '9')) {
    return (x - '0');
  } else if ((x >= 'a') && (x <= 'f')) {
    return (x + 10 - 'a');
  } else if ((x >= 'A') && (x <= 'F')) {
    return (x + 10 - 'A');
  } else {
    return (0);
  }
}

//----------------------------------------------------------------------
//
//	ProcessGetCodeSizes
//
//	Get the code sizes (stack & data) for a file.  A file descriptor
//	for the named file is returned.  This descriptor MUST be closed
//	(presumably by the caller) at some point.
//
//----------------------------------------------------------------------
int
ProcessGetCodeInfo (const char *file, uint32 *startAddr,
		    uint32 *codeStart, uint32 *codeSize,
		     uint32 *dataStart, uint32 *dataSize)
{
  int		fd;
  int		totalsize;
  char		buf[100];
  char		*pos;

  // Open the file for reading.  If it returns a negative number, the open
  // didn't work.
  if ((fd = FsOpen (file, FS_MODE_READ)) < 0) {
    dbprintf ('f', "ProcessGetCodeInfo: open of %s failed (%d).\n",
	      file, fd);
    return (-1);
  }
  dbprintf ('f', "File descriptor is now %d.\n", fd);
  if ((totalsize = FsRead (fd, buf, sizeof (buf))) != sizeof (buf)) {
    dbprintf ('f', "ProcessGetCodeInfo: read got %d (not %d) bytes from %s\n",
	      totalsize, (int)sizeof (buf), file);
    FsClose (fd);
    return (-1);
  }
  if (dstrstr (buf, "start:") == NULL) {
    dbprintf ('f', "ProcessGetCodeInfo: %s missing start line (not a DLX executable?)\n", file);
    return (-1);
  }
  pos = (char *)dindex (buf, ':') + 1;
  // Get the start address and overall size
  *startAddr = dstrtol (pos, &pos, 16);
  totalsize = dstrtol (pos, &pos, 16);
  // Get code & data section start & sizes
  *codeStart = dstrtol (pos, &pos, 16);
  *codeSize = dstrtol (pos, &pos, 16);
  *dataStart = dstrtol (pos, &pos, 16);
  *dataSize = dstrtol (pos, &pos, 16);
  // Seek to start of first real line
  FsSeek (fd, 1 + dindex (buf, '\n') - buf, 0);
  return (fd);
}


//----------------------------------------------------------------------
//
//	ProcessGetFromFile
//
//	Inputs:
//	addr -	points to an integer that contains the address of
//		the byte past that previously returned.  If this is the
//		first call to this routine, *addr should be set to 0.
//	fd -	File descriptor from which to read data.  The file format
//		is the same as that used by the DLX simulator.
//	buf -	points to a buffer that will receive data from the input
//		file.  Note that the data is NOT 0-terminated, and may
//		include any number of 0 bytes.
//	max -	maximum length of data to return.  The routine collects data
//		until either the address changes or it has read max bytes.
//
//	Returns the number of bytes actually stored into buf.  In addition,
//	*addr is updated to point to the byte following the last byte in
//	the buffer.
//
//	Load a file into memory.  The file format consists of a
//	leading address, followed by a colon, followed by the data
//	to go at that address.  If the address is omitted, the data
//	follows that from the previous line of the file.
//
//----------------------------------------------------------------------
int
ProcessGetFromFile (int fd, unsigned char *buf, uint32 *addr, int max)
{
  char	localbuf[204];
  int	nbytes;
  int	seekpos;
  unsigned char *pos = buf;
  char	*lpos = localbuf;

  // Remember our position at the start of the routine so we can adjust
  // it later.
  seekpos = FsSeek (fd, 0, FS_SEEK_CUR);
  // The maximum number of characters we could read is limited to the
  // maximum buffer space available to the caller * 2 because each 2
  // characters in the input file result in a single byte of program
  // info being read in.
  max = max * 2;
  // If the callers maximum is greater than the available buffer space,
  // limit the buffer space further.
  if (max > (sizeof(localbuf)-4)) {
    max = sizeof(localbuf)-4;
  }
  if ((nbytes = FsRead (fd, localbuf, max)) <= 0) {
    return (0);
  }
  // 'Z' is unused in load file, so use it to mark the end of the buffer
  // Back up until just after the last newline in the data we read.
  dbprintf ('f', "Got %d bytes at offset %d ...", nbytes, seekpos);
  while (localbuf[--nbytes] != '\n') {
  }
  localbuf[nbytes+1] = 'Z';
  localbuf[nbytes+2] = '\0';
  dbprintf ('f', " terminated at %d.\n", nbytes);
  dbprintf ('f', "Buffer is '%s'\n", localbuf);
  nbytes = 0;
  while (dindex (lpos, 'Z') != NULL) {
    if (dindex (lpos, ':') == NULL) {
      break;
    }
    if (*lpos != ':') {
      // If we're going to go to a new address, we break out of the
      // loop and return what we've got already.
      if (nbytes > 0) {
	break;
      }
      *addr = dstrtol (lpos, &lpos, 16);
      dbprintf ('f', "New address is 0x%x.\n", (int)(*addr));
    }
    if (*lpos != ':') {
      break;
    }
    lpos++;	// skip past colon
    while (1) {
      while (((*lpos) == ' ') || (*lpos == '\t')) {
	lpos++;
      }
      if (*lpos == '\n') {
	lpos++;
	break;
      } else if (!(isxdigit (*lpos) && isxdigit (*(lpos+1)))) {
     // Exit loop if at least one digit isn't a hex digit.
	break;
      }
      pos[nbytes++] = (getxvalue(*lpos) * 16) + getxvalue(*(lpos+1));
      lpos += 2;
      (*addr)++;
    }
  }
  // Seek to just past the last line we read.
  FsSeek (fd, seekpos + lpos - localbuf, FS_SEEK_SET);
  dbprintf ('f', "Seeking to %d and returning %d bytes!\n",
	    (int)(seekpos + lpos - localbuf), nbytes);
  return (nbytes);
}

//----------------------------------------------------------------------
//
//	main
//
//	This routine is called when the OS starts up.  It allocates a
//	PCB for the first process - the one corresponding to the initial
//	thread of execution.  Note that the stack pointer is already
//	set correctly by _osinit (assembly language code) to point
//	to the stack for the 0th process.  This stack isn't very big,
//	though, so it should be replaced by the system stack of the
//	currently running process.
//
//----------------------------------------------------------------------
void main (int argc, char *argv[])
{
  int		i;
  int		n;
  char	buf[120];
  char		*userprog = (char *)0;
  int base=0;
  int numargs=0;
  char *params[10]; // Maximum number of command-line parameters is 10

  // idle process fork addition
  char idleargs = '\0';
  // /////////////////////////
  
  debugstr[0] = '\0';

  printf ("Got %d arguments.\n", argc);
  printf ("Available memory: 0x%x -> 0x%x.\n", (int)lastosaddress, MemoryGetSize ());
  printf ("Argument count is %d.\n", argc);
  for (i = 0; i < argc; i++) {
    printf ("Argument %d is %s.\n", i, argv[i]);
  }

  FsModuleInit ();
  for (i = 0; i < argc; i++) 
  {
    if (argv[i][0] == '-') 
    {
      switch (argv[i][1]) 
      {
      case 'D':
	dstrcpy (debugstr, argv[++i]);
	break;
      case 'i':
	n = dstrtol (argv[++i], (void *)0, 0);
	ditoa (n, buf);
	printf ("Converted %s to %d=%s\n", argv[i], n, buf);
	break;
      case 'f':
      {
	int	start, codeS, codeL, dataS, dataL, fd, j;
	int	addr = 0;
	static unsigned char buf[200];
	fd = ProcessGetCodeInfo (argv[++i], &start, &codeS, &codeL, &dataS,
				 &dataL);
	printf ("File %s -> start=0x%08x\n", argv[i], start);
	printf ("File %s -> code @ 0x%08x (size=0x%08x)\n", argv[i], codeS,
		codeL);
	printf ("File %s -> data @ 0x%08x (size=0x%08x)\n", argv[i], dataS,
		dataL);
	while ((n = ProcessGetFromFile (fd, buf, &addr, sizeof (buf))) > 0) 
	{
	  for (j = 0; j < n; j += 4) 
	  {
	    printf ("%08x: %02x%02x%02x%02x\n", addr + j - n, buf[j], buf[j+1],
		    buf[j+2], buf[j+3]);
	  }
	}
	close (fd);
	break;
      }
      case 'u':
	userprog = argv[++i];
        base = i; // Save the location of the user program's name 
	break;
      default:
	printf ("Option %s not recognized.\n", argv[i]);
	break;
      }
      if(userprog)
        break;
    }
  }
  dbprintf ('i', "About to initialize queues.\n");
  AQueueModuleInit ();
  dbprintf ('i', "After initializing queues.\n");
  MemoryModuleInit ();
  dbprintf ('i', "After initializing memory.\n");

  ProcessModuleInit ();
  dbprintf ('i', "After initializing processes.\n");
  ShareModuleInit ();
  dbprintf ('i', "After initializing shared memory.\n");
  SynchModuleInit ();
  dbprintf ('i', "After initializing synchronization tools.\n");
  KbdModuleInit ();
  dbprintf ('i', "After initializing keyboard.\n");
  ClkModuleInit();
  for (i = 0; i < 100; i++) {
    buf[i] = 'a';
  }
  i = FsOpen ("vm", FS_MODE_WRITE);
  dbprintf ('i', "VM Descriptor is %d\n", i);
  FsSeek (i, 0, FS_SEEK_SET);
  FsWrite (i, buf, 80);
  FsClose (i);
  if (userprog != (char *)0) {
    numargs=0;
    for(i=0; i<argc-base; i++) {
      params[i] = argv[i+base];
      numargs++;
    }
    dbprintf('i', "main: Calling process_create with %d parameters\n", numargs);
    switch(numargs) {
      case  1: process_create(params[0], NULL); break;
      case  2: process_create(params[0], params[1], NULL); break;
      case  3: process_create(params[0], params[1], params[2], NULL); break;
      case  4: process_create(params[0], params[1], params[2], params[3], NULL); break;
      case  5: process_create(params[0], params[1], params[2], params[3], params[4], NULL); 
                              break;
      case  6: process_create(params[0], params[1], params[2], params[3], params[4], 
                              params[5], NULL); break;
      case  7: process_create(params[0], params[1], params[2], params[3], params[4], 
                              params[5], params[6], NULL); break;
      case  8: process_create(params[0], params[1], params[2], params[3], params[4], 
                              params[5], params[6], params[7], NULL); break;
      case  9: process_create(params[0], params[1], params[2], params[3], params[4], 
                              params[5], params[6], params[7], params[8], NULL); break;
      case 10: process_create(params[0], params[1], params[2], params[3], params[4], 
                              params[5], params[6], params[7], params[8], params[9], NULL); break;
      default: dbprintf('i', "ERROR: number of argument (%d) is not valid!\n", numargs);
    }
  } else {
    dbprintf('i', "No user program passed!\n");
  }

  // Create Idle Process
  ProcessFork(ProcessIdle, (uint32) &idleargs, 0, 0, "Idle", 0);

  // Start the clock which will in turn trigger periodic ProcessSchedule's
  ClkStart();

  intrreturn ();
  // Should never be called because the scheduler exits when there
  // are no runnable processes left.
  exitsim();	// NEVER RETURNS!
}

unsigned GetCurrentPid()
{
  return (unsigned)(currentPCB - pcbs);
}

unsigned findpid(PCB *pcb)
{
  return (unsigned)(pcb - pcbs);
}

uint32 get_argument(char *string)
{
  static char *str;
  static int location=0;
  int location2;
  
  if(string)
  {
    str=string;
    location = 0;
  }
    
  location2 = location;

  if(str[location]=='\0'||location>=99)
    return 0;

  for(;location<100;location++)
  {
    if(str[location]=='\0')
    {
      location++;
      break;
    }
  }
  return location2;
}

void process_create(char *name, ...)
{
  char **args;
  int i, j, k;
  char allargs[1000];
  args = &name;
  
  k=0;
  for(i=0; args[i]!=NULL; i++)
  {
    j=0;
    do {
      allargs[k] = args[i][j];
      j++; k++;
    } while(args[i][j-1]!='\0');
  }
  allargs[k] = allargs[k+1] = 0;
  ProcessFork(0, (uint32)allargs, 0, 0, name, 1);
}

int GetPidFromAddress(PCB *pcb) {
  return (int)(pcb - pcbs);
}

//--------------------------------------------------------
// ProcessSleep assumes that it will be immediately 
// followed by a call to ProcessSchedule (in traps.c).
//--------------------------------------------------------
void ProcessUserSleep(int seconds) {
  // Your code here
  currentPCB->autowake = 1;
  currentPCB->sleeptime = ClkGetCurJiffies();
  currentPCB->wakeuptime = currentPCB->sleeptime + seconds * 1000;
  ProcessSuspend(currentPCB);
}

//-----------------------------------------------------
// ProcessYield simply marks the currentPCB as yielding.
// This should immediately be followed by a call to
// ProcessSchedule (in traps.c).
//-----------------------------------------------------
void ProcessYield() {
  currentPCB->yieldflag = 1;
}

void ProcessIdle() {
  while(1) {
    //printf("idle\n");
  }
}

///////////OPTIONAL////////////////
void ProcessRecalcPriority(PCB *pcb) {
  pcb->priority = pcb->base_priority + (pcb->estcpu / 4) + (2 * pcb->pnice);
  if (pcb->priority > MAX_PRIORITY) pcb->priority = MAX_PRIORITY;
  if (pcb->priority < pcb->base_priority) pcb->priority = pcb->base_priority;
}

int  ProcessInsertRunning(PCB *pcb) {
  if (pcb->l == NULL) {
    if ((pcb->l = AQueueAllocLink(pcb)) == NULL) {
      printf("FATAL ERROR: could not get link for forked PCB in ProcessFork!\n");
      exitsim();
    }
  }
  return AQueueInsertLast(&runQueue[WhichQueue(pcb)], pcb->l);
}

void ProcessDecayEstcpu(PCB *pcb) {
  pcb->estcpu = pcb->estcpu * 2.0 / 3.0 + pcb->pnice;
}

void ProcessDecayEstcpuSleep(PCB *pcb, int time_asleep_jiffies) {
  int i;
  double mul = 1;
  int num_windows_asleep;
  if (time_asleep_jiffies >= NUM_JIFFIES_DECAY) {
    num_windows_asleep = time_asleep_jiffies / NUM_JIFFIES_DECAY;
    for (i = 0; i < num_windows_asleep; i++) {
      mul *= 2.0 / 3;
    }
    pcb->estcpu *= mul; 
  }
}

PCB* ProcessFindHighestPriorityPCB() {
  int i;
  Link * l;
  PCB* pcb;
  for (i = 0; i < NUM_PRIORITY_QUEUES; i++) {
    if (!AQueueEmpty(&runQueue[i])) {
      l = AQueueFirst(&runQueue[i]);
      pcb = (PCB*) AQueueObject(l); 
      return pcb;
    }
  }
  return NULL;
}

void ProcessDecayAllEstcpus() {
  int i; 
  Link *l;
  PCB* pcb;
  for (i = 0; i < NUM_PRIORITY_QUEUES; i++) {
    if (!AQueueEmpty(&runQueue[i])) {
      l = AQueueFirst(&runQueue[i]);
      while (l != NULL) {
        pcb = (PCB*) AQueueObject(l);
	ProcessDecayEstcpu(pcb);
	ProcessRecalcPriority(pcb);
	l = AQueueNext(l);
      }
    }
  }
}

void ProcessFixRunQueues() { 
  int i; 
  Link *l;
  PCB* pcb;
  for (i = 0; i < NUM_PRIORITY_QUEUES; i++) {
    if (!AQueueEmpty(&runQueue[i])) {
      l = AQueueFirst(&runQueue[i]);
      while (l != NULL) {
        pcb = (PCB*) AQueueObject(l);
	if (i != WhichQueue(pcb)) {
          if (AQueueRemove(&pcb->l) == QUEUE_FAIL) {
            printf("can't remove pcb from queue\n");
            exitsim();
          }
          if (ProcessInsertRunning(pcb) == QUEUE_FAIL) {
            printf("process insert running failed\n");
            exitsim();
          }
	}
	l = AQueueNext(l);
      }
    }
  }
}

int  ProcessCountAutowake() {
  int count = 0;
  Link *l;
  PCB *pcb;
  l = AQueueFirst(&waitQueue);
  while (l != NULL) {
    pcb = (PCB *) AQueueObject(l);
    if (pcb->autowake == 1) {
      count++;
    }
    l = AQueueNext(l);
  }
  return count;
}

int  ProcessPrintRunQueues() {
  int i;
  Link *l;
  PCB *pcb;
  for (i = 0; i < NUM_PRIORITY_QUEUES; i++) { 
    printf("%d: ", i);
    l = AQueueFirst(&runQueue[i]);
    while (l != NULL) {
      pcb = (PCB *) AQueueObject(l);
      printf("%d- ", GetPidFromAddress(pcb));
      printf("%f, ", pcb->estcpu);
      l = AQueueNext(l);
    }
    printf("\n");
  }
  return 1;
}

int ProcessPrintWaitQueues() {
  Link *l;
  PCB *pcb;
  printf("wait: ");
  l = AQueueFirst(&waitQueue);
  while (l != NULL) {
    pcb = (PCB *) AQueueObject(l);
    printf("%d", GetPidFromAddress(pcb));
    if (pcb->autowake == 1) printf("*");
    printf(", ");
    l = AQueueNext(l);
  }
  printf("\n");
  return 1;
}

inline int WhichQueue(PCB *pcb) {
  return pcb->priority / PRIORITIES_PER_QUEUE;
}

void WakeupSleepingProcesses() { 
  Link *l;
  PCB *pcb;
  int currTime = ClkGetCurJiffies();
  l = AQueueFirst(&waitQueue);
  while (l != NULL) {
    pcb = (PCB *) AQueueObject(l);
    //printf("PID: %d time: %d %d\n", GetPidFromAddress(pcb), currTime, pcb->wakeuptime);
    if (pcb->autowake == 1 && pcb->wakeuptime <= currTime) {
      //printf("wakeup\n");
      ProcessWakeup(pcb); 
    }
    l = AQueueNext(l);
  }
}
