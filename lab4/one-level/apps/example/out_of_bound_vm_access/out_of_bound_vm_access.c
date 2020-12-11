#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  sem_t s_procs_completed; // Semaphore to signal the original process that we're done
  int *x;
  int q, z;
  
  // Accessing memory beyond max virtual address
  Printf("About to access memory beyond the maximum virtual addres (PID: %d)\n", getpid());
  
  q = 7; 
  Printf("Address of the first memory location: %d (value there : %d)\n", &q, q);

  z = 0;
  x = q && z;
  x = x - 1;
  Printf("Addres of the new pointer: %d (Its value is: %d)\n", x, *x);

  Printf("process should have aborted (PID: %d) (should not have printed)\n", getpid());

  
}
