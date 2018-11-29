/* Сервисные функции: ведение лога, подстройка таймера, запись на SD карту */
#define _GNU_SOURCE
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <byteswap.h>
#include "proto.h"
#include "utils.h"
#include "globdefs.h"
#include "sender.h"


#define	  	MAX_TIME_STRLEN		26	/* Длина строки со временем  */
#define   	MAX_FILE_NAME_LEN	32	/* Длина файла включая имя директории с '\0' */
#define		BYTES_IN_DATA		3
#define		SHIFT_BITES		8
/*************************************************************************************
 *     Эти переменные не видимы в других файлах 
 *************************************************************************************/
static ADC_HEADER adc_hdr;	/* Заголовок перед данными АЦП */
static char *prefix = "/home/pub";	/* Начальный каталог */
static char dir_name[32];	/* Где создаем файлы */
static int log_fd = -1;		/* FD лог файла  */
static int adc_fd = -1;		/* FD файла данных */


/***************************************************************************
 * Открыть файл регистрации-пока примитивный разбор,
 * Создаем папку с названием времени запуска
***************************************************************************/
int log_create_work_dir(s32 sec)
{
    DIR *dir;
    char str[128];
    int ret = -1, x;
    TIME_DATE td;
    char sym;

    sym = get_log_file_sym();

    do {
	sec_to_td(sec, &td);

	/* Сначала пытаемся открыть директории, если она открывается - значит существует */
	for (x = 0; x < 100; x++) {
	    sprintf(dir_name, "%s/%c%02d%02d%02d%02d", prefix, sym, ((td.year - 2000) > 0) ? (td.year - 2000) % 99 : 0, td.mon % 99, td.day % 99, x % 99);

	    /* Open the directory */
	    dir = opendir(dir_name);
	    if (dir == NULL) {
		break;		/* такого названия пока нет */
	    }
	    closedir(dir);
	}

	/* Уже много директорий */
	if (x >= 99) {
	    perror("opendir");
	    break;
	}

	/* Создать папку с названием: число и номер запуска - всем можно смотреть че там внутри */
	ret = mkdir(dir_name, 0777);
	if (ret) {
	    perror("mkdir");
	    break;
	}

	ret = 0;
    } while (0);


    return ret;			/* Все получили - успех или неудача! */
}

/****************************************************************
 * Открыть старый лог-файл. 0 - при успехе
 ***************************************************************/
int log_check_old_log(void)
{
    char str0[128];
    char str1[128];
    int res = -1;
    char sym;

    sym = get_log_file_sym();

   /* Откроем старый */
    sprintf(str0, "%s/gns-110%c.log", prefix, sym);
    sprintf(str1, "%s/gns-110%c.old", prefix, sym);
	
    res = rename(str0, str1);
    if(res == 0)
	printf("save old log as %s\n", str1);
    return res;
}


/****************************************************************
 * Открыть лог-файл. 0 - при успехе
 * Открыть файл лога для добавления в директории PUB всегда, 
 * создать если нет!чужим нет доступа на "w" 
 ***************************************************************/
int log_open_log_file(void)
{
    char str[128];
    int res = -1;
    int rate;
    char sym;

    sym = get_log_file_sym();
  
    do {

       log_check_old_log();


	sprintf(str, "%s/gns-110%c.log", prefix, sym);
	log_fd = open(str, O_TRUNC | O_CREAT | O_RDWR, 0664);
	if (log_fd < 0) {
	    perror("open log");
	    break;
	}
	res = 0;
    } while (0);
    return res;
}

/****************************************************************
 * Закрыть лог-файл. 0 при успехе
 ***************************************************************/
int log_close_log_file(void)
{
    int res;
    res = close(log_fd);
    if (res == 0) {
	/* Закрылось? */
	log_fd = -1;
    }
    return res;
}


/****************************************************************
 * Записать в лог-файл. 0 - при успехе
 ***************************************************************/
