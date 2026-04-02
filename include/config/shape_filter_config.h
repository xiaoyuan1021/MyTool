#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include "filter_condition.h"

/**
 * 筛选模式枚举
 * 
 * 定义多个条件之间的逻辑关系
 */
enum class ShapeFilterLogicMode
{
    And,  // 满足所有条件（逻辑与）
    Or    // 满足任意条件（逻辑或）
};

/**
 * 获取筛选模式名称
 */
inline QString getFilterModeName(ShapeFilterLogicMode mode)
{
    return (mode == ShapeFilterLogicMode::And) ? "满足所有条件" : "满足任意条件";
}

/**
 * 形状筛选配置结构体
 * 
 * 包含多个筛选条件和筛选模式
 */
struct ShapeFilterConfig
{
    QVector<FilterCondition> conditions;  // 筛选条件列表
    ShapeFilterLogicMode mode;            // 筛选模式（AND/OR）

    /**
     * 默认构造函数
     */
    ShapeFilterConfig()
        : mode(ShapeFilterLogicMode::And)
    {}

    /**
     * 添加筛选条件
     */
    void addCondition(const FilterCondition& cond)
    {
        conditions.append(cond);
    }

    /**
     * 清空所有条件
     */
    void clear()
    {
        conditions.clear();
    }

    /**
     * 获取有效条件数量
     */
    int getEnabledCount() const
    {
        int count = 0;
        for (const auto& cond : conditions) {
            if (cond.isValid()) count++;
        }
        return count;
    }

    /**
     * 是否有有效条件
     */
    bool hasValidConditions() const
    {
        return getEnabledCount() > 0;
    }

    /**
     * 获取描述字符串
     */
    QString toString() const
    {
        if (!hasValidConditions()) {
            return "未启用筛选";
        }

        QStringList condStrs;
        for (const auto& cond : conditions)
        {
            if (cond.isValid())
            {
                condStrs.append(cond.toString());
            }
        }

        QString separator = (mode == ShapeFilterLogicMode::And) ? " 且 " : " 或 ";
        return condStrs.join(separator);
    }

    /**
     * 设置筛选模式
     */
    void setMode(ShapeFilterLogicMode newMode)
    {
        mode = newMode;
    }

    /**
     * 重置为默认配置
     */
    void reset()
    {
        conditions.clear();
        mode = ShapeFilterLogicMode::And;
    }
};