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
 * @brief 条码识别配置组件
 * 
 * 提供条码识别的配置界面，包含以下标签页：
 * 1. 图像增强 - 亮度、对比度、伽马、锐化
 * 2. 条码识别 - 启用、类型、参数、预处理
 */
class BarcodeRecognitionConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BarcodeRecognitionConfigWidget(QWidget* parent = nullptr);
    ~BarcodeRecognitionConfigWidget();

    /**
     * @brief 设置条码识别配置
     * @param config 条码识别配置
     */
    void setConfig(const BarcodeRecognitionConfig& config);

    /**
     * @brief 获取条码识别配置
     * @return 条码识别配置
     */
    BarcodeRecognitionConfig getConfig() const;

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
    void onBarcodeRecognitionChanged();
    void onResetClicked();

private:
    void initUI();
    void initConnections();
    void initEnhancementTab();
    void initBarcodeRecognitionTab();
    void updateUIFromConfig(const BarcodeRecognitionConfig& config);
    BarcodeRecognitionConfig getConfigFromUI() const;

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

    // 条码识别标签页
    QWidget* m_barcodeRecognitionTab;
    QCheckBox* m_enableBarcodeCheckBox;
    QComboBox* m_codeTypesComboBox;
    QSpinBox* m_maxNumSymbolsSpinBox;
    QCheckBox* m_returnQualityCheckBox;
    QDoubleSpinBox* m_minConfidenceSpinBox;
    QSpinBox* m_timeoutSpinBox;
    
    // 预处理参数
    QCheckBox* m_enablePreprocessingCheckBox;
    QComboBox* m_preprocessMethodComboBox;
    QSpinBox* m_binarizationThresholdSpinBox;
    QSpinBox* m_morphologySizeSpinBox;

    QPushButton* m_resetButton;
};