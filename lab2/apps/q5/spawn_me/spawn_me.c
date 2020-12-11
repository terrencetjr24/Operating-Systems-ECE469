#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"

#include "spawn.h"

void main (int argc, char *argv[])
{
  int i;
  int niters;
  sem_t s_procs_completed;
  sem_t h20_sem, h2_sem, o2_sem, so4_sem, so2_sem, h2so4_sem;

  if (argc != 10) { 
    Printf("num args %d\n", argc);
    Printf("Usage: "); Printf(argv[0]); Printf(" <numiters> <react> <comp sem> <h20 sem> <so4 sem> <h2 sem> <o2 sem> <so2 sem> <h2so4 sem>\n"); 
    Exit();
  } 
//  Printf("%c%c%c%c", argv[1][0], argv[1][1], argv[1][2], argv[1][3]);
//  Printf(", PID: %d\n", getpid());

  // Convert the command-line strings into integers for use as handles
  niters = dstrtol(argv[1], NULL, 10);
  s_procs_completed = dstrtol(argv[3], NULL, 10);
  h20_sem = dstrtol(argv[4], NULL, 10);
  so4_sem = dstrtol(argv[5], NULL, 10);
  h2_sem = dstrtol(argv[6], NULL, 10);
  o2_sem = dstrtol(argv[7], NULL, 10);
  so2_sem = dstrtol(argv[8], NULL, 10);
  h2so4_sem = dstrtol(argv[9], NULL, 10);

  for (i = 0; i < niters; i++) {
    switch (argv[2][0]) {
      case '1':
        sem_signal(h20_sem); 
	Printf("H2O injected into Radeon atmosphere, PID: %d\n", getpid());
        break;
      case '2':
        sem_signal(so4_sem);
	Printf("SO4 injected into Radeon atmosphere, PID: %d\n", getpid());
        break;
      case '3':
        sem_wait(h20_sem);
	sem_wait(h20_sem);
	sem_signal(o2_sem);
	sem_signal(h2_sem);
	sem_signal(h2_sem);
	Printf("2 H2O -> 2 H2 + O2 reacted, PID: %d\n", getpid());
        break;
      case '4':
        sem_wait(so4_sem);
	sem_signal(so2_sem);
	sem_signal(o2_sem);
	Printf("SO4 -> SO2 + O2 reacted, PID: %d\n", getpid());
        break;
      case '5':
        sem_wait(h2_sem);
	sem_wait(o2_sem);
	sem_wait(so2_sem);
	sem_signal(h2so4_sem);
	Printf("(%d) H2 + O2 + SO2 -> H2SO4 reacted, PID: %d\n", i+1, getpid());
        break;
    }
  }

  // Signal the semaphore to tell the original process that we're done
  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");

    Exit();
  }
  Exit();
}
