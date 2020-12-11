#include "usertraps.h"
#include "misc.h"

#define HELLO_WORLD "hello_world.dlx.obj"
#define OUT_OF_BOUND_VM_ACC "out_of_bound_vm_access.dlx.obj"
#define INVALID_MEM_ACCESS "invalid_mem_access.dlx.obj"
#define GROW_CALL_STACK "grow_call_stack.dlx.obj"
#define SIM_PROCS "simultaneous_process.dlx.obj"

void main (int argc, char *argv[])
{
  int num_hello_world = 0;             // Used to store number of processes to create
  int i;                               // Loop index variable
  sem_t s_procs_completed;             // Semaphore used to wait until all spawned processes have completed
  char s_procs_completed_str[10];      // Used as command-line argument to pass page_mapped handle to new processes


  // Create semaphore to not exit this process until all other processes 
  // have signalled that they are complete.
  // Created with -29 so that it won't exit before the last of the 30 simultaneous processes finishes
  if ((s_procs_completed = sem_create(-29)) == SYNC_FAIL) {
    Printf("makeprocs (%d): Bad sem_create\n", getpid());
    Exit();
  }

  // Setup the command-line arguments for the new processes.  We're going to
  // pass the handles to the semaphore as strings
  // on the command line, so we must first convert them from ints to strings.
  ditoa(s_procs_completed, s_procs_completed_str);


  Printf("-------------------------------------------------------------------------------------\n");
  Printf("makeprocs (%d): starting\n\n", getpid());

  // Running a single hello world program
  process_create(HELLO_WORLD, NULL);

  // Accessing memory beyond the maximum virtual address 
  process_create(OUT_OF_BOUND_VM_ACC, NULL);
  
  // Accessing memory inside the virtual address space, but outside of currently allocated pages
  process_create(INVALID_MEM_ACCESS, NULL);

  // Causing the user function call stack to grow larger than 1 page.
  process_create(GROW_CALL_STACK, NULL);

  // Calling hellow world 100 times
  
  for (i = 0; i < 100; i++)
    process_create(HELLO_WORLD, NULL);
 
  // Spawning 30 simultaneous processes (need semaphore in this one)
  
  for (i = 0; i < 30; i++)
    process_create(SIM_PROCS, s_procs_completed_str, NULL);
  
  
  if (sem_wait(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in %s\n", s_procs_completed, argv[0]);
    Exit();
  }

  Printf("-------------------------------------------------------------------------------------\n");
  Printf("makeprocs (%d): All other processes completed, exiting main process.\n", getpid());

}
