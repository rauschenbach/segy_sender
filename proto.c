#define _BSD_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <termios.h>
#include <sys/select.h>
#include "start.h"
#include "proto.h"
#include "mythreads.h"
#include "utils.h"
#include "my_defs.h"
#include "my_cmd.h"
#include "log.h"


// Num rates
#define NUM_RATES	9

static struct {
    int sym, data;
} rates[NUM_RATES] = {
    {
    B115200, 115200}, {
    B230400, 230400}, {
    B460800, 460800}, {
    B921600, 921600}, {
    B1000000, 1000000}, {
    B1152000, 1152000}, {
    B1500000, 1500000}, {
    B2000000, 2000000}, {
B2500000, 2500000},};

static int CommPort;
static PORT_COUNT_STRUCT my_count;
static pthread_mutex_t cs;


static void data_port_reset(void);
static bool data_port_read_any_data(void *, int);
static bool data_port_send_some_command(u8, void *, int);



/* Настраиваем порт */
bool data_port_init(char *name, u32 b)
{
    int i, baud = 0;
    struct termios oldtio, newtio;
    char buf[255];
    bool res = false;

    do {

	/* Найдем скорость  */
	for (i = 0; i < NUM_RATES; i++) {
	    if (rates[i].data == b) {
		baud = rates[i].sym;
		break;
	    }
	}

	if (baud == 0) {
	    log_write_log_file("error rate %d\n", b);
	    break;
	}

	/* Открываем порт */
	CommPort = open(name, O_RDWR | O_NOCTTY);
	if (CommPort < 0) {
	    perror("open");
	    break;
	}

	tcgetattr(CommPort, &oldtio);	/* сохранение текущих установок порта */
	bzero(&newtio, sizeof(newtio));

	/* Неканонический режим. Нет эхо. 8N2 */
	newtio.c_cflag |= (CLOCAL | CREAD);
	newtio.c_cflag &= ~PARENB;
	newtio.c_cflag |= CSTOPB; // 2 stop
	newtio.c_cflag &= ~CSIZE;
	newtio.c_cflag |= CS8;
	newtio.c_cflag &= ~CRTSCTS;

	newtio.c_lflag &= ~(ICANON | ECHO | ISIG);
#if 1
	newtio.c_cc[VTIME] = 1;	/* посимвольный таймер */
	newtio.c_cc[VMIN] = 0;
#else
	newtio.c_cc[VTIME] = 0;	/* посимвольный таймер */
	newtio.c_cc[VMIN] = 1;
#endif
	newtio.c_oflag &= ~OPOST;
	newtio.c_iflag |= IGNPAR;

	cfsetispeed(&newtio, baud);
	cfsetospeed(&newtio, baud);

	/* Noblock  */
	if (tcsetattr(CommPort, TCSANOW, &newtio) < 0) {
	    perror("tcsetatr");
	    res = false;
	    break;
	}

	i = fcntl(CommPort, F_GETFL, 0);
	fcntl(CommPort, F_SETFL, i | O_NONBLOCK);

	i = pthread_mutex_init(&cs, NULL);
	if (i != 0) {
	    log_write_log_file("error %d\n", i);
	    break;
	}
	res = true;
    } while (0);


    sprintf(buf, "open port %s on baud %d", name, b);
    PrintCommandResult(buf, !res);

    return res;
}

// Закрыть порт
bool data_port_close(void)
{
    /* Сбросим буферы */
    tcflush(CommPort, TCIFLUSH);
    tcflush(CommPort, TCOFLUSH);
    close(CommPort);
    pthread_mutex_destroy(&cs);
    PrintCommandResult("close port", 0);
    return true;
}

/* Сброс буферов  */
static void data_port_reset(void)
{
    tcflush(CommPort, TCIFLUSH);
    tcflush(CommPort, TCOFLUSH);
}


// Запись в порт
static int data_port_write(u8 *buf, int len)
{
    int num;

    if (CommPort > 0) {
	num = write(CommPort, buf, len);
	my_count.tx_pack++;	//передано
    } else {
	num = -1;
    }
    return num;
}


