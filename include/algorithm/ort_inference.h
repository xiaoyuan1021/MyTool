#pragma once

#include "dnn_inference.h"  // for DetectionResult
#include <onnxruntime_cxx_api.h>

/**
 * @brief ONNX Runtime GPU 推理封装类
 *
 * 使用 ONNX Runtime + CUDA EP 实现 YOLO 目标检测 GPU 加速推理。
 * 与 DnnInference 提供兼容的接口，可无缝替换。
 */
class OrtInference
{
public:
    OrtInference();
    ~OrtInference();

    /**
     * @brief 加载 ONNX 模型
     * @param modelPath ONNX 模型文件路径
     * @param useGpu 是否使用 CUDA GPU 加速
     * @return 是否加载成功
     */
    bool loadModel(const QString& modelPath, bool useGpu = true);

    /**
     * @brief 执行目标检测
     * @param input 输入图像 (BGR)
     * @param confThreshold 置信度阈值
     * @param nmsThreshold NMS 阈值
     * @param inputWidth 模型输入宽度
     * @param inputHeight 模型输入高度
     * @return 检测结果列表
     */
    std::vector<DetectionResult> detect(const cv::Mat& input,
                                        float confThreshold = 0.5f,
                                        float nmsThreshold = 0.4f,
                                        int inputWidth = 640,
                                        int inputHeight = 640);

    /** @brief 模型是否已加载 */
    bool isLoaded() const;

    /** @brief 是否使用 GPU */
    bool isUsingGpu() const;

    /** @brief 从文件加载类别名称 */
    bool loadClassNames(const QString& filePath);

    /** @brief 直接设置类别名称 */
    void setClassNames(const std::vector<std::string>& names);

    /** @brief 获取模型 parameter.json 中定义的输入尺寸（img_scale），用于UI自动填充 */
    int getModelImgWidth() const { return modelImgWidth_; }
    int getModelImgHeight() const { return modelImgHeight_; }

private:
    std::vector<std::string> getOutputNames() const;

    /** @brief 当 .names 不存在时，尝试从上级目录的 parameter.json 读取类名和尺寸 */
    bool loadFromParameterJson(const QString& modelDir);

    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> sessionOptions_;
    Ort::AllocatorWithDefaultOptions allocator_;

    std::vector<std::string> classNames_;
    bool loaded_ = false;
    bool usingGpu_ = false;
    int modelImgWidth_ = 0;   // parameter.json 中的 img_scale[0]
    int modelImgHeight_ = 0;  // parameter.json 中的 img_scale[1]
};
