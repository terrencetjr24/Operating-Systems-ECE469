#include "usertraps.h"
#include "misc.h"

#include "spawn.h"

void main (int argc, char *argv[])
{
  int numprocs = 5;
  int num_s2 = 0;
  int num_co = 0;
  int react1 = 0;
  int react2 = 0;
  int react3 = 0;
    
  int i;

  //mailbox creation
  mbox_t S2_mbox;
  mbox_t Co_mbox;
  mbox_t S_mbox;
  mbox_t o2_mbox;
  mbox_t C2_mbox;
  mbox_t So4_mbox;

  //Changing mbox handle to strings to pass to child process
  char S2_mbox_str [10],
    Co_mbox_str [10],
    S_mbox_str [10],
    o2_mbox_str [10],
    C2_mbox_str [10],
    So4_mbox_str [10];

  //Expected number of left over molecules
  int leftover_S;
  int leftover_Co;
  int leftover_o2;
  int leftover_C2;
  int expected_So4;

  char* receiving;

  //Checking the number of input arguments
  if (argc != 3) {
    Printf("Usage: "); Printf(argv[0]); Printf(" <number of S2 molecules> <number of CO molecules>\n");
    Exit();
  }

  //Changing the inputs to integers
  num_s2 = dstrtol(argv[1], NULL, 10);
  num_co = dstrtol(argv[2], NULL, 10);
  Printf("Creating %d S2s and %d COs\n", num_s2, num_co);

  //Calculating the number of reactions
  react1 = num_s2;
  react2 = num_co / 4;
  react3 = (2*react1 < react2) ? react1*2 : react2; 

  leftover_S = (2*react1)-react3;
  leftover_Co = num_co - (4*react2);
  leftover_o2 = react2 - react3;
  leftover_C2 = react2;
  expected_So4 = react3;

  if ((S2_mbox = mbox_create()) == MBOX_FAIL){
    Printf("Failed to create mailbox for S2\n");
    Exit();
  }
  if ((Co_mbox = mbox_create()) == MBOX_FAIL){
    Printf("Failed to create mailbox for Co\n");
    Exit();
  }
  if ((S_mbox = mbox_create()) == MBOX_FAIL){
    Printf("Failed to create mailbox for S\n");
    Exit();
  }
  if ((o2_mbox = mbox_create()) == MBOX_FAIL){
    Printf("Failed to create mailbox for o2\n");
    Exit();
  }
  if ((C2_mbox = mbox_create()) == MBOX_FAIL){
    Printf("Failed to create mailbox for C2\n");
    Exit();
  }
  if ((So4_mbox = mbox_create()) == MBOX_FAIL){
    Printf("Failed to create mailbox for So4\n");
    Exit();
  }

  //Opening the mailboxes
  if (mbox_open(S2_mbox) != MBOX_SUCCESS){
    Printf("Failed to OPEN S2 mailbox\n");
    Exit();
  }
  if (mbox_open(Co_mbox) != MBOX_SUCCESS){
    Printf("Failed to OPEN Co mailbox\n");
    Exit();
  }
  if (mbox_open(S_mbox) != MBOX_SUCCESS){
    Printf("Failed to OPEN S mailbox\n");
    Exit();
  }
  if (mbox_open(o2_mbox) != MBOX_SUCCESS){
    Printf("Failed to OPEN o2 mailbox\n");
    Exit();
  }
  if (mbox_open(C2_mbox) != MBOX_SUCCESS){
    Printf("Failed to OPEN C2 mailbox\n");
    Exit();
  }
  if (mbox_open(So4_mbox) != MBOX_SUCCESS){
    Printf("Failed to OPEN So4 mailbox\n");
    Exit();
  }

  ditoa(S2_mbox, S2_mbox_str);
  ditoa(Co_mbox, Co_mbox_str);
  ditoa(S_mbox, S_mbox_str);
  ditoa(o2_mbox, o2_mbox_str);
  ditoa(C2_mbox, C2_mbox_str);
  ditoa(So4_mbox, So4_mbox_str);
  
  //process creation (Note: each proc only runs once, up to makeprocs to call them correct # of times)

  //S2 generation
  for (i = 0; i < num_s2; i++){
    process_create(S2_TO_RUN, 0, 1, S2_mbox_str, NULL);
  }
  //Co generation
  for (i = 0; i < num_co; i++){
    process_create(Co_TO_RUN, 0, 1, Co_mbox_str,NULL);
  }
  //react1
  for (i = 0; i < react1; i++){
    process_create(REACT1_TO_RUN, 0, 1, S2_mbox_str, S_mbox_str ,NULL);
  }
  //react2
  for (i = 0; i < react2; i++){
    process_create(REACT2_TO_RUN, 0, 1, Co_mbox_str, o2_mbox_str, C2_mbox_str ,NULL);
  }
  //react3
  for (i = 0; i < react3; i++){
    process_create(REACT3_TO_RUN, 0, 1, S_mbox_str, o2_mbox_str, So4_mbox_str ,NULL);
  }

  //Recieving messages from So4 mailbox to know when to exit makeprocs
  for (i = 0; i<expected_So4; i++)
    if(mbox_recv(So4_mbox, 0, (void*)receiving) == MBOX_FAIL){
      Printf("Failed to receive a message in makeprocs. PID %d\n", getpid());
      Exit();
    }
  
  //TODO
  Printf("%d S2's leftover. ", 0);
  Printf("%d S's leftover. ", leftover_S);
  Printf("%d CO's leftover. ", leftover_Co);
  Printf("%d O2's leftover. ", leftover_o2);
  Printf("%d C2's leftover. ", leftover_C2);
  Printf("%d SO4's created.\n", expected_So4); 
}