// Чтение из порта без таймаута
static int data_port_read(u8 *buf, int len)
{
    int num = 0, res, i = 0;
    u8 *ptr;
    fd_set readfs;		/* file descriptor set */
    int maxfd;			/* maximum file desciptor used */
    struct timeval timeout;
    int ind = 0;

    if (CommPort > 0) {

	ptr = buf;
	while (1) {
//          log_write_log_file("pre-select\n");
	    FD_ZERO(&readfs);
	    FD_SET(CommPort, &readfs);	/* set testing for source 1 */
	    maxfd = CommPort + 1;	/* maximum bit entry (fd) to test */

	    timeout.tv_sec = 0;	//секунды
	    timeout.tv_usec = 25000;	// микросекунды
	    res = select(maxfd, &readfs, NULL, NULL, &timeout);

	    if (FD_ISSET(CommPort, &readfs)) {
		num = read(CommPort, ptr, len - ind);

		if(num <= 0) {
		    if(num < 0)
			log_write_log_file("----------error, num < 0!\n");	    
		    else 
			printf("nothing to do!\n");
		    break;
		}

		ptr += num;
		ind += num;
		
		if (ind >= len || is_end_thread() || num < 0) {
		    if(num < 0)
			log_write_log_file("error read!\n");
		    break;
		}
	    } else {
		break;
	    }
	}
    }
//    tcflush(CommPort, TCIFLUSH);
//    tcflush(CommPort, TCOFLUSH);

    return ind;
}


////////////////////////////////////////////////////////////////////////////////
// Видны в других файлах
////////////////////////////////////////////////////////////////////////////////
// Выдать счетчики обмена
void data_port_get_xchg_counts(void *p)
{
    if (p != NULL) {
	memcpy(p, &my_count, sizeof(my_count));
    }
}


/**
 * Получить ответ из порта
 */
static bool data_port_read_any_data(void *v, int len)
{
    int num;
    u8 *ptr;
    u16 crc16 = 0;
    bool res = false;

    do {
	if (len == 0 || v == NULL)
	    break;

	memset(v, 0, len);

	ptr = (u8 *) v;

	// Ждем ответа 
	num = data_port_read(ptr, len);
	if (num < len && num != 5) {
	    break;
	}

	/* Проверяем CRC16   */
        crc16 = test_crc16((u8*)ptr, num);
	if (crc16) {
	    break;
	} else {
	    res = true;
	}
    } while (0);


   // посмотрим - бывают ли сбои
   if(len == sizeof(ADS1282_PACK_STRUCT) && num > 6 && res == false && *(u16*)ptr == 0x04b8) {
      log_write_log_file("ERROR: crc16 is error!!!, %d was read. Try to fix!\n", num);
      print_data_hex(ptr, num);
      res = true;
    }
    return res;
}



/**
 * Послать любую команду. Оформить data по протоколу
 */
static bool data_port_send_some_command(u8 cmd, void *data, int data_len)
{
    u8 buf[255];
    int num;
    bool res = false;

    do {
	if (data_len > 255 || data_len < 0)
	    break;		// неправильная длина посылки

	buf[0] = 0xff;		// адрес
	buf[1] = 0;
	buf[2] = data_len + 1;	// на единицу больше самих данных
	buf[3] = cmd;		// Команда

	// В команде есть данные
	if (data != NULL && data_len > 0) {
	    memcpy(buf + 4, (u8 *)
		   data, data_len);
	}
	add_crc16(buf);		// Контрольная сумма

	num = data_port_write(buf, buf[2] + 5);
	if (num != buf[2] + 5) {
	    break;		// Порт не открыт
	}
	res = true;		// Все ОК
    } while (0);

    return res;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///                          Сами команды                                                                                    //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// установи время + координаты
int TimeSync(void *t)
{
    int res = -1;
    char buf[255];
    RTC_TIME_COORD *rtc;

    EnterCriticalSection(&cs);
    do {
	if (t == NULL)
	    break;

	rtc = (RTC_TIME_COORD *) t;
	if (data_port_send_some_command(UART_CMD_SYNC_RTC_CLOCK, (u8 *) rtc, sizeof(RTC_TIME_COORD)) == false) {
	    break;
	}

	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
//	    log_write_log_file("Sync: OK\n");
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);

    return res;
}

//---------------------------------------------------------------------------
// Получить статус - new
int StatusGet(DEV_STATUS_STRUCT * st)
{
    int res = -1;
    char str[256];

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_GET_DSP_STATUS, NULL, 0) == false) {
	    break;
	}
	// Читаем
	if (data_port_read_any_data((u8 *) str, sizeof(DEV_STATUS_STRUCT)) == true) {
	    memcpy(st, str, sizeof(DEV_STATUS_STRUCT));
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);


    return res;
}

