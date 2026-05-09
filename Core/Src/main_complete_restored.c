/**
 ******************************************************************************
 * @file    main.c - 完整功能版本
 * @brief   火灾监控系统主程序
 *          功能：传感器采集 + AI预测 + 报警检测 + 阈值设置 + 报警通知
 * @author  [张伯]
 * @date    2026
 ******************************************************************************
 */

#include "main.h"
#include "adc.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include "DS18B20.h"
#include "OLED.h"
#include "fire_prediction.h"
#include "sensors.h"
#include "alarm_config.h"
#include <stdio.h>
#include <string.h>

/* 外部变量声明 */
extern UART_HandleTypeDef huart1;
extern ADC_HandleTypeDef hadc1;

/* 私有变量 */
static FirePredictor predictor;

/* 传感器数据 */
static float currentTemp = 0.0f;
static float currentSmoke = 0.0f;
static float currentCO = 0.0f;

/* 串口接收缓冲区 */
#define RX_BUFFER_SIZE 64
static uint8_t rxBuffer[RX_BUFFER_SIZE];
static uint8_t rxIndex = 0;
static uint8_t rxComplete = 0;

/* 报警状态 */
static uint8_t alarmState = 0;

/* 函数声明 */
void SystemClock_Config(void);
void System_Init(void);
void Sensors_Read(void);
void Display_Update(void);
void Check_Alarm(void);
void ProcessSerialCommand(void);
void SendThresholdStatus(void);
void Send_Alarm_Notification(void);
void Send_Data_To_PC(void);

/**
 * @brief  主函数
 */
int main(void)
{
    /* HAL库初始化 */
    HAL_Init();
    SystemClock_Config();

    /* 外设初始化 */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();

    /* 系统初始化 */
    System_Init();

    /* 启动ADC DMA */
    uint16_t adc_values[2] = {0};
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_values, 2);

    /* 启动串口接收 */
    HAL_UART_Receive_IT(&huart1, &rxBuffer[0], 1);

    /* 主循环 */
    while (1)
    {
        /* 1. 读取传感器数据 */
        Sensors_Read();

        /* 2. 更新火灾预测器 */
        FirePredictor_Update(&predictor, currentTemp, currentSmoke, currentCO);

        /* 3. 更新显示 */
        Display_Update();

        /* 4. 检查报警 */
        Check_Alarm();

        /* 5. 定期发送数据到上位机 */
        if (HAL_GetTick() % 200 == 0) {
            Send_Data_To_PC();
        }

        /* 6. 处理串口命令 */
        if (rxComplete) {
            ProcessSerialCommand();
            rxComplete = 0;
            rxIndex = 0;
            HAL_UART_Receive_IT(&huart1, &rxBuffer[0], 1);
        }

        HAL_Delay(10);
    }
}

/**
 * @brief  系统初始化
 */
void System_Init(void)
{
    /* 初始化火灾预测器 */
    FirePredictor_Init(&predictor);

    /* 初始化报警配置 */
    AlarmConfig_Init();

    /* OLED初始化 */
    OLED_Init();
    OLED_Clear();

    /* 显示启动信息 */
    OLED_ShowString(0, 0, "Fire Monitor", 16);
    OLED_ShowString(0, 2, "Complete Ver", 16);
    HAL_Delay(2000);
    OLED_Clear();

    /* 关闭蜂鸣器 */
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
}

/**
 * @brief  读取传感器数据
 */
void Sensors_Read(void)
{
    /* 读取DS18B20温度 */
    currentTemp = DS18B20_ReadTemp();

    /* 读取烟雾和CO（从DMA缓冲区） */
    static uint16_t adc_values[2] = {0};
    currentSmoke = (float)(4095 - adc_values[0]) * 100.0f / 4095.0f;
    currentCO = (float)adc_values[1] * 1000.0f / 4095.0f;
}

/**
 * @brief  更新OLED显示
 */
void Display_Update(void)
{
    char buf[32];

    /* 传感器数据 */
    sprintf(buf, "T:%.1f S:%.0f%%", currentTemp, currentSmoke);
    OLED_ShowString(0, 0, buf, 12);

    /* 风险值 */
    sprintf(buf, "Risk:%.2f", predictor.currentRisk);
    OLED_ShowString(0, 2, buf, 12);

    /* 报警状态 */
    if (alarmState) {
        OLED_ShowString(0, 4, "ALARM!", 12);
    } else {
        OLED_ShowString(0, 4, "Normal", 12);
    }
}

/**
 * @brief  检查报警条件
 */
void Check_Alarm(void)
{
    uint8_t newAlarmState = 0;

    /* 检查温度报警 */
    if (currentTemp >= g_alarmConfig.tempThresholdHigh) {
        newAlarmState = 1;
    }
    /* 检查烟雾报警 */
    else if (currentSmoke >= g_alarmConfig.smokeThreshold) {
        newAlarmState = 1;
    }
    /* 检查CO报警 */
    else if (currentCO >= g_alarmConfig.coThreshold) {
        newAlarmState = 1;
    }
    /* 检查AI风险报警 */
    else if (predictor.riskLevel >= RISK_DANGER) {
        newAlarmState = 1;
    }

    /* 状态变化处理 */
    if (newAlarmState && !alarmState) {
        /* 触发新报警 */
        alarmState = 1;

        /* 发送报警通知 */
        Send_Alarm_Notification();

        /* LED指示 */
        HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);

        printf("[ALARM] 报警触发!\r\n");
    }
    else if (!newAlarmState && alarmState) {
        /* 报警解除 */
        alarmState = 0;
        HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
        printf("[ALARM Cleared] 报警解除\r\n");
    }

    /* 蜂鸣器控制 */
    if (alarmState) {
        if (HAL_GetTick() % 500 < 250) {
            HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
        }
    }
}

