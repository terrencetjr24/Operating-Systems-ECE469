#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "synch.h"
#include "queue.h"
#include "mbox.h"

mbox mboxes[MBOX_NUM_MBOXES];
mbox_message mbox_messages[MBOX_NUM_BUFFERS];

//-------------------------------------------------------
//
// void MboxModuleInit();
//
// Initialize all mailboxes.  This process does not need
// to worry about synchronization as it is called at boot
// time.  Only initialize necessary items here: you can
// initialize others in MboxCreate.  In other words, 
// don't waste system resources like locks and semaphores
// on unused mailboxes.
//
//-------------------------------------------------------

void MboxModuleInit() {
  int i;
  for (i = 0; i < MBOX_NUM_MBOXES; i++) {
    mboxes[i].inuse = 0;
  }
  for (i = 0; i < MBOX_NUM_BUFFERS; i++) {
    mbox_messages[i].inuse = 0;
  }
}

//-------------------------------------------------------
//
// mbox_t MboxCreate();
//
// Allocate an available mailbox structure for use. 
//
// Returns the mailbox handle on success
// Returns MBOX_FAIL on error.
//
//-------------------------------------------------------
mbox_t MboxCreate() {
  mbox_t mbox = 0;
  int j;
  uint32 intrval;

  intrval = DisableIntrs();
  for (mbox = 0; mbox < MBOX_NUM_MBOXES; mbox++) {
    if (mboxes[mbox].inuse == 0) {
      mboxes[mbox].inuse = 1;
      break;
    }
  }
  RestoreIntrs(intrval);

  if (mbox == MBOX_NUM_MBOXES) return MBOX_FAIL;

  if ((mboxes[mbox].lock = LockCreate()) == SYNC_FAIL) {
    printf("Lock failed to create for mbox %d for process %d\n", mbox, GetCurrentPid());
    return MBOX_FAIL;
  }
  
  if ((mboxes[mbox].not_full = CondCreate(mboxes[mbox].lock)) == SYNC_FAIL) {
    printf("NOT FULL CondVar failed to create for mbox %d for process %d\n", mbox, GetCurrentPid());
    return MBOX_FAIL;
  }

  if ((mboxes[mbox].not_empty = CondCreate(mboxes[mbox].lock)) == SYNC_FAIL) {
    printf("NOT EMPTY CondVar failed to create for mbox %d for process %d\n", mbox, GetCurrentPid());
    return MBOX_FAIL;
  }

  if (AQueueInit(&mboxes[mbox].msg_queue) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: could not initialize mbox message queue in create_mbox!\n");
    return MBOX_FAIL;
  }

  for (j = 0; j < PROCESS_MAX_PROCS; j++) {
    mboxes[mbox].procs[j] = 0;
  }

  return mbox;
}

//-------------------------------------------------------
// 
// void MboxOpen(mbox_t);
//
// Open the mailbox for use by the current process.  Note
// that it is assumed that the internal lock/mutex handle 
// of the mailbox and the inuse flag will not be changed 
// during execution.  This allows us to get the a valid 
// lock handle without a need for synchronization.
//
// Returns MBOX_FAIL on failure.
// Returns MBOX_SUCCESS on success.
//
//-------------------------------------------------------
int MboxOpen(mbox_t handle) {
  int mypid = GetCurrentPid();
  if (LockHandleAcquire(mboxes[handle].lock) != SYNC_SUCCESS) {
    return MBOX_FAIL;
  }

  mboxes[handle].procs[mypid] = 1;

  LockHandleRelease(mboxes[handle].lock);

  return MBOX_SUCCESS;
}

//-------------------------------------------------------
//
// int MboxClose(mbox_t);
//
// Close the mailbox for use to the current process.
// If the number of processes using the given mailbox
// is zero, then disable the mailbox structure and
// return it to the set of available mboxes.
//
// Returns MBOX_FAIL on failure.
// Returns MBOX_SUCCESS on success.
//
//-------------------------------------------------------
int MboxClose(mbox_t handle) {
  int i;
  int usedflag = 0;
  Link * l;
  if (LockHandleAcquire(mboxes[handle].lock) != SYNC_SUCCESS) {
    return MBOX_FAIL;
  }

  mboxes[handle].procs[GetCurrentPid()] = 0;

  for (i = 0; i < PROCESS_MAX_PROCS; i++) {
    if (mboxes[handle].procs[i] != 0) {
      usedflag = 1;
    }
  }

  if (usedflag != 1) mboxes[handle].inuse = 0;

  while (AQueueEmpty(&mboxes[handle].msg_queue) == 0) {
    l = AQueueFirst(&mboxes[handle].msg_queue);
    AQueueRemove(&l);
  }

  LockHandleRelease(mboxes[handle].lock);
  return MBOX_SUCCESS;
}

