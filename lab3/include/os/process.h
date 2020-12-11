//
//	process.h
//
//	Definitions for process creation and manipulation.  These include
//	the process control block (PCB) structure as well as information
//	about the stack format for a saved process.
//

#ifndef	__process_h__
#define	__process_h__

#include "dlxos.h"
#include "queue.h"
#include "clock.h"

#define PROCESS_FAIL 0
#define PROCESS_SUCCESS 1

#define	PROCESS_MAX_PROCS	32	// Maximum number of active processes

// lab3 inserted
#define BASE_PRIORITY		50
#define MAX_PRIORITY		127
#define PRIORITIES_PER_QUEUE	4
#define NUM_PRIORITY_QUEUES	32
#define NUM_JIFFIES_DECAY	100
// /////////////

#define	PROCESS_INIT_ISR_SYS	0x140	// Initial status reg value for system processes
#define	PROCESS_INIT_ISR_USER	0x100	// Initial status reg value for user processes

#define	PROCESS_STATUS_FREE	0x1
#define	PROCESS_STATUS_RUNNABLE	0x2
#define	PROCESS_STATUS_WAITING	0x4
#define	PROCESS_STATUS_STARTING	0x8
#define	PROCESS_STATUS_ZOMBIE	0x10
#define	PROCESS_STATUS_MASK	0x3f
#define	PROCESS_TYPE_SYSTEM	0x100
#define	PROCESS_TYPE_USER	0x200

typedef	void (*VoidFunc)();

// Process control block
typedef struct PCB {
  uint32	*currentSavedFrame; // -> current saved frame.  MUST BE 1ST!
  uint32	*sysStackPtr;	// Current system stack pointer.  MUST BE 2ND!
  uint32	sysStackArea;	// System stack area for this process
  unsigned int	flags;
  char		name[80];	// Process name
  uint32	pagetable[16];	// Statically allocated page table
  int		npages;		// Number of pages allocated to this process
  Link		*l;		// Used for keeping PCB in queues

  int           pinfo;          // Turns on printing of runtime stats
  int           pnice;          // Used in priority calculation

  int		runtime;	// total time on cpu (jiffies)
  int		switchedtime;	// time pcb was landed on cpu (used to calculate runtime) (jiffies)

  int		priority;	// priority of this process
  int		base_priority;	// base priority 
  double 	estcpu;		// estimated time on cpu
  int		num_quanta;

  int		autowake;	// auto wake flag
  int		sleeptime;	// time it went to sleep
  int		wakeuptime;	// time to wake up

  int		yieldflag;
} PCB;

// Offsets of various registers from the stack pointer in the register
// save frame.  Offsets are in WORDS (4 byte chunks)
#define	PROCESS_STACK_IREG	10	// Offset of r0 (grows upwards)
// NOTE: r0 isn't actually stored!  This is for convenience - r1 is the
// first stored register, and is at location PROCESS_STACK_IREG+1
#define	PROCESS_STACK_FREG	(PROCESS_STACK_IREG+32)	// Offset of f0
#define	PROCESS_STACK_IAR	(PROCESS_STACK_FREG+32) // Offset of IAR
#define	PROCESS_STACK_ISR	(PROCESS_STACK_IAR+1)
#define	PROCESS_STACK_CAUSE	(PROCESS_STACK_IAR+2)
#define	PROCESS_STACK_FAULT	(PROCESS_STACK_IAR+3)
#define	PROCESS_STACK_PTBASE	(PROCESS_STACK_IAR+4)
#define	PROCESS_STACK_PTSIZE	(PROCESS_STACK_IAR+5)
#define	PROCESS_STACK_PTBITS	(PROCESS_STACK_IAR+6)
#define	PROCESS_STACK_PREV_FRAME 10	// points to previous interrupt frame
#define	PROCESS_STACK_FRAME_SIZE 85	// interrupt frame is 85 words
#define PROCESS_MAX_NAME_LENGTH  100    // Maximum length of an executable's filename
#define SIZE_ARG_BUFF  	1024		// Max number of characters in the
					// command-line arguments
#define MAX_ARGS	128		// Max number of command-line
					// arguments

// Number of jiffies in a single process quantum (i.e. how often ProcessSchedule is called)
#define PROCESS_QUANTUM_JIFFIES  CLOCK_PROCESS_JIFFIES

// Use this format string for printing CPU stats
#define PROCESS_CPUSTATS_FORMAT "CPUStats: Process %d has run for %d jiffies, prio = %d\n"

extern PCB	*currentPCB;

int ProcessFork (VoidFunc func, uint32 param, int pnice, int pinfo,char *name, int isUser);
extern void	ProcessSchedule ();
extern void	ContextSwitch(void *, void *, int);
extern void	ProcessSuspend (PCB *);
extern void	ProcessWakeup (PCB *);
extern void	ProcessSetResult (PCB *, uint32);
extern void	ProcessSleep ();
extern void     ProcessDestroy(PCB *pcb);
extern unsigned GetCurrentPid();
void process_create(char *name, ...);
int GetPidFromAddress(PCB *pcb);

void ProcessUserSleep(int seconds);
void ProcessYield();

void ProcessIdle();
void ProcessRecalcPriority(PCB *pcb);
int  ProcessInsertRunning(PCB *pcb);
void ProcessDecayEstcpu(PCB *pcb);
void ProcessDecayEstcpuSleep(PCB *pcb, int time_asleep_jiffies);
PCB* ProcessFindHighestPriorityPCB();
void ProcessDecayAllEstcpus();
void ProcessFixRunQueues();
int  ProcessCountAutowake();
int  ProcessPrintRunQueues();
int ProcessPrintWaitQueues();
void WakeupSleepingProcesses();
inline int WhichQueue(PCB *pcb);

#endif	/* __process_h__ */
