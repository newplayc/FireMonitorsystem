/**
  ******************************************************************************
  * @file    power_manager.c
  * @brief   自适应低功耗管理实现
  *          核心创新：根据火灾风险动态调整系统功耗
  * @author  [你的姓名]
  * @date    2024
  ******************************************************************************
  */

#include "power_manager.h"
#include "stm32f1xx_hal.h"  /* HAL库头文件 */
#include <string.h>
#include <stdio.h>

/* 功耗参数定义 (STM32F103典型值) */
#define CURRENT_ACTIVE      45.0f   /* 正常工作电流 (mA) */
#define CURRENT_SLEEP       8.0f    /* 睡眠模式电流 (mA) */
#define CURRENT_STOP        0.8f    /* 停机模式电流 (mA) */
#define BATTERY_CAPACITY    2000.0f /* 电池容量 (mAh) */

/**
  * @brief  初始化电源管理器
  * @param  manager: 管理器结构体指针
  * @retval None
  */
void PowerManager_Init(PowerManager* manager)
{
    memset(manager, 0, sizeof(PowerManager));
    manager->currentMode = POWER_MODE_NORMAL;
    manager->currentInterval = INTERVAL_NORMAL;
    manager->lastSampleTime = HAL_GetTick();
    manager->modeEnterTime = HAL_GetTick();
    
    /* 初始化统计信息 */
    manager->stats.averageCurrent = CURRENT_ACTIVE;
    manager->stats.estimatedBatteryLife = 
        BATTERY_CAPACITY / manager->stats.averageCurrent / 24.0f; /* 天 */
}

/**
  * @brief  根据风险等级选择功耗模式
  *         核心算法：风险越高，采样越频繁
  * @param  risk: 火灾风险值 (0-1)
  * @retval 功耗模式
  */
PowerMode PowerManager_SelectMode(float risk)
{
    if (risk < 0.1f) {
        /* 风险极低：长时间休眠 */
        return POWER_MODE_SLEEP;
    } else if (risk < 0.3f) {
        /* 风险较低：降低采样频率 */
        return POWER_MODE_LOW;
    } else if (risk < 0.5f) {
        /* 中等风险：正常采样 */
        return POWER_MODE_NORMAL;
    } else if (risk < 0.7f) {
        /* 较高风险：高频监测 */
        return POWER_MODE_HIGH;
    } else {
        /* 高风险：紧急监测 */
        return POWER_MODE_EMERGENCY;
    }
}

/**
  * @brief  更新功耗模式
  *         根据预测器输出的风险值动态调整
  * @param  manager: 管理器结构体指针
  * @param  predictor: 火灾预测器指针
  * @retval None
  */
void PowerManager_UpdateMode(PowerManager* manager, FirePredictor* predictor)
{
    PowerMode newMode = PowerManager_SelectMode(predictor->currentRisk);
    
    /* 如果模式发生变化 */
    if (newMode != manager->currentMode) {
        /* 记录旧模式的持续时间 */
        uint32_t duration = HAL_GetTick() - manager->modeEnterTime;
        
        /* 更新统计 */
        switch (manager->currentMode) {
            case POWER_MODE_SLEEP:
                manager->sleepCount++;
                manager->stats.sleepTime += duration / 1000;
                break;
            case POWER_MODE_LOW:
                manager->lowCount++;
                break;
            case POWER_MODE_NORMAL:
                manager->normalCount++;
                break;
            case POWER_MODE_HIGH:
                manager->highCount++;
                manager->stats.activeTime += duration / 1000;
                break;
            case POWER_MODE_EMERGENCY:
                manager->emergencyCount++;
                manager->stats.activeTime += duration / 1000;
                break;
        }
        
        /* 切换到新模式 */
        manager->currentMode = newMode;
        manager->modeEnterTime = HAL_GetTick();
        
        /* 设置新的采样间隔 */
        switch (newMode) {
            case POWER_MODE_SLEEP:
                manager->currentInterval = INTERVAL_SLEEP;
                break;
            case POWER_MODE_LOW:
                manager->currentInterval = INTERVAL_LOW;
                break;
            case POWER_MODE_NORMAL:
                manager->currentInterval = INTERVAL_NORMAL;
                break;
            case POWER_MODE_HIGH:
                manager->currentInterval = INTERVAL_HIGH;
                break;
            case POWER_MODE_EMERGENCY:
                manager->currentInterval = INTERVAL_EMERGENCY;
                break;
        }
        
#ifdef DEBUG_MODE
        printf("[Power] Mode switched to %s, interval: %lu ms\n",
               PowerManager_GetModeString(newMode),
               manager->currentInterval);
#endif
    }
}

