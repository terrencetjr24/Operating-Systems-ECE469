#include "usertraps.h"
#include "misc.h"

#include "spawn.h"

void main (int argc, char *argv[])
{
  int i;
  mbox_t S_mbox;
  mbox_t o2_mbox;
  mbox_t So4_mbox;
  char sending;
  char receiving;
  int flag;
  if (argc != 4) { 
    Printf("Usage: "); Printf(argv[0]); Printf(" <handle_to_S_mbox> <handle_to_o2_mbox> <handle_to_So4_mbox> <\n"); 
    Exit();
  } 

  S_mbox = dstrtol(argv[1], NULL, 10);
  o2_mbox = dstrtol(argv[2], NULL, 10);
  So4_mbox = dstrtol(argv[3], NULL, 10);
  sending = ' ';

  if (mbox_open(S_mbox) == MBOX_FAIL) {
    Printf("Failed to open mailbox in react3. PID %d\n", getpid());
  }
  
  if (mbox_open(o2_mbox) == MBOX_FAIL) {
    Printf("Failed to open mailbox in react3. PID %d\n", getpid());
  }
  
  if (mbox_open(So4_mbox) == MBOX_FAIL) {
    Printf("Failed to open mailbox in react3. PID %d\n", getpid());
  }
  
  //Receiving 3 messages
  if(mbox_recv(S_mbox, 0, (void*)&receiving) == MBOX_FAIL){
    Printf("Failed to receive a message in react3. PID %d\n", getpid());
    Exit();
  }
  for (i = 0; i<2; i++){
    flag = mbox_recv(o2_mbox, 0, (void*)&receiving);
    if(flag == MBOX_FAIL){
      Printf("Failed to receive a O2 message in react3. PID %d\n", getpid());
      Exit();
    }
  }
  //Sending 1 message to So4 mbox 
  if (mbox_send(So4_mbox, 0, (void*)&sending) == MBOX_FAIL) {
    Printf("Failed to send a message in react3. PID %d\n", getpid());
  }
  
  Printf("S + 2 O2 -> SO4 reacted, PID: %d\n", getpid());

  Exit();
}
