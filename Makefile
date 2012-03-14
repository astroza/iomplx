CC=gcc
CFLAGS=-Wall -O3
INCLUDE=-I.

SRC= \
    dlist.c \
    iomplx.c \
    iomplx_inet.c

UQUEUE=NOUQUEUE

ifneq ($(findstring /usr/include/sys/epoll.h, $(wildcard /usr/include/sys/*.h)), )
	UQUEUE=EPOLL
endif

ifneq ($(findstring /usr/include/sys/event.h, $(wildcard /usr/include/sys/*.h)), )
	UQUEUE=KQUEUE
endif

UQUEUE_SRC=backend/$(shell echo $(UQUEUE) | tr A-Z a-z).c
SRC+=$(UQUEUE_SRC)

OBJ=$(SRC:.c=.o)
all: prepare lib
	@echo "Done"

prepare:
	mkdir -p objs/backend

lib: $(OBJ)
	cd objs; $(CC) -shared -fPIC $^ -o ../iomplx.so -lpthread

%.o: %.c
	$(CC) -fPIC -D$(UQUEUE) $? -c $(CFLAGS) $(INCLUDE) -o objs/$@

example: prepare lib
	$(CC) -D$(UQUEUE) example.c $(CFLAGS) $(INCLUDE) $(PWD)/iomplx.so -o $@

clean:
	rm -fr objs iomplx.so example
