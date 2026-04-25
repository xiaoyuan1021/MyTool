#pragma once

/**
 * 项目全局常量定义
 * 
 * 将散布在代码中的魔法值集中管理，提高可维护性
 * 
 * 使用方式：
 *   #include "constants.h"
 *   timer->setInterval(AppConstants::DEBOUNCE_FILTER_MS);
 */

namespace AppConstants {

    // ========== 防抖定时器时间 (ms) ==========
    
    /// 主处理流程防抖时间（Tab切换、参数变化等）
    constexpr int DEBOUNCE_PROCESS_MS = 150;
    
    /// 过滤器Tab防抖时间
    constexpr int DEBOUNCE_FILTER_MS = 50;
    
    /// 增强参数防抖时间
    constexpr int DEBOUNCE_ENHANCE_MS = 200;

    // ========== 显示与UI超时 (ms) ==========
    
    /// 状态栏消息默认显示时间
    constexpr int STATUS_MESSAGE_TIMEOUT_MS = 5000;
    
    /// 像素信息显示超时时间
    constexpr int PIXEL_INFO_TIMEOUT_MS = 5000;
    
    /// 检测完成状态消息显示时间
    constexpr int DETECTION_COMPLETE_TIMEOUT_MS = 10000;

    // ========== 系统监控 ==========
    
    /// 系统监控更新间隔
    constexpr int SYSTEM_MONITOR_INTERVAL_MS = 1000;
    
    /// 系统监控最小更新间隔
    constexpr int SYSTEM_MONITOR_MIN_INTERVAL_MS = 100;

    // ========== Pipeline渲染 ==========
    
    /// 遮罩叠加层默认透明度 (0.0 ~ 1.0)
    constexpr float DEFAULT_OVERLAY_ALPHA = 0.3f;

    // ========== 处理间隔 ==========
    
    /// Pending处理请求的延迟触发时间
    constexpr int PENDING_PROCESS_DELAY_MS = 50;

} // namespace AppConstants