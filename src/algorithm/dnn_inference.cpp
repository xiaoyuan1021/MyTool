#include "dnn_inference.h"
#include <spdlog/spdlog.h>

bool DnnInference::loadModel(const QString& modelPath, const QString& configPath)
{
    loaded_ = false;

    try
    {
        std::string model = modelPath.toStdString();
        std::string config = configPath.toStdString();

        // OpenCV DNN 会根据文件扩展名自动推断模型格式
        if (config.empty())
        {
            net_ = cv::dnn::readNet(model);
        }
        else
        {
            net_ = cv::dnn::readNet(model, config);
        }

        // 验证模型是否有效（通过检查层数量，避免使用不可靠的 empty()）
        try {
            auto layerNames = net_.getLayerNames();
            if (layerNames.empty()) {
                spdlog::error("DnnInference: 模型加载后层列表为空: {}", model);
                return false;
            }
            spdlog::info("DnnInference: 模型包含 {} 层", layerNames.size());
        } catch (...) {
            spdlog::warn("DnnInference: 无法验证模型层信息，但仍标记为已加载");
        }

        loaded_ = true;
        spdlog::info("DnnInference: 模型加载成功: {}", model);
        return true;
    }
    catch (const cv::Exception& e)
    {
        spdlog::error("DnnInference: OpenCV异常 - {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        spdlog::error("DnnInference: 异常 - {}", e.what());
        return false;
    }
    catch (...)
    {
        spdlog::error("DnnInference: 未知异常");
        return false;
    }
}

cv::Mat DnnInference::forward(const cv::Mat& input)
{
    if (!loaded_)
    {
        spdlog::error("DnnInference: 模型未加载，无法执行推理");
        return cv::Mat();
    }

    try
    {
        // 创建 blob 作为输入（默认：不缩放、不均值归一化、BGR输入）
        cv::Mat blob = cv::dnn::blobFromImage(input);
        net_.setInput(blob);

        cv::Mat output = net_.forward();
        return output;
    }
    catch (const cv::Exception& e)
    {
        spdlog::error("DnnInference: 推理异常 - {}", e.what());
        return cv::Mat();
    }
    catch (const std::exception& e)
    {
        spdlog::error("DnnInference: 推理异常 - {}", e.what());
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

    std::vector<cv::String> names;
    std::vector<int> outLayers = net_.getUnconnectedOutLayers();
    std::vector<std::string> layerNames = net_.getLayerNames();

    for (int i : outLayers)
    {
        names.push_back(layerNames[i - 1]);
    }

    return names;
}