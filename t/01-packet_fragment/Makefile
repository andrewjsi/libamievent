CC = gcc
# Wall-t visszakapcsolni!
CFLAGS = -Wall -ggdb -I../../../libc-jss -I../../..

LIBS = -lev -lm
OBJ=test.o ../../../libc-jss/netsocket.o ../../../libc-jss/logger.o ../../../libc-jss/misc.o
BIN=test

all: clean compile

compile: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LIBS)

clean:
	rm -rf $(OBJ) $(BIN) core


