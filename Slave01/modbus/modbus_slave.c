#include "modbus_slave.h"
#include <string.h>

/* ---------- CRC16 逐位计算（100%正确，不用查表） ---------- */
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

/* ---------- 寄存器区 ---------- */
uint16_t holding_reg[HOLDING_REG_NUM] = {0};

static UART_HandleTypeDef *s_huart = NULL;

/* ---------- 接收状态 ---------- */
#define RX_BUF_SIZE 64
static uint8_t  rx_buf[RX_BUF_SIZE];
static uint8_t  rx_len = 0;
static uint32_t last_rx_tick = 0;
/* 9600bps 下 3.5 字符约 4ms，留 5ms 余量 */
#define FRAME_IDLE_MS 5

void Modbus_Slave_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    rx_len = 0;
}

void Modbus_Slave_RxByte(uint8_t b)
{
	/* 帧首字节不是自己的地址，整帧丢弃 */
    if (rx_len == 0 && b != SLAVE_ADDR) return;
    if (rx_len < RX_BUF_SIZE) {
        rx_buf[rx_len++] = b;
    }
    last_rx_tick = HAL_GetTick();
}

/* 组发送一帧（带方向控制） */
static void slave_send(uint8_t *data, uint8_t len)
{
    RS485_TX_ENABLE();
    HAL_UART_Transmit(s_huart, data, len, 100);
    /* 等最后一个字节完全移出 */
    while (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_TC) == RESET);
    RS485_RX_ENABLE();
}

/* 处理一帧完整请求 */
static void process_frame(void)
{
    uint8_t *rx = rx_buf;
    uint8_t  len = rx_len;
    uint8_t  reply[64];
    uint8_t  plen = 0;
    uint16_t crc;

    /* 最短请求 8 字节：地址+功能+起址(2)+数量(2)+CRC(2) */
    if (len < 8) return;

    /* 地址不匹配：直接丢弃，不应答 */
    if (rx[0] != SLAVE_ADDR) return;

    /* CRC 校验 */
    uint16_t crc_recv = (uint16_t)rx[len-2] | ((uint16_t)rx[len-1] << 8);
    if (crc16(rx, len - 2) != crc_recv) return;

    uint8_t func = rx[1];

    if (func == 0x03) {
        /* 读保持寄存器 */
        uint16_t start  = ((uint16_t)rx[2] << 8) | rx[3];
        uint16_t count  = ((uint16_t)rx[4] << 8) | rx[5];
        if (start + count > HOLDING_REG_NUM) {
            /* 异常码：非法数据地址 */
            reply[0] = SLAVE_ADDR;
            reply[1] = 0x83;
            reply[2] = 0x02;
            crc = crc16(reply, 3);
            reply[3] = crc & 0xFF; reply[4] = crc >> 8;
            slave_send(reply, 5);
            return;
        }
        /* 组正常应答 */
        reply[0] = SLAVE_ADDR;
        reply[1] = 0x03;
        reply[2] = (uint8_t)(count * 2);
        plen = 3;
        for (uint16_t i = 0; i < count; i++) {
            reply[plen++] = (holding_reg[start + i] >> 8) & 0xFF;
            reply[plen++] = holding_reg[start + i] & 0xFF;
        }
        crc = crc16(reply, plen);
        reply[plen++] = crc & 0xFF;
        reply[plen++] = crc >> 8;
        slave_send(reply, plen);
    } else {
        /* 不支持的功能码，回 0x8X + 01 非法功能 */
        reply[0] = SLAVE_ADDR;
        reply[1] = func | 0x80;
        reply[2] = 0x01;
        crc = crc16(reply, 3);
        reply[3] = crc & 0xFF; reply[4] = crc >> 8;
        slave_send(reply, 5);
    }
}

void Modbus_Slave_Poll(void)
{
    /* 清Overrun错误 */
    if (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(s_huart);
    }
    /* 轮询读字节 */
    while (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_RXNE)) {
        uint8_t b = (uint8_t)(s_huart->Instance->DR & 0xFF);
        Modbus_Slave_RxByte(b);
    }

    if (rx_len == 0) return;
    /* 空闲超过阈值，认为一帧结束 */
    if (HAL_GetTick() - last_rx_tick < FRAME_IDLE_MS) return;

    process_frame();
    rx_len = 0;
    memset(rx_buf, 0, RX_BUF_SIZE);
}
