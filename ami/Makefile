CC = gcc
# Wall-t visszakapcsolni!
CFLAGS = -ggdb

LIBS = -lev -lm
OBJ=test_ami.o ami.o
BIN=test_ami

all: compile
#~ all: RSOE

compile: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LIBS)

clean:
	rm -rf $(OBJ) $(BIN) core

#~ storm: storm.o stormc.o netsocket.o
	#~ $(CC) $(CFLAGS) -o storm storm.o stormc.o netsocket.o packet.o $(LIBS)


