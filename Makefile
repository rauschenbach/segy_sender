PROGNAME = sender
CC = gcc



CFLAGS = -o0 -ggdb3 ${CMP_OPTS} -DTEST_NMEA_TIME
###CFLAGS = -oS
LDFLAG = -D_REENTERANT -lgps -lpthread 

OBJS = circbuf.o log.o nmea.o proto.o rtc.o sender.o start.o utils.o mythreads.o
C_FILES = circbuf.c log.c nmea.c proto.c rtc.o sender.c start.c time.o utils.c mythreads.c

TMPFILES =  *.c~ *.h~

all: ${PROGNAME}

${PROGNAME}: ${OBJS} Makefile
	${CC} -o ${PROGNAME} ${OBJS} ${LDFLAG}

clean:
	@rm -f $(subst /,\,${OBJS}) ${PROGNAME} *.*~

%.o: %.c 
	${CC} ${CFLAGS} -c $^ -o $@


.PHONY : clean
