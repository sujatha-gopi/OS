CC	 = gcc
CFLAGS	= -g -Wall
#CFLAGS  = -DDEBUG
USER	 = user
OSS	= oss
USER_OBJ	= user.o
OSS_OBJ	= oss.o
HEADER = oss.h
#LINK             = -lm -lpthread
LINK	= -lm
TARGETS		= $(OSS) $(USER)


.SUFFIXES: .c .o

all:	$(TARGETS)

$(USER): $(USER_OBJ) $(HEADER)
	$(CC) -o $@ $(USER_OBJ) $(LINK)

$(OSS): $(OSS_OBJ) $(HEADER)
	$(CC) -o $@ $(OSS_OBJ) $(LINK)

.c.o:
	$(CC) -c $(CFLAGS) $<

#$(SERVER_OBJ): defs.h

.PHONY: clean
clean:
	rm -rf $(TARGETS) *.o *~

