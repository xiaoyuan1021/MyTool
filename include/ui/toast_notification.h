#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>

class ToastNotification : public QWidget
{
    Q_OBJECT
public:
    explicit ToastNotification(QWidget* parent = nullptr);
    void showMessage(const QString& msg, int timeoutMs = 2500);

private:
    QLabel* m_label;
    QTimer* m_timer;
};
