/**
  ******************************************************************************
  * @file    alarm_config.h
  * @brief   报警阈值配置文件 - 支持动态修改和上下位机同步
  * @author  [张伯]
  * @date    2026
  ******************************************************************************
  */

#ifndef __ALARM_CONFIG_H
#define __ALARM_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * 默认阈值配置（上电初始值）
 * 这些值会被Qt上位机通过串口命令修改
 * ============================================================================ */

/* 温度阈值 (摄氏度) */
#define DEFAULT_TEMP_THRESHOLD_HIGH     50.0f   /* 高温报警阈值 */
#define DEFAULT_TEMP_THRESHOLD_LOW      5.0f    /* 低温报警阈值（可选） */

/* 烟雾阈值 (百分比 0-100%) */
#define DEFAULT_SMOKE_THRESHOLD         60.0f   /* 烟雾浓度报警阈值 */

/* CO阈值 (ppm) */
#define DEFAULT_CO_THRESHOLD            100.0f  /* CO浓度报警阈值 */

/* 风险值阈值 (0-1，用于AI预测算法) */
#define DEFAULT_RISK_THRESHOLD          0.6f    /* 风险等级达到DANGER时报警 */

/* ============================================================================
 * 阈值结构体 - 运行时可修改
 * ============================================================================ */
typedef struct {
    /* 传统传感器阈值 */
    float tempThresholdHigh;      /* 温度高阈值 */
    float tempThresholdLow;       /* 温度低阈值 */
    float smokeThreshold;         /* 烟雾阈值 */
    float coThreshold;            /* CO阈值 */
    
    /* AI风险阈值 */
    float riskThreshold;          /* 风险值阈值 */
    
    /* 使能开关 */
    bool tempAlarmEnabled;        /* 温度报警使能 */
    bool smokeAlarmEnabled;       /* 烟雾报警使能 */
    bool coAlarmEnabled;          /* CO报警使能 */
    bool riskAlarmEnabled;        /* 风险值报警使能 */
    
    /* 版本号（用于同步校验） */
    uint16_t configVersion;
} AlarmConfig;

/* ============================================================================
 * 全局变量声明
 * ============================================================================ */
extern AlarmConfig g_alarmConfig;  /* 全局阈值配置 */

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
  * @brief  初始化报警配置（加载默认值）
  * @retval None
  */
void AlarmConfig_Init(void);

/**
  * @brief  设置温度阈值
  * @param  high: 高温阈值
  * @param  low: 低温阈值（不需要可传0）
  * @retval true: 成功, false: 失败
  */
bool AlarmConfig_SetTempThreshold(float high, float low);

/**
  * @brief  设置烟雾阈值
  * @param  threshold: 烟雾浓度阈值 (0-100%)
  * @retval true: 成功, false: 失败
  */
bool AlarmConfig_SetSmokeThreshold(float threshold);

/**
  * @brief  设置CO阈值
  * @param  threshold: CO浓度阈值 (ppm)
  * @retval true: 成功, false: 失败
  */
bool AlarmConfig_SetCOThreshold(float threshold);

/**
  * @brief  设置风险值阈值
  * @param  threshold: 风险值阈值 (0-1)
  * @retval true: 成功, false: 失败
  */
bool AlarmConfig_SetRiskThreshold(float threshold);

/**
  * @brief  设置报警使能
  * @param  type: 传感器类型 ("TEMP"/"SMOKE"/"CO"/"RISK")
  * @param  enabled: true=使能, false=禁用
  * @retval true: 成功, false: 失败
  */
bool AlarmConfig_SetAlarmEnabled(const char* type, bool enabled);

/**
  * @brief  获取当前配置版本号（用于同步校验）
  * @retval 版本号
  */
uint16_t AlarmConfig_GetVersion(void);

/**
  * @brief  增加版本号（修改配置后调用）
  * @retval None
  */
void AlarmConfig_IncrementVersion(void);

/**
  * @brief  从字符串解析并设置阈值（用于串口接收）
  * @param  cmd: 命令字符串，格式 "TEMP:50.0" 或 "SMOKE:60.0" 等
  * @retval true: 解析成功, false: 解析失败
  */
bool AlarmConfig_ParseCommand(const char* cmd);

/**
  * @brief  将当前配置格式化为字符串（用于串口发送）
  * @param  buf: 输出缓冲区
  * @param  size: 缓冲区大小
  * @retval 实际写入的字节数
  */
int AlarmConfig_ToString(char* buf, int size);

/**
  * @brief  保存配置到Flash（断电保存，可选）
  * @retval true: 成功, false: 失败
  */
bool AlarmConfig_SaveToFlash(void);

/**
  * @brief  从Flash加载配置（断电恢复，可选）
  * @retval true: 成功, false: 失败（使用默认值）
  */
bool AlarmConfig_LoadFromFlash(void);

#endif /* __ALARM_CONFIG_H */
