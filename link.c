#include "plugin.h"
#include "link.h"
#include "sender.h"
#include <byteswap.h>
#include <stdio.h>

void send_raw_data(ADS1282_PACK_STRUCT* pack)
{
    s32 x[NUM_ADS1282_PACK];
    s32 y[NUM_ADS1282_PACK];
    s32 z[NUM_ADS1282_PACK];
    s32 h[NUM_ADS1282_PACK];

    const char *station = get_station_name();
    const char channel_x[] = "AX";
    const char channel_y[] = "AY";
    const char channel_z[] = "AZ";
    const char channel_h[] = "AH";

    int i;
    for(i = 0; i < NUM_ADS1282_PACK; i++) {
#if 1
	x[i] = bswap_32((s32)((pack->data[i].x) << 8 / 256));
	y[i] = bswap_32((s32)((pack->data[i].y) << 8 / 256));
	z[i] = bswap_32((s32)((pack->data[i].z) << 8 / 256));
	h[i] = bswap_32((s32)((pack->data[i].h) << 8 / 256));
#else
	x[i] = ((s32)((pack->data[i].x) << 8 / 256));
	y[i] = ((s32)((pack->data[i].y) << 8 / 256));
	z[i] = ((s32)((pack->data[i].z) << 8 / 256));
	h[i] = ((s32)((pack->data[i].h) << 8 / 256));
#endif

    }

    double t = pack->sec + pack->msec/1000.;
    //printf("%.3f %d\n", t, NUM_ADS1282_PACK);
#if 0
    send_raw_depoch(station, channel_x, t, 0, 100, x, NUM_ADS1282_PACK); 
    send_raw_depoch(station, channel_y, t, 0, 100, y, NUM_ADS1282_PACK); 
    send_raw_depoch(station, channel_z, t, 0, 100, z, NUM_ADS1282_PACK); 
    send_raw_depoch(station, channel_h, t, 0, 100, h, NUM_ADS1282_PACK); 
#endif
}

