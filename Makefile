CC=gcc
CFLAGS=-Wall -g
include config.mk

SRC= \
    dlist.c \
    mempool.c \
    iomplx.c \
    iomplx_0.c \
    iomplx_N.c \
    iomplx_T.c \
    iomplx_inet.c

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
