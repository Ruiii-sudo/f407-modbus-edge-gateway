#ifndef SENSOR_H
#define SENSOR_H

#include "stm32f1xx_hal.h"

/* 从站1：SHT30 温湿度 + BH1750 光照 */
void Sensor_Init(void);
void Sensor_RefreshRegs(void);

#endif
