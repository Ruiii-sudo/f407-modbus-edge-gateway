#ifndef __MODBUS_H
#define __MODBUS_H

#include "stm32f4xx_hal.h"

/* 初始化Modbus主站（绑定USART3） */
void Modbus_Master_Init(void);

/* 轮询所有从站，返回最新一帧环境数据 */
/* 成功返回0，失败返回-1 */
int Modbus_Master_PollAll(void);

#endif
