CC = gcc
# Wall-t visszakapcsolni!
CFLAGS = -Wall -ggdb -Ilibc-jss

LIBS = -lev -lm
OBJ=ami.o originate.o libc-jss/netsocket.o libc-jss/logger.o libc-jss/misc.o

all: clean compile

compile: $(OBJ) test_ami.o test_ami2.o calltrack.o amilog.o volcon.o meetmetalk.o
	$(CC) $(CFLAGS) -o test_ami test_ami.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o test_ami2 test_ami2.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o calltrack calltrack.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o amilog amilog.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o volcon volcon.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o meetmetalk meetmetalk.o $(OBJ) $(LIBS)

clean:
	rm -rf $(OBJ) test_ami.o test_ami2.o calltrack.o amilog.o test_ami test_ami2 calltrack core amilog volcon meetmetalk

volcon.o: volcon.c volcon_config.h

install: compile
	install -s volcon /usr/local/bin

