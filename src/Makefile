#SDIS_SDK=../Stardis-0.3.0-GNU-Linux64
SDIS_SDK=/home/piaud/code/star-engine-stardis/local/
SDIS_SDK_DBG=/home/piaud/code/star-engine-stardis/local/

INC=-I${SDIS_SDK}/include
CFLAGS=-std=c99 -pedantic -Wall
LDFLAGS    =-lsdis     -lsstl -lrsys -ltinyexpr -lm -L${SDIS_SDK_DBG}/lib -Wl,-rpath=${SDIS_SDK_DBG}/lib
LDFLAGS_DBG=-lsdis-dbg -lsstl -lrsys -ltinyexpr -lm -L${SDIS_SDK_DBG}/lib -Wl,-rpath=${SDIS_SDK_DBG}/lib

SRC=main.c stardis-app.c stardis-compute.c 
DEPS=args.h stardis-app.h

all: stardis-app 

stardis-app: ${SRC} ${DEPS}
	${CC} -o stardis-app -O2 ${SRC} ${INC} ${CFLAGS} ${LDFLAGS}

debug: $(SRC) $(DEPS)
	${CC} -g -o stardis-app-dbg ${SRC} ${INC} ${CFLAGS} ${LDFLAGS_DBG}

clean:
	rm -rf stardis-app stardis-app-dbg