//-------------------------------------------------------
//
// int MboxSend(mbox_t handle,int length, void* message);
//
// Send a message (pointed to by "message") of length
// "length" bytes to the specified mailbox.  Messages of
// length 0 are allowed.  The call 
// blocks when there is not enough space in the mailbox.
// Messages cannot be longer than MBOX_MAX_MESSAGE_LENGTH.
// Note that the calling process must have opened the 
// mailbox via MboxOpen.
//
// Returns MBOX_FAIL on failure.
// Returns MBOX_SUCCESS on success.
//
//-------------------------------------------------------
int MboxSend(mbox_t handle, int length, void* message) {
  int i = 0;
  int mypid = GetCurrentPid();
  Link * msg_link;
  uint32 intrval;

  if (LockHandleAcquire(mboxes[handle].lock) != SYNC_SUCCESS) {
    return MBOX_FAIL;
  }

  if (mboxes[handle].procs[mypid] != 1) {
    LockHandleRelease(mboxes[handle].lock);
    return MBOX_FAIL;
  }

  while (AQueueLength(&mboxes[handle].msg_queue) >= MBOX_MAX_BUFFERS_PER_MBOX || i == MBOX_NUM_BUFFERS) {
    CondHandleWait(mboxes[handle].not_full);
    printf("stuck PID: %d\n", mypid);

    intrval = DisableIntrs();
    for (i = 0; i < MBOX_NUM_BUFFERS; i++) {
      if (mbox_messages[i].inuse == 0) {
        mbox_messages[i].inuse = 1;
        break;
      }
    }
    RestoreIntrs(intrval);
  }

  if (length > MBOX_MAX_MESSAGE_LENGTH) {
    LockHandleRelease(mboxes[handle].lock);
    return MBOX_FAIL;
  }
  bcopy(message, mbox_messages[i].buffer, length);
  mbox_messages[i].length = length;
  
  if ((msg_link = AQueueAllocLink((void *) &mbox_messages[i])) == NULL) {
    printf("no more links MboxSend; PID %d\n", mypid);
    exitsim();
  }

  if (AQueueInsertLast(&mboxes[handle].msg_queue, msg_link) != QUEUE_SUCCESS) {
    printf("FATAL ERROR: MboxSend can't add message to queue PID %d\n", mypid);
    exitsim();
  }

  CondHandleSignal(mboxes[handle].not_empty);
  LockHandleRelease(mboxes[handle].lock);

  return MBOX_SUCCESS;
}

//-------------------------------------------------------
//
// int MboxRecv(mbox_t handle, int maxlength, void* message);
//
// Receive a message from the specified mailbox.  The call 
// blocks when there is no message in the buffer.  Maxlength
// should indicate the maximum number of bytes that can be
// copied from the buffer into the address of "message".  
// An error occurs if the message is larger than maxlength.
// Note that the calling process must have opened the mailbox 
// via MboxOpen.
//   
// Returns MBOX_FAIL on failure.
// Returns number of bytes written into message on success.
//
//-------------------------------------------------------
int MboxRecv(mbox_t handle, int maxlength, void* message) {
  int mypid = GetCurrentPid();
  Link * msg_link;
  mbox_message * msg;

  if (LockHandleAcquire(mboxes[handle].lock) != SYNC_SUCCESS) {
    return MBOX_FAIL;
  }

  if (mboxes[handle].procs[mypid] != 1) {
    LockHandleRelease(mboxes[handle].lock);
    return MBOX_FAIL;
  }

  while (AQueueEmpty(&mboxes[handle].msg_queue) == 1) {
    CondHandleWait(mboxes[handle].not_empty);
  }

  msg_link = AQueueFirst(&mboxes[handle].msg_queue);
  msg = (mbox_message *) AQueueObject(msg_link);
  if (msg->length > maxlength) {
    LockHandleRelease(mboxes[handle].lock);
    return MBOX_FAIL;
  }
  bcopy(msg->buffer, message, msg->length);
  
  if (AQueueRemove(&msg_link) != QUEUE_SUCCESS) {
    printf("FATAL ERROR MboxRecv no remove link PID %d\n", mypid);
    exitsim();
  }

  CondHandleSignal(mboxes[handle].not_full);
  LockHandleRelease(mboxes[handle].lock);

  return msg->length;
}

//--------------------------------------------------------------------------------
// 
// int MboxCloseAllByPid(int pid);
//
// Scans through all mailboxes and removes this pid from their "open procs" list.
// If this was the only open process, then it makes the mailbox available.  Call
// this function in ProcessFreeResources in process.c.
//
// Returns MBOX_FAIL on failure.
// Returns MBOX_SUCCESS on success.
//
//--------------------------------------------------------------------------------
int MboxCloseAllByPid(int pid) {
  int i;
  int j;
  int box_in_use = 0;
  for (i = 0; i < MBOX_NUM_MBOXES; i++) {
    if (mboxes[i].inuse == 1) {
      
      if (LockHandleAcquire(mboxes[i].lock) != SYNC_SUCCESS) {
	return MBOX_FAIL;
      }
      box_in_use = 0;

      mboxes[i].procs[pid] = 0;
      for (j = 0; j < PROCESS_MAX_PROCS ; j++)     {
	if (mboxes[i].procs[j] != 0){
	  box_in_use = 1;
	  break;
	}
      }
      
      if (box_in_use == 0) {
	mboxes[i].inuse = 0;
      } 
      LockHandleRelease(mboxes[i].lock);
    }
  }
  
  return MBOX_SUCCESS;
}