/**
  * @brief  判断是否应该采样
  * @param  manager: 管理器结构体指针
  * @retval true: 应该采样, false: 继续休眠
  */
bool PowerManager_ShouldSample(PowerManager* manager)
{
    uint32_t currentTime = HAL_GetTick();
    uint32_t elapsed = currentTime - manager->lastSampleTime;
    
    if (elapsed >= manager->currentInterval) {
        manager->lastSampleTime = currentTime;
        return true;
    }
    
    return false;
}

/**
  * @brief  进入低功耗睡眠模式
  *         在两次采样之间进入睡眠节省电量
  * @param  manager: 管理器结构体指针
  * @retval None
  */
void PowerManager_EnterSleep(PowerManager* manager)
{
    /* 根据当前模式选择睡眠深度 */
    switch (manager->currentMode) {
        case POWER_MODE_SLEEP:
            /* 深度睡眠：进入STOP模式 */
            /* 关闭外设 */
            HAL_SuspendTick();
            
            /* 进入STOP模式 - 功耗最低 */
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
            
            /* 唤醒后恢复 */
            SystemClock_Config();
            HAL_ResumeTick();
            break;
            
        case POWER_MODE_LOW:
            /* 中度睡眠：SLEEP模式 */
            HAL_SuspendTick();
            HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
            HAL_ResumeTick();
            break;
            
        case POWER_MODE_NORMAL:
        case POWER_MODE_HIGH:
        case POWER_MODE_EMERGENCY:
            /* 正常或高频模式：短暂延时，不进入深度睡眠 */
            /* 使用HAL_Delay会让CPU空转，但响应更快 */
            break;
    }
}

/**
  * @brief  唤醒后的初始化
  * @param  manager: 管理器结构体指针
  * @retval None
  */
void PowerManager_WakeUp(PowerManager* manager)
{
    /* 如果需要，重新初始化外设 */
    if (manager->currentMode == POWER_MODE_SLEEP) {
        /* STOP模式唤醒后需要重新配置时钟 */
        SystemClock_Config();
        
        /* 重新初始化ADC和串口 */
        extern void MX_ADC1_Init(void);
        extern void MX_USART1_UART_Init(void);
        MX_ADC1_Init();
        MX_USART1_UART_Init();
    }
}

/**
  * @brief  更新功耗统计
  * @param  manager: 管理器结构体指针
  * @retval None
  */
void PowerManager_UpdateStatistics(PowerManager* manager)
{
    /* 计算各模式的时间占比 */
    uint32_t totalTime = manager->stats.totalRuntime;
    if (totalTime == 0) return;
    
    /* 计算加权平均电流 */
    float weightedCurrent = 0;
    
    /* 假设各模式占比 */
    float sleepRatio = (float)manager->stats.sleepTime / totalTime;
    float activeRatio = (float)manager->stats.activeTime / totalTime;
    float normalRatio = 1.0f - sleepRatio - activeRatio;
    
    weightedCurrent = sleepRatio * CURRENT_STOP +
                      normalRatio * CURRENT_SLEEP +
                      activeRatio * CURRENT_ACTIVE;
    
    manager->stats.averageCurrent = weightedCurrent;
    
    /* 计算预计续航时间 */
    manager->stats.estimatedBatteryLife = 
        BATTERY_CAPACITY / weightedCurrent / 24.0f; /* 转换为天 */
}

/**
  * @brief  获取模式字符串
  * @param  mode: 功耗模式
  * @retval 模式名称字符串
  */
const char* PowerManager_GetModeString(PowerMode mode)
{
    switch (mode) {
        case POWER_MODE_SLEEP:      return "SLEEP";
        case POWER_MODE_LOW:        return "LOW";
        case POWER_MODE_NORMAL:     return "NORMAL";
        case POWER_MODE_HIGH:       return "HIGH";
        case POWER_MODE_EMERGENCY:  return "EMERGENCY";
        default:                    return "UNKNOWN";
    }
}

/**
  * @brief  获取当前模式的电流消耗
  * @param  mode: 功耗模式
  * @retval 电流值 (mA)
  */
float PowerManager_GetCurrentConsumption(PowerMode mode)
{
    switch (mode) {
        case POWER_MODE_SLEEP:      return CURRENT_STOP;
        case POWER_MODE_LOW:        return CURRENT_SLEEP;
        case POWER_MODE_NORMAL:     return CURRENT_ACTIVE * 0.5f;
        case POWER_MODE_HIGH:       return CURRENT_ACTIVE;
        case POWER_MODE_EMERGENCY:  return CURRENT_ACTIVE * 1.2f;
        default:                    return CURRENT_ACTIVE;
    }
}
