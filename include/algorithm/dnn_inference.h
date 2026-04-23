#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <QString>

/**
 * DNN推理封装类 - 基于OpenCV DNN模块的深度学习推理
 */
class DnnInference
{
public:
    DnnInference() = default;

    /**
     * 加载模型
     * @param modelPath 模型文件路径（支持 ONNX/Caffe/TF/Darknet 等）
     * @param configPath 配置文件路径（可选，如 Caffe 的 .prototxt 或 TF 的 .pbtxt）
     * @return 是否加载成功
     */
    bool loadModel(const QString& modelPath, const QString& configPath = "");

    /**
     * 执行前向推理
     * @param input 输入图像
     * @return 输出张量（cv::Mat）
     */
    cv::Mat forward(const cv::Mat& input);

    /**
     * 判断模型是否已加载
     */
    bool isLoaded() const;

    /**
     * 获取模型是否支持多个输出
     */
    std::vector<cv::String> getOutputNames() const;

private:
    cv::dnn::Net net_;
    bool loaded_ = false;
};