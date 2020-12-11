#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  sem_t s_procs_completed; // Semaphore to signal the original process that we're done
  int forkResult;
  int x = 0;
  if (argc != 2) { 
    Printf("Usage: %s <handle_to_procs_completed_semaphore>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  s_procs_completed = dstrtol(argv[1], NULL, 10);

  // Now print a message to show that everything worked
  forkResult = fork();
  if (forkResult != 0){
    Printf("hello_world from parent (%d): Hello world!\n", getpid());
    Printf("X is %d\n", x);
    //return;
  }
  else{
    Printf("child iteration ONE born (%d)\n", getpid());
    Printf("X is %d\n", x);
  }
  x++;
  forkResult = fork();
  if (forkResult != 0){
    Printf("hello_world from child iteration ONE (%d): Hello world!\n", getpid());
    Printf("   X1 is %d\n", x);
    //return;
  }
  else{
    Printf("child iteration TWO born (%d)\n", getpid());
    Printf("X is %d\n", x);
  }
  x++;
  forkResult = fork();
  if (forkResult != 0){
    Printf("hello_world from child iteration TWO (%d): Hello world!\n", getpid());
    Printf("   X2 is %d\n", x);
    //return;
  }
  else{
    Printf("child iteration THREE born (%d)\n", getpid());
    Printf("X is %d\n", x);
  }
  x++;
  Printf("hello_world from child iteration THREE (%d): Hello world!\n", getpid());
  Printf("   X3 is %d\n", x);

  /*
  // Signal the semaphore to tell the original process that we're done
  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("hello_world (%d): Bad semaphore s_procs_completed (%d)!\n", getpid(), s_procs_completed);
    Exit();
  }
  */

  Printf("hello_world (%d): Done!\n", getpid());
}
