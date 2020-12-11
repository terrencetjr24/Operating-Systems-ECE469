# How to build solution
 - to run question 2, run the following commands: 
```
$ cd lab3/os
$ make

$ cd ../apps/q2	
$ make
$ make run
```

# Peculiarities about our Solution
 - When running the example code inside of prio_test, our processes display their print statements in the proper order except for two statements.
 - The two statements are A22 and C5 that are swapped
 - The reason that the two statements are swapped is likely becasue the C process spends too much time in the same queue as the A process (allowing it to finish C5 before A22), which was likely casued by a rounding difference from our estcpu calculations

# External Sources
 - none
