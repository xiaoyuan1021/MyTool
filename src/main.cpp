#include "mainwindow.h"

#include <QApplication>
#include <QResource>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("EdgeVision——让智能视觉检测触手可及");
    w.setWindowIcon(QIcon(":/icons/keji.png"));
    w.show();
    return a.exec();
}
