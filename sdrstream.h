#pragma once

#include <complex>
#include <pthread.h>
#include <semaphore.h>
#include "devices.h"
#include "vfo.h"

#include "DataBuffer.h"
#include "sdrberry.h"

#define NUM_THREADS	4
#define RX_THREAD	0

extern mutex stream_write;

int create_rx_streaming_thread(struct device_structure *sdr_dev);
void stream_rx_set_frequency(struct device_structure *sdr_dev, unsigned long freq);
int create_tx_streaming_thread(struct device_structure *sdr_dev);
void stream_tx_set_frequency(struct device_structure *sdr_dev, unsigned long freq);