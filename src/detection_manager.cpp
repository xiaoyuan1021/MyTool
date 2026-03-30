#include "detection_manager.h"
#include <QDebug>

DetectionManager::DetectionManager(QObject* parent)
    : QObject(parent)
    , m_currentSelectedId(-1)
    , m_nextId(1)
{
}

DetectionManager::~DetectionManager()
{
}

int DetectionManager::addDetectionItem(DetectionType type, const QString& name)
{
    // 生成唯一ID
    int newId = generateUniqueId();
    
    // 生成名称
    QString itemName = name.isEmpty() ? generateDetectionItemName(type) : name;
    
    // 创建检测项
    DetectionItem newItem(itemName, type);
    newItem.itemId = QString("det_%1").arg(newId, 8, 16, QChar('0'));
    
    // 添加到列表
    m_detectionItems.append(newItem);
    m_idToIndexMap[newId] = m_detectionItems.size() - 1;
    
    // 发送信号
    emit detectionItemAdded(newId);
    
    qDebug() << "添加检测项: ID=" << newId << ", 类型=" << detectionTypeToString(type) << ", 名称=" << itemName;
    
    return newId;
}

bool DetectionManager::removeDetectionItem(int id)
{
    if (!m_idToIndexMap.contains(id)) {
        qWarning() << "尝试删除不存在的检测项: ID=" << id;
        return false;
    }
    
    int index = m_idToIndexMap[id];
    
    // 如果删除的是当前选中的检测项，需要更新选中状态
    if (m_currentSelectedId == id) {
        m_currentSelectedId = -1;
        // 可以选择选中前一个或后一个检测项
        if (m_detectionItems.size() > 1) {
            int newSelectedIndex = (index > 0) ? index - 1 : 0;
            if (newSelectedIndex < m_detectionItems.size() && newSelectedIndex != index) {
                m_currentSelectedId = newSelectedIndex + 1; // 使用索引+1作为ID
            }
        }
    }
    
    // 从列表中移除
    m_detectionItems.removeAt(index);
    
    // 更新映射
    m_idToIndexMap.remove(id);
    // 更新后续索引
    for (auto it = m_idToIndexMap.begin(); it != m_idToIndexMap.end(); ++it) {
        if (it.value() > index) {
            it.value()--;
        }
    }
    
    // 发送信号
    emit detectionItemRemoved(id);
    
    qDebug() << "删除检测项: ID=" << id;
    
    return true;
}

DetectionItem* DetectionManager::getDetectionItem(int id)
{
    if (!m_idToIndexMap.contains(id)) {
        return nullptr;
    }
    
    int index = m_idToIndexMap[id];
    if (index >= 0 && index < m_detectionItems.size()) {
        return &m_detectionItems[index];
    }
    
    return nullptr;
}

void DetectionManager::setCurrentSelectedId(int id)
{
    if (m_currentSelectedId == id) {
        return;
    }
    
    int oldId = m_currentSelectedId;
    m_currentSelectedId = id;
    
    emit currentSelectionChanged(oldId, id);
    
    qDebug() << "切换选中检测项: " << oldId << " -> " << id;
}

int DetectionManager::getDetectionItemCountByType(DetectionType type) const
{
    int count = 0;
    for (const DetectionItem& item : m_detectionItems) {
        if (item.type == type) {
            count++;
        }
    }
    return count;
}

void DetectionManager::clearAllDetectionItems()
{
    m_detectionItems.clear();
    m_idToIndexMap.clear();
    m_currentSelectedId = -1;
    m_nextId = 1;
}

QJsonArray DetectionManager::toJsonArray() const
{
    QJsonArray jsonArray;
    for (const DetectionItem& item : m_detectionItems) {
        jsonArray.append(item.toJson());
    }
    return jsonArray;
}

void DetectionManager::fromJsonArray(const QJsonArray& jsonArray)
{
    clearAllDetectionItems();
    
    for (const QJsonValue& value : jsonArray) {
        if (value.isObject()) {
            QJsonObject obj = value.toObject();
            DetectionItem item;
            item.fromJson(obj);
            
            // 生成新的ID
            int newId = generateUniqueId();
            m_detectionItems.append(item);
            m_idToIndexMap[newId] = m_detectionItems.size() - 1;
        }
    }
}

TabConfig DetectionManager::getTabConfig(int id) const
{
    const DetectionItem* item = const_cast<DetectionManager*>(this)->getDetectionItem(id);
    if (item) {
        return TabConfig::getConfigForType(item->type);
    }
    return TabConfig();
}

TabConfig DetectionManager::getCurrentTabConfig() const
{
    if (m_currentSelectedId == -1) {
        return TabConfig();
    }
    return getTabConfig(m_currentSelectedId);
}

void DetectionManager::updateDetectionItemConfig(int id, const QJsonObject& config)
{
    // 暂时不实现配置更新，因为DetectionItem没有config成员
    Q_UNUSED(id);
    Q_UNUSED(config);
}

QString DetectionManager::generateDetectionItemName(DetectionType type)
{
    int count = getDetectionItemCountByType(type);
    return QString("%1_%2").arg(detectionTypeToString(type)).arg(count + 1);
}

int DetectionManager::generateUniqueId()
{
    return m_nextId++;
}