int log_write_log_file(char *fmt, ...)
{
    char str[256];
    int res;			/* Result code */
    unsigned bw;		/* Прочитано или записано байт  */
    int i, n, ret = -1;
    va_list p_vargs;		/* return value from vsnprintf  */

    do {
	/* Не монтировано (нет фс - работаем от PC), или с карточкой проблемы */
	if (log_fd <= 0) {
	    break;
	}

	/* Получаем текущее время - MAX_TIME_STRLEN символов с пробелом - всегда пишем */
	time_to_str(str);
	va_start(p_vargs, fmt);
	i = vsnprintf(str + MAX_TIME_STRLEN, sizeof(str), fmt, p_vargs);

	va_end(p_vargs);
	if (i < 0) {		/* formatting error?            */
	    printf("format error\n");
	    break;
	}

	/* Пишем в лог  */
	n = write(log_fd, str, strlen(str));
	if (n < 0) {
	    printf("write log error\n");
	    break;
	}
	/* Отступим и на экран */
	printf("%s", str + MAX_TIME_STRLEN);
	ret = 0;
    } while (0);

    return ret;
}



/****************************************************************
 * Записать в лог-файл. 0 - при успехе
 ***************************************************************/
int log_write_error_to_log(char *fmt, ...)
{
    char str[256];
    int res;			/* Result code */
    unsigned bw;		/* Прочитано или записано байт  */
    int i, n, ret = -1;
    va_list p_vargs;		/* return value from vsnprintf  */

    do {
	/* Не монтировано (нет фс - работаем от PC), или с карточкой проблемы */
	if (log_fd <= 0) {
	    break;
	}

	/* Получаем текущее время - MAX_TIME_STRLEN символов с пробелом - всегда пишем */
	va_start(p_vargs, fmt);
	i = vsnprintf(str, sizeof(str), fmt, p_vargs);
	va_end(p_vargs);

	if (i < 0) {		/* formatting error?            */
	    printf("format error\n");
	    break;
	}

	/* Пишем в лог  */
	n = write(log_fd, str, strlen(str));
	if (n < 0) {
	    printf("write log error\n");
	    break;
	}
	/* Отступим и на экран */
	printf("%s", str + MAX_TIME_STRLEN);
	ret = 0;
    } while (0);

    return ret;
}


/*********************************************************************
 * Создаем заголовок - он будет использоваться для файла данных 
 * Вызывается из файла main
 * структуру будем переделывать!!!
 *********************************************************************/
