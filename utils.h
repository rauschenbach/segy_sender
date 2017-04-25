#ifndef _UTILS_H
#define _UTILS_H

#include "my_defs.h"

/* utils.c */
u16 test_crc16(u8 *mes, int);


void add_crc16(u8 *buf);
unsigned short check_crc16(unsigned char *, unsigned short);
void sec_to_str(long ls, char *str);
long td_to_sec(TIME_DATE *t);
int sec_to_td(long ls, TIME_DATE *t);
void print_data_hex(u8*, int);
s64 get_msec_ticks(void);
s32 get_sec_ticks(void);


int get_adc_freq(int);
int get_adc_pga(int);

void time_to_str(char*);


void PrintCommandResult(char *, int);
void EnterCriticalSection(void*);
void LeaveCriticalSection(void*);


#endif /* utils.h */


