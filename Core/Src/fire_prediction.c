/**
  ******************************************************************************
  * @file    fire_prediction.c
  * @brief   AI火灾风险预测算法实现
  *          核心创新：基于时间序列的多因素融合预测模型
  * @author  [你的姓名]
  * @date    2024
  ******************************************************************************
  */

#include "fire_prediction.h"
#include "stm32f1xx_hal.h"  /* 修复：添加HAL库头文件 */
#include <math.h>
#include <string.h>

/**
  * @brief  初始化火灾预测器
  * @param  predictor: 预测器结构体指针
  * @retval None
  */
void FirePredictor_Init(FirePredictor* predictor)
{
    memset(predictor, 0, sizeof(FirePredictor));
    predictor->currentRisk = 0.0f;
    predictor->riskLevel = RISK_SAFE;
    predictor->predictionTime = 0;
}

/**
  * @brief  更新传感器数据
  * @param  predictor: 预测器结构体指针
  * @param  temp: 温度值 (°C)
  * @param  smoke: 烟雾浓度 (%)
  * @param  co: CO浓度 (ppm)
  * @retval None
  */
void FirePredictor_Update(FirePredictor* predictor, float temp, float smoke, float co)
{
    /* 获取当前时间戳 */
    uint32_t currentTime = HAL_GetTick();  /* 假设使用HAL库 */
    
    /* 创建数据点 */
    SensorDataPoint newPoint = {
        .temperature = temp,
        .smoke = smoke,
        .co = co,
        .timestamp = currentTime
    };
    
    /* 写入环形缓冲区 */
    predictor->history.data[predictor->history.head] = newPoint;
    predictor->history.head = (predictor->history.head + 1) % HISTORY_SIZE;
    
    if (predictor->history.count < HISTORY_SIZE) {
        predictor->history.count++;
    }
    
    /* 计算风险值 */
    float newRisk = FirePredictor_CalculateRisk(predictor);
    
    /* 检测风险趋势 */
    if (newRisk > predictor->currentRisk) {
        predictor->isRiskIncreasing = true;
        predictor->consecutiveHighRisk++;
    } else {
        predictor->isRiskIncreasing = false;
        if (newRisk < 0.3f) {
            predictor->consecutiveHighRisk = 0;
        }
    }
    
    predictor->currentRisk = newRisk;
    predictor->riskLevel = FirePredictor_GetRiskLevel(newRisk);
    
    /* 计算预测提前时间 */
    if (predictor->isRiskIncreasing && newRisk > 0.5f) {
        /* 线性外推预测到达危险阈值的时间 */
        float rate = (newRisk - predictor->currentRisk);  /* 风险上升速率 */
        if (rate > 0.001f) {
            predictor->predictionTime = (uint32_t)((0.8f - newRisk) / rate);
        }
    }
}

/**
  * @brief  计算火灾风险值 (核心算法 - 创新点)
  *         多因素加权融合模型
  * @param  predictor: 预测器结构体指针
  * @retval 风险值 (0.0 - 1.0)
  */
float FirePredictor_CalculateRisk(FirePredictor* predictor)
{
    if (predictor->history.count < TREND_WINDOW) {
        return 0.0f;  /* 数据不足 */
    }
    
    float risk = 0.0f;
    TrendAnalysis trend = FirePredictor_AnalyzeTrend(predictor);
    
    /* 获取当前传感器值 */
    SensorDataPoint* current = &predictor->history.data[
        (predictor->history.head - 1 + HISTORY_SIZE) % HISTORY_SIZE
    ];
    
    /* ====== 因素1: 温度基础风险 (权重25%) ====== */
    if (current->temperature > 40.0f) {
        risk += 0.25f * ((current->temperature - 40.0f) / 60.0f);
    }
    
    /* ====== 因素2: 温度趋势风险 (权重35%) - 核心创新 ====== */
    /* 温度快速上升是火灾的重要前兆 */
    if (trend.tempTrend > 2.0f) {
        /* 趋势越陡峭，风险越高 */
        float trendRisk = (trend.tempTrend / 10.0f);
        if (trendRisk > 1.0f) trendRisk = 1.0f;
        risk += 0.35f * trendRisk;
        
        /* 加速上升额外加分 */
        if (trend.tempTrend > 5.0f) {
            risk += 0.1f;
        }
    }
    
    /* ====== 因素3: 温度波动性 (权重15%) ====== */
    /* 温度剧烈波动可能预示燃烧不稳定 */
    if (trend.tempVariance > 9.0f) {
        risk += 0.15f * (trend.tempVariance / 25.0f);
    }
    
    /* ====== 因素4: 烟雾验证 (权重25%) ====== */
    /* 烟雾存在验证温度异常，防止误报 */
    if (current->smoke > 20.0f) {
        float smokeRisk = (current->smoke / 100.0f);
        if (smokeRisk > 1.0f) smokeRisk = 1.0f;
        risk += 0.25f * smokeRisk;
        
        /* 烟雾快速上升 */
        if (trend.smokeTrend > 5.0f) {
            risk += 0.1f;
        }
    }
    
    /* ====== 因素5: CO验证 (额外加权) ====== */
    /* CO是燃烧的明确指标 */
    if (current->co > 50.0f) {
        risk += 0.1f * ((current->co - 50.0f) / 150.0f);
    }
    
    /* ====== 多传感器相关性验证 (防误报机制) ====== */
    /* 如果温度高但烟雾和CO都正常，可能是误报（如加热器） */
    if (current->temperature > 50.0f && current->smoke < 15.0f && current->co < 30.0f) {
        risk *= 0.5f;  /* 降低风险值 */
    }
    
    /* 风险值归一化 */
    if (risk > 1.0f) risk = 1.0f;
    if (risk < 0.0f) risk = 0.0f;
    
    return risk;
}

