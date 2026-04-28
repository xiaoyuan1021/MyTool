#include "profile_tab_widget.h"
#include "ui_profile_tab.h"
#include "controllers/profile_manager.h"
#include "ui/logger.h"
#include <QInputDialog>
#include <QMessageBox>

ProfileTabWidget::ProfileTabWidget(ProfileManager* profileManager,
                                   QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ProfileTabWidget)
    , m_profileManager(profileManager)
{
    m_ui->setupUi(this);
    setupConnections();
    refreshProfileList();
}

ProfileTabWidget::~ProfileTabWidget()
{
    delete m_ui;
}

void ProfileTabWidget::setupConnections()
{
    connect(m_ui->btn_saveProfile, &QPushButton::clicked,
            this, &ProfileTabWidget::onSaveProfileClicked);
    connect(m_ui->btn_loadProfile, &QPushButton::clicked,
            this, &ProfileTabWidget::onLoadProfileClicked);
    connect(m_ui->btn_deleteProfile, &QPushButton::clicked,
            this, &ProfileTabWidget::onDeleteProfileClicked);
    connect(m_ui->btn_refreshProfiles, &QPushButton::clicked,
            this, &ProfileTabWidget::onRefreshClicked);
    connect(m_ui->listWidget_profiles, &QListWidget::currentRowChanged,
            this, &ProfileTabWidget::onProfileSelectionChanged);
}

// ==================== 刷新 ====================

void ProfileTabWidget::refreshProfileList()
{
    m_ui->listWidget_profiles->clear();

    QList<ProfileManager::ProfileSummary> profiles = m_profileManager->getProfileList();
    for (const auto& p : profiles) {
        QString displayText = QString("📋 %1\n    ROI: %2 个 | 模板: %3 个\n    %4")
            .arg(p.profileName)
            .arg(p.roiCount)
            .arg(p.templateCount)
            .arg(p.updatedAt.toString("yyyy-MM-dd HH:mm"));

        QListWidgetItem* item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, p.profileId);
        item->setSizeHint(QSize(0, 60));
        m_ui->listWidget_profiles->addItem(item);
    }

    // 恢复选中状态
    QString activeId = m_profileManager->activeProfileId();
    if (!activeId.isEmpty()) {
        for (int i = 0; i < m_ui->listWidget_profiles->count(); ++i) {
            QListWidgetItem* item = m_ui->listWidget_profiles->item(i);
            if (item->data(Qt::UserRole).toString() == activeId) {
                m_ui->listWidget_profiles->setCurrentItem(item);
                break;
            }
        }
    }
}

// ==================== 按钮槽函数 ====================

void ProfileTabWidget::onSaveProfileClicked()
{
    bool ok;
    QString name = QInputDialog::getText(
        this, "保存检测方案", "方案名称:", QLineEdit::Normal, QString(), &ok);

    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    // 检查同名方案是否已存在
    QList<ProfileManager::ProfileSummary> existing = m_profileManager->getProfileList();
    for (const auto& p : existing) {
        if (p.profileName == name.trimmed()) {
            QMessageBox::StandardButton btn = QMessageBox::question(
                this, "方案已存在",
                QString("方案 '%1' 已存在，是否覆盖？").arg(name.trimmed()),
                QMessageBox::Yes | QMessageBox::No);
            if (btn != QMessageBox::Yes) {
                return;
            }
            break;
        }
    }

    if (m_profileManager->saveCurrentAsProfile(name.trimmed())) {
        QMessageBox::information(this, "保存成功",
            QString("方案 '%1' 已保存").arg(name.trimmed()));
        refreshProfileList();
    } else {
        QMessageBox::warning(this, "保存失败", "保存方案失败，请检查日志");
    }
}

