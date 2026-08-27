#include "MainWindow.h"
#include "App/Settings/SettingsRepository.h"

#include <QApplication>

int main(int argumentCount, char* arguments[])
{
    QApplication application(argumentCount, arguments);
    QApplication::setOrganizationName(ClipCutter::SettingsRepository::OrganisationName());
    QApplication::setApplicationName(ClipCutter::SettingsRepository::ApplicationName());

    ClipCutter::MainWindow window;
    window.show();

    return QApplication::exec();
}
