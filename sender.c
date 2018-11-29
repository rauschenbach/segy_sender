#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "start.h"
#include "mythreads.h"
#include "proto.h"
#include "utils.h"
#include "log.h"

static char dev[32];
static char name[32];


static struct {
    int  adc_data_rate;		// скорость АЦП
    int  net_port_num;		// сетевой порт
    int  test_buf_size;		// размер кольцевого буфера
    char station_name[32];	// имя станции
    char log_file_sym;
} start_param_struct;


static void sig_handler(int num)
{
    printf("signal SIGINT!\n");
    halt_all_threads();
}

int main(int argc, char *argv[])
{
    int res;
    char dev[32];

    DEV_STATUS_STRUCT status;

    if (argc < 3) {
	printf("error: need 2 arg, use: sender port adc_rate station_name\n");
	exit(-1);
    }
    // Имя COM порта
    strncpy(dev, argv[1], sizeof(dev));
    start_param_struct.adc_data_rate = atoi(argv[2]);	// adc rate
    if (start_param_struct.adc_data_rate != 250 && start_param_struct.adc_data_rate != 2000) {
	printf("ERROR: ADC sample rate has to be 250 or 2000\n");
	return -1;
    }
    
    start_param_struct.test_buf_size = (start_param_struct.adc_data_rate == 2000)? 50 : 8;
    start_param_struct.net_port_num = (start_param_struct.adc_data_rate == 2000)?  1025 : 1026;
    start_param_struct.log_file_sym = (start_param_struct.adc_data_rate == 2000)? 'A' : 'B';
    

    strncpy(start_param_struct.station_name, argv[3], sizeof(start_param_struct.station_name));

    signal(SIGINT, sig_handler);

    if (log_open_log_file() != 0) {
	exit(-1);
    }


    if (data_port_init(dev, 460800) == false) {
	exit(-1);
    }


    /* Плату подцепить! */
    res = StatusGet(&status);
    if (res == 0) {
	run_all_threads();
    }

    data_port_close();
    log_write_log_file("Stop device\n");
    log_close_log_file();
}

//----------------------------------------------------------------------------------------------
int get_adc_data_rate(void)
{
  return start_param_struct.adc_data_rate;
}


int get_net_port_num(void)
{
  return start_param_struct.net_port_num;
}

int get_test_buf_size(void)
{
  return start_param_struct.test_buf_size;
}

char get_log_file_sym(void)
{
  return start_param_struct.log_file_sym;
}


char *get_station_name(void)
{
  return start_param_struct.station_name;
}

