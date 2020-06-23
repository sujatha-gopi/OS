# Makefile with suffix rules

CC	 = gcc
CFLAGS	= -g -Wall
TARGET	= bt
OBJS	= bt.o bfs.o
LIBOBJS	=
LIB	=
.SUFFIXES: .c .o

ALL:	$(LIB) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIB)

$(LIB): $(LIBOBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $(LIBOBJS)

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	/bin/rm -f *.o *~ $(LIB) $(TARGET)

