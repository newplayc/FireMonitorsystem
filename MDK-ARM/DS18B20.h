#ifndef __DS18__H
#define __DS18__H


#include "main.h"
#define  DS18B20_DQ_OUT_HIGH       HAL_GPIO_WritePin(DQ_GPIO_Port, DQ_Pin, GPIO_PIN_SET)
#define  DS18B20_DQ_OUT_LOW        HAL_GPIO_WritePin(DQ_GPIO_Port, DQ_Pin, GPIO_PIN_RESET)
#define  DS18B20_DQ_IN             HAL_GPIO_ReadPin(DQ_GPIO_Port, DQ_Pin)

/* 初始化函数 */
uint8_t DS18B20_Init(void);

/* 温度读取函数 */
short DS18B20_Get_Temperature(void);

/* 单总线底层操作函数（供外部使用） */
void DS18B20_Rst(void);
uint8_t DS18B20_Check(void);
void DS18B20_WriteByte(uint8_t data);
uint8_t DS18B20_Read_Byte(void);
void DS18B20_IO_IN(void);
void DS18B20_IO_OUT(void);

#endif