void log_create_adc_header(START_ADC_STRUCT* start, DEV_STATUS_STRUCT * status)
{
    char str[MAX_TIME_STRLEN];	
    u32 block;
    u8 sps;

    do {
	if (start == NULL || status == NULL)
	    break;

	sps = get_adc_freq(start->freq);

	adc_hdr.ConfigWord = sps + 1;	/* 8 миллисекунд (частота 125 Гц + 3 байт */
	adc_hdr.ChannelBitMap = 0x0f;	/* Будет 1.2.3.4 канала */
	adc_hdr.SampleBytes = 12;	/* Размер 1 сампла со всех работающих АЦП сразу */
	block = 7500 * (1 << sps);	/* Столько самплов будет за 1 минуту (SPSxxx * 60) */

#if defined		ENABLE_NEW_SIVY
	strncpy(adc_hdr.DataHeader, "SeismicDat1\0", 12);	/* Заголовок данных SeismicDat1\0 - новый Sivy */
	adc_hdr.HeaderSize = sizeof(ADC_HEADER);	/* Размер заголовка     */
	adc_hdr.GPSTime = status->time;	/* Время синхронизации: наносекунды */
	adc_hdr.Drift = status->drift;	/* Дрифт от точных часов GPS: наносекунды  */
	adc_hdr.lat = status->lat;	/* широта: 55417872 = 5541.7872N */
	adc_hdr.lon = status->lon;	/* долгота:37213760 = 3721.3760E */
	adc_hdr.BlockSamples = block;	/* В новом Sivy размер блока  - 4 байт */
#else
	TIME_DATE data;
	long t0;
	u8 ind;

	strncpy(adc_hdr.DataHeader, "SeismicData\0", 12);	/* Дата ID  */
	adc_hdr.HeaderSize = sizeof(ADC_HEADER);	/* Размер заголовка     */

	/* секунды */
	t0 = status->nmea_time;
	sec_to_td(t0, &data);	/* время синхронизации сюда */
	memcpy(&adc_hdr.GPSTime, &data, sizeof(TIME_DATE));	/* Время получения дрифта по GPS */

	t0 = status->prtc_time;
	sec_to_td(t0, &data);	/* Sedis time */
	memcpy(&adc_hdr.SedisTime, &data, sizeof(TIME_DATE));

	adc_hdr.NumberSV = 3;	/* Число спутников: пусть будет 3 */
	adc_hdr.params.coord.comp = true;	/* Строка компаса  */

	/* Координаты чудес - где мы находимся */
	if (!status->lat && !status->lon)
	    ind = 90;
	else
	    ind = 12;		/* +554177+03721340000009 */

	/* Исправлено странное копирование */
	snprintf(str, sizeof(str), "%c%06d%c%07d%06d%02d",
		 (status->lat >= 0) ? '+' : '-', abs(status->lat / 100),
		 (status->lon >= 0) ? '+' : '-', abs(status->lon / 100), 0, ind);
	memcpy(adc_hdr.params.coord.pos, str, sizeof(adc_hdr.params.coord.pos));
	adc_hdr.BlockSamples = block & 0xffff;
	adc_hdr.params.coord.blk_hi = (block >> 16) & 0xff;
	adc_hdr.Rev = 2;	/* Номер ревизии д.б. 2 */

	/* Дрифт в наносекундах */
	adc_hdr.drift_lo = status->drift & 0xffff;
	adc_hdr.drift_hi = (status->drift >> 16) & 0xffff;	

#endif
	log_write_log_file("INFO: Create ADS1282 header OK\n");
    } while (0);
}


/************************************************************************
 * Для изменения заголовка. Подсчитать координаты и напряжения.
 * Заполняется по приеме статуса
 ************************************************************************/
void log_change_adc_header(void *p)
{
    DEV_STATUS_STRUCT *status;

    if (p != NULL) {
	status = (DEV_STATUS_STRUCT *) p;

#if defined		ENABLE_NEW_SIVY
	adc_hdr.u_pow = status->regpwr_volt;	/* Напряжение питания, U mv */
	adc_hdr.i_pow = status->ireg_sense;	/* Ток питания, U ma */
	adc_hdr.u_mod = status->am_power_volt;	/* Напряжение модема, U mv */
	adc_hdr.i_mod = status->iam_sense;	/* Ток модема, U ma */

	adc_hdr.t_reg = status->temper0;	/* Температура регистратора, десятые доли градуса */
	adc_hdr.t_sp = status->temper1;	/* Температура внешней платы, десятые доли градуса */
	adc_hdr.p_reg = status->press;	/* Давление внутри сферы */
	adc_hdr.pitch = status->pitch;
	adc_hdr.roll = status->roll;
	adc_hdr.head = status->head;
#else
	/* Пересчитываем по формулам DataFormatProposal */
	adc_hdr.Bat = (int) status->regpwr_volt * 1024 / 50000;	/* Будем считать что батарея 12 вольт */
	adc_hdr.Temp = (((int) status->temper1 + 600) * 1024 / 5000);	/* Будем считать что температура * 10 */

	/* Дрифт в наносекундах */
	adc_hdr.drift_lo = status->drift & 0xffff;
	adc_hdr.drift_hi = (status->drift >> 16) & 0xffff;	

#endif
    }
}


/****************************************************************************
 * Создавать заголовок каждый час, далее - в нем будет изменяться только время
 * Здесь же открыть файл для записи значений полученный с АЦП
 * Профилировать эту функцию!!!!! 
 ****************************************************************************/
