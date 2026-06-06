#pragma once

/**
 * @brief Blob分析判定配置（区域数量阈值）
 *
 * 对应 JudgeTabWidget 的参数。
 * 仅用于 Pipeline 全局配置中的判定阈值，
 * 与 DetectionItem 中 BlobAnalysisConfig 的 minBlobCount/maxBlobCount 不同。
 */
struct BlobJudgeConfig
{
    int minRegionCount = 0;
    int maxRegionCount = 1000;
    int currentRegionCount = 0;
};
