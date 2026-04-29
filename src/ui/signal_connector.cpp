#include "signal_connector.h"
#include "logger.h"
#include "ui/image_view.h"
#include "controllers/roi_ui_controller.h"
#include "widgets/i_signal_connectable.h"

SignalConnector::SignalConnector(
    PipelineManager* pipelineManager,
    RoiManager* roiManager,
    ImageView* view,
    RoiUiController* roiUiController,
    QObject* parent)
    : QObject(parent)
    , m_pipelineManager(pipelineManager)
    , m_roiManager(roiManager)
    , m_view(view)
    , m_roiUiController(roiUiController)
{
}

void SignalConnector::connectTabSignals(const QString& tabName, QWidget* widget)
{
    Q_UNUSED(tabName);

    // 使用 ISignalConnectable 接口进行多态连接
    // 每个 Tab Widget 自己知道如何建立自己的信号连接
    auto* connectable = dynamic_cast<ISignalConnectable*>(widget);
    if (connectable) {
        connectable->connectSignals(
            m_pipelineManager, m_roiManager, m_view, m_roiUiController,
            [this]() { emit requestRefresh(); },
            [this]() { emit processAndDisplay(); }
        );
    } else {
        Logger::instance()->warning(
            QString("[SignalConnector] Tab '%1' 未实现 ISignalConnectable 接口，跳过信号连接").arg(tabName));
    }
}