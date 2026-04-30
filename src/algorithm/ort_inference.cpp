#include "algorithm/ort_inference.h"
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>

OrtInference::OrtInference()
    : env_(ORT_LOGGING_LEVEL_WARNING, "OrtInference")
{
}

OrtInference::~OrtInference() = default;

bool OrtInference::loadModel(const QString& modelPath, bool useGpu)
{
    loaded_ = false;
    usingGpu_ = false;

    try
    {
        std::string model = modelPath.toStdString();

        sessionOptions_ = std::make_unique<Ort::SessionOptions>();
        sessionOptions_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // 设置线程数（避免过度占用CPU）
        sessionOptions_->SetIntraOpNumThreads(4);

        if (useGpu)
        {
            try
            {
                // 双显卡笔记本：默认 device_id=0 可能是核显
                // 通过 ONNX Runtime 的 GetAvailableProviders 验证 CUDA EP 是否可用
                // device_id=1 通常是独显，这里硬编码为 1（如果是单显卡会自动 fallback）
                int deviceId = 0;

                OrtCUDAProviderOptions cudaOptions;
                cudaOptions.device_id = deviceId;
                sessionOptions_->AppendExecutionProvider_CUDA(cudaOptions);
                usingGpu_ = true;
                spdlog::info("OrtInference: trying CUDA GPU backend on device {}", deviceId);
            }
            catch (const Ort::Exception& e)
            {
                spdlog::warn("OrtInference: CUDA not available, falling back to CPU: {}", e.what());
                // 创建新的 SessionOptions，只用 CPU
                sessionOptions_ = std::make_unique<Ort::SessionOptions>();
                sessionOptions_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
                sessionOptions_->SetIntraOpNumThreads(4);
                usingGpu_ = false;
            }
            catch (const std::exception& e)
            {
                spdlog::warn("OrtInference: CUDA init failed, falling back to CPU: {}", e.what());
                sessionOptions_ = std::make_unique<Ort::SessionOptions>();
                sessionOptions_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
                sessionOptions_->SetIntraOpNumThreads(4);
                usingGpu_ = false;
            }
        }

        // 创建 Session（延迟加载，减少初始化时间）
        sessionOptions_->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);

        // ONNX Runtime Windows 版本只接受 wchar_t* 宽字符路径
        std::wstring wModel = modelPath.toStdWString();
        session_ = std::make_unique<Ort::Session>(
            env_, wModel.c_str(), *sessionOptions_);

        if (!session_)
        {
            spdlog::error("OrtInference: failed to create session");
            return false;
        }

        loaded_ = true;
        spdlog::info("OrtInference: model loaded successfully (backend: {})",
                     usingGpu_ ? "CUDA" : "CPU");

        // 自动加载同名 .names 文件
        QFileInfo modelFileInfo(modelPath);
        QString namesPath = modelFileInfo.dir().filePath(
            modelFileInfo.completeBaseName() + ".names");
        if (QFile::exists(namesPath))
        {
            loadClassNames(namesPath);
        }

        return true;
    }
    catch (const Ort::Exception& e)
    {
        spdlog::error("OrtInference: ORT exception - {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        spdlog::error("OrtInference: exception - {}", e.what());
        return false;
    }
    catch (...)
    {
        spdlog::error("OrtInference: unknown exception");
        return false;
    }
}

