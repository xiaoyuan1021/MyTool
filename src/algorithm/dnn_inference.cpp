#include "dnn_inference.h"
#include <spdlog/spdlog.h>

bool DnnInference::loadModel(const QString& modelPath, const QString& configPath)
{
    loaded_ = false;

    try
    {
        std::string model = modelPath.toStdString();
        std::string config = configPath.toStdString();

        if (config.empty())
        {
            net_ = cv::dnn::readNet(model);
        }
        else
        {
            net_ = cv::dnn::readNet(model, config);
        }

        // 检查网络是否有效
        if (net_.empty())
        {
            spdlog::error("DnnInference: model is empty after loading");
            return false;
        }

        loaded_ = true;
        spdlog::info("DnnInference: model loaded successfully");
        return true;
    }
    catch (const cv::Exception& e)
    {
        spdlog::error("DnnInference: OpenCV exception - {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        spdlog::error("DnnInference: exception - {}", e.what());
        return false;
    }
    catch (...)
    {
        spdlog::error("DnnInference: unknown exception");
        return false;
    }
}

std::vector<DetectionResult> DnnInference::detect(const cv::Mat& input,
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
        // 获取输入图像尺寸
        int imgWidth = input.cols;
        int imgHeight = input.rows;

        // 创建blob - YOLOv8标准预处理
        cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0,
            cv::Size(inputWidth, inputHeight),
            cv::Scalar(0, 0, 0), true, false);
        net_.setInput(blob);

        // 执行推理
        std::vector<cv::String> outputNames = getOutputNames();
        if (outputNames.empty())
        {
            spdlog::error("DnnInference: no output layers found");
            return results;
        }

        std::vector<cv::Mat> outputs;
        net_.forward(outputs, outputNames);

        // YOLOv8 输出格式: [1, num_classes+4, num_detections]
        // 即每一列是一个检测结果，前4行是 x,y,w,h，后面是各类别置信度
        if (outputs.empty())
        {
            return results;
        }

        cv::Mat output = outputs[0];
        int numDetections = output.size[2];   // 检测数量
        int numAttributes = output.size[1];   // 4 + num_classes

        // 重塑为 2D: [num_detections, num_attributes]
        cv::Mat flatOutput = output.reshape(1, numAttributes);

        float xScale = static_cast<float>(imgWidth) / inputWidth;
        float yScale = static_cast<float>(imgHeight) / inputHeight;

        // 解析每个检测结果
        for (int i = 0; i < numDetections; ++i)
        {
            // 获取该检测的所有属性（列数据）
            cv::Mat detCol = flatOutput.col(i);

            float cx = detCol.at<float>(0);
            float cy = detCol.at<float>(1);
            float w  = detCol.at<float>(2);
            float h  = detCol.at<float>(3);

            // 找到最大置信度的类别
            float maxConf = 0.0f;
            int bestClassId = 0;
            for (int j = 4; j < numAttributes; ++j)
            {
                float conf = detCol.at<float>(j);
                if (conf > maxConf)
                {
                    maxConf = conf;
                    bestClassId = j - 4;
                }
            }

            // 置信度过滤
            if (maxConf < confThreshold)
                continue;

            // 转换为左上角坐标并缩放到原图尺寸
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
            det.className = "class_" + std::to_string(bestClassId);
            results.push_back(det);
        }

        // NMS后处理
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
    catch (const cv::Exception& e)
    {
        spdlog::error("DnnInference: detect OpenCV exception - {}", e.what());
    }
    catch (const std::exception& e)
    {
        spdlog::error("DnnInference: detect exception - {}", e.what());
    }

    return results;
}

cv::Mat DnnInference::forward(const cv::Mat& input)
{
    if (!loaded_)
    {
        spdlog::error("DnnInference: model not loaded");
        return cv::Mat();
    }

    try
    {
        cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0,
            cv::Size(640, 640), cv::Scalar(0, 0, 0), true, false);
        net_.setInput(blob);

        cv::Mat output = net_.forward();
        return output;
    }
    catch (const cv::Exception& e)
    {
        spdlog::error("DnnInference: forward OpenCV exception - {}", e.what());
        return cv::Mat();
    }
    catch (const std::exception& e)
    {
        spdlog::error("DnnInference: forward exception - {}", e.what());
        return cv::Mat();
    }
}

bool DnnInference::isLoaded() const
{
    return loaded_;
}

std::vector<cv::String> DnnInference::getOutputNames() const
{
    if (!loaded_)
    {
        return {};
    }

    try
    {
        std::vector<cv::String> names;
        std::vector<int> outLayers = net_.getUnconnectedOutLayers();

        if (outLayers.empty())
        {
            return {};
        }

        std::vector<std::string> layerNames = net_.getLayerNames();

        for (int i : outLayers)
        {
            if (i > 0 && i <= static_cast<int>(layerNames.size()))
            {
                names.push_back(layerNames[i - 1]);
            }
        }

        return names;
    }
    catch (...)
    {
        return {};
    }
}