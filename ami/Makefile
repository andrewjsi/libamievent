CC = gcc
# Wall-t visszakapcsolni!
CFLAGS = -ggdb -I../libc-jss

LIBS = -lev -lm
OBJ=test_ami.o ami.o ../libc-jss/netsocket.o ../libc-jss/logger.o
BIN=test_ami

all: clean compile

compile: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LIBS)

clean:
	rm -rf $(OBJ) $(BIN) core


