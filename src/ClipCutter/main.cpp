#include "ccmainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CClipCutterWindow w;

    a.setStyle("fusion");
    w.show();

    return a.exec();
}
