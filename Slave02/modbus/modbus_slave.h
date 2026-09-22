#ifndef __MODBUS_SLAVE_H
#define __MODBUS_SLAVE_H

#include "main.h"

/* 从站2地址 = 0x02 */
#define SLAVE_ADDR       0x02
#define HOLDING_REG_NUM  8

extern uint16_t holding_reg[HOLDING_REG_NUM];

void Modbus_Slave_Init(UART_HandleTypeDef *huart);
void Modbus_Slave_Poll(void);

#endif
