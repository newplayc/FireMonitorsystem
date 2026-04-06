/**
  ******************************************************************************
  * @file    sensors.h
  * @brief   传感器采集模块头文件
  *          包含DS18B20温度、MQ-2烟雾、MQ-7 CO传感器的采集函数
  * @author  [你的姓名]
  * @date    2024
  ******************************************************************************
  */

#ifndef __SENSORS_H
#define __SENSORS_H

#include "main.h"
#include "stm32f1xx_hal.h"

/* 滤波配置 */
#define FILTER_SAMPLES      10      /* 滑动平均采样次数 */
#define FILTER_DELAY_MS     10      /* 采样间隔ms */

/* ADC通道配置 */
#define MQ2_ADC_CHANNEL     ADC_CHANNEL_0   /* MQ-2接通道0 */
#define MQ7_ADC_CHANNEL     ADC_CHANNEL_1   /* MQ-7接通道1 */

/**
  * @brief  初始化所有传感器
  * @retval None
  */
void Sensors_Init(void);

/**
  * @brief  读取DS18B20温度传感器（高精度版本）
  * @retval 温度值（摄氏度，float精度0.0625°C）
  */
float DS18B20_ReadTemp(void);

/**
  * @brief  读取MQ-2烟雾传感器（带滑动平均滤波）
  * @retval 烟雾浓度（0-100%）
  */
uint16_t MQ2_ReadValue(void);

/**
  * @brief  读取MQ-7 CO传感器（带滑动平均滤波）
  * @retval CO浓度（0-1000 ppm）
  */
uint16_t MQ7_ReadValue(void);

/**
  * @brief  单次读取MQ-2（快速模式，无滤波）
  * @retval 原始ADC值（0-4095）
  */
uint16_t MQ2_ReadRaw(void);

/**
  * @brief  单次读取MQ-7（快速模式，无滤波）
  * @retval 原始ADC值（0-4095）
  */
uint16_t MQ7_ReadRaw(void);

/**
  * @brief  配置ADC通道
  * @param  channel: ADC通道号
  * @retval None
  */
void ADC_SelectChannel(uint32_t channel);

#endif /* __SENSORS_H */
