#include "sensor.h"
#include "modbus_slave.h"
#include <stdint.h>

/* ============================================================
 *  软件 I2C：PB6=SCL, PB7=SDA
 * ============================================================ */
#define SCL_H()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)
#define SCL_L()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)
#define SDA_H()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)
#define SDA_L()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)
#define SDA_READ() HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)

static void i2c_delay(void) { for (volatile int i = 0; i < 8; i++); }

static void i2c_start(void)
{
    SDA_H(); SCL_H(); i2c_delay();
    SDA_L(); i2c_delay();
    SCL_L(); i2c_delay();
}
static void i2c_stop(void)
{
    SDA_L(); SCL_H(); i2c_delay();
    SDA_H(); i2c_delay();
}
static uint8_t i2c_write(uint8_t d)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (d & 0x80) SDA_H(); else SDA_L();
        d <<= 1;
        i2c_delay(); SCL_H(); i2c_delay(); SCL_L();
    }
    SDA_H(); i2c_delay(); SCL_H(); i2c_delay();
    uint8_t ack = (SDA_READ() == 0) ? 0 : 1;
    SCL_L(); i2c_delay();
    return ack;
}
static uint8_t i2c_read(uint8_t ack)
{
    uint8_t d = 0;
    SDA_H();
    for (uint8_t i = 0; i < 8; i++) {
        d <<= 1;
        SCL_H(); i2c_delay();
        if (SDA_READ()) d |= 1;
        SCL_L(); i2c_delay();
    }
    if (ack) SDA_L(); else SDA_H();
    i2c_delay(); SCL_H(); i2c_delay(); SCL_L(); i2c_delay();
    SDA_H();
    return d;
}

/* ============================================================
 *  SHT30：地址 0x44 —— 非阻塞状态机
 * ============================================================ */
static uint8_t  sht_state = 0;
static uint32_t sht_start_tick = 0;

static void sht30_start_measure(void)
{
    i2c_start();
    i2c_write(0x88);
    i2c_write(0x2C);
    i2c_write(0x06);
    i2c_stop();
    sht_start_tick = HAL_GetTick();
    sht_state = 1;
}

static void sht30_poll(void)
{
    if (sht_state == 0) {
        sht30_start_measure();
        return;
    }
    if (sht_state == 1) {
        if (HAL_GetTick() - sht_start_tick < 20) return;  /* 等20ms，不阻塞 */
        /* 读数据 */
        i2c_start();
        if (i2c_write(0x89)) { i2c_stop(); sht_state = 0; return; }
        uint8_t t1 = i2c_read(1);
        uint8_t t2 = i2c_read(1);
        i2c_read(1);
        uint8_t h1 = i2c_read(1);
        uint8_t h2 = i2c_read(0);
        i2c_stop();

        uint16_t tr = ((uint16_t)t1 << 8) | t2;
        uint16_t hr = ((uint16_t)h1 << 8) | h2;
        float t = -45.0f + 175.0f * tr / 65535.0f;
        float h = 100.0f * hr / 65535.0f;
        holding_reg[0] = (uint16_t)(t * 10.0f);
        holding_reg[1] = (uint16_t)(h * 10.0f);
        sht_state = 0;
    }
}

/* ============================================================
 *  BH1750：地址 0x23 —— 连续测量模式，直接读
 * ============================================================ */
static void bh1750_read(void)
{
    i2c_start();
    if (i2c_write(0x47)) { i2c_stop(); return; }
    uint8_t b1 = i2c_read(1);
    uint8_t b2 = i2c_read(0);
    i2c_stop();
    float lux = ((uint16_t)((b1 << 8) | b2)) / 1.2f;
    uint32_t v = (uint32_t)(lux * 1000.0f);
    holding_reg[2] = (uint16_t)(v >> 16);
    holding_reg[3] = (uint16_t)(v & 0xFFFF);
}

/* ============================================================
 *  对外接口 —— 每次调用立刻返回，不阻塞
 * ============================================================ */
void Sensor_Init(void)
{
    /* BH1750 上电发连续高分辨率测量命令 */
    i2c_start(); i2c_write(0x46); i2c_write(0x10); i2c_stop();
    sht_state = 0;
}

void Sensor_RefreshRegs(void)
{
    sht30_poll();   /* 非阻塞，立刻返回 */
    bh1750_read();  /* BH1750连续模式，读一次很快 */
}
