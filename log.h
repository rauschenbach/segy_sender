#ifndef _LOG_H
#define _LOG_H

#include "proto.h"
#include "globdefs.h"

/*
int  log_create_work_dir(s32);
int  log_write_log_file(char*, ...);
int  log_error_to_log(char*, ...);
int  log_close_log_file(void);
void log_change_adc_header(void *);
int  log_parse_and_write_data(ADS1282_PACK_STRUCT*, int);
*/


/* log.c */
int log_create_work_dir(int32_t sec);
int log_open_log_file(void);
int log_check_old_log(void);
int log_close_log_file(void);
int log_write_log_file(char *fmt, ...);
int log_write_error_to_log(char *fmt, ...);
void log_create_adc_header(START_ADC_STRUCT *start, DEV_STATUS_STRUCT *status);
void log_change_adc_header(void *p);
int log_create_hour_data_file(uint64_t ns);
int log_close_data_file(void);
int log_write_adc_header_to_file(uint64_t ns);
int log_write_adc_data_to_file(void *data, int len);


#endif /* log.h */
