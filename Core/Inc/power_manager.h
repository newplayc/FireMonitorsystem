/**
  ******************************************************************************
  * @file    power_manager.h
  * @brief   自适应低功耗管理模块 - 创新点2
  *          根据风险等级动态调整功耗模式
  * @author  [你的姓名]
  * @date    2024
  ******************************************************************************
  */

#ifndef __POWER_MANAGER_H
#define __POWER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "fire_prediction.h"  /* 引入预测算法 */

/* 功耗模式定义 */
typedef enum {
    POWER_MODE_SLEEP,       /* 睡眠模式: 30秒采样一次 */
    POWER_MODE_LOW,         /* 低功耗: 10秒采样一次 */
    POWER_MODE_NORMAL,      /* 正常: 2秒采样一次 */
    POWER_MODE_HIGH,        /* 高频: 1秒采样一次 */
    POWER_MODE_EMERGENCY    /* 紧急: 0.5秒采样一次 */
} PowerMode;

/* 采样间隔定义 (毫秒) */
#define INTERVAL_SLEEP      30000   /* 30秒 */
#define INTERVAL_LOW        10000   /* 10秒 */
#define INTERVAL_NORMAL     2000    /* 2秒 */
#define INTERVAL_HIGH       1000    /* 1秒 */
#define INTERVAL_EMERGENCY  500     /* 0.5秒 */

/* 功耗统计 */
typedef struct {
    uint32_t totalRuntime;      /* 总运行时间 (秒) */
    uint32_t sleepTime;         /* 睡眠时间 (秒) */
    uint32_t activeTime;        /* 活跃时间 (秒) */
    float averageCurrent;       /* 平均电流 (mA) */
    float estimatedBatteryLife; /* 预计续航 (天) */
} PowerStatistics;

/* 电源管理器 */
typedef struct {
    PowerMode currentMode;          /* 当前功耗模式 */
    uint32_t lastSampleTime;        /* 上次采样时间 */
    uint32_t currentInterval;       /* 当前采样间隔 */
    uint32_t modeEnterTime;         /* 进入当前模式的时间 */
    PowerStatistics stats;          /* 功耗统计 */
    
    /* 模式切换计数 */
    uint32_t sleepCount;
    uint32_t lowCount;
    uint32_t normalCount;
    uint32_t highCount;
    uint32_t emergencyCount;
} PowerManager;

/* 函数声明 */
void PowerManager_Init(PowerManager* manager);
void PowerManager_UpdateMode(PowerManager* manager, FirePredictor* predictor);
bool PowerManager_ShouldSample(PowerManager* manager);
void PowerManager_EnterSleep(PowerManager* manager);
void PowerManager_WakeUp(PowerManager* manager);
void PowerManager_UpdateStatistics(PowerManager* manager);
PowerMode PowerManager_SelectMode(float risk);
const char* PowerManager_GetModeString(PowerMode mode);
float PowerManager_GetCurrentConsumption(PowerMode mode);

/* STM32 HAL库相关函数 (需要在主程序中实现) */
extern void SystemClock_Config(void);
extern void EnterStopMode(void);
extern void EnterSleepMode(void);

#endif /* __POWER_MANAGER_H */
