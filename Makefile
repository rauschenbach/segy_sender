PROGNAME = sender
CC = gcc



CFLAGS = -o0 -ggdb3 ${CMP_OPTS}
###CFLAGS = -oS
LDFLAG = -D_REENTERANT -lgps -lpthread 

OBJS = circbuf.o log.o nmea.o proto.o sender.o start.o utils.o mythreads.o
C_FILES = circbuf.c log.c nmea.c proto.c sender.c start.c utils.c mythreads.c

TMPFILES =  *.c~ *.h~

all: ${PROGNAME}

${PROGNAME}: ${OBJS} Makefile
	${CC} -o ${PROGNAME} ${OBJS} ${LDFLAG}

clean:
	@rm -f $(subst /,\,${OBJS}) ${PROGNAME} *.*~

%.o: %.c 
	${CC} ${CFLAGS} -c $^ -o $@


.PHONY : clean
