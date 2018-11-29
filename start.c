#include <stdlib.h>
#include "mythreads.h"
#include "start.h"
#include "proto.h"
#include "globdefs.h"
#include "sender.h"


static bool b_end_threads = false;

/* Поток завершен? */
bool is_end_thread(void)
{
    return b_end_threads;
}

/* Завершить потоки */
void halt_all_threads(void)
{
    pthread_t thread;
    int sock;

    get_read_port_thread_id(&thread);
    b_end_threads = true;
    close_net_channel();
    pthread_cancel(thread);
}


void start_all_dev(void)
{
    START_ADC_STRUCT start;
    start.freq = get_adc_data_rate();
    start.pga = 1;
    start.mode = 2;
    start.cons = 1;
    start.chop = 1;
    start.float_data.fPar = 0;

    prepare_start_dev(&start);
}
