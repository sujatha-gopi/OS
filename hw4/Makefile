CC	 = gcc
CFLAGS	= -g -Wall
#CFLAGS  = -DDEBUG
BINARY	 = binary
OSS	= oss
BINARY_OBJ	= binary.o
OSS_OBJ	= oss.o
HEADER = oss.h
#LINK             = -lm -lpthread
LINK	= -lm
TARGETS		= $(OSS) $(BINARY)


.SUFFIXES: .c .o

all:	$(TARGETS)

$(BIN_ADDR): $(BIN_ADDR_OBJ) $(HEADER)
	$(CC) -o $@ $(BIN_ADDR_OBJ) $(LINK)

$(OSS): $(OSS_OBJ) $(HEADER)
	$(CC) -o $@ $(OSS_OBJ) $(LINK)

.c.o:
	$(CC) -c $(CFLAGS) $<

#$(SERVER_OBJ): defs.h

.PHONY: clean
clean:
	rm -rf $(TARGETS) *.o *~

