#include "usertraps.h"

void main (int x)
{
  Printf("Current Process ID: %d", Getpid());
  while(1); // Use CTRL-C to exit the simulator
}
