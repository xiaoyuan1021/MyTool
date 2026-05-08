#include "core/profile_manager.h"
#include "roi_manager.h"
#include "core/pipeline_manager.h"
#include "logger.h"
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>

ProfileManager::ProfileManager(RoiManager* roiManager,
                               PipelineManager* pipelineManager,
                               QObject* parent)
    : QObject(parent)
    , m_roiManager(roiManager)
    , m_pipelineManager(pipelineManager)
{
    scanProfiles();
}

// ==================== 路径管理 ====================

QString ProfileManager::profilesDirectory() const
{
    // 存储在应用程序目录下的 resources/profiles
    QString appDir = QCoreApplication::applicationDirPath();
    return appDir + "/profiles";
}

void ProfileManager::ensureProfilesDir() const
{
    QDir dir(profilesDirectory());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

// ==================== 扫描方案 ====================

void ProfileManager::scanProfiles()
{
    m_profiles.clear();

    QDir dir(profilesDirectory());
    if (!dir.exists()) {
        return;
    }

    // 扫描所有子目录
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& subDir : subDirs) {
        QString profilePath = profilesDirectory() + "/" + subDir;
        InspectionProfile profile;
        if (profile.loadFromDirectory(profilePath)) {
            m_profiles[profile.profileId] = profile;
        }
    }

    Logger::instance()->info(
        QString("[ProfileManager] 扫描到 %1 个检测方案").arg(m_profiles.size()));
}

// ==================== 方案管理 ====================

bool ProfileManager::saveCurrentAsProfile(const QString& profileName,
                                          const QString& description)
{
    if (profileName.isEmpty()) {
        Logger::instance()->error("[ProfileManager] 方案名称为空，无法保存");
        return false;
    }

    cv::Mat currentImage = m_roiManager->getFullImage();
    QSize imageSize(currentImage.cols, currentImage.rows);

    // 从 RoiManager 导出方案
    InspectionProfile profile = m_roiManager->exportAsProfile(profileName, imageSize);
    profile.description = description;

    // 如果有活跃方案ID，保留ID
    if (!m_activeProfileId.isEmpty() && m_profiles.contains(m_activeProfileId)) {
        if (m_profiles[m_activeProfileId].profileName == profileName) {
            profile.profileId = m_activeProfileId;
        }
    }

    // 保存到目录
    ensureProfilesDir();
    if (!profile.saveToDirectory(profilesDirectory())) {
        Logger::instance()->error(
            QString("[ProfileManager] 保存方案失败: %1").arg(profileName));
        return false;
    }

    // 更新缓存
    m_profiles[profile.profileId] = profile;
    m_activeProfileId = profile.profileId;

    Logger::instance()->info(
        QString("[ProfileManager] 方案已保存: %1 (ROI数: %2, 模板数: %3)")
            .arg(profileName)
            .arg(profile.roiTemplates.size())
            .arg(profile.templates.size()));

    emit profileSaved(profile.profileId, profileName);
    emit profileListChanged();
    return true;
}

bool ProfileManager::loadProfile(const QString& profileId)
{
    if (!m_profiles.contains(profileId)) {
        Logger::instance()->error(
            QString("[ProfileManager] 方案不存在: %1").arg(profileId));
        return false;
    }

    const InspectionProfile& profile = m_profiles[profileId];

    cv::Mat currentImage = m_roiManager->getFullImage();
    if (currentImage.empty()) {
        Logger::instance()->error("[ProfileManager] 当前无图片，无法应用方案");
        return false;
    }

    QSize imageSize(currentImage.cols, currentImage.rows);
    m_roiManager->applyProfile(profile, imageSize);

    m_activeProfileId = profileId;

    Logger::instance()->info(
        QString("[ProfileManager] 方案已加载: %1 (ROI数: %2)")
            .arg(profile.profileName)
            .arg(profile.roiTemplates.size()));

    emit profileLoaded(profileId, profile.profileName);
    return true;
}

QList<ProfileManager::ProfileSummary> ProfileManager::getProfileList() const
{
    QList<ProfileSummary> list;
    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        ProfileSummary summary;
        summary.profileId = it.value().profileId;
        summary.profileName = it.value().profileName;
        summary.description = it.value().description;
        summary.updatedAt = it.value().updatedAt;
        summary.roiCount = it.value().roiTemplates.size();
        summary.templateCount = it.value().templates.size();
        list.append(summary);
    }

    // 按更新时间倒序
    std::sort(list.begin(), list.end(),
        [](const ProfileSummary& a, const ProfileSummary& b) {
            return a.updatedAt > b.updatedAt;
        });

    return list;
}

