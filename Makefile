CC = gcc 
CFLAGS = -Wall -ansi -pedantic -g 
LDFLAGS = -pthread -D_DEFAULT_SOURCE
LDLIBS = -lm -lrt 


lift_sim_B: lift_sim_B.c lift.h buffer.h buffer.o
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEBUG buffer.o lift_sim_B.c -o lift_sim_B $(LDLIBS)

lift_sim_A: lift_sim_A.c lift.h buffer.h buffer.o
	$(CC) $(CFLAGS) $(LDFLAGS) buffer.o lift_sim_A.c -o lift_sim_A

lift_sim_A_debug: lift_sim_A.c lift.h buffer.o
	$(CC) $(CFLAGS) $(LDFLAGS) buffer.o lift_sim_A.c -DDEBUG -o lift_sim_A

buffer.o: buffer.c buffer.h lift.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c buffer.c

clean:
	rm -rf *.o lift_sim_A



