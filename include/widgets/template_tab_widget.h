#pragma once

#include <QObject>
#include <QWidget>
#include <QMap>
#include <memory>
#include <opencv2/opencv.hpp>
#include "../algorithm/match_strategy.h"

namespace Ui
{
class TemplateTabWidget;
}

class ImageView;
class RoiManager;
class BatchMatchDialog;
class ProfileManager;

enum class MatchType
{
    OpenCVTM
};

class TemplateTabWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TemplateTabWidget(ImageView* view,
                               RoiManager* roiManager,
                               QWidget* parent = nullptr);
    ~TemplateTabWidget();

    void initialize();

    // 策略管理
    void setMatchType(MatchType type);
    MatchType getCurrentMatchType() const { return m_currentType; }
    QString getCurrentStrategyName() const;

    // 模板操作
    bool hasTemplate() const;
    void clearTemplate();
    TemplateParams getDefaultParams() const { return m_defaultParams; }

    // 方案模板操作
    void setProfileManager(ProfileManager* pm) { m_profileManager = pm; }
    void saveTemplateToProfile();
    void loadTemplateFromProfile();

    // 批量操作
    void batchFindTemplate();

public slots:
    void drawTemplate();
    void clearTemplateDrawing();
    void createTemplate();
    void findTemplate();
    void saveTemplate();
    void loadTemplate();
    void clearMatchResults();
    void onMatchTypeChanged(int index);

    /// 导入文件夹中的所有图片
    void importFolder();

    /// 保存到当前方案（快捷槽函数）
    void onSaveToProfileClicked();
    /// 从方案加载（快捷槽函数）
    void onLoadFromProfileClicked();

    /// 显示批量匹配结果图片
    void showBatchResultImage(const QString& imageId, const cv::Mat& resultImage,
                              const QVector<MatchResult>& matches);
signals:
    void imageToShow(const cv::Mat& image);
    void templateCreated(const QString& name);
    void matchCompleted(int count);

    /// 请求主窗口显示指定图片
    void requestShowImage(const cv::Mat& image);

private:
    void updateUIState(bool hasTemplate);
    void createTemplateFromPolygon(const QVector<QPointF>& points, const QString& name);
    void initializeStrategies();

private:
    Ui::TemplateTabWidget* m_ui;
    ImageView* m_view;
    RoiManager* m_roiManager;
    QWidget* m_parent;

    // 策略管理
    std::shared_ptr<IMatchStrategy> m_currentStrategy;
    MatchType m_currentType;
    QMap<MatchType, std::shared_ptr<IMatchStrategy>> m_strategies;
    TemplateParams m_defaultParams;

    // 方案管理器
    ProfileManager* m_profileManager = nullptr;

    // 批量匹配对话框
    BatchMatchDialog* m_batchDialog = nullptr;

    // 最近一次匹配结果缓存（用于双击查看）
    cv::Mat m_lastResultImage;
    QVector<MatchResult> m_lastMatches;
};
