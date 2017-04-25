#ifndef _MYTHREADS_H
#define _MYTHREADS_H

#include <pthread.h>
#include "proto.h"

int  run_all_threads(void);
void get_read_port_thread_id(pthread_t *);

bool open_net_channel(void);
void close_net_channel(void);
int prepare_start_dev(void *par);
void prepare_stop_device();

#endif /* mythreads.h */
