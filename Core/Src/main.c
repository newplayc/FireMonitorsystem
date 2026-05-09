/**
 ******************************************************************************
 * @file    main.c - 完整功能版本
 * @brief   火灾监控系统主程序
 *          功能：传感器采集 + AI预测 + 报警检测 + 阈值设置 + 报警通知
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

/* 按键定义 */
#define KEY_PIN GPIO_PIN_13
#define KEY_PORT GPIOC

/* OLED显示行坐标定义（像素值 - 根据OLED库修正）*/
#define OLED_ROW_0    0   // 第1行 (Y=0像素)
#define OLED_ROW_1    16  // 第2行 (Y=16像素)
#define OLED_ROW_2    32  // 第3行 (Y=32像素)
#define OLED_ROW_3    48  // 第4行 (Y=48像素)
#define OLED_ROW_4    56  // 第5行 (Y=56像素)

/* 私有变量 */
static FirePredictor predictor;

/* ADC DMA缓冲区 - 全局变量 */
volatile uint16_t adc_dma_buffer[2] = {0};

/* 传感器数据 */
static float currentTemp = 0.0f;
static float currentSmoke = 0.0f;
static float currentCO = 0.0f;

/* DS18B20状态 */
static uint8_t ds18b20_ok = 0;

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
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc);
void Check_System_Status(void);
void Test_OLED(void);
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
    printf("SystemClock configured\r\n");

    /* 外设初始化 */
    MX_GPIO_Init();
    printf("GPIO initialized\r\n");
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();
    printf("All peripherals initialized\r\n");

    /* 系统初始化 */
    System_Init();

    /* 添加调试输出 */
    printf("\r\n=== Fire Monitor System Started ===\r\n");
    printf("ADC DMA Buffer: [%d, %d]\r\n", adc_dma_buffer[0], adc_dma_buffer[1]);

    /* 启动ADC DMA */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buffer, 2) != HAL_OK) {
        printf("ADC DMA Start Error!\r\n");
    } else {
        printf("ADC DMA Started\r\n");
    }

    /* 启动串口接收 */
    HAL_UART_Receive_IT(&huart1, &rxBuffer[0], 1);

    /* 主循环 */
    uint32_t lastDataSend = 0;

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

        /* 5. 每200ms发送数据到上位机 */
        if (HAL_GetTick() - lastDataSend > 200) {
            Send_Data_To_PC();
            lastDataSend = HAL_GetTick();
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
    
    /* 显示启动信息 - 使用像素坐标 */
    OLED_ShowString(0, OLED_ROW_0, "Fire Monitor", OLED_6X8);
    OLED_ShowString(0, OLED_ROW_2, "Starting...", OLED_6X8);
    OLED_Update();
    HAL_Delay(1000);
    
    /* 初始化传感器 */
    printf("Initializing sensors...\r\n");
    Sensors_Init();
    
    /* 检测DS18B20 */
    if(DS18B20_Init() == 0) {
        ds18b20_ok = 1;
        OLED_ShowString(0, OLED_ROW_3, "DS18B20: OK", OLED_6X8);
        printf("DS18B20 found!\r\n");
    } else {
        ds18b20_ok = 0;
        OLED_ShowString(0, OLED_ROW_3, "DS18B20: ERR", OLED_6X8);
        printf("DS18B20 not found! Check wiring and pullup resistor.\r\n");
    }
    OLED_Update();
    HAL_Delay(2000);
    
    /* 清屏 */
    OLED_Clear();
    OLED_Update();
    
    /* 关闭蜂鸣器（低电平触发，所以设为高电平）*/
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);

    /* 测试OLED */
    Test_OLED();
}

/**
 * @brief  读取传感器数据
 */
