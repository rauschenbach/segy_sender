#define _BSD_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <netdb.h>
#include <byteswap.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "mythreads.h"
#include "circbuf.h"
#include "utils.h"
#include "my_cmd.h"
#include "start.h"
#include "nmea.h"
#include "sender.h"
#include "log.h"
#include "rtc.h"
#include "link.h"



#define 	TIMER_HSEC_DIVIDER	(500)
#define 	TIMER_SEC_DIVIDER	(1000)
#define 	TIMER_10SEC_DIVIDER	(10000)
#define 	PONG_FLAG		0
#define		PING_FLAG		1
#define		BYTES_IN_DATA		3
#define		SHIFT_BITES		8





/**********************************************************************
 * Статические пременные
 *********************************************************************/
static pthread_t read_port_thread;	/* Чтение из порта */
static pthread_t write_net_thread;	/* Исполнение команды по сетке */
static pthread_t read_nmea_thread;	/* Чтение данных NMEA */


static int net_fd;		/* Сокет UDP (пока!) */
static struct sockaddr_in sa;	/* переменная адреса интерфейса */
static bool start_stop_data = 0;
static CircularBuffer cb;



/* Структура, для подсчета пакетов */
static struct {
    int sig_num_count;
    int num_pack;
    int pack_in_min;		/* Число записей заголовка в минуту */
    int file_in_hour;		/* Запись файла раз в 4 часа */
    int adc_freq;		/* Частота АЦП */
    int num_data;		/* Номер данных  */
    int num_err;		/* Сохраненные ошибки */
    int flag;			/* Флаг пинг или понг */
} data_count_struct;

/* 2 буфера,хранящие данные */
static u8 *ADC_DATA_ping;
static u8 *ADC_DATA_pong;
static DEV_STATUS_STRUCT status;
static PORT_COUNT_STRUCT cnt;



int prepare_start_dev(void *);
static void sig_write_data_handler(int);
static void parse_adc_pack(ADS1282_PACK_STRUCT *, u8 *);
static void *read_port_thread_func(void *);
static void *write_net_thread_func(void *);



/* Подготовить к записи данных */
int prepare_start_dev(void *par)
{
    START_ADC_STRUCT *start;
    int res = -1, t0;

    do {
	t0 = get_sec_ticks();

	if (log_create_work_dir(t0) != 0) {
	    log_write_log_file("Error create work dir\n");
	    break;
	}

	if (par == NULL)
	    break;

	start = (START_ADC_STRUCT *) par;

	data_count_struct.adc_freq = start->freq;
	data_count_struct.num_pack = 0;

	/* Как часто писать минутный заголовок. В одном пакете NUM_ADC_PACK измерений */
	data_count_struct.pack_in_min = data_count_struct.adc_freq * 60 / NUM_ADS1282_PACK;
	log_write_log_file("freq: %d, %d packs per minute\n", data_count_struct.adc_freq, 
                                                              data_count_struct.pack_in_min);


	/* Каждые 4 часа будет приходить сигнал на создание файла */
	data_count_struct.file_in_hour = data_count_struct.pack_in_min * 60 * 4;

	/* Создаем 2 минутных буфера. Размер зависит от частоты */
	if (ADC_DATA_ping != NULL || ADC_DATA_pong != NULL) {
	    log_write_log_file("ERROR: calloc. mempool already exists\n");
	    break;
	}

	/* Считаем размер буфера в одной минуте */
	ADC_DATA_ping = calloc(data_count_struct.pack_in_min, sizeof(ADS1282_PACK_STRUCT));
	ADC_DATA_pong = calloc(data_count_struct.pack_in_min, sizeof(ADS1282_PACK_STRUCT));
	if (ADC_DATA_ping == NULL || ADC_DATA_pong == NULL) {
	    log_write_log_file("ERROR: calloc. can't alloc mempool\n");
	    break;
	}

	log_write_log_file("SUCCESS: alloc 2 x %lld bytes\n", 
			(long long) (data_count_struct.pack_in_min * sizeof(ADS1282_PACK_STRUCT)));
	log_close_data_file();

	log_create_adc_header(start, &status);
	res = StartDevice(start);
	if (res == 0) {
	    signal(SIGUSR1, sig_write_data_handler);	/* Создаем сигнал, который будет указывать что данные готовы! */
	    data_count_struct.sig_num_count = 0;	/* Обнуляем то что в пакете */
	}
    }
    while (0);
    return res;
}


/*******************************************************************
 * Удалить выделенную память и закрыть файл данных
 ********************************************************************/
