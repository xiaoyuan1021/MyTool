#include "mainwindow.h"

#include <QApplication>
#include <QResource>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("Vision_Tool");
    w.setWindowIcon(QIcon(":/keji.png"));
    w.show();
    return a.exec();
}
