CC=gcc
CFLAGS=-c -g -Wall -DLINUX -D_GNU_SOURCE -DCLIENT -DUSE_SELECT -D_LINUX_ -DENABLE_P2PLOG
LDFLAGS=-lpthread -lrt
SOURCES=utils/list.c \
		utils/udptransport.c \
		utils/p2pcommon.c \
		utils/mytimer.c \
	    main.c
        
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=nettest

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(EXECUTABLE) $(OBJECTS) *.log *.user *.suo

