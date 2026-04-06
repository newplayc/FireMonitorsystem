/**
  ******************************************************************************
  * @file    main.c (创新版本)
  * @brief   主程序 - 整合AI预测算法和低功耗管理
  *          创新点展示：智能预测 + 自适应功耗
  * @author  [张伯]
  * @date    2026
  ******************************************************************************
  */

#include "main.h"
#include "adc.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include "fire_prediction.h"   /* 创新模块1 */
#include "power_manager.h"     /* 创新模块2 */
#include "sensors.h"           /* 传感器采集模块 */
#include "alarm_config.h"      /* 报警阈值配置 */

/* OLED显示用于展示预测结果 */
#include "OLED.h"

/* 修复：添加标准库头文件 */
#include <stdio.h>
#include <string.h>

/* 修复：外部函数和变量声明 */
extern UART_HandleTypeDef huart1;
extern ADC_HandleTypeDef hadc1;

/* 私有变量 */
static FirePredictor predictor;      /* 火灾预测器 */
static PowerManager powerManager;  /* 电源管理器 */

/* 传感器原始数据 */
static float currentTemp = 0.0f;
static float currentSmoke = 0.0f;
static float currentCO = 0.0f;

/* 函数声明 */
void System_Init(void);
void Sensors_Read(void);
void Display_Update(void);
void Alarm_Check(void);
void Enter_LowPower_Mode(void);
void ProcessSerialCommand(const char* cmd);
void SendThresholdStatus(void);

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
    
    /* OLED初始化 */
    OLED_Init();
    OLED_Clear();
    
    /* 创新模块初始化 */
    FirePredictor_Init(&predictor);
    PowerManager_Init(&powerManager);
    
    /* 传感器模块初始化（包含滤波算法） */
    Sensors_Init();
    
    /* 报警阈值配置初始化 */
    AlarmConfig_Init();
    
    /* 显示启动信息 */
    OLED_ShowString(0, 0, "Fire Monitor AI", 16);
    OLED_ShowString(0, 2, "Innovation Ver.", 16);
    HAL_Delay(2000);
    OLED_Clear();
    
    /* 主循环 */
    while (1)
    {
        /* 根据功耗管理器决定是否采样 */
        if (PowerManager_ShouldSample(&powerManager))
        {
            /* 1. 读取传感器数据 */
            Sensors_Read();
            
            /* 2. 更新火灾预测器（创新点1） */
            FirePredictor_Update(&predictor, currentTemp, currentSmoke, currentCO);
            
            /* 3. 根据风险值更新功耗模式（创新点2） */
            PowerManager_UpdateMode(&powerManager, &predictor);
            
            /* 4. 更新显示 */
            Display_Update();
            
            /* 5. 检查是否需要报警 */
            Alarm_Check();
            
            /* 6. 更新功耗统计 */
            PowerManager_UpdateStatistics(&powerManager);
            
            /* 7. 串口输出详细数据（用于调试和论文数据收集） */
            printf("[DATA] T:%.1fC S:%.1f%% CO:%.0f | ", 
                   currentTemp, currentSmoke, currentCO);
            printf("Risk:%.2f Mode:%s | ", 
                   predictor.currentRisk,
                   PowerManager_GetModeString(powerManager.currentMode));
            printf("BatteryLife:%.1fdays\r\n", 
                   powerManager.stats.estimatedBatteryLife);
        }
        else
        {
            /* 不需要采样时，进入低功耗模式（创新点2） */
            Enter_LowPower_Mode();
        }
    }
}

/**
  * @brief  读取所有传感器数据（使用滑动平均滤波）
  *         算法创新点：多次采样取平均，提高数据稳定性
  */
void Sensors_Read(void)
{
    /* 1. 读取DS18B20温度传感器（高精度，0.0625°C精度） */
    currentTemp = DS18B20_ReadTemp();
    
    /* 检查温度读数是否有效（传感器是否正常工作） */
    if (currentTemp < -50.0f || currentTemp > 150.0f) {
        currentTemp = 25.0f;  /* 使用默认值，防止异常数据 */
    }
    
    /* 2. 读取MQ-2烟雾传感器（带10次滑动平均滤波） */
    currentSmoke = (float)MQ2_ReadValue();
    
    /* 3. 读取MQ-7 CO传感器（带10次滑动平均滤波） */
    currentCO = (float)MQ7_ReadValue();
}

