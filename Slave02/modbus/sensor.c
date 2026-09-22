#include "sensor.h"
#include "modbus_slave.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

void Sensor_Init(void)
{
    /* 连续转换模式：启动一次就自动一直转 */
    HAL_ADC_Start(&hadc1);
}

void Sensor_RefreshRegs(void)
{
    /* 直接读当前转换结果 */
    holding_reg[0] = HAL_ADC_GetValue(&hadc1);
}
