/**
  ******************************************************************************
  * @file    alarm_config.c
  * @brief   报警阈值配置实现 - 支持动态修改和上下位机同步
  * @author  [张伯]
  * @date    2026
  ******************************************************************************
  */

#include "alarm_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 全局阈值配置变量 */
AlarmConfig g_alarmConfig;

/**
  * @brief  初始化报警配置（加载默认值）
  */
void AlarmConfig_Init(void)
{
    /* 加载默认阈值 */
    g_alarmConfig.tempThresholdHigh = DEFAULT_TEMP_THRESHOLD_HIGH;
    g_alarmConfig.tempThresholdLow = DEFAULT_TEMP_THRESHOLD_LOW;
    g_alarmConfig.smokeThreshold = DEFAULT_SMOKE_THRESHOLD;
    g_alarmConfig.coThreshold = DEFAULT_CO_THRESHOLD;
    g_alarmConfig.riskThreshold = DEFAULT_RISK_THRESHOLD;
    
    /* 默认全部使能 */
    g_alarmConfig.tempAlarmEnabled = true;
    g_alarmConfig.smokeAlarmEnabled = true;
    g_alarmConfig.coAlarmEnabled = true;
    g_alarmConfig.riskAlarmEnabled = true;
    
    /* 初始版本号 */
    g_alarmConfig.configVersion = 1;
    
    /* 尝试从Flash加载（如果之前有保存过） */
    /* AlarmConfig_LoadFromFlash(); */
}

/**
  * @brief  设置温度阈值
  */
bool AlarmConfig_SetTempThreshold(float high, float low)
{
    if (high <= low || high > 150.0f || low < -50.0f) {
        return false;  /* 参数无效 */
    }
    
    g_alarmConfig.tempThresholdHigh = high;
    g_alarmConfig.tempThresholdLow = low;
    AlarmConfig_IncrementVersion();
    
    return true;
}

/**
  * @brief  设置烟雾阈值
  */
bool AlarmConfig_SetSmokeThreshold(float threshold)
{
    if (threshold < 0.0f || threshold > 100.0f) {
        return false;
    }
    
    g_alarmConfig.smokeThreshold = threshold;
    AlarmConfig_IncrementVersion();
    
    return true;
}

/**
  * @brief  设置CO阈值
  */
bool AlarmConfig_SetCOThreshold(float threshold)
{
    if (threshold < 0.0f || threshold > 1000.0f) {
        return false;
    }
    
    g_alarmConfig.coThreshold = threshold;
    AlarmConfig_IncrementVersion();
    
    return true;
}

/**
  * @brief  设置风险值阈值
  */
bool AlarmConfig_SetRiskThreshold(float threshold)
{
    if (threshold < 0.0f || threshold > 1.0f) {
        return false;
    }
    
    g_alarmConfig.riskThreshold = threshold;
    AlarmConfig_IncrementVersion();
    
    return true;
}

/**
  * @brief  设置报警使能
  */
bool AlarmConfig_SetAlarmEnabled(const char* type, bool enabled)
{
    if (strcmp(type, "TEMP") == 0) {
        g_alarmConfig.tempAlarmEnabled = enabled;
    } else if (strcmp(type, "SMOKE") == 0) {
        g_alarmConfig.smokeAlarmEnabled = enabled;
    } else if (strcmp(type, "CO") == 0) {
        g_alarmConfig.coAlarmEnabled = enabled;
    } else if (strcmp(type, "RISK") == 0) {
        g_alarmConfig.riskAlarmEnabled = enabled;
    } else {
        return false;  /* 未知类型 */
    }
    
    AlarmConfig_IncrementVersion();
    return true;
}

/**
  * @brief  获取当前配置版本号
  */
uint16_t AlarmConfig_GetVersion(void)
{
    return g_alarmConfig.configVersion;
}

/**
  * @brief  增加版本号
  */
void AlarmConfig_IncrementVersion(void)
{
    g_alarmConfig.configVersion++;
    if (g_alarmConfig.configVersion == 0) {
        g_alarmConfig.configVersion = 1;  /* 避免溢出为0 */
    }
}

