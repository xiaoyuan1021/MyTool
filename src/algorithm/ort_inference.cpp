#include "algorithm/ort_inference.h"
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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

        // 从 ONNX Session 读取实际输入形状（最可靠的方式）
        if (session_->GetInputCount() > 0)
        {
            auto inputType = session_->GetInputTypeInfo(0);
            auto tensorInfo = inputType.GetTensorTypeAndShapeInfo();
            auto shape = tensorInfo.GetShape();
            // YOLO 模型输入 shape 通常为 [N, C, H, W] 或 [N, C, H, W]（动态维度为-1）
            if (shape.size() == 4)
            {
                // 跳过 batch 和 channel，取 H 和 W
                int h = static_cast<int>(shape[2]);
                int w = static_cast<int>(shape[3]);
                // 过滤动态维度（-1）和不合理值
                if (h > 0 && w > 0)
                {
                    modelImgWidth_ = w;
                    modelImgHeight_ = h;
                    spdlog::info("OrtInference: actual input shape from ONNX = {}x{} (WxH)", w, h);
                }
            }
        }

        // 自动加载同名 .names 文件
        QFileInfo modelFileInfo(modelPath);
        QString namesPath = modelFileInfo.dir().filePath(
            modelFileInfo.completeBaseName() + ".names");
        if (QFile::exists(namesPath))
        {
            loadClassNames(namesPath);
        }
        else
        {
            // .names 不存在时，尝试从上级目录的 parameter.json 读取类名和尺寸
            loadFromParameterJson(modelFileInfo.dir().absolutePath());
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
        spdlog::warn("OrtInference: detect skipped - loaded={}, empty={}", loaded_, input.empty());
        return results;
    }

    try
    {
        int imgWidth = input.cols;
        int imgHeight = input.rows;
        spdlog::info("OrtInference: detect input image size = {}x{}", imgWidth, imgHeight);

        // === YOLOv8 标准预处理：Letterbox + BGR→RGB + /255 ===

        // 1. Letterbox resize（保持宽高比，用114灰色填充到正方形）
        float r = std::min(static_cast<float>(inputWidth) / imgWidth,
                           static_cast<float>(inputHeight) / imgHeight);
        int newW = static_cast<int>(imgWidth * r);
        int newH = static_cast<int>(imgHeight * r);

        cv::Mat resized;
        cv::resize(input, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

        // 创建灰色画布（114是YOLO默认填充值）
        cv::Mat canvas(inputHeight, inputWidth, CV_8UC3, cv::Scalar(114, 114, 114));

        // 计算居中偏移
        int padLeft = (inputWidth - newW) / 2;
        int padTop = (inputHeight - newH) / 2;

        // 将resize后的图像放到画布中心
        cv::Mat roi = canvas(cv::Rect(padLeft, padTop, newW, newH));
        resized.copyTo(roi);

        // 2. 转为 float，归一化到 [0,1]
        cv::Mat floatImg;
        canvas.convertTo(floatImg, CV_32FC3, 1.0 / 255.0);

        // 3. BGR → RGB（OpenCV默认BGR，YOLO训练时用RGB）
        cv::Mat rgbImg;
        cv::cvtColor(floatImg, rgbImg, cv::COLOR_BGR2RGB);

        // 4. HWC -> CHW
        std::vector<float> inputTensorValues(inputWidth * inputHeight * 3);
        for (int c = 0; c < 3; ++c)
        {
            for (int h = 0; h < inputHeight; ++h)
            {
                for (int w = 0; w < inputWidth; ++w)
                {
                    inputTensorValues[c * inputWidth * inputHeight + h * inputWidth + w] =
                        rgbImg.at<cv::Vec3f>(h, w)[c];
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
        int numAttributes = static_cast<int>(outputShape[1]);
        int numDetections = static_cast<int>(outputShape[2]);
        spdlog::info("OrtInference: output shape = [{}, {}, {}], inputWidth={}, inputHeight={}",
                      outputShape[0], outputShape[1], outputShape[2], inputWidth, inputHeight);

        float xScale = static_cast<float>(imgWidth) / inputWidth;
        float yScale = static_cast<float>(imgHeight) / inputHeight;

        // === 诊断：打印输出数据的原始值 ===
        // 打印前10个值，检查数据是否正常
        spdlog::info("OrtInference: first 10 output values:");
        for (int k = 0; k < 10; ++k)
        {
            spdlog::info("  outputData[{}] = {:.6f}", k, outputData[k]);
        }
        // === 诊断结束 ===

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

        // 打印前5个候选的详情用于诊断
        int logCount = std::min(5, static_cast<int>(results.size()));
        for (int k = 0; k < logCount; ++k)
        {
            spdlog::info("OrtInference: candidate[{}] class={}({}) conf={:.4f} box=({},{},{},{})",
                         k, results[k].className, results[k].classId, results[k].confidence,
                         results[k].box.x, results[k].box.y, results[k].box.width, results[k].box.height);
        }
        spdlog::info("OrtInference: candidates after conf filter (threshold={:.2f}) = {}", confThreshold, results.size());

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
            spdlog::info("OrtInference: final results after NMS = {}", nmsResults.size());
            return nmsResults;
        }
        spdlog::info("OrtInference: no candidates to NMS");
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

bool OrtInference::loadFromParameterJson(const QString& modelDir)
{
    // 向上级目录查找 parameter.json（ort/ 的上级就是 model_pin/）
    QString paramPath = QFileInfo(QDir(modelDir), "../parameter.json").absoluteFilePath();
    if (!QFile::exists(paramPath))
    {
        spdlog::info("OrtInference: no parameter.json found at {}", paramPath.toStdString());
        return false;
    }

    QFile file(paramPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        spdlog::warn("OrtInference: cannot open parameter.json: {}", paramPath.toStdString());
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        spdlog::warn("OrtInference: parameter.json parse error: {}", parseError.errorString().toStdString());
        return false;
    }

    QJsonObject root = doc.object();

    // 读取 class_name 数组
    if (classNames_.empty() && root.contains("class_name") && root["class_name"].isArray())
    {
        QJsonArray classNamesArray = root["class_name"].toArray();
        classNames_.clear();
        for (const auto& val : classNamesArray)
        {
            classNames_.push_back(val.toString().toStdString());
        }
        spdlog::info("OrtInference: loaded {} class names from parameter.json",
                      classNames_.size());
    }

    // 读取 img_scale 仅作为 fallback（ONNX Session 读取的尺寸优先级更高）
    if ((modelImgWidth_ <= 0 || modelImgHeight_ <= 0) &&
        root.contains("img_scale") && root["img_scale"].isArray())
    {
        QJsonArray imgScale = root["img_scale"].toArray();
        if (imgScale.size() >= 2)
        {
            modelImgWidth_ = imgScale[0].toInt(0);
            modelImgHeight_ = imgScale[1].toInt(0);
            spdlog::info("OrtInference: model img_scale from parameter.json = {}x{}",
                          modelImgWidth_, modelImgHeight_);
        }
    }

    return !classNames_.empty();
}
