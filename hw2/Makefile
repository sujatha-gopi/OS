CC	= gcc
CFLAGS	= -g -Wall
TARGET	= oss prime 
OBJS	= oss.o prime.o
LIBOBJS	=
LIB	=
#.SUFFIXES: .c .o

ALL:	$(LIB) $(TARGET)

$(TARGET): % : %.o
	$(CC) -o $@ $<

$(OBJS): %.o :%.c
	$(CC) -c $<

$(LIB):	$(LIBOBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $(LIBOBJS)

#.c.o:
#	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	/bin/rm -f *.o *~ $(LIB) $(TARGET)

