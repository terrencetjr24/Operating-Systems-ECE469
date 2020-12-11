#include "usertraps.h"
#include "misc.h"

#include "spawn.h"

void main (int argc, char *argv[])
{
  sem_t s_procs_completed; // Semaphore to signal the original process that we're done
  int program_index;
  int i=0,j=0;             // Loop index variables

  if (argc != 3) { 
    Printf("Usage: %s <program index> <handle_to_page_mapped_semaphore> (argc was %d)\n", argc); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  program_index = dstrtol(argv[1], NULL, 10);
  s_procs_completed = dstrtol(argv[2], NULL, 10);

  //if (program_index == 0) sleep(30);
  // Now print messages to see if priority scheduling is working
  for(i=0; i<30; i++) {
    //if (program_index == 0 && i % 2 == 0) yield();
    Printf("spawn_me (%d): %c%d\n", getpid(), 'A'+program_index, i);
    for(j=0; j<50000; j++);  // just busy-wait awhile
  }

  // Signal the semaphore to tell the original process that we're done
  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("spawn_me (%d): Bad semaphore s_procs_completed (%d)!\n", getpid(), s_procs_completed);
    Exit();
  }

  Printf("spawn_me (%d): Done!\n", getpid());
}
