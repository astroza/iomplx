CC=gcc
CFLAGS=-g -Wall
INCLUDE=-I.

SRC=example.c \
    dlist.c \
    mplx3.c

ifneq ($(findstring /usr/include/sys/epoll.h, $(wildcard /usr/include/sys/*.h)), )
	UQUEUE=EPOLL
endif

ifneq ($(findstring /usr/include/sys/event.h, $(wildcard /usr/include/sys/*.h)), )
	UQUEUE=KQUEUE
endif

UQUEUE_SRC=backend/$(shell echo $(UQUEUE) | tr A-Z a-z).c
SRC+=$(UQUEUE_SRC)

OBJ=$(SRC:.c=.o)
all: $(OBJ)
	$(CC) $? -o example -lpthread
%.o: %.c
	$(CC) -D$(UQUEUE) $(include) $? -c -g $(CFLAGS) $(INCLUDE)

clean:
	rm -f *.o example
