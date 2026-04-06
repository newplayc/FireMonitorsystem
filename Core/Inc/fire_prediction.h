/**
  ******************************************************************************
  * @file    fire_prediction.h
  * @brief   AI火灾风险预测算法 - 创新点1
  *          基于时间序列分析的火灾风险预测模型
  * @author  [你的姓名]
  * @date    2024
  ******************************************************************************
  */

#ifndef __FIRE_PREDICTION_H
#define __FIRE_PREDICTION_H

#include <stdint.h>
#include <stdbool.h>

/* 历史数据缓冲区大小 */
#define HISTORY_SIZE    50
#define TREND_WINDOW    10      /* 趋势计算窗口 */

/* 风险等级定义 */
typedef enum {
    RISK_SAFE       = 0,    /* 安全: 0-0.2 */
    RISK_NOTICE     = 1,    /* 注意: 0.2-0.4 */
    RISK_WARNING    = 2,    /* 警告: 0.4-0.6 */
    RISK_DANGER     = 3,    /* 危险: 0.6-0.8 */
    RISK_CRITICAL   = 4     /* 紧急: 0.8-1.0 */
} RiskLevel;

/* 传感器数据结构 */
typedef struct {
    float temperature;      /* 温度 (°C) */
    float smoke;           /* 烟雾浓度 (%) */
    float co;              /* CO浓度 (ppm) */
    uint32_t timestamp;    /* 时间戳 (ms) */
} SensorDataPoint;

/* 历史数据环形缓冲区 */
typedef struct {
    SensorDataPoint data[HISTORY_SIZE];
    uint16_t head;         /* 写入位置 */
    uint16_t count;        /* 有效数据数量 */
} HistoryBuffer;

/* 趋势分析结果 */
typedef struct {
    float tempTrend;       /* 温度变化趋势 (°C/s) */
    float tempVariance;    /* 温度方差 */
    float smokeTrend;      /* 烟雾变化趋势 */
    float coTrend;         /* CO变化趋势 */
    float correlation;     /* 多传感器相关性 */
} TrendAnalysis;

/* 火灾风险预测器 */
typedef struct {
    HistoryBuffer history;
    float currentRisk;         /* 当前风险值 (0-1) */
    RiskLevel riskLevel;       /* 风险等级 */
    bool isRiskIncreasing;   /* 风险是否在上升 */
    uint32_t predictionTime;   /* 预测提前时间 (秒) */
    uint8_t consecutiveHighRisk;  /* 连续高风险次数 */
} FirePredictor;

/* 函数声明 */
void FirePredictor_Init(FirePredictor* predictor);
void FirePredictor_Update(FirePredictor* predictor, float temp, float smoke, float co);
float FirePredictor_CalculateRisk(FirePredictor* predictor);
RiskLevel FirePredictor_GetRiskLevel(float risk);
bool FirePredictor_IsEarlyWarning(FirePredictor* predictor);
TrendAnalysis FirePredictor_AnalyzeTrend(FirePredictor* predictor);

/* 辅助函数 */
float calculate_average(float* data, uint16_t size);
float calculate_variance(float* data, uint16_t size, float avg);
float calculate_linear_regression_slope(float* x, float* y, uint16_t size);
float constrain(float value, float min, float max);

#endif /* __FIRE_PREDICTION_H */
