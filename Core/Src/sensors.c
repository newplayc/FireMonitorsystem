/**
  ******************************************************************************
  * @file    sensors.c
  * @brief   传感器采集模块实现
  *          DS18B20温度 + MQ-2烟雾 + MQ-7 CO传感器
  *          采用滑动平均滤波算法提高数据稳定性
  * @author  [你的姓名]
  * @date    2024
  ******************************************************************************
  */

#include "sensors.h"
#include "DS18B20.h"
#include <string.h>

/* 外部ADC句柄声明 */
extern ADC_HandleTypeDef hadc1;

/**
  * @brief  初始化所有传感器
  * @retval None
  */
void Sensors_Init(void)
{
    /* DS18B20初始化 */
    DS18B20_Init();
    
    /* MQ传感器预热（建议上电后等待60秒） */
    HAL_Delay(1000);  /* 基础延时，实际预热建议60秒 */
    
    /* ADC校准（提高精度） */
    HAL_ADCEx_Calibration_Start(&hadc1);
}

/**
  * @brief  读取DS18B20温度传感器（高精度版本）
  *         使用单总线协议，12位精度（0.0625°C）
  * @retval 温度值（摄氏度，float类型）
  */
float DS18B20_ReadTemp(void)
{
    uint8_t temp[2];
    int16_t temperature;
    
    /* 第1步：发送温度转换命令 */
    DS18B20_Rst();                          /* 复位脉冲 */
    if (DS18B20_Check() != 0) return -999.0f;  /* 检查设备是否存在 */
    
    DS18B20_WriteByte(0xCC);              /* 跳过ROM（单设备） */
    DS18B20_WriteByte(0x44);              /* 启动温度转换命令 */
    
    HAL_Delay(750);                         /* 等待转换完成（必须>=750ms） */
    
    /* 第2步：读取温度值 */
    DS18B20_Rst();                          /* 再次复位 */
    if (DS18B20_Check() != 0) return -999.0f;
    
    DS18B20_WriteByte(0xCC);              /* 跳过ROM */
    DS18B20_WriteByte(0xBE);              /* 读取暂存器命令 */
    
    temp[0] = DS18B20_Read_Byte();        /* 温度低字节 */
    temp[1] = DS18B20_Read_Byte();        /* 温度高字节 */
    
    /* 合并数据 */
    temperature = (temp[1] << 8) | temp[0];
    
    /* 转换为摄氏度（12位精度，每bit=0.0625°C） */
    return (float)temperature * 0.0625f;
}

/**
  * @brief  配置ADC通道
  * @param  channel: ADC通道号（如ADC_CHANNEL_0）
  * @retval None
  */
void ADC_SelectChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;  /* 71.5个周期采样时间 */
    
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/**
  * @brief  单次读取MQ-2（快速模式，无滤波）
  * @retval 原始ADC值（0-4095）
  */
uint16_t MQ2_ReadRaw(void)
{
    uint32_t adcValue = 0;
    
    /* 选择通道0 */
    ADC_SelectChannel(MQ2_ADC_CHANNEL);
    
    /* 启动转换 */
    HAL_ADC_Start(&hadc1);
    
    /* 等待转换完成（超时100ms） */
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
        adcValue = HAL_ADC_GetValue(&hadc1);
    }
    
    HAL_ADC_Stop(&hadc1);
    
    return (uint16_t)adcValue;
}

/**
  * @brief  读取MQ-2烟雾传感器（带滑动平均滤波）
  *         算法创新点：多次采样取平均，去除随机噪声
  * @retval 烟雾浓度（0-100%）
  */
uint16_t MQ2_ReadValue(void)
{
    uint32_t adc_sum = 0;
    uint16_t adc_value;
    
    /* 滑动平均滤波：采集10次取平均 */
    for (int i = 0; i < FILTER_SAMPLES; i++) {
        adc_sum += MQ2_ReadRaw();
        HAL_Delay(FILTER_DELAY_MS);  /* 采样间隔，减少连续采样误差 */
    }
    
    /* 计算平均值 */
    adc_value = (uint16_t)(adc_sum / FILTER_SAMPLES);
    
    /* 转换为烟雾浓度（0-100%）
     * 原理：ADC值越小，烟雾浓度越高（MQ-2是电阻型传感器）
     * 公式：(4095 - ADC) / 4095 * 100 */
    if (adc_value > 4095) adc_value = 4095;
    
    return (uint16_t)((4095 - adc_value) * 100 / 4095);
}

/**
  * @brief  单次读取MQ-7（快速模式，无滤波）
  * @retval 原始ADC值（0-4095）
  */
uint16_t MQ7_ReadRaw(void)
{
    uint32_t adcValue = 0;
    
    /* 选择通道1 */
    ADC_SelectChannel(MQ7_ADC_CHANNEL);
    
    /* 启动转换 */
    HAL_ADC_Start(&hadc1);
    
    /* 等待转换完成 */
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
        adcValue = HAL_ADC_GetValue(&hadc1);
    }
    
    HAL_ADC_Stop(&hadc1);
    
    return (uint16_t)adcValue;
}

/**
  * @brief  读取MQ-7 CO传感器（带滑动平均滤波）
  *         算法创新点：多次采样取平均，提高CO浓度检测稳定性
  * @retval CO浓度（0-1000 ppm）
  */
uint16_t MQ7_ReadValue(void)
{
    uint32_t adc_sum = 0;
    uint16_t adc_value;
    
    /* 滑动平均滤波：采集10次取平均 */
    for (int i = 0; i < FILTER_SAMPLES; i++) {
        adc_sum += MQ7_ReadRaw();
        HAL_Delay(FILTER_DELAY_MS);
    }
    
    /* 计算平均值 */
    adc_value = (uint16_t)(adc_sum / FILTER_SAMPLES);
    
    /* 转换为CO浓度（0-1000 ppm）
     * 注意：MQ-7需要预热60秒，且需要定期校准
     * 此处使用线性近似，实际应用需根据传感器特性校准 */
    if (adc_value > 4095) adc_value = 4095;
    
    return (uint16_t)(adc_value * 1000 / 4095);
}
