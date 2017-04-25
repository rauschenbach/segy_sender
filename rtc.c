#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <gps.h>
#include "start.h"
#include "utils.h"
#include "rtc.h"
#include "log.h"


static RTC_TIME_COORD rtc_data;



/* функция чтения времени - параметр не м.б. NULL */
void *rtc_read_port_func(void *par)
{
    char byte;
    char str[128], str0[128];
    int t0 = 0, t1;
    int ret_code = EXIT_FAILURE;

    do {

	/* Чтение и разбор данных  */
	while (!is_end_thread()) {

	    t1 = (s32) get_sec_ticks();

	    /* if we can get coordinates */
	    if (t1 != t0) {
		t0 = t1;
		rtc_data.lon = (s32) (37.617 * 10000);
		rtc_data.lat = (s32) (55.755 * 10000);
  	        rtc_data.time = t1;
		rtc_data.check = true;
	    }
	}

	ret_code = EXIT_SUCCESS;
    } while (0);
    log_write_log_file("INFO: exit time thread\n");
    pthread_exit(&ret_code);
}


/* Параметры NMEA */
void rtc_get_param(void *par)
{
    if (par != NULL) {
	memcpy((u8 *) par, &rtc_data, sizeof(RTC_TIME_COORD));
    }
}
