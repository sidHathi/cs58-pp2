.SUFFIXES: .c

SRCS = ledyard.c
OBJS = $(SRCS:.c=.o)
OUTPUT = ledyard

all: ledyard

ledyard: ledyard.c
	gcc -std=c99 -Wall -g -o ledyard ledyard.c -pthread

clean:
	rm -f $(OBJS) $(OUTPUT)

depend:
	makedepend -I/usr/local/include/g++ -- $(CFLAGS) -- $(SRCS) 

# DO NOT DELETE