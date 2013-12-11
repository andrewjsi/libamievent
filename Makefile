CC = gcc
CFLAGS = -Wall -ggdb -Ilibc-jss
LDFLAGS = -lev -lm

OBJ=ami.o originate.o libc-jss/netsocket.o libc-jss/logger.o libc-jss/misc.o
PROGS=cel test_ami test_ami2 ori_test amilog volcon meetmetalk

.PHONY: all
all: $(patsubst %, %.o, $(PROGS)) $(OBJ) $(PROGS)

%: %.o $(OBJ)
	gcc $(CFLAGS) -o $@ $< $(OBJ) $(LDFLAGS)

clean:
	rm -f *.o $(OBJ) $(PROGS)

volcon.o: volcon.c volcon_config.h

install: compile
	install -s volcon /usr/local/bin