bool ProfileManager::deleteProfile(const QString& profileId)
{
    if (!m_profiles.contains(profileId)) {
        return false;
    }

    const InspectionProfile& profile = m_profiles[profileId];
    QString dirPath = profilesDirectory() + "/" + profile.profileName;

    // 删除目录
    QDir dir(dirPath);
    if (dir.exists()) {
        dir.removeRecursively();
    }

    QString name = profile.profileName;
    m_profiles.remove(profileId);

    if (m_activeProfileId == profileId) {
        m_activeProfileId.clear();
    }

    Logger::instance()->info(
        QString("[ProfileManager] 方案已删除: %1").arg(name));

    emit profileDeleted(profileId);
    emit profileListChanged();
    return true;
}

InspectionProfile ProfileManager::getProfile(const QString& profileId) const
{
    if (m_profiles.contains(profileId)) {
        return m_profiles[profileId];
    }
    return InspectionProfile();
}

void ProfileManager::setActiveProfile(const QString& profileId)
{
    if (m_profiles.contains(profileId)) {
        m_activeProfileId = profileId;
    }
}

// ==================== 模板管理 ====================

bool ProfileManager::saveTemplateToProfile(const QString& profileId,
                                           const QString& templateName,
                                           const cv::Mat& templateImage,
                                           const QVector<QPointF>& polygonPoints,
                                           int matchMethod)
{
    if (!m_profiles.contains(profileId)) {
        Logger::instance()->error(
            QString("[ProfileManager] 方案不存在: %1").arg(profileId));
        return false;
    }

    InspectionProfile& profile = m_profiles[profileId];

    TemplateEntry entry(templateName);
    entry.templateImage = templateImage.clone();
    entry.params.polygonPoints = polygonPoints;
    entry.params.matchMethod = matchMethod;

    profile.addTemplate(entry);

    // 重新保存方案到目录
    ensureProfilesDir();
    profile.saveToDirectory(profilesDirectory());

    Logger::instance()->info(
        QString("[ProfileManager] 模板已保存: 方案=%1, 模板=%2")
            .arg(profile.profileName).arg(templateName));

    emit templateSaved(profileId, templateName);
    return true;
}

bool ProfileManager::loadTemplateFromProfile(const QString& profileId,
                                             const QString& templateName,
                                             cv::Mat& outImage,
                                             QVector<QPointF>& outPolygonPoints,
                                             int& outMatchMethod)
{
    if (!m_profiles.contains(profileId)) {
        return false;
    }

    const InspectionProfile& profile = m_profiles[profileId];
    const TemplateEntry* entry = profile.findTemplate(templateName);
    if (!entry) {
        Logger::instance()->warning(
            QString("[ProfileManager] 模板不存在: 方案=%1, 模板=%2")
                .arg(profile.profileName).arg(templateName));
        return false;
    }

    // 如果模板图像为空，尝试从文件加载
    if (entry->templateImage.empty()) {
        QString dirPath = profilesDirectory() + "/" + profile.profileName;
        TemplateEntry* nonConstEntry = const_cast<TemplateEntry*>(entry);
        if (!nonConstEntry->load(dirPath)) {
            Logger::instance()->error(
                QString("[ProfileManager] 模板加载失败: %1").arg(templateName));
            return false;
        }
    }

    outImage = entry->templateImage.clone();
    outPolygonPoints = entry->params.polygonPoints;
    outMatchMethod = entry->params.matchMethod;
    return true;
}

QStringList ProfileManager::getTemplateNames(const QString& profileId) const
{
    if (!m_profiles.contains(profileId)) {
        return {};
    }
    return m_profiles[profileId].templateNames();
}

bool ProfileManager::removeTemplateFromProfile(const QString& profileId,
                                               const QString& templateName)
{
    if (!m_profiles.contains(profileId)) {
        return false;
    }

    InspectionProfile& profile = m_profiles[profileId];
    if (profile.removeTemplate(templateName)) {
        // 重新保存方案
        ensureProfilesDir();
        profile.saveToDirectory(profilesDirectory());

        emit templateRemoved(profileId, templateName);
        return true;
    }
    return false;
}