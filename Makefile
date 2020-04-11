CC = gcc 
CFLAGS = -Wall -ansi -pedantic -g -Werror
LDLIBS = -pthread -D _DEFAULT_SOURCE


lift_sim_A: lift_sim_A.c lift.h buffer.h buffer.o
	$(CC) $(CFLAGS) $(LDLIBS) buffer.o lift_sim_A.c -o lift_sim_A

lift_sim_A_debug: lift_sim_A.c lift.h buffer.o
	$(CC) $(CFLAGS) $(LDLIBS) buffer.o lift_sim_A.c -DDEBUG=1 -o lift_sim_A

buffer.o: buffer.c buffer.h lift.h
	$(CC) $(CFLAGS) $(LDLIBS) -c buffer.c

clean:
	rm -rf *.o lift_sim_A



