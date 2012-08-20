CC = gcc
# Wall-t visszakapcsolni!
CFLAGS = -ggdb -I../libc-jss

LIBS = -lev -lm
OBJ=ami.o ../libc-jss/netsocket.o ../libc-jss/logger.o ../libc-jss/misc.o

all: clean compile

compile: $(OBJ) test_ami.o calltrack.o
	$(CC) $(CFLAGS) -o test_ami test_ami.o $(OBJ) $(LIBS)
	$(CC) $(CFLAGS) -o calltrack calltrack.o $(OBJ) $(LIBS)

clean:
	rm -rf $(OBJ) test_ami.o calltrack.o test_ami calltrack core

