#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  int q;
  int *z;
  
  // Performing an invalid memory access on memory outside of currently allocated page
  Printf("About to perform invalid memory access outside of allocated page (PID: %d)\n", getpid());

  q = 7;
  Printf("The address of allocated memory is: %d (the value is: %d)\n", &q, q);
  z = &q - 1200;
  Printf("(SHOULD NOT PRINT) new add: %d (new val: %d)\n", z, *z);


  Printf("This should not print (PID: %d)\n", getpid());


}