/**
  * @brief  从字符串解析并设置阈值
  * 支持格式：
  *   "TEMP:50.0"      - 设置温度高阈值为50.0
  *   "TEMP:50.0,5.0"  - 设置温度高阈值50.0，低阈值5.0
  *   "SMOKE:60.0"     - 设置烟雾阈值60.0%
  *   "CO:100.0"       - 设置CO阈值100ppm
  *   "RISK:0.6"       - 设置风险阈值0.6
  *   "ENABLE:TEMP"    - 使能温度报警
  *   "DISABLE:CO"     - 禁用CO报警
  *   "GET"            - 获取当前配置
  */
bool AlarmConfig_ParseCommand(const char* cmd)
{
    char buffer[64];
    char* type;
    char* value;
    
    strncpy(buffer, cmd, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* 查找冒号分隔符 */
    type = buffer;
    value = strchr(buffer, ':');
    
    if (value == NULL) {
        /* 没有冒号，检查特殊命令 */
        if (strcmp(buffer, "GET") == 0) {
            return true;  /* GET命令有效 */
        }
        return false;
    }
    
    *value = '\0';  /* 分割字符串 */
    value++;        /* value指向数值部分 */
    
    /* 解析ENABLE/DISABLE命令 */
    if (strcmp(type, "ENABLE") == 0) {
        return AlarmConfig_SetAlarmEnabled(value, true);
    }
    if (strcmp(type, "DISABLE") == 0) {
        return AlarmConfig_SetAlarmEnabled(value, false);
    }
    
    /* 解析阈值设置命令 */
    float val = atof(value);
    
    if (strcmp(type, "TEMP") == 0) {
        /* 检查是否有两个值（高阈值,低阈值） */
        char* lowVal = strchr(value, ',');
        if (lowVal != NULL) {
            *lowVal = '\0';
            lowVal++;
            float low = atof(lowVal);
            return AlarmConfig_SetTempThreshold(val, low);
        }
        /* 只设置高阈值，低阈值保持默认 */
        return AlarmConfig_SetTempThreshold(val, g_alarmConfig.tempThresholdLow);
    }
    
    if (strcmp(type, "SMOKE") == 0) {
        return AlarmConfig_SetSmokeThreshold(val);
    }
    
    if (strcmp(type, "CO") == 0) {
        return AlarmConfig_SetCOThreshold(val);
    }
    
    if (strcmp(type, "RISK") == 0) {
        return AlarmConfig_SetRiskThreshold(val);
    }
    
    return false;  /* 未知命令 */
}

/**
  * @brief  将当前配置格式化为字符串
  * 输出格式: "TEMP:50.0/5.0,SMOKE:60.0,CO:100.0,RISK:0.60,VER:1"
  */
int AlarmConfig_ToString(char* buf, int size)
{
    return snprintf(buf, size, 
        "TEMP:%.1f/%.1f,SMOKE:%.1f,CO:%.1f,RISK:%.2f,VER:%d,EN:%d%d%d%d",
        g_alarmConfig.tempThresholdHigh,
        g_alarmConfig.tempThresholdLow,
        g_alarmConfig.smokeThreshold,
        g_alarmConfig.coThreshold,
        g_alarmConfig.riskThreshold,
        g_alarmConfig.configVersion,
        g_alarmConfig.tempAlarmEnabled,
        g_alarmConfig.smokeAlarmEnabled,
        g_alarmConfig.coAlarmEnabled,
        g_alarmConfig.riskAlarmEnabled);
}

/**
  * @brief  保存配置到Flash（断电保存，可选实现）
  * 注：需要配合Flash操作代码
  */
bool AlarmConfig_SaveToFlash(void)
{
    /* TODO: 实现Flash写入 */
    /* 示例：HAL_FLASH_Unlock(); ... HAL_FLASH_Lock(); */
    return false;  /* 暂未实现 */
}

/**
  * @brief  从Flash加载配置（断电恢复，可选实现）
  */
bool AlarmConfig_LoadFromFlash(void)
{
    /* TODO: 实现Flash读取 */
    return false;  /* 暂未实现，使用默认值 */
}