std::vector<DetectionResult> OrtInference::detect(const cv::Mat& input,
                                                   float confThreshold,
                                                   float nmsThreshold,
                                                   int inputWidth,
                                                   int inputHeight)
{
    std::vector<DetectionResult> results;

    if (!loaded_ || input.empty())
    {
        return results;
    }

    try
    {
        int imgWidth = input.cols;
        int imgHeight = input.rows;

        // 预处理: resize + normalize
        cv::Mat resized;
        cv::resize(input, resized, cv::Size(inputWidth, inputHeight));

        // 转为 float，归一化到 [0,1]，HWC -> CHW
        cv::Mat floatImg;
        resized.convertTo(floatImg, CV_32FC3, 1.0 / 255.0);

        // HWC -> CHW
        std::vector<float> inputTensorValues(inputWidth * inputHeight * 3);
        for (int c = 0; c < 3; ++c)
        {
            for (int h = 0; h < inputHeight; ++h)
            {
                for (int w = 0; w < inputWidth; ++w)
                {
                    inputTensorValues[c * inputWidth * inputHeight + h * inputWidth + w] =
                        floatImg.at<cv::Vec3f>(h, w)[c];
                }
            }
        }

        // 获取输入节点名（必须保持生命周期）
        std::vector<Ort::AllocatedStringPtr> inputNamePtrs;
        std::vector<const char*> inputNamesCStr;
        for (size_t i = 0; i < session_->GetInputCount(); ++i)
        {
            inputNamePtrs.push_back(
                session_->GetInputNameAllocated(i, allocator_));
            inputNamesCStr.push_back(inputNamePtrs.back().get());
        }

        // 获取输出节点名（必须保持生命周期）
        std::vector<Ort::AllocatedStringPtr> outputNamePtrs;
        std::vector<const char*> outputNamesCStr;
        for (size_t i = 0; i < session_->GetOutputCount(); ++i)
        {
            outputNamePtrs.push_back(
                session_->GetOutputNameAllocated(i, allocator_));
            outputNamesCStr.push_back(outputNamePtrs.back().get());
        }

        if (inputNamesCStr.empty() || outputNamesCStr.empty())
        {
            spdlog::error("OrtInference: failed to get input/output names");
            return results;
        }

        // 构造输入 tensor shape: [1, 3, H, W]
        std::vector<int64_t> inputShape = {1, 3, inputHeight, inputWidth};

        // 创建输入 Ort::Value
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(Ort::Value::CreateTensor<float>(
            memoryInfo, inputTensorValues.data(), inputTensorValues.size(),
            inputShape.data(), inputShape.size()));

        // 执行推理
        Ort::RunOptions runOptions;
        std::vector<Ort::Value> outputTensors = session_->Run(
            runOptions,
            inputNamesCStr.data(),
            inputTensors.data(), inputTensors.size(),
            outputNamesCStr.data(), outputNamesCStr.size());

        if (outputTensors.empty() || !outputTensors[0].IsTensor())
        {
            spdlog::error("OrtInference: output tensor is empty");
            return results;
        }

        // 获取输出数据
        const float* outputData = outputTensors[0].GetTensorData<float>();
        auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();

        // YOLOv8 输出: [1, 4+numClasses, numDetections]
        // 转置为 [numDetections, 4+numClasses]
        int numAttributes = static_cast<int>(outputShape[1]);
        int numDetections = static_cast<int>(outputShape[2]);

        float xScale = static_cast<float>(imgWidth) / inputWidth;
        float yScale = static_cast<float>(imgHeight) / inputHeight;

        // 解析每个检测结果
        for (int i = 0; i < numDetections; ++i)
        {
            // 找最大置信度的类别
            float maxConf = 0.0f;
            int bestClassId = 0;
            for (int j = 4; j < numAttributes; ++j)
            {
                // YOLOv8输出列主序: outputData[j * numDetections + i]
                float conf = outputData[j * numDetections + i];
                if (conf > maxConf)
                {
                    maxConf = conf;
                    bestClassId = j - 4;
                }
            }

            // 置信度过滤
            if (maxConf < confThreshold)
                continue;

            // cx, cy, w, h
            float cx = outputData[0 * numDetections + i];
            float cy = outputData[1 * numDetections + i];
            float w  = outputData[2 * numDetections + i];
            float h  = outputData[3 * numDetections + i];

            int left   = static_cast<int>((cx - w / 2.0f) * xScale);
            int top    = static_cast<int>((cy - h / 2.0f) * yScale);
            int width  = static_cast<int>(w * xScale);
            int height = static_cast<int>(h * yScale);

            // 边界裁剪
            left   = std::max(0, left);
            top    = std::max(0, top);
            width  = std::min(width, imgWidth - left);
            height = std::min(height, imgHeight - top);

            DetectionResult det;
            det.box = cv::Rect(left, top, width, height);
            det.confidence = maxConf;
            det.classId = bestClassId;
            if (bestClassId >= 0 && bestClassId < static_cast<int>(classNames_.size()))
            {
                det.className = classNames_[bestClassId];
            }
            else
            {
                det.className = "class_" + std::to_string(bestClassId);
            }
            results.push_back(det);
        }

        // NMS 后处理
        if (!results.empty())
        {
            std::vector<cv::Rect> boxes;
            std::vector<float> confs;
            for (const auto& r : results)
            {
                boxes.push_back(r.box);
                confs.push_back(r.confidence);
            }

            std::vector<int> indices;
            cv::dnn::NMSBoxes(boxes, confs, confThreshold, nmsThreshold, indices);

            std::vector<DetectionResult> nmsResults;
            for (int idx : indices)
            {
                nmsResults.push_back(results[idx]);
            }
            return nmsResults;
        }
    }
    catch (const Ort::Exception& e)
    {
        spdlog::error("OrtInference: detect ORT exception - {}", e.what());
    }
    catch (const std::exception& e)
    {
        spdlog::error("OrtInference: detect exception - {}", e.what());
    }

    return results;
}

bool OrtInference::isLoaded() const
{
    return loaded_;
}

bool OrtInference::isUsingGpu() const
{
    return usingGpu_;
}

bool OrtInference::loadClassNames(const QString& filePath)
{
    classNames_.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        spdlog::warn("OrtInference: cannot open class names file: {}",
                      filePath.toStdString());
        return false;
    }

    while (!file.atEnd())
    {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.isEmpty())
        {
            classNames_.push_back(line.toStdString());
        }
    }

    file.close();
    spdlog::info("OrtInference: loaded {} class names from {}",
                  classNames_.size(), filePath.toStdString());
    return !classNames_.empty();
}

void OrtInference::setClassNames(const std::vector<std::string>& names)
{
    classNames_ = names;
}

std::vector<std::string> OrtInference::getOutputNames() const
{
    if (!session_)
    {
        return {};
    }

    try
    {
        std::vector<std::string> names;
        size_t numOutputs = session_->GetOutputCount();
        for (size_t i = 0; i < numOutputs; ++i)
        {
            auto name = session_->GetOutputNameAllocated(i, allocator_);
            names.push_back(name.get());
        }
        return names;
    }
    catch (...)
    {
        return {};
    }
}