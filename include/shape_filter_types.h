#pragma once

/**
 * 形状筛选相关类型聚合头文件
 * 
 * 这个文件现在只做include聚合，保持向后兼容性
 * 实际定义已分离到独立的头文件中：
 * - shape_feature.h: ShapeFeature枚举和相关函数
 * - filter_condition.h: FilterCondition结构体
 * - shape_filter_config.h: ShapeFilterConfig结构体和ShapeFilterLogicMode枚举
 */

#include "shape_feature.h"
#include "filter_condition.h"
#include "shape_filter_config.h"

// 向后兼容的别名（可选）
using FilterMode = ShapeFilterLogicMode;