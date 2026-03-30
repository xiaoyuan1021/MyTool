#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include "roi_config.h"

/**
 * @brief Tab配置结构
 * 定义每种检测类型需要显示的Tab列表
 */
struct TabConfig {
    QStringList tabNames;      // Tab名称列表
    QList<int> tabIndices;     // Tab索引列表（动态维护）
    
    TabConfig() {}
    
    TabConfig(const QStringList& names) : tabNames(names) {}
    
    /**
     * @brief 获取Blob分析的Tab配置
     */
    static TabConfig getBlobConfig() {
        return TabConfig({
            "图像",
            "增强", 
            "补正",
            "过滤",
            "处理",
            "提取",
            "判定"
        });
    }
    
    /**
     * @brief 获取直线检测的Tab配置
     */
    static TabConfig getLineConfig() {
        return TabConfig({
            "图像",
            "增强",
            "直线"
        });
    }
    
    /**
     * @brief 获取条码识别的Tab配置
     */
    static TabConfig getBarcodeConfig() {
        return TabConfig({
            "图像",
            "增强",
            "条码"
        });
    }
    
    /**
     * @brief 根据检测类型获取Tab配置
     */
    static TabConfig getConfigForType(DetectionType type) {
        switch (type) {
            case DetectionType::Blob:
                return getBlobConfig();
            case DetectionType::Line:
                return getLineConfig();
            case DetectionType::Barcode:
                return getBarcodeConfig();
            default:
                return TabConfig();
        }
    }
};

/**
 * @brief 检测项管理器
 * 
 * 负责管理所有检测项，提供添加、删除、切换等功能
 */
class DetectionManager : public QObject
{
    Q_OBJECT

public:
    explicit DetectionManager(QObject* parent = nullptr);
    ~DetectionManager();

    /**
     * @brief 添加检测项
     * @param type 检测类型
     * @param name 检测项名称（可选，为空时自动生成）
     * @return 新添加的检测项ID，失败返回-1
     */
    int addDetectionItem(DetectionType type, const QString& name = "");

    /**
     * @brief 删除检测项
     * @param id 检测项ID
     * @return 是否删除成功
     */
    bool removeDetectionItem(int id);

    /**
     * @brief 获取检测项
     * @param id 检测项ID
     * @return 检测项指针，不存在返回nullptr
     */
    DetectionItem* getDetectionItem(int id);

    /**
     * @brief 获取所有检测项
     * @return 检测项列表
     */
    const QList<DetectionItem>& getAllDetectionItems() const { return m_detectionItems; }

    /**
     * @brief 获取当前选中的检测项ID
     * @return 当前选中的检测项ID，未选中返回-1
     */
    int getCurrentSelectedId() const { return m_currentSelectedId; }

    /**
     * @brief 设置当前选中的检测项
     * @param id 检测项ID
     */
    void setCurrentSelectedId(int id);

    /**
     * @brief 获取检测项数量
     * @return 检测项数量
     */
    int getDetectionItemCount() const { return m_detectionItems.size(); }

    /**
     * @brief 获取指定类型的检测项数量
     * @param type 检测类型
     * @return 该类型的检测项数量
     */
    int getDetectionItemCountByType(DetectionType type) const;

    /**
     * @brief 清空所有检测项
     */
    void clearAllDetectionItems();

    /**
     * @brief 序列化为JSON
     * @return JSON数组
     */
    QJsonArray toJsonArray() const;

    /**
     * @brief 从JSON数组加载
     * @param jsonArray JSON数组
     */
    void fromJsonArray(const QJsonArray& jsonArray);

    /**
     * @brief 获取Tab配置
     * @param id 检测项ID
     * @return Tab配置
     */
    TabConfig getTabConfig(int id) const;

    /**
     * @brief 获取当前选中检测项的Tab配置
     * @return Tab配置
     */
    TabConfig getCurrentTabConfig() const;

    /**
     * @brief 更新检测项配置
     * @param id 检测项ID
     * @param config 配置JSON对象
     */
    void updateDetectionItemConfig(int id, const QJsonObject& config);

signals:
    /**
     * @brief 检测项添加信号
     * @param id 新添加的检测项ID
     */
    void detectionItemAdded(int id);

    /**
     * @brief 检测项删除信号
     * @param id 被删除的检测项ID
     */
    void detectionItemRemoved(int id);

    /**
     * @brief 当前选中检测项改变信号
     * @param oldId 之前选中的ID
     * @param newId 新选中的ID
     */
    void currentSelectionChanged(int oldId, int newId);

    /**
     * @brief 检测项配置更新信号
     * @param id 检测项ID
     */
    void detectionItemConfigUpdated(int id);

private:
    /**
     * @brief 生成检测项名称
     * @param type 检测类型
     * @return 生成的名称
     */
    QString generateDetectionItemName(DetectionType type);

    /**
     * @brief 生成唯一ID
     * @return 唯一ID
     */
    int generateUniqueId();

private:
    QList<DetectionItem> m_detectionItems;  // 检测项列表
    QMap<int, int> m_idToIndexMap;          // ID到索引的映射
    int m_currentSelectedId;                // 当前选中的检测项ID
    int m_nextId;                           // 下一个可用ID
};