void prepare_stop_dev(void)
{
    int n;

    do {
	n = StopDevice(NULL);
	if (n != 0)
	    break;

	log_close_data_file();

	if (ADC_DATA_ping) {
	    free(ADC_DATA_ping);
	    ADC_DATA_ping = NULL;
	}

	if (ADC_DATA_pong) {
	    free(ADC_DATA_pong);
	    ADC_DATA_pong = NULL;
	}
	memset(&data_count_struct, 0, sizeof(data_count_struct));
	signal(SIGUSR1, SIG_IGN);	/* Игнорировать */

	log_write_log_file("mem free\n");
    } while (0);
}



/* Чтение / запись в порт */
static void *read_port_thread_func(void *par)
{
    ADS1282_PACK_STRUCT pack, *ptr;
    s32 t0, t1;
    char *fix;
    char str1[128], str0[128];
    s32 res, len;
    s32 time, lat, lon;
    bool check = false;
    RTC_TIME_COORD rtc;


    len = sizeof(ADS1282_PACK_STRUCT);

    log_write_log_file("pthread begin\n");
    while (!is_end_thread()) {

	/* Ставим время NMEA раз в секунду */
#if defined TEST_NMEA_TIME
	rtc_get_param(&rtc);
#else
	nmea_get_param(&rtc);
#endif
	t0 = rtc.time;

	/* Читаем статус раз в секунду меняем заголовок */
	if (t0 != t1) {
	    TimeSync(&rtc);	    /*  пока нет качественного GPS */

	    t1 = t0;

	    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	    if (StatusGet(&status) == 0) {
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		log_change_adc_header(&status);

		if (check == false) {

		    /* Настроен - запускаем! */
		    if (status.st_main & STATUS_DEV_SET && check == false) {
			check = true;
			start_all_dev();
			log_write_log_file("================START!==================\n");
		    }
#if 1
		    sec_to_str(status.nmea_time, str0);
		    sec_to_str(status.prtc_time, str1);
		    log_write_log_file("%s: nmea: %s, prtc: %s, lat: %d, lon: %d\n", rtc.check ? "3dFix" : "NoFix", str0, str1, status.lat, status.lon);
#endif
		}
	    }
	}

	/* Читаем данные если запущено */
	while (1) {

	    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	    memset(&pack, 0, sizeof(pack));
	    res = DataGet(&pack);
	    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	    if (res == 0 && ADC_DATA_ping != NULL && ADC_DATA_pong != NULL) {

		if (data_count_struct.flag == PONG_FLAG) {
		    ptr = (ADS1282_PACK_STRUCT *) ADC_DATA_pong;
		} else {
		    ptr = (ADS1282_PACK_STRUCT *) ADC_DATA_ping;
		}


		/* Пишем в ping или понга */
		memcpy((u8 *) ptr + data_count_struct.sig_num_count * len, (u8 *) & pack, len);
/*		send_raw_data(&pack); */
		data_count_struct.sig_num_count++;

		/* Собрали > Сбрасываем счетчик и меняем флаг */
		if (data_count_struct.sig_num_count >= data_count_struct.pack_in_min) {
		    data_count_struct.flag = !data_count_struct.flag;
		    raise(SIGUSR1);
		    data_count_struct.sig_num_count = 0;
		}
		/* в кольцевой буфер */
		cb_write(&cb, &pack);

	    } else {
		break;
	    }
	}
    }
    StopDevice(NULL);
    log_write_log_file("INFO: read comport pthread end\n");
    pthread_exit(EXIT_SUCCESS);
}

/* Разобрать пакет данных. data - будет NUM_ADC_PACK измерений всех каналов! */
static void parse_adc_pack(ADS1282_PACK_STRUCT * pack, u8 * data)
{
    int i, shift = 0;
    u32 x, y, z, h;

    /* Пройдемся по пакету b запишем в буфер data */
    for (i = 0; i < NUM_ADS1282_PACK; i++) {
	x = bswap_32((u32) pack->data[i].x << SHIFT_BITES);
	memcpy(data + shift, &x, BYTES_IN_DATA);
	shift += BYTES_IN_DATA;

	y = bswap_32((u32) pack->data[i].y << SHIFT_BITES);
	memcpy(data + shift, &y, BYTES_IN_DATA);
	shift += BYTES_IN_DATA;

	z = bswap_32((u32) pack->data[i].z << SHIFT_BITES);
	memcpy(data + shift, &z, BYTES_IN_DATA);
	shift += BYTES_IN_DATA;

	h = bswap_32((u32) pack->data[i].h << SHIFT_BITES);
	memcpy(data + shift, &h, BYTES_IN_DATA);
	shift += BYTES_IN_DATA;
    }
}