void Sensors_Read(void)
{
    static uint32_t lastTempTime = 0;
    uint32_t currentTime = HAL_GetTick();

    /* 每2秒读取一次温度 */
    if (currentTime - lastTempTime > 2000) {
        if(ds18b20_ok) {
            int16_t temp_raw = DS18B20_Get_Temperature();
            if (temp_raw > 0 && temp_raw < 850) {
                currentTemp = (float)temp_raw / 10.0f;
                printf("Temperature: %.1f°C\r\n", currentTemp);
            } else {
                printf("Temperature read error: %d\r\n", temp_raw);
            }
        }
        lastTempTime = currentTime;
    }

    /* 读取烟雾和CO（从DMA缓冲区） */
    if (adc_dma_buffer[0] != 0 || adc_dma_buffer[1] != 0) {
        /* 烟雾传感器 - PA6 (ADC1_IN6) */
        currentSmoke = (float)adc_dma_buffer[0] * 100.0f / 4095.0f;
        if(currentSmoke > 100) currentSmoke = 100;
        
        /* CO传感器 - PA2 (ADC1_IN2) */
        currentCO = (float)adc_dma_buffer[1] * 1000.0f / 4095.0f;
        
        /* 减少调试输出 */
        static uint32_t lastPrint = 0;
        if(HAL_GetTick() - lastPrint > 3000) {
            printf("ADC: Smoke=%d(%.0f%%) CO=%d(%.0fppm)\r\n", 
                   adc_dma_buffer[0], currentSmoke, adc_dma_buffer[1], currentCO);
            lastPrint = HAL_GetTick();
        }
    }
}

/**
 * @brief  更新OLED显示 - 使用像素坐标，不重叠
 */
void Display_Update(void)
{
    char buf[32];
    
    /* 第1行：温度 (Y=0) */
    if(ds18b20_ok && currentTemp > 0) {
        sprintf(buf, "Temp:%.1fC", currentTemp);
    } else {
        sprintf(buf, "Temp:Error");
    }
    OLED_ShowString(0, OLED_ROW_0, buf, OLED_6X8);
    
    /* 第2行：烟雾浓度 (Y=16) */
    sprintf(buf, "Smog:%.0f%%", currentSmoke);
    OLED_ShowString(0, OLED_ROW_1, buf, OLED_6X8);
    
    /* 第3行：CO浓度 (Y=32) */
    sprintf(buf, "CO:%.0fppm", currentCO);
    OLED_ShowString(0, OLED_ROW_2, buf, OLED_6X8);
    
    /* 第4行：风险值 (Y=48) */
    sprintf(buf, "Risk:%.2f", predictor.currentRisk);
    OLED_ShowString(0, OLED_ROW_3, buf, OLED_6X8);
    
    /* 第5行：报警状态 (Y=56) */
    if (alarmState) {
        OLED_ShowString(0, OLED_ROW_4, "!!! ALARM !!!", OLED_6X8);
    } else {
        OLED_ShowString(0, OLED_ROW_4, "Normal", OLED_6X8);
    }
    
    /* 更新显示 */
    OLED_Update();

    /* 每2秒打印一次数据用于调试 */
    static uint32_t lastPrintTime = 0;
    if (HAL_GetTick() - lastPrintTime > 2000) {
        printf("T:%.1f S:%.0f%% CO:%.0fppm Risk:%.2f\r\n",
               currentTemp, currentSmoke, currentCO, predictor.currentRisk);
        lastPrintTime = HAL_GetTick();
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
        alarmState = 1;
        Send_Alarm_Notification();
        printf("[ALARM] 报警触发! T:%.1f S:%.0f%% CO:%.0fppm\r\n", 
               currentTemp, currentSmoke, currentCO);
    }
    else if (!newAlarmState && alarmState) {
        alarmState = 0;
        printf("[ALARM Cleared] 报警解除\r\n");
    }

    /* 蜂鸣器控制（低电平触发）*/
    static uint32_t lastBeep = 0;
    if (alarmState) {
        /* 蜂鸣器响300ms，停300ms */
        if (HAL_GetTick() - lastBeep > 300) {
            static uint8_t beepState = 0;
            beepState = !beepState;
            HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, beepState ? GPIO_PIN_RESET : GPIO_PIN_SET);
            lastBeep = HAL_GetTick();
        }
    } else {
        HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
    }
}

/**
 * @brief  发送报警通知到上位机
 */
void Send_Alarm_Notification(void)
{
    char alarmMsg[128];
    sprintf(alarmMsg, "[ALARM] T:%.1f S:%.0f%% CO:%.0fppm Risk:%.2f\r\n",
            currentTemp, currentSmoke, currentCO, predictor.currentRisk);
    HAL_UART_Transmit(&huart1, (uint8_t*)alarmMsg, strlen(alarmMsg), 100);
}

/**
 * @brief  发送数据到上位机
 */
