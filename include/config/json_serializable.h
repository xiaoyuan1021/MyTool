#pragma once

#include <QJsonObject>

/**
 * @brief JSON 序列化接口
 * 
 * 所有可序列化配置结构体都实现此接口
 * 统一序列化契约，消除重复样板代码
 */
class IJsonSerializable
{
public:
    virtual ~IJsonSerializable() = default;
    
    /**
     * @brief 转换为 JSON 对象
     * @return 序列化后的 JSON 对象
     */
    virtual QJsonObject toJson() const = 0;
    
    /**
     * @brief 从 JSON 对象加载配置
     * @param obj 源 JSON 对象
     */
    virtual void fromJson(const QJsonObject& obj) = 0;
};


// ====================== 序列化辅助宏 ======================

/**
 * @brief 数值类型字段序列化辅助宏
 * 
 * 自动生成简单数值类型的序列化代码
 * 使用方式: JSON_SERIALIZE_INT(fieldName, defaultValue)
 */
#define JSON_SERIALIZE_INT(field, def) \
    obj[#field] = field;

#define JSON_SERIALIZE_DOUBLE(field, def) \
    obj[#field] = field;

#define JSON_SERIALIZE_BOOL(field, def) \
    obj[#field] = field;

#define JSON_SERIALIZE_STRING(field, def) \
    obj[#field] = field;

/**
 * @brief 数值类型字段反序列化辅助宏
 */
#define JSON_DESERIALIZE_INT(field, def) \
    field = obj[#field].toInt(def);

#define JSON_DESERIALIZE_DOUBLE(field, def) \
    field = obj[#field].toDouble(def);

#define JSON_DESERIALIZE_BOOL(field, def) \
    field = obj[#field].toBool(def);

#define JSON_DESERIALIZE_STRING(field, def) \
    field = obj[#field].toString(def);