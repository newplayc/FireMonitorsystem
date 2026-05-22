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
#include "alarm_config.h"  /* 使用统一的阈值配置 */
#include "stm32f1xx_hal.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

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
  * @brief  计算火灾风险值 (核心算法 - 数学优化版)
  *
  *         使用Sigmoid函数实现平滑过渡:
  *         f(x) = 1 / (1 + e^(-k*(x - x0)))
  *
  *         其中 x0 = 阈值，k = 斜率
  *
  * @param  predictor: 预测器结构体指针
  * @retval 风险值 (0.0 - 1.0)
  */
float FirePredictor_CalculateRisk(FirePredictor* predictor)
{
    if (predictor->history.count < TREND_WINDOW) {
        return 0.0f;  /* 数据不足 */
    }

    TrendAnalysis trend = FirePredictor_AnalyzeTrend(predictor);

    /* 获取当前传感器值 */
    SensorDataPoint* current = &predictor->history.data[
        (predictor->history.head - 1 + HISTORY_SIZE) % HISTORY_SIZE
    ];

    float tempRisk = 0.0f, smokeRisk = 0.0f, coRisk = 0.0f, trendRisk = 0.0f;

    /* 使用统一配置的阈值 */
    float tempThreshold = g_alarmConfig.tempThresholdHigh;
    float smokeThreshold = g_alarmConfig.smokeThreshold;
    float coThreshold = g_alarmConfig.coThreshold;

    /* 调试输出 */
    static uint32_t lastDebug = 0;
    if (HAL_GetTick() - lastDebug > 5000) {
        printf("[RISK DEBUG] T:%.1f/%.0f S:%.1f/%.0f CO:%.1f/%.0f\r\n",
               current->temperature, tempThreshold,
               current->smoke, smokeThreshold,
               current->co, coThreshold);
        lastDebug = HAL_GetTick();
    }

    /*
     * ====== 因素1: 温度风险 (权重20%) ======
     */
    tempRisk = 1.0f / (1.0f + expf(-0.3f * (current->temperature - tempThreshold)));

    /*
     * ====== 因素2: 烟雾风险 (权重35%) ======
     */
    smokeRisk = 1.0f / (1.0f + expf(-0.2f * (current->smoke - smokeThreshold)));

    /*
     * ====== 因素3: CO风险 (权重25%) ======
     */
    coRisk = 1.0f / (1.0f + expf(-0.1f * (current->co - coThreshold)));

    /*
     * ====== 因素4: 温度趋势风险 (权重20%) ======
     */
    float tempRate = trend.tempTrend;
    if (tempRate < 0) tempRate = 0;
    trendRisk = 1.0f / (1.0f + expf(-2.0f * (tempRate - 2.0f)));

    /*
     * ====== 综合风险计算 ======
     * 策略：加权平均 + 最大风险增强
     * 如果任何单项风险很高，综合风险也应该很高
     */
    float maxRisk = tempRisk;
    if (smokeRisk > maxRisk) maxRisk = smokeRisk;
    if (coRisk > maxRisk) maxRisk = coRisk;
    if (trendRisk > maxRisk) maxRisk = trendRisk;

    float weightedRisk = 0.20f * tempRisk +
                         0.35f * smokeRisk +
                         0.25f * coRisk +
                         0.20f * trendRisk;

    /* 综合风险 = 加权平均(60%) + 最大风险(40%) */
    float totalRisk = 0.6f * weightedRisk + 0.4f * maxRisk;

    /* 调试输出各项风险 */
    if (HAL_GetTick() - lastDebug < 100) {
        printf("[RISK] tempR=%.2f smokeR=%.2f coR=%.2f trendR=%.2f max=%.2f total=%.2f\r\n",
               tempRisk, smokeRisk, coRisk, trendRisk, maxRisk, totalRisk);
    }

    /*
     * ====== 多因素相关性修正 (防误报) ======
     * 如果仅温度高但烟雾和CO正常，可能是加热器等误报
     */
    if (current->temperature > tempThreshold + 5.0f &&
        current->smoke < smokeThreshold * 0.2f &&
        current->co < coThreshold * 0.1f) {
        totalRisk *= 0.4f;  /* 显著降低风险 */
    }

    /*
     * ====== 多因素联动增强 ======
     * 如果多个指标同时异常，增加风险权重
     */
    if (current->temperature > tempThreshold &&
        current->smoke > smokeThreshold * 0.4f &&
        current->co > coThreshold * 0.08f) {
        totalRisk *= 1.3f;  /* 增加30%风险 */
    }

    /* 风险值归一化 */
    if (totalRisk > 1.0f) totalRisk = 1.0f;
    if (totalRisk < 0.0f) totalRisk = 0.0f;

    return totalRisk;
}

/**
  * @brief  获取风险等级
  * @param  risk: 风险值 (0-1)
  * @retval 风险等级枚举
  * @note   使用配置的风险阈值进行判断
  *         风险值 < 阈值 → SAFE/NOTICE（不报警）
  *         风险值 >= 阈值 → WARNING/DANGER/CRITICAL（报警）
  */
RiskLevel FirePredictor_GetRiskLevel(float risk)
{
    /* 使用配置的风险阈值 */
    float riskThreshold = g_alarmConfig.riskThreshold;

    /* 低于阈值时不报警 */
    if (risk < riskThreshold * 0.3f) return RISK_SAFE;
    else if (risk < riskThreshold * 0.6f) return RISK_NOTICE;
    else if (risk < riskThreshold) return RISK_NOTICE;  /* 接近阈值但仍低于，只是注意级别 */

    /* 达到或超过阈值时才报警 */
    else if (risk < riskThreshold + (1.0f - riskThreshold) * 0.3f) return RISK_WARNING;
    else if (risk < riskThreshold + (1.0f - riskThreshold) * 0.6f) return RISK_DANGER;
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
