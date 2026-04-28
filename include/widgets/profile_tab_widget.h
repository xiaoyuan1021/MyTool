#pragma once

#include <QWidget>
#include <QListWidgetItem>

namespace Ui {
class ProfileTabWidget;
}

class ProfileManager;

/**
 * @brief 检测方案管理Tab Widget
 *
 * 提供方案的浏览、保存、加载、删除功能
 */
class ProfileTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileTabWidget(ProfileManager* profileManager,
                              QWidget* parent = nullptr);
    ~ProfileTabWidget();

    /// 刷新方案列表
    void refreshProfileList();

signals:
    /// 方案已加载，请求主窗口刷新ROI显示
    void profileApplied(const QString& profileName);

private slots:
    void onSaveProfileClicked();
    void onLoadProfileClicked();
    void onDeleteProfileClicked();
    void onRefreshClicked();
    void onProfileSelectionChanged();

private:
    void setupConnections();
    void updateProfileDetail(const QString& profileId);
    QString getSelectedProfileId() const;

    Ui::ProfileTabWidget* m_ui;
    ProfileManager* m_profileManager;
};