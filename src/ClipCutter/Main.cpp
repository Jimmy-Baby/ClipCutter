#include <Windows.h>
#include <filesystem>

#include "Classes/C_GuiApp.hpp"

int main(int argc, char** argv)
{
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	QApplication qtApp(argc, argv);
	C_GuiApp guiApp(std::filesystem::current_path().generic_string() + "/MassClipCutter");

	guiApp.Show();

	return qtApp.exec();
}
