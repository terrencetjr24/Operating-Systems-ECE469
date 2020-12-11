#include "usertraps.h"
#include "misc.h"

#define HELLO_WORLD "hello_world.dlx.obj"

void main (int argc, char *argv[])
{
  int* a;
  int* b;
  int* c;
  int *d;
  int q,w,e,r;

  if (argc != 1) {
    Printf("Usage: %s \n", argv[0]);
    Exit();
  }


  Printf("-------------------------------------------------------------------------------------\n");

  a = (int *) malloc(31);
  if (a == NULL)
    Printf("Error in variable 'a' allocation\n");
  else
    Printf("allcoated variable 'a'\n");
  
  b = (int *) malloc(64);
  if (b == NULL)
    Printf("Error in variable 'b' allocation\n");
  else
    Printf("allcoated variable 'b'\n");
  
  c = (int *) malloc(65);
  if (c == NULL)
    Printf("Error in variable 'c' allocation\n");
  else
    Printf("allcoated variable 'c'\n");
  
  d = (int *) malloc(256);
  if (d == NULL)
    Printf("Error in variable 'd' allocation\n");
  else
    Printf("allcoated variable 'd'\n");

  q = mfree((void *) b);
  Printf("Freed variable 'b' (%d bytes freed)\n", q);
  w = mfree((void *) d);
  Printf("Freed variable 'd' (%d bytes freed)\n", w);
  e = mfree((void *) a);
  Printf("Freed variable 'a' (%d bytes freed)\n", e);
  r = mfree((void *) c);
  Printf("Freed variable 'c' (%d bytes freed)\n", r);

  Printf("-------------------------------------------------------------------------------------\n");
  Printf("makeprocs (%d): All other processes completed, exiting main process.\n", getpid());

}
