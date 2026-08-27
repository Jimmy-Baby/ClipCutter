#include "MainWindow.h"

#include <QApplication>

int main(int argumentCount, char* arguments[])
{
    QApplication application(argumentCount, arguments);
    QApplication::setStyle(QStringLiteral("fusion"));

    ClipCutter::MainWindow window;
    window.show();

    return QApplication::exec();
}
