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
#include "nmea.h"
#include "log.h"

#define HOSTNAME	"localhost"
#define PORTNUM		"2947"


static struct gps_data_t gps_data_struct;
static RTC_TIME_COORD rtc_data;


/* Запускаем поток получения даных */
static int nmea_open_stream(void)
{
    int ret = -1;
    ret = gps_open(HOSTNAME, PORTNUM, &gps_data_struct);

    if (ret == 0)
	(void) gps_stream(&gps_data_struct, WATCH_ENABLE | WATCH_JSON, NULL);

    PrintCommandResult("nmea init", ret);
    return ret;
}


/* закрыть поток */
static void nmea_close_stream(void)
{
    (void) gps_stream(&gps_data_struct, WATCH_DISABLE, NULL);
    (void) gps_close(&gps_data_struct);
}


/* Если поток потерялся */
static void nmea_recon_stream(void)
{
    // nmea_close_stream();
    nmea_open_stream();
}


static void nmea_set_false_time(void)
{
    gps_data_struct.fix.time = get_sec_ticks();
    gps_data_struct.status = 0;
    gps_data_struct.fix.latitude = 0;
    gps_data_struct.fix.longitude = 0;
}



/* функция чтения из порта NMEA - параметр не м.б. NULL */
void *nmea_read_port_func(void *par)
{
    char byte;
    char str[128], str0[128];
    int t0, t1 = 0;
    int ret_code = EXIT_FAILURE;

    do {
	if (nmea_open_stream() < 0) {
	    break;
	}

	/* Чтение и разбор данных  */
	while (!is_end_thread()) {

	    /* Put this in a loop with a call to a high resolution sleep () in it. */
	    if (gps_waiting(&gps_data_struct, 5)) {
		errno = 0;

		if (gps_read(&gps_data_struct) == -1) {
		    nmea_set_false_time();

		    // Перезапустим раз в 10 секунд
		    t0 = get_msec_ticks() / 10000;
		    if (t0 != t1) {
			t1 = t0;
			printf("GPS read data error\n");
		    }

		} else {
		    /* Display data from the GPS receiver. */
		    if (gps_data_struct.set & TIME_SET) {
			rtc_data.time = (s32) gps_data_struct.fix.time;

			/* if we can get coordinates */
			if (gps_data_struct.fix.mode > 1) {
			    rtc_data.lat = (s32) (gps_data_struct.fix.latitude * 10000);
			    rtc_data.lon = (s32) (gps_data_struct.fix.longitude * 10000);
			    rtc_data.check = true;
			} else {
			    rtc_data.lat = 0;
			    rtc_data.lon = 0;
			    rtc_data.check = false;
			}
/*
			t0 = rtc_data.time;
			if (t0 != t1) {
			    sec_to_str(t0, str);
			    printf("%s: fix: %d, sat: %d\n", str, gps_data_struct.status, gps_data_struct.satellites_used);
			    t1 = t0;
			}
*/
		    }
		}
	    }
	}

	nmea_close_stream();
	ret_code = EXIT_SUCCESS;
    } while (0);
    log_write_log_file("INFO: exit nmea thread\n");
    pthread_exit(&ret_code);
}


/* Параметры NMEA */
void nmea_get_param(void *par)
{
    if (par != NULL) {
	memcpy((u8 *) par, &rtc_data, sizeof(RTC_TIME_COORD));
    }
}
