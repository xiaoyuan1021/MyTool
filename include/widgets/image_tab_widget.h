#ifndef IMAGE_TAB_WIDGET_H
#define IMAGE_TAB_WIDGET_H

#include <QWidget>
#include "pipeline_manager.h"
#include "pipeline.h"
#include "widgets/i_tab_interfaces.h"

namespace Ui {
class ImageTabWidget;
}

class ImageTabWidget : public QWidget, public ISignalConnectable {
    Q_OBJECT

public:
    explicit ImageTabWidget(PipelineManager* pipelineManager, QWidget* parent = nullptr);
    ~ImageTabWidget();

    void connectSignals(PipelineManager* pm, RoiManager* rm,
                        ImageView* view, RoiUiController* roiCtrl,
                        std::function<void()> onConfigChanged,
                        std::function<void()> onExecuteRequested) override;

signals:
    void channelChanged(int channel);

private slots:
    void on_btn_applyChannel_clicked();
    void on_comboBox_channels_currentIndexChanged(int index);

private:
    PipelineConfig::Channel channelFromIndex(int index) const;

    Ui::ImageTabWidget* m_ui;
    PipelineManager* m_pipelineManager;
};

#endif // IMAGE_TAB_WIDGET_H