/* Сигнал USR1 - запись минутных данных в файл */
static void sig_write_data_handler(int signo)
{
    ADS1282_PACK_STRUCT pack, *ptr;
    /*  нашел ошыпку - оставалось после изменения размера:  u8 data[240]; */
    u8 data[NUM_ADS1282_PACK * BYTES_IN_DATA * ADC_CHAN];
    char str[128];
    int i, len;
    s64 ns;


    /* Если пишется в ping - берем из понга */
    if (data_count_struct.flag == PING_FLAG) {
	ptr = (ADS1282_PACK_STRUCT *) ADC_DATA_pong;
    } else {
	ptr = (ADS1282_PACK_STRUCT *) ADC_DATA_ping;
    }

    len = sizeof(ADS1282_PACK_STRUCT);

    /* Скопируем данные с самого начала */
    for (i = 0; i < data_count_struct.pack_in_min; i++) {

	memcpy(&pack, (u8 *) ptr + len * i, len);
	parse_adc_pack(&pack, data);

	if (i == 0) {
	    ns = (s64) pack.sec * TIMER_NS_DIVIDER;

	    /* Раз в 4 часа создаем файл */
	    if (0 == data_count_struct.num_pack % (60 * 4)) {
		log_create_hour_data_file(ns);
	    }

	    /* Скидываем минутный заголовок */
	    log_write_adc_header_to_file(ns);

	    sec_to_str(pack.sec, str);	/* Время в пакете */
	    log_write_log_file("%s.%03d, drift: %d ns, num rec: %d, missed: %d\n", str, pack.msec % 1000, status.drift, data_count_struct.num_pack, pack.err0);
	}

	log_write_adc_data_to_file(data, sizeof(data));
    }
    data_count_struct.num_pack++;

    /* Восстанавлиаем сами себя! */
    signal(SIGUSR1, sig_write_data_handler);
}

