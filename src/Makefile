CC=gcc
INC=-I../Stardis-0.1.0-GNU-Linux64/include
LDFLAGS=-lsdis -lsstl -lrsys -L../Stardis-0.1.0-GNU-Linux64/lib -Wl,-rpath=../Stardis-0.1.0-GNU-Linux64/lib
SRC=main.c stardis-app.c stardis-compute.c
DEPS=args.h stardis-app.h

all: stardis-app 

stardis-app: $(SRC) $(DEPS)
	$(CC) -o stardis-app $(SRC) $(INC) $(LDFLAGS)

debug: main.c
	$(CC) -g -o stardis-app-dbg $(SRC) $(INC) $(LDFLAGS)

clean:
	rm -rf stardis-app stardis-app-dbg
