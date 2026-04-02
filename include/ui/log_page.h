#ifndef LOG_PAGE_H
#define LOG_PAGE_H

#include <QWidget>

namespace Ui {
class LogPage;
}

class LogPage : public QWidget {
    Q_OBJECT

public:
    explicit LogPage(QWidget* parent = nullptr);
    ~LogPage();

    void appendLog(const QString& message);
    void clearLog();
    
    // 初始化日志页面，连接Logger
    void initialize();
    
    // 刷新日志显示（带颜色）
    void refreshLogs();

signals:
    // 请求返回主页信号
    void requestGoHome();

private slots:
    void on_btn_clearLog_clicked();
    void on_btn_openLog_clicked();
    void on_btn_home_clicked();

private:
    Ui::LogPage* m_ui;
};

#endif // LOG_PAGE_H