void Send_Data_To_PC(void)
{
    /* 二进制数据帧格式: [0xFF][温度低][温度高][烟雾低][烟雾高][CO低][CO高][风险等级][0xFE] */
    uint8_t data[9] = {0xFF};

    /* 温度（乘以10保留1位小数） */
    uint16_t tempInt = (uint16_t)(currentTemp * 10);
    data[1] = tempInt & 0xFF;
    data[2] = (tempInt >> 8) & 0xFF;

    /* 烟雾（百分比） */
    uint16_t smokeInt = (uint16_t)currentSmoke;
    data[3] = smokeInt & 0xFF;
    data[4] = (smokeInt >> 8) & 0xFF;

    /* CO（ppm） */
    uint16_t coInt = (uint16_t)currentCO;
    data[5] = coInt & 0xFF;
    data[6] = (coInt >> 8) & 0xFF;

    /* 风险等级（0-4） */
    data[7] = predictor.riskLevel;

    /* 帧尾 */
    data[8] = 0xFE;

    /* 发送二进制数据帧 */
    HAL_UART_Transmit(&huart1, data, 9, 100);
}

/**
 * @brief  处理串口命令
 */
void ProcessSerialCommand(void)
{
    if (rxIndex > 0 && (rxBuffer[rxIndex-1] == '\n' || rxBuffer[rxIndex-1] == '\r')) {
        rxBuffer[rxIndex-1] = '\0';
        
        /* 移除可能的回车符 */
        if(rxIndex > 1 && rxBuffer[rxIndex-2] == '\r') {
            rxBuffer[rxIndex-2] = '\0';
        }
        
        printf("[CMD] Received: %s\r\n", rxBuffer);
        
        /* 解析命令 */
        if (AlarmConfig_ParseCommand((char*)rxBuffer)) {
            printf("[CMD] Threshold updated\r\n");
            SendThresholdStatus();
        } else {
            /* 其他命令处理 */
            if (strcmp((char*)rxBuffer, "STATUS") == 0) {
                SendThresholdStatus();
            } else if (strcmp((char*)rxBuffer, "RESET") == 0) {
                AlarmConfig_Init();
                SendThresholdStatus();
            } else {
                printf("[CMD] Unknown: %s\r\n", rxBuffer);
            }
        }
    }
}

/**
 * @brief  发送当前阈值配置
 */
void SendThresholdStatus(void)
{
    char buf[128];
    sprintf(buf, "[CONFIG] Temp_H:%.0f Temp_L:%.0f Smoke:%.0f CO:%.0f\r\n",
            g_alarmConfig.tempThresholdHigh,
            g_alarmConfig.tempThresholdLow,
            g_alarmConfig.smokeThreshold,
            g_alarmConfig.coThreshold);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 100);
    printf("%s", buf);
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
        
        /* 检查是否收到换行符 */
        if (rxIndex > 0 && (rxBuffer[rxIndex-1] == '\n' || rxBuffer[rxIndex-1] == '\r')) {
            rxComplete = 1;
        }
    }
}

/**
 * @brief  重定向printf到串口
 */
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
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
    printf("System Error!\r\n");
    __disable_irq();
    while (1)
    {
        HAL_GPIO_TogglePin(Buzzer_GPIO_Port, Buzzer_Pin);
        HAL_Delay(200);
    }
}

/**
 * @brief  ADC DMA转换完成回调函数
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if(hadc == &hadc1)
    {
        /* 重新启动DMA转换 */
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buffer, 2);
    }
}

/**
 * @brief  系统状态检查函数
 */
void Check_System_Status(void)
{
    printf("[STATUS] Temp:%.1f Smoke:%.0f%% CO:%.0fppm Risk:%.2f ADC:[%d,%d]\r\n",
           currentTemp, currentSmoke, currentCO, predictor.currentRisk,
           adc_dma_buffer[0], adc_dma_buffer[1]);
}

/**
 * @brief  OLED测试函数
 */
void Test_OLED(void)
{
    OLED_Clear();
    
    /* 使用像素坐标测试 */
    OLED_ShowString(0, OLED_ROW_0, "STM32 OK", OLED_6X8);
    OLED_ShowString(0, OLED_ROW_1, "ADC Ready", OLED_6X8);
    OLED_ShowString(0, OLED_ROW_2, "UART OK", OLED_6X8);
    OLED_ShowString(0, OLED_ROW_3, "DS18B20 Ready", OLED_6X8);
    
    OLED_Update();
    HAL_Delay(2000);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif