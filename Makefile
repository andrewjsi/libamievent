CC = gcc
# Wall-t visszakapcsolni!
CFLAGS = -Wall -ggdb -I../libc-jss

LIBS = -lev -lm
OBJ=ami.o originate.o ../libc-jss/netsocket.o ../libc-jss/logger.o ../libc-jss/misc.o

all: clean compile

compile: $(OBJ) test_ami.o calltrack.o amilog.o volcon.o
	$(CC) $(CFLAGS) -o test_ami test_ami.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o calltrack calltrack.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o amilog amilog.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o volcon volcon.o $(OBJ) $(LIBS)

clean:
	rm -rf $(OBJ) test_ami.o calltrack.o amilog.o test_ami calltrack core amilog volcon

volcon.o: volcon.c volcon_config.h

install: compile
	install -s volcon /usr/local/bin