void ProfileTabWidget::onLoadProfileClicked()
{
    QString profileId = getSelectedProfileId();
    if (profileId.isEmpty()) return;

    QMessageBox::StandardButton btn = QMessageBox::question(
        this, "加载方案",
        "加载方案将替换当前ROI配置，是否继续？",
        QMessageBox::Yes | QMessageBox::No);

    if (btn != QMessageBox::Yes) {
        return;
    }

    if (m_profileManager->loadProfile(profileId)) {
        const InspectionProfile profile = m_profileManager->getProfile(profileId);
        emit profileApplied(profile.profileName);
        QMessageBox::information(this, "加载成功",
            QString("方案 '%1' 已应用").arg(profile.profileName));
    } else {
        QMessageBox::warning(this, "加载失败", "加载方案失败，请先加载图片");
    }
}

void ProfileTabWidget::onDeleteProfileClicked()
{
    QString profileId = getSelectedProfileId();
    if (profileId.isEmpty()) return;

    const InspectionProfile profile = m_profileManager->getProfile(profileId);

    QMessageBox::StandardButton btn = QMessageBox::question(
        this, "删除方案",
        QString("确定要删除方案 '%1' 吗？\n此操作不可撤销。").arg(profile.profileName),
        QMessageBox::Yes | QMessageBox::No);

    if (btn != QMessageBox::Yes) {
        return;
    }

    if (m_profileManager->deleteProfile(profileId)) {
        refreshProfileList();
        m_ui->label_profileInfo->setText("请选择一个方案查看详情");
    }
}

void ProfileTabWidget::onRefreshClicked()
{
    refreshProfileList();
}

// ==================== 选中变化 ====================

void ProfileTabWidget::onProfileSelectionChanged()
{
    QString profileId = getSelectedProfileId();
    bool hasSelection = !profileId.isEmpty();

    m_ui->btn_loadProfile->setEnabled(hasSelection);
    m_ui->btn_deleteProfile->setEnabled(hasSelection);

    if (hasSelection) {
        updateProfileDetail(profileId);
    }
}

void ProfileTabWidget::updateProfileDetail(const QString& profileId)
{
    InspectionProfile profile = m_profileManager->getProfile(profileId);

    QString info;
    info += QString("<b>方案名称:</b> %1<br>").arg(profile.profileName);
    if (!profile.description.isEmpty()) {
        info += QString("<b>描述:</b> %1<br>").arg(profile.description);
    }
    info += QString("<b>创建时间:</b> %1<br>").arg(profile.createdAt.toString("yyyy-MM-dd HH:mm"));
    info += QString("<b>更新时间:</b> %1<br>").arg(profile.updatedAt.toString("yyyy-MM-dd HH:mm"));
    info += QString("<b>参考图像尺寸:</b> %1 x %2<br>")
        .arg(profile.referenceImageSize.width())
        .arg(profile.referenceImageSize.height());

    info += "<br><b>ROI 模板列表:</b><br>";
    if (profile.roiTemplates.isEmpty()) {
        info += "  （无ROI模板）<br>";
    } else {
        for (const auto& roi : profile.roiTemplates) {
            info += QString("  📍 %1 (相对位置: x=%2, y=%3, w=%4, h=%5)<br>")
                .arg(roi.name)
                .arg(roi.relativeRect.x(), 0, 'f', 3)
                .arg(roi.relativeRect.y(), 0, 'f', 3)
                .arg(roi.relativeRect.width(), 0, 'f', 3)
                .arg(roi.relativeRect.height(), 0, 'f', 3);
            for (const auto& det : roi.detectionItems) {
                info += QString("    └─ %1 [%2]<br>")
                    .arg(det.itemName)
                    .arg(det.enabled ? "启用" : "禁用");
            }
        }
    }

    info += "<br><b>模板库:</b><br>";
    if (profile.templates.isEmpty()) {
        info += "  （无保存的模板）<br>";
    } else {
        for (const auto& tpl : profile.templates) {
            info += QString("  🖼️ %1<br>").arg(tpl.templateName);
        }
    }

    m_ui->label_profileInfo->setText(info);
}

QString ProfileTabWidget::getSelectedProfileId() const
{
    QListWidgetItem* item = m_ui->listWidget_profiles->currentItem();
    if (!item) return QString();
    return item->data(Qt::UserRole).toString();
}