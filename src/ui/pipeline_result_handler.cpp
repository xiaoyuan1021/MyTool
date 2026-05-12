#include "pipeline_result_handler.h"
#include "algorithm/image_utils.h"
#include "ui/display_renderer.h"
#include "widgets/judge_tab_widget.h"
#include "widgets/line_tab_widget.h"
#include "widgets/barcode_tab_widget.h"
#include "widgets/object_detection_tab_widget.h"
#include "logger.h"

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

void PipelineResultHandler::watchPipeline(QFutureWatcher<PipelineContext>* watcher)
{
    m_watcher = watcher;
    connect(m_watcher, &QFutureWatcher<PipelineContext>::finished,
            this, &PipelineResultHandler::onPipelineFinished);
}

void PipelineResultHandler::onPipelineFinished()
{
    if (!m_watcher) return;

    try {
        PipelineContext result = m_watcher->result();
        cv::Mat displayImage = DisplayRenderer::render(
            result, m_pipelineManager->getDisplayMode());

        handleJudgeTabResult(result);
        handleLineTabResult(result);
        handleBarcodeTabResult(result);
        handleObjectDetection(displayImage);

        if (m_imageView) {
            QImage qimg = ImageUtils::matToQImage(displayImage);
            m_imageView->setImage(qimg);
        }

        emit statusMessage("处理完成", 2000);
    } catch (const cv::Exception& ex) {
        qDebug() << "[PipelineResult] OpenCV错误:" << ex.what();
        Logger::instance()->error(QString("Pipeline结果处理错误: %1").arg(ex.what()));
        emit statusMessage("处理失败", 3000);
    } catch (const std::exception& ex) {
        qDebug() << "[PipelineResult] 标准异常:" << ex.what();
        Logger::instance()->error(QString("Pipeline结果处理异常: %1").arg(ex.what()));
        emit statusMessage("处理失败", 3000);
    } catch (...) {
        qDebug() << "[PipelineResult] 未知异常";
        Logger::instance()->error("Pipeline结果处理未知异常");
        emit statusMessage("处理失败", 3000);
    }

    emit processingFinished();
}

void PipelineResultHandler::handleJudgeTabResult(const PipelineContext& result)
{
    if (!m_tabManager) return;
    
    if (auto* judgeTab = m_tabManager->getTabAs<JudgeTabWidget>("判定")) {
        judgeTab->setCurrentRegionCount(result.currentRegions);
    }
}

void PipelineResultHandler::handleLineTabResult(const PipelineContext& result)
{
    if (!m_tabManager) return;

    if (auto* lineTab = m_tabManager->getTabAs<LineDetectTabWidget>("直线")) {
        PipelineConfig cfg = m_pipelineManager->getConfigSnapshot();
        lineTab->updateMatchResultStatus(
            result.matchedLineCount,
            result.totalLineCount,
            cfg.lineDetect.angleThreshold,
            cfg.lineDetect.distanceThreshold,
            cfg.lineDetect.enableReferenceLineMatch
        );
    }
}

void PipelineResultHandler::handleBarcodeTabResult(const PipelineContext& result)
{
    if (!m_tabManager) return;
    
    if (auto* barcodeTab = m_tabManager->getTabAs<BarcodeTabWidget>("条码")) {
        barcodeTab->updateResultsTable(result.barcodeResults);
        barcodeTab->updateStatus(result.barcodeStatus);
    }
}

void PipelineResultHandler::handleObjectDetection(cv::Mat& displayImage)
{
    if (!m_tabManager || !m_roiManager) return;
    
    if (auto* objTab = m_tabManager->getTabAs<ObjectDetectionTabWidget>("目标检测"); objTab && objTab->isModelLoaded()) {
        cv::Mat detectImage = m_roiManager->getCurrentImage();
        if (!detectImage.empty()) {
            std::vector<DetectionResult> detResults = objTab->runDetection(detectImage);
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
        qDebug() << "[DrawResults] OpenCV错误:" << ex.what();
        Logger::instance()->error(QString("绘制检测结果错误: %1").arg(ex.what()));
    } catch (...) {
        qDebug() << "[DrawResults] 未知异常";
        Logger::instance()->error("绘制检测结果未知异常");
    }
}