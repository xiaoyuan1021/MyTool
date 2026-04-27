#pragma once

#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>

/**
 * 双向绑定 QSlider 与 QSpinBox 的值变化信号。
 * 调用后拖动 Slider 会更新 SpinBox，修改 SpinBox 会更新 Slider。
 * 使用前请确保两者的 setRange() 一致，否则 setValue 会静默 clamp。
 */
inline void bindSliderAndSpinBox(QSlider* slider, QSpinBox* spinBox)
{
    QObject::connect(slider, &QSlider::valueChanged, spinBox, &QSpinBox::setValue);
    QObject::connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
}
