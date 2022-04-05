#include <Windows.h>
#include <filesystem>
#include <thread>

#include "Classes/C_GuiApp.hpp"

int main(int argc, char** argv)
{
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	QApplication qtApp(argc, argv);
	C_GuiApp guiApp(std::filesystem::current_path().generic_string() + "/MassClipCutter");

	guiApp.Show();

	std::thread inputThread(C_GuiApp::InputThread);

	return qtApp.exec();
}
