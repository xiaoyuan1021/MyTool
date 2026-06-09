#include "pipeline_result_handler.h"
#include "algorithm/image_utils.h"
#include "algorithm/display_renderer.h"
#include "widgets/i_tab_interfaces.h"
#include "widgets/object_detection_tab_widget.h"
#include "logger.h"
#include "utils/benchmark.h"

PipelineResultHandler::PipelineResultHandler(QObject *parent)
    : QObject(parent)
{
}

void PipelineResultHandler::setDependencies(TabManager* tabManager, RoiManager* roiManager, ImageView* imageView, PipelineManager* pipelineManager)
{
    m_tabManager = tabManager;
    m_roiManager = roiManager;
    m_imageView = imageView;
    m_pipelineManager = pipelineManager;
}

void PipelineResultHandler::onPipelineResult(const PipelineResult& result)
{
    try {
        if (!result.isSuccess()) {
            spdlog::debug("[PipelineResult] Pipeline执行失败: {}", result.errorMessage().toStdString());
            emit statusMessage("处理失败: " + result.errorMessage(), 3000);
            return;
        }

        const PipelineContext& ctx = result.context();
        cv::Mat displayImage = DisplayRenderer::render(
            ctx, m_pipelineManager->getDisplayMode());

        // [NOTE] 将结果存入per-ROI缓存
        if (m_roiManager) {
            QString activeRoiId = m_roiManager->getActiveRoiId();
            if (!activeRoiId.isEmpty()) {
                m_pipelineManager->cacheResult(activeRoiId, ctx);
            }
        }

        // 通过 IResultUpdatable 接口分发结果
        distributeResults(ctx);

        // 目标检测特殊处理（不走 Pipeline），单独计时
        // [NOTE] 传入 ctx.srcBgr 作为检测源图像，避免异步竞态导致检测跑在错误的图片上
        double pipelineMs = result.elapsedMs();
        double detectionMs = 0;
        {
            BenchmarkTimer t("ObjectDetection", &detectionMs);
            handleObjectDetection(displayImage, ctx.srcBgr);
        }

        if (m_imageView) {
            QImage qimg = ImageUtils::matToQImage(displayImage);
            m_imageView->setImage(qimg);
        }

        // 显示总处理时间（Pipeline + 目标检测）
        double totalMs = pipelineMs + detectionMs;
        QString msg = QString("处理完成 (%1 ms)").arg(totalMs, 0, 'f', 1);
        emit statusMessage(msg, 2000);
        emit pipelineTimingUpdated(totalMs);
    } catch (const cv::Exception& ex) {
        spdlog::error("Pipeline结果处理错误: {}", ex.what());
        emit statusMessage("处理失败", 3000);
    } catch (const std::exception& ex) {
        spdlog::error("Pipeline结果处理异常: {}", ex.what());
        emit statusMessage("处理失败", 3000);
    } catch (...) {
        spdlog::info("[PipelineResult] 未知异常");
        spdlog::error("Pipeline结果处理未知异常");
        emit statusMessage("处理失败", 3000);
    }
}

void PipelineResultHandler::distributeResults(const PipelineContext& result)
{
    if (!m_tabManager) return;

    // 遍历所有已创建的 Tab，通过 IResultUpdatable 接口分发结果
    // 新增检测类型只需在 Tab Widget 中实现 IResultUpdatable，此处无需修改
    for (auto it = m_tabManager->allTabs().constBegin();
         it != m_tabManager->allTabs().constEnd(); ++it) {
        if (auto* updatable = dynamic_cast<IResultUpdatable*>(it.value())) {
            updatable->updateFromPipeline(result);
        }
    }
}

void PipelineResultHandler::setVideoMode(bool active)
{
    m_isVideoMode = active;
}

void PipelineResultHandler::handleObjectDetection(cv::Mat& displayImage, const cv::Mat& pipelineSource)
{
    if (!m_tabManager || !m_roiManager || !m_pipelineManager) return;

    if (!m_pipelineManager->getConfigSnapshot().enableObjectDetection) return;

    if (auto* objTab = m_tabManager->getTabAs<ObjectDetectionTabWidget>("目标检测"); objTab && objTab->isModelLoaded()) {
        // [NOTE] 使用 Pipeline 实际处理的源图像进行检测，而不是 m_roiManager->getCurrentImage()
        // 避免异步 Pipeline 执行期间用户切换图片，导致检测跑在错误的图片上
        cv::Mat detectImage = pipelineSource;
        if (!detectImage.empty()) {
            std::vector<DetectionResult> detResults;
            if (m_isVideoMode) {
                // 视频推理：使用 ONNX Runtime（GPU 加速）
                detResults = objTab->runDetectionOrt(detectImage);
            } else {
                // 静图检测：使用 OpenCV DNN
                detResults = objTab->runDetection(detectImage);
            }
            objTab->updateDetectionResults(detResults);
            drawDetectionResults(displayImage, detResults);
        }
    }
}

void PipelineResultHandler::drawDetectionResults(cv::Mat& image, const std::vector<DetectionResult>& results)
{
    if (image.empty()) return;

    try {
        for (const auto& det : results) {
            cv::rectangle(image, det.box, cv::Scalar(0, 255, 0), 2);

            std::string label = det.className + " " + cv::format("%.2f", det.confidence);
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.8, 2, &baseline);

            cv::Point textOrg(det.box.x, det.box.y - 5);
            if (textOrg.y - textSize.height < 0) {
                textOrg.y = det.box.y + textSize.height + 5;
            }

            cv::rectangle(image,
                cv::Point(textOrg.x, textOrg.y - textSize.height - 2),
                cv::Point(textOrg.x + textSize.width + 2, textOrg.y + 4),
                cv::Scalar(0, 255, 0), -1);

            cv::putText(image, label, textOrg,
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 2);
        }
    } catch (const cv::Exception& ex) {
        spdlog::error("绘制检测结果错误: {}", ex.what());
    } catch (...) {
        spdlog::info("[DrawResults] 未知异常");
        spdlog::error("绘制检测结果未知异常");
    }
}
