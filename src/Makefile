CC=gcc
SDIS_SDK=../Stardis-0.3.0-GNU-Linux64

INC=-I$(SDIS_SDK)/include
LDFLAGS=-lsdis -lsstl -lrsys -ltinyexpr -lm -L$(SDIS_SDK)/lib -Wl,-rpath=$(SDIS_SDK)/lib
SRC=main.c stardis-app.c stardis-compute.c 
DEPS=args.h stardis-app.h

all: stardis-app 

stardis-app: $(SRC) $(DEPS)
	$(CC) -o stardis-app $(SRC) $(INC) $(LDFLAGS)

debug: $(SRC) $(DEPS)
	$(CC) -g -o stardis-app-dbg $(SRC) $(INC) $(LDFLAGS)

clean:
	rm -rf stardis-app stardis-app-dbg
