#ifndef ProtoH
#define ProtoH

#include "globdefs.h"


/**
 * Параметры АЦП для всех 4-х каналов.
 * Смещение 0 и усиление 
 */
typedef struct {
    u32 magic;			/* Магич. число */
    struct {
	u32 offset;		/* коэффициент 1 - смещение */
	u32 gain;		/* коэффициент 2 - усиление */
    } chan[4];			/* 4 канала */
} ADS1282_Regs;

typedef struct {
    u32 freq;
    u32 pga;
    u32 mode;
    u32 cons;
    u32 chop;
    union {
	u8 cPar[4];
	u32 lPar;
	f32 fPar;
    } float_data;
} START_ADC_STRUCT;

#pragma pack (1)
typedef struct {
    u8 len;
    i32 rx_pack;		/* Принятые пакеты */
    i32 tx_pack;		/* переданные пакеты */
    i32 rx_cmd_err;		/* Ошибка в команде */
    i32 rx_stat_err;		/* Ошибки набегания, кадра (без стопов) и пр */
    i32 rx_crc_err;		/* Ошибки контрольной суммы */
    u16 crc16;
} PORT_COUNT_STRUCT;



/* proto.c */
unsigned char data_port_init(char *, u32);
unsigned char data_port_close(void);
void 	      data_port_get_xchg_counts(void *p);


int SendCmdPC(void);
int TimeSync(void *);
int CountsGet(PORT_COUNT_STRUCT *cnt);
int StatusGet(DEV_STATUS_STRUCT *st);
int DataGet(void *p);
int StopDevice(void *par);
int ClearBuf(void *par);
int InitTest(void *par);
int ResetDSP(void *par);
int PowerOff(void *par);
int SetId(void *par);
int SetTimesToZero(void *par);
int GetConst(void *in);
int SetConst(void *in);
int StartDevice(START_ADC_STRUCT *start);

#endif				/* proto.h  */
