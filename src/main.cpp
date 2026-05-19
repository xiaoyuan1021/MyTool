#include "mainwindow.h"

#include <QApplication>
#include <QResource>
#include <QSplashScreen>
#include <QPainter>
#include <QFont>
#include <QTimer>

static QPixmap createSplashPixmap()
{
    QPixmap pix(600, 380);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // 渐变背景
    QLinearGradient bg(0, 0, 600, 380);
    bg.setColorAt(0.0, QColor("#F0F4FA"));
    bg.setColorAt(0.5, QColor("#F8FAFE"));
    bg.setColorAt(1.0, QColor("#E8F0FA"));
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, 600, 380, 16, 16);

    // 顶部分割装饰线
    QLinearGradient lineGrad(0, 0, 600, 0);
    lineGrad.setColorAt(0.0, QColor(79, 172, 254, 0));
    lineGrad.setColorAt(0.5, QColor(79, 172, 254, 180));
    lineGrad.setColorAt(1.0, QColor(0, 242, 254, 0));
    p.setBrush(lineGrad);
    p.drawRoundedRect(0, 0, 600, 4, 2, 2);

    // 底部装饰线
    p.setBrush(QColor(79, 172, 254, 40));
    p.drawRoundedRect(0, 376, 600, 4, 2, 2);

    // 中心渐变圆装饰
    QRadialGradient circle(300, 160, 180);
    circle.setColorAt(0.0, QColor(79, 172, 254, 20));
    circle.setColorAt(0.5, QColor(0, 242, 254, 10));
    circle.setColorAt(1.0, QColor(79, 172, 254, 0));
    p.setBrush(circle);
    p.drawEllipse(QPointF(300, 160), 180, 120);

    // 产品名
    QFont titleFont("Microsoft YaHei", 32, QFont::Bold);
    p.setFont(titleFont);

    QLinearGradient textGrad(0, 0, 300, 0);
    textGrad.setColorAt(0.0, QColor("#4FACFE"));
    textGrad.setColorAt(1.0, QColor("#00F2FE"));
    p.setPen(QPen(textGrad, 1));
    p.drawText(QRect(0, 100, 600, 60), Qt::AlignCenter, "EdgeVision");

    // 副标题
    QFont subFont("Microsoft YaHei", 13, QFont::Normal);
    p.setFont(subFont);
    p.setPen(QColor("#6B7280"));
    p.drawText(QRect(0, 160, 600, 40), Qt::AlignCenter, "让智能视觉检测触手可及");

    // 版本信息
    QFont verFont("Microsoft YaHei", 10, QFont::Normal);
    p.setFont(verFont);
    p.setPen(QColor("#9CA3AF"));
    p.drawText(QRect(0, 300, 600, 30), Qt::AlignCenter, "v1.0  |  研电赛参赛作品");

    // 底部加载指示
    QFont loadFont("Microsoft YaHei", 9, QFont::Normal);
    p.setFont(loadFont);
    p.setPen(QColor("#B0BEC5"));

    // 小点装饰
    for (int i = 0; i < 3; ++i) {
        int x = 280 + i * 20;
        p.setBrush(QColor(79, 172, 254, 100 + i * 50));
        p.drawEllipse(QPointF(x, 340), 4, 4);
    }

    p.end();
    return pix;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 创建启动闪屏
    QPixmap splashPix = createSplashPixmap();
    QSplashScreen splash(splashPix, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    splash.setWindowFlags(splash.windowFlags() | Qt::Tool);
    splash.show();
    a.processEvents();

    MainWindow w;
    w.setWindowTitle("EdgeVision——让智能视觉检测触手可及");
    w.setWindowIcon(QIcon(":/icons/keji.png"));

    // 平滑过渡：先显示主窗口但透明，闪屏渐出
    w.show();
    splash.finish(&w);

    return a.exec();
}
