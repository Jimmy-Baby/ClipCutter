#include <Windows.h>
#include <filesystem>

#include <QtWidgets/QApplication>
#include "Classes/CCutterApp.hpp"


int main(int argc, char** argv)
{
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	QApplication qtApp(argc, argv);
	CCutterApp guiApp(std::filesystem::current_path().string() + "/MassClipCutter");

	QCoreApplication::instance()->installEventFilter(&guiApp);

	guiApp.Show();

	return QApplication::exec();
}
