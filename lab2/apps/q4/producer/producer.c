#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"

#include "spawn.h"

void main (int argc, char *argv[])
{
  circular_buffer *cb;        // Used to access missile codes in shared memory page
  uint32 h_mem;            // Handle to the shared memory page
  sem_t s_procs_completed; // Semaphore to signal the original process that we're done
  lock_t cbuff_lock;
  int i;
  char * message = "Hello World";
  int message_len = dstrlen(message);
  cond_t empty_cv;
  cond_t full_cv;

  if (argc != 6) { 
    Printf("Usage: "); Printf(argv[0]); Printf(" <handle_to_shared_memory_page> <handle_to_page_mapped_semaphore> <handle_to_lock> <handle_to_empty_cv> <handle_to_full_cv>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  h_mem = dstrtol(argv[1], NULL, 10); // The "10" means base 10
  s_procs_completed = dstrtol(argv[2], NULL, 10);
  cbuff_lock = dstrtol(argv[3], NULL, 10);
  empty_cv = dstrtol(argv[4], NULL, 10);
  full_cv = dstrtol(argv[5], NULL, 10);

  // Map shared memory page into this process's memory space
  if ((cb = (circular_buffer *)shmat(h_mem)) == NULL) {
    Printf("Could not map the virtual address to the memory in "); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }

  // Now print a message to show that everything worked

  for (i = 0; i < message_len; i++) {
    if (lock_acquire(cbuff_lock) != SYNC_SUCCESS) {
      Printf("Bad lock cbuff_lock (%d) in ", cbuff_lock); Printf(argv[0]); Printf(", exiting...\n");
    }

    while ((cb->head + 1) % BUFFERSIZE == cb->tail) {
      cond_wait(full_cv);
    }

    Printf("Producer %d inserted: %c \n", getpid(), message[i]);
    cb->buff[cb->head % BUFFERSIZE] = message[i];
    cb->head++;
    cond_signal(empty_cv);
    if (lock_release(cbuff_lock) != SYNC_SUCCESS) {
      Printf("Bad lock cbuff_lock (%d) in ", cbuff_lock); Printf(argv[0]); Printf(", exiting...\n");
    } 
  }

  // Signal the semaphore to tell the original process that we're done
  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}
