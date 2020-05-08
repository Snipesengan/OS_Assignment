# Problem
There are three elevator **Lift-1**, **Lift-2**, **Lift-3**, which are servicing a 20-floor building (Floors 1 to 20). Assume that intially all lifts are Floor 1. Each **lift** waits for lift **request** from any floor (1 to 20). Assume that initially all lifts are in Floor 1. Each **lift** waits for lift **requests** from any floor (1 to 20) and serves one **request** at a time.

# Implementation
The program is implemented in C using pthread library and somaphores for thread syncronisation. See Makefile. The general problem that this program is solving is the N consumer-producer problem /w bounded buffer.

# To Run

Create a file **"sim_input.txt"** to store n requests in the following formats, for n between 50 and 100.\
1 5\
7 2\
3 8\
4 11\
.\
.\
.\
12 19\
20 7

```
make lift_sim_A
./lift_sim_A m t

make lift_sim_B
./lift_sim_B m t

make clean
```
where m = buffer size and t = time for each lift request in miliseconds. The output is placed in **"sim_out.txt"**.
