#include "ccmainwindow.h"
#include "ffmpeg.h"

#include <QApplication>
#include <QMessageBox>
#include <Windows.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setStyle("fusion");

    // Check if FFmpeg is working correctly
    if (FFmpeg::FFmpegTest() == false)
    {
        DWORD lastError = GetLastError();

        if (lastError == ERROR_FILE_NOT_FOUND)
        {
            QMessageBox::critical(nullptr, "FFmpeg error", "FFmpeg could not be found, ensure ClipCutter is running from it's program folder. Closing...");
            return 0;
        }
        else
        {
            QMessageBox::critical(nullptr, "FFmpeg error", "Unknown error relating to FFmpeg, ensure ClipCutter is running from it's program folder. Closing...");
            return 0;
        }
    }

    CClipCutterWindow w;
    w.show();

    return QApplication::exec();
}
