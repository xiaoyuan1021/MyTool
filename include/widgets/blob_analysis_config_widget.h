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
 * @brief Blob分析配置组件
 * 
 * 提供Blob分析的完整配置界面，包含以下标签页：
 * 1. 图像增强 - 亮度、对比度、伽马、锐化
 * 2. 过滤 - 面积、圆形度、凸性、惯性比
 * 3. 补正 - 启用补正、补正方法、阈值
 * 4. 处理 - 阈值类型、阈值、形态学操作
 * 5. 提取 - 提取方法、轮廓模式
 * 6. 判定 - Blob数量、面积阈值
 */
class BlobAnalysisConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BlobAnalysisConfigWidget(QWidget* parent = nullptr);
    ~BlobAnalysisConfigWidget();

    /**
     * @brief 设置Blob分析配置
     * @param config Blob分析配置
     */
    void setConfig(const BlobAnalysisConfig& config);

    /**
     * @brief 获取Blob分析配置
     * @return Blob分析配置
     */
    BlobAnalysisConfig getConfig() const;

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
    void onFilterChanged();
    void onCorrectionChanged();
    void onProcessingChanged();
    void onExtractionChanged();
    void onJudgmentChanged();
    void onResetClicked();

private:
    void initUI();
    void initConnections();
    void initEnhancementTab();
    void initFilterTab();
    void initCorrectionTab();
    void initProcessingTab();
    void initExtractionTab();
    void initJudgmentTab();
    void updateUIFromConfig(const BlobAnalysisConfig& config);
    BlobAnalysisConfig getConfigFromUI() const;

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

    // 过滤标签页
    QWidget* m_filterTab;
    QSpinBox* m_minAreaSpinBox;
    QSpinBox* m_maxAreaSpinBox;
    QDoubleSpinBox* m_minCircularitySpinBox;
    QDoubleSpinBox* m_maxCircularitySpinBox;
    QDoubleSpinBox* m_minConvexitySpinBox;
    QDoubleSpinBox* m_maxConvexitySpinBox;
    QDoubleSpinBox* m_minInertiaSpinBox;
    QDoubleSpinBox* m_maxInertiaSpinBox;

    // 补正标签页
    QWidget* m_correctionTab;
    QCheckBox* m_enableCorrectionCheckBox;
    QComboBox* m_correctionMethodComboBox;
    QDoubleSpinBox* m_correctionThresholdSpinBox;

    // 处理标签页
    QWidget* m_processingTab;
    QComboBox* m_thresholdTypeComboBox;
    QSpinBox* m_thresholdValueSpinBox;
    QCheckBox* m_invertBinaryCheckBox;
    QComboBox* m_morphologyOpComboBox;
    QSpinBox* m_morphologySizeSpinBox;

    // 提取标签页
    QWidget* m_extractionTab;
    QComboBox* m_extractionMethodComboBox;
    QCheckBox* m_useHierarchyCheckBox;
    QComboBox* m_contourModeComboBox;

    // 判定标签页
    QWidget* m_judgmentTab;
    QSpinBox* m_minBlobCountSpinBox;
    QSpinBox* m_maxBlobCountSpinBox;
    QDoubleSpinBox* m_minAreaThresholdSpinBox;
    QDoubleSpinBox* m_maxAreaThresholdSpinBox;

    QPushButton* m_resetButton;
};