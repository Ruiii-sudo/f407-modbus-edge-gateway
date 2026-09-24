#include "modbus.h"
#include "usart.h"
#include <string.h>
#include "sensor_data.h"

extern UART_HandleTypeDef huart3;

/* CRC16 逐位计算 */
static uint16_t crc16(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* 发送一帧并等待应答，返回实际收到的字节数，超时返回0 */
static uint8_t modbus_tx_rx(uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t timeout_ms)
{
    /* 清Overrun */
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&huart3);
    /* 清接收DMA/中断残留 */
    __HAL_UART_FLUSH_DRREGISTER(&huart3);

    HAL_UART_Transmit(&huart3, tx, tx_len, 100);
    /* 等最后一个字节发完 */
    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET);

    /* 等应答 */
    uint8_t rx_len = 0;
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout_ms) {
		if (rx_len >= 64) break;
        if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE)) {
            rx[rx_len++] = (uint8_t)(huart3.Instance->DR & 0xFF);
            /* 收到功能码+字节数后，按长度判断收完 */
            if (rx_len >= 5) {
                /* 0x03应答: 地址+功能码+字节数+数据+CRC(2) */
                uint8_t data_bytes = rx[2];
                if (rx_len >= (uint32_t)(3 + data_bytes + 2))
                    break;
            }
        }
    }
    return rx_len;
}

/* 读保持寄存器，从站addr，起始reg start，数量count，结果存regs */
/* 成功返回0，失败返回-1 */
static int modbus_read_holding(uint8_t addr, uint16_t start, uint16_t count, uint16_t *regs)
{
    uint8_t tx[8], rx[64];
    tx[0] = addr;
    tx[1] = 0x03;
    tx[2] = (start >> 8) & 0xFF;
    tx[3] = start & 0xFF;
    tx[4] = (count >> 8) & 0xFF;
    tx[5] = count & 0xFF;
    uint16_t crc = crc16(tx, 6);
    tx[6] = crc & 0xFF;
    tx[7] = (crc >> 8) & 0xFF;

    uint8_t rlen = modbus_tx_rx(tx, 8, rx, 100);
    if (rlen < 5) return -1;

    /* 校验地址和功能码 */
    if (rx[0] != addr || rx[1] != 0x03) return -1;

    /* 校验CRC */
    uint16_t crc_recv = (uint16_t)rx[rlen-2] | ((uint16_t)rx[rlen-1] << 8);
    if (crc16(rx, rlen - 2) != crc_recv) return -1;

    /* 解析数据 */
    uint8_t data_bytes = rx[2];
    if (data_bytes != count * 2) return -1;
    for (uint16_t i = 0; i < count; i++) {
        regs[i] = ((uint16_t)rx[3 + i*2] << 8) | rx[4 + i*2];
    }
    return 0;
}

void Modbus_Master_Init(void)
{
    /* USART3已在CubeMX里初始化为9600 8N1，这里不需要额外操作 */
}

/* 从从站1(0x01)读温湿度+光照，从从站2(0x02)读MQ2 */
int Modbus_Master_PollAll(void)
{
    EnvData_t env = SensorData_GetCurrent();
    uint16_t regs[4];
    int ok = 0;

    /* 读从站1: 4个寄存器（温湿度×2 + 光照×2） */
    if (modbus_read_holding(0x01, 0, 4, regs) == 0) {
        env.temp = regs[0] / 10.0f;
        env.humi = regs[1] / 10.0f;
        uint32_t lux_raw = ((uint32_t)regs[2] << 16) | regs[3];
        env.light = (uint16_t)(lux_raw / 1000);
        ok++;
    }
	
	HAL_Delay(20);

    /* 读从站2: 1个寄存器（MQ2 ADC） */
    uint16_t gas_reg;
    if (modbus_read_holding(0x02, 0, 1, &gas_reg) == 0) {
        env.gas = gas_reg;
        ok++;
    }

    if (ok > 0) {
        /* 有数据就更新，没读到的保持上次值 */
        /* 这里简单处理：只要至少读到一个从站就更新 */
        SensorData_Update(&env);
    }
    return ok == 2 ? 0 : -1;
}
