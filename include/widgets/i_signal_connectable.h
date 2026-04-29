#pragma once

#include <functional>

class PipelineManager;
class RoiManager;
class ImageView;
class RoiUiController;

/**
 * @brief Tab Widget 信号连接接口
 *
 * 每个需要与外部模块建立信号连接的 Tab Widget 都应实现此接口。
 * 新增 Tab 时只需：① 继承此接口 ② 实现 connectSignals()
 * SignalConnector 永远不需要修改。
 */
class ISignalConnectable
{
public:
    virtual ~ISignalConnectable() = default;

    /**
     * @brief 建立该 Tab Widget 与其他模块之间的信号连接
     * @param pm              PipelineManager（配置读写、Pipeline执行）
     * @param rm              RoiManager（ROI数据管理）
     * @param view            ImageView（图像显示）
     * @param roiCtrl         RoiUiController（ROI UI交互，部分Tab需要）
     * @param requestRefresh  请求刷新（触发防抖+脏标记）
     * @param processAndDisplay 请求立即处理并显示
     */
    virtual void connectSignals(PipelineManager* pm, RoiManager* rm,
                                ImageView* view, RoiUiController* roiCtrl,
                                std::function<void()> requestRefresh,
                                std::function<void()> processAndDisplay) = 0;
};