/* Создадим сокет приема и передачи */
bool open_net_channel(void)
{
    bool res = false;
    int port;


    do {

	/* Создадим датаграмный сокет UDP */
	if ((net_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	    perror("socket");
	    break;
	}
        port = get_net_port_num();  

	/* Обнуляем переменную m_addr и забиваем её нужными значениями */
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;	/* обязательно AF_INET! */
	sa.sin_addr.s_addr = htonl(INADDR_ANY);	/* C любого интерфейса принимаем */
	sa.sin_port = htons(port);	/* 0 - выдать порт автоматом */

	/* привяжем сокет */
	if (bind(net_fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
	    perror("bind");
	    close(net_fd);
	    break;
	}
	res = true;
    } while (0);
    return res;
}


/* Закрыть сетевой канал */
void close_net_channel(void)
{
    shutdown(net_fd, SHUT_RD);
    close(net_fd);
}


/* Запуск всех потоков  */
int run_all_threads(void)
{
    int i;
    int res = EXIT_FAILURE;

    do {
        i = get_test_buf_size();
       
	/* Создадим кольцевой буфер ~10 секунд */
	if (cb_init(&cb, i)) {
	    log_write_log_file("ERROR: can't create circular buffer\n");
	    break;
	}
	/* Создаем сетевой сокет  */
	if (open_net_channel() != true) {
	    log_write_log_file("ERROR: create net channel\n");
	    break;
	}


	/* Создаем поток для работы по сети  */
	if (pthread_create(&write_net_thread, NULL, write_net_thread_func, NULL) != 0) {
	    log_write_log_file("ERROR: create net thread\n");
	    break;
	}
#if defined TEST_NMEA_TIME
	if (pthread_create(&read_nmea_thread, NULL, rtc_read_port_func, NULL) != 0) {
	    log_write_log_file("ERROR: create rtc thread\n");
	    break;
	}
#else
	/* Создаем поток для NMEA  */
	if (pthread_create(&read_nmea_thread, NULL, nmea_read_port_func, NULL) != 0) {
	    log_write_log_file("ERROR: create nmea thread\n");
	    break;
	}
#endif

	/* Создаем поток для чтения из порта */
	if (pthread_create(&read_port_thread, NULL, read_port_thread_func, NULL) != 0) {
	    log_write_log_file("ERROR: create port thread\n");
	    break;
	}

	if (pthread_join(write_net_thread, NULL) != 0) {
	    log_write_log_file("ERROR: net pthread_join\n");
	    break;
	}
#if 1
	if (pthread_join(read_nmea_thread, NULL) != 0) {
	    log_write_log_file("ERROR: net pthread_join\n");
	    break;
	}
#endif
	if (pthread_join(read_port_thread, NULL) != 0) {
	    log_write_log_file("ERROR: port pthread_join\n");
	    break;
	}
	res = EXIT_SUCCESS;
    } while (0);
    log_write_log_file("SUCCESS: thread exit\n");
    return res;
}


/* Вернуть дескриптор потока */
void get_read_port_thread_id(pthread_t * thread)
{
    thread = &read_port_thread;
}


/*******************************************************************
 * Потоковая функция передачи и приема по сети
 *******************************************************************/
static void *write_net_thread_func(void *par)
{
    int n, i = 0;
    u32 fromlen;
    char rd_buf[1024];
    char wr_buf[1024];
    struct hostent *ht;
    struct sockaddr_in caddr;
    START_ADC_STRUCT start;
    ADS1282_PACK_STRUCT pack;
    s32 t0;
    char *name;
    u8 cmd;
    int q = 0;

    caddr.sin_family = AF_INET;
    fromlen = sizeof(sa);
    while (!is_end_thread()) {

	n = recvfrom(net_fd, (void *) rd_buf, sizeof(rd_buf), 0, (struct sockaddr *) &caddr, &fromlen);
	if (n < 0) {
	    perror("recfrom");
	    break;
	    /*exit(EXIT_FAILURE); */
	}

	cmd = rd_buf[3];
	n = 0;
	switch (cmd) {
	case UART_CMD_GET_DSP_STATUS:
	    memcpy(wr_buf, &status, sizeof(status));
	    n = sendto(net_fd, wr_buf, sizeof(status), 0, (struct sockaddr *) &caddr, fromlen);
	    break;

	case UART_CMD_DSP_RESET:
	    log_write_log_file("reset\n");
	    memcpy(wr_buf, (u8 *) & status, 5);
	    wr_buf[0] = 1;
	    wr_buf[1] = 0;
	    n = sendto(net_fd, wr_buf, 5, 0, (struct sockaddr *) &caddr, fromlen);
	    ResetDSP(NULL);
	    break;

	case UART_CMD_INIT_TEST:
	    InitTest(NULL);
	    memcpy(wr_buf, (u8 *) & status, 5);
	    wr_buf[0] = 1;
	    wr_buf[1] = 0;
	    n = sendto(net_fd, wr_buf, 5, 0, (struct sockaddr *) &caddr, fromlen);
	    log_write_log_file("Test\n");
	    break;

	case UART_CMD_DEV_STOP:
	    prepare_stop_dev();
	    log_write_log_file("Stop device\n");

	    memcpy(wr_buf, (u8 *) & status, 5);
	    wr_buf[0] = 1;
	    wr_buf[1] = 0;
	    n = sendto(net_fd, wr_buf, 5, 0, (struct sockaddr *) &caddr, fromlen);
	    break;

	case UART_CMD_DEV_START:
	    memcpy(&start, rd_buf + 4, sizeof(start));
	    if (prepare_start_dev(&start) == 0) {
		log_write_log_file("Start OK\n");
	    }
	    memcpy(wr_buf, (u8 *) & status, 5);
	    wr_buf[0] = 1;
	    wr_buf[1] = 0;
	    n = sendto(net_fd, wr_buf, 5, 0, (struct sockaddr *) &caddr, fromlen);
	    break;

	    // Очистить буфер
	case UART_CMD_CLR_BUF:
	    cb_clear(&cb);
	    memcpy(wr_buf, (u8 *) & status, 5);
	    wr_buf[0] = 1;
	    wr_buf[1] = 0;
	    n = sendto(net_fd, wr_buf, 5, 0, (struct sockaddr *) &caddr, fromlen);
	    break;

	case UART_CMD_GET_DATA:
	    if (!cb_is_empty(&cb)) {
		cb_read(&cb, (ElemType *) & pack);
		pack.len = sizeof(pack) - 4;
		n = sendto(net_fd, (u8 *) & pack, sizeof(pack), 0, (struct sockaddr *) &caddr, fromlen);
	    } else {		/* нет данных пока */
		memcpy(wr_buf, (u8 *) & status, 5);
		wr_buf[0] = 1;
		wr_buf[1] = 0;
		n = sendto(net_fd, wr_buf, 5, 0, (struct sockaddr *) &caddr, fromlen);
	    }
	    break;


	default:
	    break;
	}
    }

    log_write_log_file("read net sock pthread end\n");
    pthread_exit(EXIT_SUCCESS);
}