//---------------------------------------------------------------------------
// команда Выдай данные измерений или статус 5 байт
int DataGet(void *p)
{
    int res = -1;
    int len;
    ADS1282_PACK_STRUCT *pack;

    if (p == NULL)
	return -1;
    pack = (ADS1282_PACK_STRUCT *) p;
    EnterCriticalSection(&cs);
    do {

	if (data_port_send_some_command(UART_CMD_GET_DATA, NULL, 0) == false) {
	    break;
	}
	// затрем и читаем
	bzero((u8 *) pack, sizeof(ADS1282_PACK_STRUCT));
	res = data_port_read_any_data((u8 *) pack, sizeof(ADS1282_PACK_STRUCT));

	if (res == true) {
	    len = pack->len;

	    if (len != 1 && len != 1208)
		log_write_log_file("read: %d\n", len);

	    if (len == (sizeof(ADS1282_PACK_STRUCT) - 4)) {	// данные
		res = 0;
	    } else if (len == 0x01) {	//Или статус?
		res = 1;// 
	    } else {
		res = -1;
	    }
	} else {
	    res = -1;
	}

#if 0
	// Наплодим
	pack->len = sizeof(ADS1282_PACK_STRUCT) - 4;
	pack->msec = 0;
	pack->sec = get_sec_ticks();
	pack->err0 = 0;
	res = 0;
#endif


    } while (0);
    LeaveCriticalSection(&cs);
    return res;
}

//---------------------------------------------------------------------------
//  Команда: Стоп измерения - new
int StopDevice(void *par)
{
    int res = -1;
    u8 buf[2048];

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_DEV_STOP, NULL, 0) == false) {
	    break;
	}
	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    PrintCommandResult("Стоп измерения!", res);
    return res;
}


//---------------------------------------------------------------------------
//  Команда: Очистить буфер данных
int ClearBuf(void *par)
{
    int res = -1;
    u8 buf[2048];

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_CLR_BUF, NULL, 0) == false) {
	    break;
	}
	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    PrintCommandResult("Очистка кольцевого буфера!", res);
    return res;
}

//---------------------------------------------------------------------------
// Запуск тестирования - new
int InitTest(void *par)
{
    int res = -1;
    u8 buf[2048];

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_INIT_TEST, NULL, 0) == false) {
	    break;
	}
	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    return res;
}


//---------------------------------------------------------------------------
// Сброс DSP - new
int ResetDSP(void *par)
{
    int res = -1;
    u8 buf[2048];

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_DSP_RESET, NULL, 0) == false) {
	    break;
	}
	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    return res;
}

//---------------------------------------------------------------------------
// Управление питанием - new
int PowerOff(void *par)
{
    int res = -1;
    u8 buf[2048];

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_POWER_OFF, NULL, 0) == false) {
	    break;
	}
	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    return res;
}

//---------------------------------------------------------------------------
// Задать номер станции - new
int SetId(void *par)
{
    int res = -1;
    char buf[255];
    u16 num;

    if (par == NULL)
	return -1;

    num = *(u16 *) par;

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_SET_DSP_ADDR, (u8 *) & num, 2) == false) {
	    break;
	}
	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);

    return res;
}

//---------------------------------------------------------------------------
// -- Сбросить EEPROM
int SetTimesToZero(void *par)
{
    char buf[255];
    int res;

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_ZERO_ALL_EEPROM, NULL, 0) == false) {
	    break;
	}
	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    return res;
}

//---------------------------------------------------------------------------
// Получить константы  всех каналов
int GetConst(void *in)
{
    int res = -1;
    char buf[2048];

    if (in == NULL)
	return res;

    EnterCriticalSection(&cs);
    do {
	if (data_port_send_some_command(UART_CMD_GET_ADC_CONST, NULL, 0) == false) {
	    break;
	}
	// Читаем константы
	if (data_port_read_any_data(buf, sizeof(ADS1282_Regs) + 3) == true) {
	    memcpy(in, buf + 1, sizeof(ADS1282_Regs));
	    log_write_log_file("GetConst: OK\n");
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    return res;
}


//---------------------------------------------------------------------------
// Начать штатные измерения ускорения
// В критическую секцию - 2 раза, т.к. у нас есть ожидание
// Пусть другие процессы не ждут
int StartDevice(START_ADC_STRUCT * start)
{
    u8 buf[1024];
    int num, res = -1;

    EnterCriticalSection(&cs);
    do {
	if (start->freq != 250 && start->freq != 500 && start->freq != 1000 && start->freq != 2000 && start->freq != 4000)
	    break;

	// Выдать команду "Задание с заданными параметрами"
	if (data_port_send_some_command(UART_CMD_DEV_START, (u8 *) start, sizeof(START_ADC_STRUCT)) == false) {
	    break;
	}
	sleep(1);		// Ждем ответа-короткий статус с ожыданием!

	// Читаем 5 байт статуса
	if (data_port_read_any_data(buf, 5) == true) {
	    res = 0;
	}
    } while (0);
    LeaveCriticalSection(&cs);
    sprintf((char*)buf, "Start device: freq = %d, pga = %d", start->freq, start->pga);
    PrintCommandResult((char*)buf, res);
    return res;
}
