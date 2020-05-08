CC = gcc 
CFLAGS = -Wall -ansi -pedantic -g -fstack-protector
LDFLAGS = -pthread -D_DEFAULT_SOURCE
LDLIBS = -lm 


lift_sim_B: lift_sim_B.o fifo_buf.o
	$(CC) $(CFLAGS) $(LDFLAGS) fifo_buf.o lift_sim_B.o -o lift_sim_B 

lift_sim_A: lift_sim_A.o fifo_buf.o
	$(CC) $(CFLAGS) $(LDFLAGS) fifo_buf.o lift_sim_A.o -o lift_sim_A

lift_sim_B.o: lift_sim_B.c lift.h fifo_buf.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c lift_sim_B.c

lift_sim_A.o: lift_sim_A.c lift.h fifo_buf.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c lift_sim_A.c

fifo_buf.o: fifo_buf.c fifo_buf.h lift.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c fifo_buf.c

clean:
	rm -rf *.o lift_sim_A lift_sim_B