/**
  * @brief  获取风险等级
  * @param  risk: 风险值 (0-1)
  * @retval 风险等级枚举
  */
RiskLevel FirePredictor_GetRiskLevel(float risk)
{
    if (risk < 0.2f) return RISK_SAFE;
    else if (risk < 0.4f) return RISK_NOTICE;
    else if (risk < 0.6f) return RISK_WARNING;
    else if (risk < 0.8f) return RISK_DANGER;
    else return RISK_CRITICAL;
}

/**
  * @brief  判断是否应发出早期预警
  *         创新点：在达到阈值前提前预警
  * @param  predictor: 预测器结构体指针
  * @retval true: 需要预警, false: 不需要
  */
bool FirePredictor_IsEarlyWarning(FirePredictor* predictor)
{
    /* 条件1: 风险值持续上升 */
    if (!predictor->isRiskIncreasing) return false;
    
    /* 条件2: 当前风险在中等以上 */
    if (predictor->currentRisk < 0.45f) return false;
    
    /* 条件3: 连续多次检测到高风险（确认不是噪声） */
    if (predictor->consecutiveHighRisk < 3) return false;
    
    /* 条件4: 预测在30秒内会达到危险等级 */
    if (predictor->predictionTime > 0 && predictor->predictionTime <= 30) {
        return true;
    }
    
    /* 条件5: 风险值快速跃升（前兆明显） */
    if (predictor->currentRisk > 0.6f && predictor->consecutiveHighRisk >= 2) {
        return true;
    }
    
    return false;
}

/**
  * @brief  趋势分析（核心算法）
  *         计算温度、烟雾、CO的变化趋势
  * @param  predictor: 预测器结构体指针
  * @retval 趋势分析结果结构体
  */
TrendAnalysis FirePredictor_AnalyzeTrend(FirePredictor* predictor)
{
    TrendAnalysis result = {0};
    
    if (predictor->history.count < TREND_WINDOW) {
        return result;
    }
    
    /* 提取最近N个数据点用于趋势计算 */
    float temps[TREND_WINDOW];
    float smokes[TREND_WINDOW];
    float cos[TREND_WINDOW];
    float times[TREND_WINDOW];
    
    uint16_t idx = (predictor->history.head + HISTORY_SIZE - TREND_WINDOW) % HISTORY_SIZE;
    
    for (int i = 0; i < TREND_WINDOW; i++) {
        SensorDataPoint* point = &predictor->history.data[(idx + i) % HISTORY_SIZE];
        temps[i] = point->temperature;
        smokes[i] = point->smoke;
        cos[i] = point->co;
        times[i] = (float)i;  /* 用索引代替时间，简化计算 */
    }
    
    /* 计算线性回归斜率（趋势） */
    result.tempTrend = calculate_linear_regression_slope(times, temps, TREND_WINDOW);
    result.smokeTrend = calculate_linear_regression_slope(times, smokes, TREND_WINDOW);
    result.coTrend = calculate_linear_regression_slope(times, cos, TREND_WINDOW);
    
    /* 转换为每秒变化率 */
    /* 假设采样间隔为1秒，如果不同需要调整 */
    
    /* 计算方差 */
    float avgTemp = calculate_average(temps, TREND_WINDOW);
    result.tempVariance = calculate_variance(temps, TREND_WINDOW, avgTemp);
    
    /* 计算多传感器相关性 */
    /* 如果温度、烟雾、CO同时上升，相关性高，火灾可能性大 */
    float correlation = 0;
    if (result.tempTrend > 0 && result.smokeTrend > 0) correlation += 0.5f;
    if (result.smokeTrend > 0 && result.coTrend > 0) correlation += 0.5f;
    result.correlation = correlation;
    
    return result;
}

/* ==================== 辅助函数 ==================== */

/**
  * @brief  计算平均值
  */
float calculate_average(float* data, uint16_t size)
{
    float sum = 0.0f;
    for (uint16_t i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum / size;
}

/**
  * @brief  计算方差
  */
float calculate_variance(float* data, uint16_t size, float avg)
{
    float sum = 0.0f;
    for (uint16_t i = 0; i < size; i++) {
        float diff = data[i] - avg;
        sum += diff * diff;
    }
    return sum / size;
}

/**
  * @brief  计算线性回归斜率（最小二乘法）
  *         y = slope * x + intercept
  * @param  x: 自变量数组
  * @param  y: 因变量数组
  * @param  size: 数组大小
  * @retval 斜率slope
  */
float calculate_linear_regression_slope(float* x, float* y, uint16_t size)
{
    float sumX = 0.0f, sumY = 0.0f, sumXY = 0.0f, sumX2 = 0.0f;
    
    for (uint16_t i = 0; i < size; i++) {
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i];
    }
    
    float denominator = size * sumX2 - sumX * sumX;
    if (fabs(denominator) < 1e-6f) {  /* 修复：增大阈值，提高稳定性 */
        return 0.0f;  /* 避免除零 */
    }
    
    float slope = (size * sumXY - sumX * sumY) / denominator;
    return slope;
}

/**
  * @brief  限幅函数
  */
float constrain(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
