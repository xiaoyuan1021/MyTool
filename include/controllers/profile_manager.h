#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include "data/inspection_profile.h"

class RoiManager;
class PipelineManager;

/**
 * @brief 检测方案管理器
 *
 * 职责：
 * 1. 从 RoiManager 导出/导入检测方案
 * 2. 管理已保存的方案列表（浏览、加载、删除）
 * 3. 管理方案中的模板库（保存模板、加载模板）
 * 4. 方案的持久化存储
 */
class ProfileManager : public QObject
{
    Q_OBJECT

public:
    explicit ProfileManager(RoiManager* roiManager,
                            PipelineManager* pipelineManager,
                            QObject* parent = nullptr);
    ~ProfileManager() = default;

    // ==================== 方案管理 ====================

    /// 保存当前状态为方案
    bool saveCurrentAsProfile(const QString& profileName,
                              const QString& description = QString());

    /// 加载方案到当前图片
    bool loadProfile(const QString& profileId);

    /// 获取所有已保存方案的摘要
    struct ProfileSummary {
        QString profileId;
        QString profileName;
        QString description;
        QDateTime updatedAt;
        int roiCount;
        int templateCount;
    };
    QList<ProfileSummary> getProfileList() const;

    /// 删除方案
    bool deleteProfile(const QString& profileId);

    /// 获取方案详情
    InspectionProfile getProfile(const QString& profileId) const;

    // ==================== 模板管理 ====================

    /// 将当前模板保存到指定方案
    bool saveTemplateToProfile(const QString& profileId,
                               const QString& templateName,
                               const cv::Mat& templateImage,
                               const QVector<QPointF>& polygonPoints,
                               int matchMethod = 5);

    /// 从方案加载模板
    bool loadTemplateFromProfile(const QString& profileId,
                                 const QString& templateName,
                                 cv::Mat& outImage,
                                 QVector<QPointF>& outPolygonPoints,
                                 int& outMatchMethod);

    /// 获取方案中所有模板名称
    QStringList getTemplateNames(const QString& profileId) const;

    /// 从方案中移除模板
    bool removeTemplateFromProfile(const QString& profileId,
                                   const QString& templateName);

    // ==================== 路径管理 ====================

    /// 获取方案存储根目录
    QString profilesDirectory() const;

    /// 获取当前活跃方案ID
    QString activeProfileId() const { return m_activeProfileId; }

    /// 设置当前活跃方案
    void setActiveProfile(const QString& profileId);

signals:
    void profileSaved(const QString& profileId, const QString& profileName);
    void profileLoaded(const QString& profileId, const QString& profileName);
    void profileDeleted(const QString& profileId);
    void profileListChanged();
    void templateSaved(const QString& profileId, const QString& templateName);
    void templateRemoved(const QString& profileId, const QString& templateName);

private:
    /// 扫描方案目录，构建内存缓存
    void scanProfiles();

    /// 检查方案目录是否存在
    void ensureProfilesDir() const;

    RoiManager* m_roiManager;
    PipelineManager* m_pipelineManager;
    QString m_activeProfileId;  ///< 当前活跃的方案ID

    /// 内存缓存：profileId -> InspectionProfile
    QMap<QString, InspectionProfile> m_profiles;
};