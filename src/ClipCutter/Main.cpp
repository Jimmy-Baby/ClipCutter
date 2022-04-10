#include <Windows.h>
#include <filesystem>
#include <thread>

#include <QtWidgets/QApplication>
#include "Classes/C_GuiApp.hpp"

int InputThread(C_GuiApp* guiApp, const std::atomic_bool* flag)
{
	while (flag->load(std::memory_order_acquire) == false)
	{
		if (guiApp->IsTyping() == false)
		{
			if (GetAsyncKeyState(0x51) & 0x8000)
				guiApp->SetStartPoint();

			if (GetAsyncKeyState(0x57) & 0x8000)
				guiApp->SetEndPoint();
		}

		Sleep(50);
	}

	return 0;
}

int main(int argc, char** argv)
{
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	std::atomic_bool threadShutdown = false;
	QApplication qtApp(argc, argv);
	C_GuiApp guiApp(std::filesystem::current_path().generic_string() + "/MassClipCutter");

	guiApp.SetThreadShutdownFlag(threadShutdown);
	guiApp.Show();

	std::thread(InputThread, &guiApp, &threadShutdown).detach();

	return qtApp.exec();
}
