#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* ============================================================
 *  从站1：地址 0x01
 *  负责：SHT30 温湿度 + BH1750 光照
 * ============================================================ */
#define SLAVE_ADDR          0x01

/* RS485 方向脚：自动方向模块留空 */
#define RS485_TX_ENABLE()
#define RS485_RX_ENABLE()

/* 保持寄存器区（0x03 功能码读取）
 * [0] = 温度 ×10
 * [1] = 湿度 ×10
 * [2] = 光照 高16位
 * [3] = 光照 低16位  */
#define HOLDING_REG_NUM     8
extern uint16_t holding_reg[HOLDING_REG_NUM];

void Modbus_Slave_Init(UART_HandleTypeDef *huart);
void Modbus_Slave_RxByte(uint8_t b);
void Modbus_Slave_RxCplt(void);   /* 在 HAL_UART_RxCpltCallback 里调 */
void Modbus_Slave_Poll(void);

#endif