/**
  * @brief  更新OLED显示 - 展示预测结果
  */
void Display_Update(void)
{
    char buf[32];
    
    /* 第0行：传感器数据 */
    sprintf(buf, "T:%.1f S:%.0f%%", currentTemp, currentSmoke);
    OLED_ShowString(0, 0, buf, 16);
    
    /* 第2行：风险值和等级 */
    sprintf(buf, "Risk:%.2f", predictor.currentRisk);
    OLED_ShowString(0, 2, buf, 16);
    
    /* 第4行：风险等级文字 */
    char* levelStr;
    switch (predictor.riskLevel) {
        case RISK_SAFE:     levelStr = "SAFE    "; break;
        case RISK_NOTICE:   levelStr = "NOTICE  "; break;
        case RISK_WARNING:  levelStr = "WARNING "; break;
        case RISK_DANGER:   levelStr = "DANGER  "; break;
        case RISK_CRITICAL: levelStr = "CRITICAL"; break;
        default:            levelStr = "UNKNOWN "; break;
    }
    OLED_ShowString(0, 4, levelStr, 16);
    
    /* 第6行：功耗模式和预测时间 */
    if (predictor.predictionTime > 0 && predictor.isRiskIncreasing) {
        sprintf(buf, "Warn in %us", predictor.predictionTime);
        OLED_ShowString(0, 6, buf, 16);
    } else {
        sprintf(buf, "Mode:%s", PowerManager_GetModeString(powerManager.currentMode));
        OLED_ShowString(0, 6, buf, 16);
    }
}

/**
  * @brief  检查报警条件 - 整合智能预测 + 传统阈值报警
  *         支持Qt上位机动态设置阈值，带报警去抖机制
  */
void Alarm_Check(void)
{
    static uint32_t lastAlarmTime = 0;
    static uint8_t alarmConfirmCount = 0;  /* 报警确认计数（去抖） */
    uint32_t currentTime = HAL_GetTick();
    
    bool alarmTriggered = false;
    char alarmReason[64] = "";
    
    /* ========== 传统阈值检测（Qt可设置）========== */
    
    /* 1. 温度报警检测 */
    if (g_alarmConfig.tempAlarmEnabled && 
        currentTemp >= g_alarmConfig.tempThresholdHigh) {
        alarmTriggered = true;
        snprintf(alarmReason, sizeof(alarmReason), 
                 "TEMP:%.1fC >= %.1fC", 
                 currentTemp, g_alarmConfig.tempThresholdHigh);
    }
    
    /* 2. 烟雾报警检测 */
    if (g_alarmConfig.smokeAlarmEnabled && 
        currentSmoke >= g_alarmConfig.smokeThreshold) {
        alarmTriggered = true;
        snprintf(alarmReason, sizeof(alarmReason), 
                 "SMOKE:%.1f%% >= %.1f%%", 
                 currentSmoke, g_alarmConfig.smokeThreshold);
    }
    
    /* 3. CO报警检测 */
    if (g_alarmConfig.coAlarmEnabled && 
        currentCO >= g_alarmConfig.coThreshold) {
        alarmTriggered = true;
        snprintf(alarmReason, sizeof(alarmReason), 
                 "CO:%.0fppm >= %.0fppm", 
                 currentCO, g_alarmConfig.coThreshold);
    }
    
    /* ========== AI预测报警（创新点）========== */
    
    /* 4. 早期预警（在达到阈值前预测） */
    if (!alarmTriggered && FirePredictor_IsEarlyWarning(&predictor))
    {
        /* 提前预警 - 轻柔报警 */
        if (currentTime - lastAlarmTime > 3000)  /* 3秒间隔 */
        {
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);  /* LED闪烁 */
            
            printf("[EARLY WARNING] Fire predicted in %u seconds! Risk:%.2f\r\n", 
                   predictor.predictionTime, predictor.currentRisk);
            
            lastAlarmTime = currentTime;
        }
        return;  /* 早期预警已处理，不再执行后续报警 */
    }
    
    /* 5. AI风险值报警（使用Qt设置的风险阈值） */
    if (g_alarmConfig.riskAlarmEnabled && 
        !alarmTriggered && 
        predictor.currentRisk >= g_alarmConfig.riskThreshold) {
        alarmTriggered = true;
        snprintf(alarmReason, sizeof(alarmReason), 
                 "RISK:%.2f >= %.2f", 
                 predictor.currentRisk, g_alarmConfig.riskThreshold);
    }
    
    /* ========== 报警去抖机制（新增）========== */
    if (alarmTriggered) {
        alarmConfirmCount++;
        if (alarmConfirmCount < 3) {
            /* 需要连续3次确认才触发报警（去抖） */
            return;
        }
    } else {
        /* 未检测到报警，重置计数 */
        alarmConfirmCount = 0;
    }
    
    /* ========== 执行报警动作并通知上位机 ========== */
    
    if (alarmTriggered && alarmConfirmCount >= 3) {
        /* 危险等级 - 强烈报警 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);  /* LED常亮 */
        
        if (currentTime - lastAlarmTime > 3000)  /* 3秒间隔，避免重复发送 */
        {
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);  /* 蜂鸣器 */
            
            /* 发送报警信息到上位机 */
            printf("[ALARM_TRIGGERED] %s | Risk:%.2f\r\n", 
                   alarmReason, predictor.currentRisk);
            
            lastAlarmTime = currentTime;
        }
    }
    else {
        /* 安全状态 - 关闭报警 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
        
        /* 发送安全状态到上位机（可选） */
        static uint32_t lastSafeNotifyTime = 0;
        if (currentTime - lastSafeNotifyTime > 5000) {  /* 5秒通知一次 */
            printf("[ALARM_CLEARED] System Safe\r\n");
            lastSafeNotifyTime = currentTime;
        }
    }
}

