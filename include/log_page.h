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

private slots:
    void on_btn_clearLog_clicked();
    void on_btn_openLog_clicked();

private:
    Ui::LogPage* m_ui;
};

#endif // LOG_PAGE_H
