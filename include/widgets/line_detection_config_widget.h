#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include "roi_config.h"

/**
 * @brief 直线检测配置组件
 * 
 * 提供直线检测的配置界面，包含以下标签页：
 * 1. 图像增强 - 亮度、对比度、伽马、锐化
 * 2. 直线检测 - 算法、参数、参考线匹配
 */
class LineDetectionConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LineDetectionConfigWidget(QWidget* parent = nullptr);
    ~LineDetectionConfigWidget();

    /**
     * @brief 设置直线检测配置
     * @param config 直线检测配置
     */
    void setConfig(const LineDetectionConfig& config);

    /**
     * @brief 获取直线检测配置
     * @return 直线检测配置
     */
    LineDetectionConfig getConfig() const;

    /**
     * @brief 重置为默认配置
     */
    void resetToDefault();

signals:
    /**
     * @brief 配置变化信号
     */
    void configChanged();

private slots:
    void onEnhancementChanged();
    void onLineDetectionChanged();
    void onResetClicked();

private:
    void initUI();
    void initConnections();
    void initEnhancementTab();
    void initLineDetectionTab();
    void updateUIFromConfig(const LineDetectionConfig& config);
    LineDetectionConfig getConfigFromUI() const;

private:
    QTabWidget* m_tabWidget;

    // 图像增强标签页
    QWidget* m_enhancementTab;
    QSlider* m_brightnessSlider;
    QSlider* m_contrastSlider;
    QSlider* m_gammaSlider;
    QSlider* m_sharpenSlider;
    QSpinBox* m_brightnessSpinBox;
    QSpinBox* m_contrastSpinBox;
    QSpinBox* m_gammaSpinBox;
    QSpinBox* m_sharpenSpinBox;

    // 直线检测标签页
    QWidget* m_lineDetectionTab;
    QComboBox* m_algorithmComboBox;
    QDoubleSpinBox* m_rhoSpinBox;
    QDoubleSpinBox* m_thetaSpinBox;
    QSpinBox* m_thresholdSpinBox;
    QDoubleSpinBox* m_minLineLengthSpinBox;
    QDoubleSpinBox* m_maxLineGapSpinBox;
    
    // 参考线匹配
    QCheckBox* m_enableReferenceLineCheckBox;
    QDoubleSpinBox* m_angleThresholdSpinBox;
    QDoubleSpinBox* m_distanceThresholdSpinBox;
    QSpinBox* m_searchRegionWidthSpinBox;

    QPushButton* m_resetButton;
};