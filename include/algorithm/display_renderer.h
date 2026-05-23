#pragma once

#include <opencv2/opencv.hpp>
#include <QVector>
#include "config/display_config.h"
#include "data/barcode_result.h"
#include "data/ocr_region.h"

struct PipelineContext;

/**
 * DisplayRenderer - Pipeline数据与显示渲染的分离层
 *
 * PipelineContext只存储数据，渲染逻辑集中在这里。
 * 根据指定的显示模式，从PipelineContext中提取或合成对应的显示图像。
 */
namespace DisplayRenderer {

/**
 * @brief 根据显示模式渲染Pipeline结果
 * @param ctx  Pipeline执行结果数据
 * @param mode 显示模式
 * @return     渲染后的BGR图像，失败时返回空Mat
 */
cv::Mat render(const PipelineContext& ctx, DisplayConfig::Mode mode);

/**
 * @brief 将二值Mask叠加到原图上（Mask区域显示为绿色）
 * @param bgr   原图（BGR 3通道）
 * @param mask  二值Mask（CV_8U，0/255）
 * @param alpha 叠加透明度
 * @return      叠加后的BGR图像
 */
cv::Mat overlayMaskOnImage(const cv::Mat& bgr, const cv::Mat& mask, float alpha = 0.3f);

/**
 * @brief 在原图上绘制条码识别结果
 * @param bgr      原图
 * @param barcodes 条码识别结果列表
 * @return         绘制后的BGR图像
 */
cv::Mat drawBarcodeOverlay(const cv::Mat& bgr, const QVector<BarcodeResult>& barcodes);

/**
 * @brief 在原图上绘制OCR识别结果
 * @param bgr     原图
 * @param regions OCR识别区域列表
 * @return        绘制后的BGR图像
 */
cv::Mat drawOcrOverlay(const cv::Mat& bgr, const QVector<OcrRegion>& regions);

} // namespace DisplayRenderer
