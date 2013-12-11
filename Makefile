CC = gcc
# Wall-t visszakapcsolni!
CFLAGS = -Wall -ggdb -Ilibc-jss

LIBS = -lev -lm
OBJ=ami.o originate.o libc-jss/netsocket.o libc-jss/logger.o libc-jss/misc.o

all: clean compile

compile: $(OBJ) test_ami.o test_ami2.o ori_test.o amilog.o volcon.o meetmetalk.o cel.o
	$(CC) $(CFLAGS) -o test_ami test_ami.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o test_ami2 test_ami2.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o ori_test ori_test.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o cel cel.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o amilog amilog.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o volcon volcon.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o meetmetalk meetmetalk.o $(OBJ) $(LIBS)

clean:
	rm -rf $(OBJ) test_ami.o test_ami2.o ori_test.o amilog.o cel.o test_ami test_ami2 ori_test core amilog volcon meetmetalk cel

volcon.o: volcon.c volcon_config.h

install: compile
	install -s volcon /usr/local/bin