/**
  * @brief  进入低功耗模式
  *         创新点2的核心实现
  */
void Enter_LowPower_Mode(void)
{
    /* 根据当前功耗模式决定睡眠策略 */
    switch (powerManager.currentMode)
    {
        case POWER_MODE_SLEEP:
        case POWER_MODE_LOW:
            /* 可以进入深度睡眠 */
            PowerManager_EnterSleep(&powerManager);
            break;
            
        case POWER_MODE_NORMAL:
        case POWER_MODE_HIGH:
        case POWER_MODE_EMERGENCY:
            /* 正常或高频模式，短暂延时即可 */
            HAL_Delay(10);
            break;
    }
}

/**
  * @brief  重定向printf到串口（用于调试和论文数据采集）
  */
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

/**
  * @brief  处理串口接收的阈值设置命令
  *         支持Qt上位机动态修改阈值
  * @param  cmd: 命令字符串
  *         格式: "TEMP:50.0" / "SMOKE:60.0" / "CO:100.0" / "RISK:0.6"
  *              "ENABLE:TEMP" / "DISABLE:CO"
  *              "GET" - 获取当前配置
  * @retval None
  */
void ProcessSerialCommand(const char* cmd)
{
    /* 处理GET命令 - 发送当前阈值配置给Qt */
    if (strcmp(cmd, "GET") == 0) {
        SendThresholdStatus();
        return;
    }
    
    /* 解析并执行阈值设置命令 */
    if (AlarmConfig_ParseCommand(cmd)) {
        printf("[CONFIG] Threshold updated: %s\r\n", cmd);
        
        /* 可选：保存到Flash实现断电保存 */
        /* AlarmConfig_SaveToFlash(); */
    } else {
        printf("[CONFIG] Invalid command: %s\r\n", cmd);
    }
}

/**
  * @brief  发送当前阈值配置状态给Qt上位机
  *         用于同步显示和数据校验
  * @retval None
  */
void SendThresholdStatus(void)
{
    char buf[128];
    
    /* 格式化当前阈值配置 */
    AlarmConfig_ToString(buf, sizeof(buf));
    
    /* 发送给Qt */
    printf("[STATUS] %s\r\n", buf);
}
