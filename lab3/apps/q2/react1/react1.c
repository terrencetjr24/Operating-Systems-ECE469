#include "usertraps.h"
#include "misc.h"

#include "spawn.h"

void main (int argc, char *argv[])
{
  mbox_t S2_mbox;
  mbox_t S_mbox;
  char sending;
  char receiving;
  if (argc != 3) { 
    Printf("Usage: "); Printf(argv[0]); Printf(" <handle_to_S2_mbox> <handle_to_S_box\n"); 
    Exit();
  } 

  S2_mbox = dstrtol(argv[1], NULL, 10);
  S_mbox = dstrtol(argv[2], NULL, 10);
  sending = ' ';

  if (mbox_open(S2_mbox) == MBOX_FAIL) {
    Printf("Failed to open mailbox in react1. PID %d\n", getpid());
  }
  
  if (mbox_open(S_mbox) == MBOX_FAIL) {
    Printf("Failed to open mailbox in react1. PID %d\n", getpid());
  }
  
  if(mbox_recv(S2_mbox, 0, (void*)&receiving) == MBOX_FAIL){
    Printf("Failed to receive a message in react1. PID %d\n", getpid());
    Exit();
  }
  //Sending 2 messages to S_mbox
  if (mbox_send(S_mbox, 0, (void*)&sending) == MBOX_FAIL)
    Printf("Failed to send message 1 in react1. PID %d\n", getpid());
  
  if (mbox_send(S_mbox, 0, (void*)&sending) == MBOX_FAIL)
    Printf("Failed to send message 2 in react1. PID %d\n", getpid());
  
  Printf("S2 -> S + S reacted, PID: %d\n", getpid());

  Exit();
}
