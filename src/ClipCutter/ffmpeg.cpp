#include <Windows.h>
#include <process.h>

#include <QDir>
#include <QProcess>
#include <QMessageBox>
#include <QApplication>

#include "ffmpeg.h"

namespace FFmpeg
{
    bool ExecuteFFmpeg(const QString& args, bool showFfmpeg)
	{
        const QByteArray ffmpegPath = (QCoreApplication::applicationDirPath() + "/ffmpeg").toLatin1();
        const QByteArray argsAsByteArray = args.toLatin1();

		SHELLEXECUTEINFOA shExecInfo = { 0 };
		shExecInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
		shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shExecInfo.hwnd = nullptr;
		shExecInfo.lpVerb = nullptr;
        shExecInfo.lpFile = ffmpegPath.data();
        shExecInfo.lpParameters = argsAsByteArray.data();
        shExecInfo.lpDirectory = nullptr;
		shExecInfo.hInstApp = nullptr;

        if (showFfmpeg)
        {
            shExecInfo.nShow = SW_SHOW;
        }
        else
        {
            shExecInfo.nShow = SW_HIDE;
        }

		const bool result = ShellExecuteExA(&shExecInfo);

		WaitForSingleObject(shExecInfo.hProcess, INFINITE);
		CloseHandle(shExecInfo.hProcess);

		return result;
	}

    QString ConstructCmdArgs(const QueueItem* item, const QString& outputDirectory, EReEncodeQuality quality, EReEncodeSpeed speed)
	{
		QString arguments;

		// Tells ffmpeg to overwrite output files
		arguments += " -y";

		// Tells ffmpeg the input path
		arguments += QString(" -i \"%1\"").arg(QDir::toNativeSeparators(item->OriginalPath));

        // Tells ffmpeg the start time
        arguments += QString(" -ss %1ms").arg(item->StartTimeMs);

		// Tells ffmpeg the end time
		arguments += QString(" -to %1ms").arg(item->EndTimeMs);

		// Construct output path
        const QString outputPath = QDir::toNativeSeparators(outputDirectory + QDir::separator() + item->GetOutputName());

        // Create encoding preset string
        QString preset;
        switch (speed)
        {
        case ENCODE_FAST:
            preset = "fast";
            break;

        case ENCODE_MEDIUM:
            preset = "medium";
            break;

        case ENCODE_SLOW:
            preset = "slow";
            break;
        }

        // Tells ffmpeg the encoding settings and output directory
        switch (quality)
        {
        case QUALITY_COPY:
            arguments += QString(" -c:v copy -c:a copy \"%1\"").arg(outputPath);
            break;

        case QUALITY_LOW:
            arguments += QString(" -c:v libx264 -crf 28 -preset %1 -c:a copy \"%2\"").arg(preset, outputPath);
            break;

        case QUALITY_MEDIUM:
            arguments += QString(" -c:v libx264 -crf 23 -preset %1 -c:a copy \"%2\"").arg(preset, outputPath);
            break;

        case QUALITY_HIGH:
            arguments += QString(" -c:v libx264 -crf 18 -preset %1 -c:a copy \"%2\"").arg(preset, outputPath);
            break;
        }

		return arguments;
	}

    void ProcessQueueItem(const QueueItem* item, const QString& outputDirectory, EReEncodeQuality quality, EReEncodeSpeed speed, bool showFfmpeg)
	{
        const QString arguments = ConstructCmdArgs(item, outputDirectory, quality, speed);
        ExecuteFFmpeg(arguments, showFfmpeg);
	}

    bool FFmpegTest()
    {
        return ExecuteFFmpeg("", false);
    }
}
