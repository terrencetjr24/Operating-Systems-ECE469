#include "usertraps.h"
#include "misc.h"

#include "spawn.h"

void main (int argc, char *argv[])
{
  int i;
  mbox_t Co_mbox;
  char *sending;
  if (argc != 2) { 
    Printf("Usage: "); Printf(argv[0]); Printf(" <handle_to_Co_mbox> \n"); 
    Exit();
  } 

  Co_mbox = dstrtol(argv[1], NULL, 10);
  sending = ' ';
  
  if (mbox_open(Co_mbox) == MBOX_FAIL) {
    Printf("Failed to open mailbox in CoGen. PID %d\n", getpid());
  }

  if (mbox_send(Co_mbox, 0, (void*)sending) == MBOX_FAIL) {
    Printf("Failed to send a message in CoGen. PID %d\n", getpid());
  }

  Printf("CO injected into Radeon atmosphere, PID: %d\n", getpid());

  Exit();
}
