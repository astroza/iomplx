CC=gcc
CFLAGS=-g -Wall
INCLUDE=-I.
UQUEUE=KQUEUE

all: dlist.o mplx3.o kqueue.o demo.o
	$(CC) $? -o demo -lpthread

dlist.o: dlist.c
	$(CC) $? -c $(CFLAGS) $(INCLUDE)

mplx3.o: mplx3.c
	$(CC) -D$(UQUEUE) $? -c -g $(CFLAGS) $(INCLUDE)

kqueue.o: backend/kqueue.c
	$(CC) -D$(UQUEUE) $? -c -g $(CFLAGS) $(INCLUDE)

demo.o: demo.c
	$(CC) -D$(UQUEUE) $? -c -g $(CFLAGS) $(INCLUDE)

clean:
	rm -f *.o demo
