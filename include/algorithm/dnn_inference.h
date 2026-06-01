#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <QString>
#include <vector>
#include <string>

/**
 * 单个检测结果
 */
struct DetectionResult
{
    cv::Rect box;           // 检测框
    float confidence;       // 置信度
    int classId;            // 类别ID
    std::string className;  // 类别名称
};

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
     * @param configPath 配置文件路径（可选）
     * @param useGpu 是否使用GPU加速（默认false，直接使用CPU推理避免加载慢）
     * @return 是否加载成功
     */
    bool loadModel(const QString& modelPath, const QString& configPath = "", bool useGpu = false);

    /**
     * 执行目标检测（YOLO系列）
     * @param input 输入图像
     * @param confThreshold 置信度阈值
     * @param nmsThreshold NMS阈值
     * @param inputWidth 输入宽度
     * @param inputHeight 输入高度
     * @return 检测结果列表
     */
    std::vector<DetectionResult> detect(const cv::Mat& input,
                                        float confThreshold = 0.5f,
                                        float nmsThreshold = 0.4f,
                                        int inputWidth = 640,
                                        int inputHeight = 640);

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
     * 获取当前推理后端是否为GPU
     */
    bool isUsingGpu() const { return usingGpu_; }

    /**
     * 获取输出层名称
     */
    std::vector<cv::String> getOutputNames() const;

    /**
     * 从文件加载类别名称（每行一个名称）
     * 也支持 OpenCV 的 format: 'name1\nname2\n...'
     * @param filePath 类别名称文件路径
     * @return 是否加载成功
     */
    bool loadClassNames(const QString& filePath);

    /**
     * 直接设置类别名称列表
     */
    void setClassNames(const std::vector<std::string>& names);

private:
    cv::dnn::Net net_;
    bool loaded_ = false;
    bool usingGpu_ = false;
    std::vector<std::string> classNames_;
};
