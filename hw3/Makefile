CC	 = gcc
CFLAGS	= -g -Wall
#CFLAGS  = -DDEBUG
BIN_ADDR	 = bin_adder
MASTER		= master
BIN_ADDR_OBJ	= bin_adder.o
MASTER_OBJ	= master.o
LINK             = -lm -lpthread
TARGETS		= $(MASTER) $(BIN_ADDR)

.SUFFIXES: .c .o

all:	$(TARGETS)

$(BIN_ADDR): $(BIN_ADDR_OBJ)
	$(CC) -o $@ $(BIN_ADDR_OBJ) $(LINK)

$(MASTER): $(MASTER_OBJ)
	$(CC) -o $@ $(MASTER_OBJ) $(LINK)

.c.o:
	$(CC) -c $(CFLAGS) $<

#$(SERVER_OBJ): defs.h

.PHONY: clean
clean:
	rm -rf $(TARGETS) *.o *~