/**
 * @brief  发送报警通知到上位机
 */
void Send_Alarm_Notification(void)
{
    /* 报警帧格式: [0xAA][报警类型][报警值][风险等级][0x55] */
    uint8_t alarmData[5];

    /* 判断报警类型 */
    if (currentTemp >= g_alarmConfig.tempThresholdHigh) {
        alarmData[0] = 0xAA;
        alarmData[1] = 0x01;  // 温度报警
        alarmData[2] = (uint8_t)currentTemp;
        alarmData[3] = predictor.riskLevel;
        alarmData[4] = 0x55;
    }
    else if (currentSmoke >= g_alarmConfig.smokeThreshold) {
        alarmData[0] = 0xAA;
        alarmData[1] = 0x02;  // 烟雾报警
        alarmData[2] = (uint8_t)currentSmoke;
        alarmData[3] = predictor.riskLevel;
        alarmData[4] = 0x55;
    }
    else if (currentCO >= g_alarmConfig.coThreshold) {
        alarmData[0] = 0xAA;
        alarmData[1] = 0x03;  // CO报警
        alarmData[2] = (uint8_t)(currentCO / 10);
        alarmData[3] = predictor.riskLevel;
        alarmData[4] = 0x55;
    }
    else {
        alarmData[0] = 0xAA;
        alarmData[1] = 0x04;  // 风险报警
        alarmData[2] = (uint8_t)(predictor.currentRisk * 100);
        alarmData[3] = predictor.riskLevel;
        alarmData[4] = 0x55;
    }

    /* 发送报警帧 */
    HAL_UART_Transmit(&huart1, alarmData, 5, 100);

    /* 发送文字描述 */
    char alarmMsg[64];
    sprintf(alarmMsg, "[ALARM] Temp:%.1f Smoke:%.0f CO:%.0f Risk:%.2f\r\n",
            currentTemp, currentSmoke, currentCO, predictor.currentRisk);
    HAL_UART_Transmit(&huart1, (uint8_t*)alarmMsg, strlen(alarmMsg), 100);
}

/**
 * @brief  发送数据到上位机
 */
void Send_Data_To_PC(void)
{
    /* 数据帧格式 */
    uint8_t data[9] = {0xFF};

    /* 温度 */
    uint16_t tempInt = (uint16_t)(currentTemp * 10);
    data[1] = tempInt & 0xFF;
    data[2] = (tempInt >> 8) & 0xFF;

    /* 烟雾 */
    uint16_t smokeInt = (uint16_t)currentSmoke;
    data[3] = smokeInt & 0xFF;
    data[4] = (smokeInt >> 8) & 0xFF;

    /* CO */
    uint16_t coInt = (uint16_t)currentCO;
    data[5] = coInt & 0xFF;
    data[6] = (coInt >> 8) & 0xFF;

    /* 风险等级 */
    data[7] = predictor.riskLevel;

    /* 帧尾 */
    data[8] = 0xFE;

    /* 发送 */
    HAL_UART_Transmit(&huart1, data, 9, 100);
}

/**
 * @brief  处理串口命令
 */
void ProcessSerialCommand(void)
{
    /* 检查命令结束 */
    if (rxBuffer[rxIndex-1] == '\n' || rxBuffer[rxIndex-1] == '\r') {
        rxBuffer[rxIndex-1] = '\0';

        /* 解析命令 */
        if (AlarmConfig_ParseCommand((char*)rxBuffer)) {
            printf("[CMD] Success: %s\r\n", rxBuffer);
            SendThresholdStatus();
        } else {
            printf("[CMD] Failed: %s\r\n", rxBuffer);
        }
    }
}

/**
 * @brief  发送当前阈值配置
 */
void SendThresholdStatus(void)
{
    char buf[128];
    AlarmConfig_ToString(buf, sizeof(buf));
    printf("[STATUS] %s\r\n", buf);
}

/**
 * @brief  串口接收回调函数
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        if (rxIndex < RX_BUFFER_SIZE - 1) {
            rxIndex++;
            HAL_UART_Receive_IT(&huart1, &rxBuffer[rxIndex], 1);
        } else {
            rxIndex = 0;
            HAL_UART_Receive_IT(&huart1, &rxBuffer[0], 1);
        }

        if (rxBuffer[rxIndex-1] == '\n' || rxBuffer[rxIndex-1] == '\r') {
            rxComplete = 1;
        }
    }
}

/**
 * @brief  重定向printf到串口
 */
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

/**
 * @brief  系统时钟配置
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief  错误处理函数
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        HAL_GPIO_TogglePin(Buzzer_GPIO_Port, Buzzer_Pin);
        HAL_Delay(200);
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* User can add his own implementation */
}
#endif