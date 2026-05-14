#pragma once

#include <QWidget>
#include <QListWidgetItem>
#include <QCheckBox>
#include "config/pipeline_config.h"

namespace Ui {
class ProfileTabWidget;
}

class ProfileManager;
class PipelineManager;

/**
 * @brief 检测方案管理Tab Widget
 *
 * 提供方案的浏览、保存、加载、删除功能
 * 以及 Pipeline 各步骤的启用/禁用控制
 */
class ProfileTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileTabWidget(ProfileManager* profileManager,
                              PipelineManager* pipelineManager,
                              QWidget* parent = nullptr);
    ~ProfileTabWidget();

    /// 刷新方案列表
    void refreshProfileList();

signals:
    /// 方案已加载，请求主窗口刷新ROI显示
    void profileApplied(const QString& profileName);
    /// 步骤启用状态变化，请求刷新Pipeline
    void stepConfigChanged();

private slots:
    void onSaveProfileClicked();
    void onLoadProfileClicked();
    void onDeleteProfileClicked();
    void onRefreshClicked();
    void onProfileSelectionChanged();

private:
    void setupConnections();
    void setupStepCheckboxes();
    void updateProfileDetail(const QString& profileId);
    QString getSelectedProfileId() const;
    void onStepCheckboxChanged(int stepIndex, bool checked);

    Ui::ProfileTabWidget* m_ui;
    ProfileManager* m_profileManager;
    PipelineManager* m_pipelineManager;

    QCheckBox* m_stepCheckboxes[PipelineConfig::STEP_COUNT]{};
};