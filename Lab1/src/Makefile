NAME_SERVER = application
SRC_SERVER = application.c writenoncanonical.c noncanonical.c common.c
OBJ_SERVER = ${SRC_SERVER:.c=.o}

# compiler and linker
CC = gcc
# flags
CFLAGS = -Wall

all: rcom

.c.o:
	${CC} -c ${CFLAGS} $<

rcom: ${OBJ_SERVER}
	${CC} ${CFLAGS} -o ${NAME_SERVER} ${OBJ_SERVER}

clean:
	rm -f ${NAME_SERVER} ${OBJ_SERVER}

.PHONY: all clean rcom