CC	= gcc
CFLAGS	= -Wall -O2 -fomit-frame-pointer -D_GNU_SOURCE
LDFLAGS	= -s
PROGS   = runlevel mountpoint

all: $(PROGS)

runlevel:	runlevel.o
		$(CC) $(LDFLAGS) -o $@ runlevel.o

mountpoint:	mountpoint.o
		$(CC) $(LDFLAGS) -o $@ mountpoint.o

clean:
	rm $(PROGS)
	rm *.o
