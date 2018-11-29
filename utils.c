//---------------------------------------------------------------------------
// Различные утилиты, коорые исполльзуются различными модулями
#define _XOPEN_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "utils.h"
#include "log.h"
#include "globdefs.h"

/* Значение частоты для запуска */
static u32 adc_freq_arr[] = { 125, 250, 500, 1000, 2000, 4000 };

/* Значения PGA для запуска АЦП */
static u32 adc_pga_arr[] = { 1, 2, 4, 8, 16, 32, 64 };

/* Получить номер в массиве */
int get_adc_freq(int freq)
{
    int i;
    int res = -1;

    for (i = 0; i < sizeof(adc_freq_arr) / sizeof(u32); i++) {
	if (adc_freq_arr[i] == freq) {
	    res = i;
	    break;
	}
    }
    return res;
}


/* Получить номер в массиве PGA */
int get_adc_pga(int pga)
{
    int i;
    int res = -1;

    for (i = 0; i < sizeof(adc_pga_arr) / sizeof(u32); i++) {
	if (adc_pga_arr[i] == pga) {
	    res = i;
	    break;
	}
    }
    return res;
}


/*
  Name  : CRC-16
  Poly  : 0x8005    x^16 + x^15 + x^2 + 1
  Init  : 0xFFFF
  Revert: true
  XorOut: 0x0000
  Check : 0x4B37 ("123456789")
  MaxLen: 4095 байт (32767 бит) - обнаружение
  одинарных, двойных, тройных и всех нечетных ошибок
*/
#pragma pack(push, 2)
static const u16 Crc16Table[256] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
    0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
    0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
    0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
    0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
    0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
    0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
    0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
    0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
    0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
    0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
    0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
    0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
    0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
    0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
    0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
    0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
    0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
    0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
    0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
    0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
    0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
    0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
    0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
    0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
    0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
    0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
    0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
    0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
    0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
    0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
    0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

#pragma pack(pop)
//---------------------------------------------------------------------------
// Проверка контрольной суммы
// mes - pointer to NA-01 reply
u16 test_crc16(u8 * mes, int size)
{
    u8 ind;
    u16 len, i, j = 0;
    u16 crc = 0;		// returns 0 = OK; !0 = wrong   
    int res = 0;


    do {
	len = (mes[0] | ((u16) mes[1] << 8)) + 4;

	// заплатка! если смещается заголовок   
	if ((size == sizeof(DEV_STATUS_STRUCT) || size == sizeof(ADS1282_PACK_STRUCT)) && len != size) {

	    /* Пройдем по первым 10 байтам заголовка */
	    for (i = 0; i < 10; i++) {
		len = (mes[i] | ((u16) mes[i + 1] << 8)) + 4;

		if (len == size) {
		    memmove(mes, mes + i, size - i);
		    j = 1;
		    if (i > 1)
			log_write_log_file("WARN: move header from pos %d: len %d, size %d!\n", i, len, size);

		    break;
		}
	    }
	    // did not move
	    if(size == sizeof(ADS1282_PACK_STRUCT) && j == 0)
		return -1;

	    *(u16 *) mes = size - 4;
	    return 0;
	}

	for (i = 0; i < size; i++) {
	    ind = (u8) ((crc >> 8) & 0xff);
	    crc = (u16) (((crc & 0xff) << 8) | (mes[i] & 0xff));
	    crc ^= Crc16Table[ind];
	}
    } while (0);

    return crc & 0xffffu;
}



/*************************************************************
 * Добавляет контрольную суммы к сообщению cmd - правильное!!!
 *************************************************************/
void add_crc16(u8 * buf)
{
    int i, len, ind;
    unsigned crc = 0;
    // length - 3 байт + 5
    len = buf[2] + 5;
    buf[len - 2] = buf[len - 1] = 0;
    for (i = 0; i < len; ++i) {
	ind = (crc >> 8) & 255;
	crc = ((crc << 8) + buf[i]) ^ Crc16Table[ind];
    }

    buf[len - 1] = (u8) (crc & 0xff);
    buf[len - 2] = (u8) ((crc >> 8) & 0xff);
}

/***********************************************************************************
 *  Расчёт CRC-16 плохой как для модема :(
 ************************************************************************************/
u16 check_crc16(u8 * buf, u16 len)
{
    unsigned short crc = 0xFFFF;
    unsigned char i;
    while (len--) {
	crc ^= *buf;
	buf++;
	for (i = 0; i < 8; i++)
	    crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
    }

    return crc;
}

