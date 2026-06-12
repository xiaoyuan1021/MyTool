#pragma once

#include <QString>

/**
 * 模型预热工具类
 * 在应用启动时集中预加载所有模型，避免首次使用时的延迟
 */
class ModelWarmUp
{
public:
    /**
     * 预热所有模型（异步执行，不阻塞UI）
     * - OCR: RapidOCR PP-OCRv4 语言模型
     * - 条码: ZXing 读取器配置
     *
     * 注意: 目标检测模型不在此处预热，由 ObjectDetectionTabWidget 按需加载
     */
    static void warmUpAll();

    /**
     * 预热 OCR 引擎
     * @param language 语言，默认 "chi_sim+eng"
     */
    static void warmUpOcr(const QString& language = "chi_sim+eng");

    /**
     * 预热条码读取器（轻量级，仅配置格式）
     */
    static void warmUpBarcode();
};
