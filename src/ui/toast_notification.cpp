#include "ui/toast_notification.h"
#include <QVBoxLayout>

static const int MARGIN = 8;
static const int MAX_WIDTH = 480;

ToastNotification::ToastNotification(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    m_label = new QLabel(this);
    m_label->setStyleSheet(
        "background-color: rgba(0, 120, 215, 220);"
        "color: white;"
        "padding: 10px 24px;"
        "border-radius: 6px;"
        "font-size: 14px;"
        "font-weight: bold;"
    );
    m_label->setAlignment(Qt::AlignCenter);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &QWidget::hide);
}

void ToastNotification::showMessage(const QString& msg, int timeoutMs)
{
    m_timer->stop();

    m_label->setText(msg);
    m_label->adjustSize();

    int pw = parentWidget()->width();
    int w = qMin(MAX_WIDTH, pw - MARGIN * 2);
    setFixedSize(w, m_label->sizeHint().height());
    QPoint parentTopLeft = parentWidget()->mapToGlobal(QPoint(0, 0));
    move(parentTopLeft.x() + qMax(0, (pw - w) / 2), parentTopLeft.y() + MARGIN);

    show();
    raise();
    m_timer->start(timeoutMs);
}