int log_create_hour_data_file(u64 ns)
{
    char name[32];		/* Имя файла */
    TIME_DATE date;
    int res = -1;		/* Result code */
    u32 sec = ns / TIMER_NS_DIVIDER;

    do {
	/* Если уже есть окрытый файл - закроем его и создадим новый  */
	if (adc_fd > 0) {
	    res = close(adc_fd);
	    if (res != 0) {
		log_write_log_file("error close data file!\n");
		break;
	    }
	    adc_fd = -1;
	}

	if(sec < 100) { /* плохое время! */
    	    log_write_log_file("Bad time for file name!\n");
    	    sec = get_sec_ticks();
	}


	/* Получили время в нашем формате - это нужно для названия */
	if (sec_to_td(sec, &date) != -1) {

	    /* Название файла по времени: год-месяц-день-часы.минуты */
	    snprintf(name, MAX_FILE_NAME_LEN, "%s/%02d%02d%02d%02d.%02d",
		     dir_name, ((date.year - 2000) > 0) ? (date.year - 2000) : 0, date.month, date.day, date.hour, date.min);


	    log_write_log_file("open new adc file name: %s\n", name);
	    /* Откроем новый */
	    adc_fd = open(name, O_CREAT | O_RDWR, 0664);
	    if (adc_fd < 0) {
		break;
	    }
	    res = 0;		/* Все OK */
	}

    } while (0);
    return res;
}

/*********************************************************************
 * Закрыть файл АЦП-перед этим сбросим буферы на диск 
 ********************************************************************/
int log_close_data_file(void)
{
    int res = -1;		/* Result code */

    do {
	if (adc_fd < 0)		/* нет файла еще */
	    break;

	/* Обязательно запишем */
	res = close(adc_fd);
	if (res) {
	    break;
	}
	adc_fd = -1;
	res = 0;
    } while (0);
    return res;			/* Все нормально! */
}


/*********************************************************************************
 * Подготовить и сбросывать каждую минуту заголовок на SD карту и  делать F_SYNC!
 * В заголовке меняем только время 
 * Профилировать эту функцию!!!!! 
 ********************************************************************************/
int log_write_adc_header_to_file(u64 ns)
{
    TIME_DATE date;
    unsigned bw;		/* Прочитано или записано байт  */
    int res = -1;		/* Result code */
    

    do {
#if defined		ENABLE_NEW_SIVY
	adc_hdr.SampleTime = ns;	/* Время сампла, наносекунды */
#else
	u64 msec = ns / TIMER_US_DIVIDER;
        s32 sec;

	sec_to_td(msec / TIMER_MS_DIVIDER, &date);	/* Получили секунды и время в нашем формате */
	memcpy(&adc_hdr.SampleTime, &date, sizeof(TIME_DATE));	/* Записали время начала записи данных */


	sec = get_sec_ticks(); /* время компьютера */
	sec_to_td(sec, &date);	/* Получили секунды и время в нашем формате */
      	memcpy(&adc_hdr.SedisTime, &date, sizeof(TIME_DATE));	/* Пишем миллисекунды в резервированные места */


#endif

	/* Скинем в файл заголовок */
	res = write(adc_fd, &adc_hdr, sizeof(ADC_HEADER));
	if (res < 0) {
	    break;
	}

	/* Обязательно запишем! не потеряем файл если выдернем SD карту */
        syncfs(adc_fd);
	res = 0;
    } while (0);
    return res;
}


/***********************************************************************
 * Запись в файл данных АЦП: данные и размер в байтах
 * Sync делаем раз в минуту!
 * Профилировать эту функцию!!!!! 
 ***********************************************************************/
int log_write_adc_data_to_file(void *data, int len)
{
    unsigned bw;		/* Прочитано или записано байт  */
    int res = -1;		/* Result code */
    do {
	res = write(adc_fd, (char *) data, len);
	if (res < 0) {
	    break;
	}
	res = 0;
    } while (0);
    return res;			/* Записали OK */
}