/*******************************************************************
 *  Секунды в строку времени
 ******************************************************************/
void sec_to_str(long ls, char *str)
{
    TIME_DATE t;
    /* Записываем дату в формате: [08-11-2012 - 08:57:22] */
    if (sec_to_td(ls, &t) != -1) {
	sprintf(str, "%02d.%02d.%04d - %02d:%02d:%02d", t.day, t.mon, t.year, t.hour, t.min, t.sec);
    } else {
	sprintf(str, "[set time error] ");
    }
}

/************************************************************
 * Для записи времени в лог - получить время
 ************************************************************/
void time_to_str(char *str)
{
    TIME_DATE t;
    s64 msec;
    char sym = 'G';
    msec = get_msec_ticks();	/* Получаем время */
    /* Записываем дату в формате: P: 09-07-2013 - 13:11:39.871  */
    if (sec_to_td(msec / TIMER_MS_DIVIDER, &t) != -1) {
	sprintf(str, "G %02d-%02d-%04d %02d:%02d:%02d.%03d ", t.day, t.mon, t.year, t.hour, t.min, t.sec, (u32) (msec % TIMER_MS_DIVIDER));
    } else {
	sprintf(str, "set time error ");
    }
}


/****************************************************************************
 * Выдать время в секундах UNIX
*****************************************************************************/
long td_to_sec(TIME_DATE * t)
{
    long r;
    char *tz;
    char buf[128];
    struct tm tm_time;


    tm_time.tm_sec = t->sec;
    tm_time.tm_min = t->min;
    tm_time.tm_hour = t->hour;
    tm_time.tm_mday = t->day;
    tm_time.tm_mon = t->mon - 1;
    tm_time.tm_year = t->year - 1900;
    tm_time.tm_wday = 0;
    tm_time.tm_yday = 0;
    tm_time.tm_isdst = 0;


    tz = getenv("TZ");
    putenv("TZ=UTC0");
    tzset();

    r = mktime(&tm_time);

    if (tz) {
	sprintf(buf, "TZ=%s", tz);
	putenv(buf);
    } else {
	putenv("TZ=");
    }
    tzset();
    return r;
}

/****************************************************************************
 * Выдать время в милисекундах
*****************************************************************************/
s64 get_msec_ticks(void)
{
    s64 res = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    res = (s64) tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return res;
}

/****************************************************************************
 * Выдать время в секундах
*****************************************************************************/
s32 get_sec_ticks(void)
{
    s32 res = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    res = (s32) tv.tv_sec;
    return res;
}

/************************************************************ 
* Время UNIX в формат DATE
**************************************************************/
int sec_to_td(long ls, TIME_DATE * t)
{
    struct tm *tim;
    if ((int) ls != -1) {
	tim = gmtime(&ls);
	t->sec = tim->tm_sec;
	t->min = tim->tm_min;
	t->hour = tim->tm_hour;
	t->day = tim->tm_mday;
	t->mon = tim->tm_mon + 1;	/* Г‚ TIME_DATE Г¬ГҐГ±ГїГ¶Г» Г± 1 ГЇГ® 12, Гў tm_time Г± 0 ГЇГ® 11 */
	t->year = tim->tm_year + 1900;	/* Г‚ tm ГЈГ®Г¤Г» Г±Г·ГЁГІГ ГѕГІГ±Гї Г± 1900 - ГЈГ®, Гў TIME_DATA Г± 0-ГЈГ® */
	return 0;
    }

    else
	return -1;
}

/*************************************************************
 * Вывод данных в HEX на экран
 *************************************************************/
void print_data_hex(u8 * data, int len)
{
    int i;
    for (i = 0; i < len; i++) {
	if (i % 8 == 0 && i != 0)
	    log_write_error_to_log(" ");
	if (i % 16 == 0 && i != 0 && i != 8)
	    log_write_error_to_log("\n");
	log_write_error_to_log("%02X ", data[i]);
    }
    log_write_error_to_log("\n");
}


/******************************************************
* Результат выполнения чего нибудь
****************************************************/
void PrintCommandResult(char *data, int err)
{
    log_write_log_file("%s: %s\n", err == 0 ? "SUCCESS" : "ERROR", data);
}


void EnterCriticalSection(void *i)
{
    pthread_mutex_lock(i);
    return;
}

void LeaveCriticalSection(void *i)
{
    pthread_mutex_unlock(i);
    return;
}
