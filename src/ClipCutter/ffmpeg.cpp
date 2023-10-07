#include <Windows.h>
#include <process.h>

#include <QDir>
#include <QProcess>
#include <QMessageBox>

#include "ffmpeg.h"

namespace FFmpeg
{
	bool ExecuteFFmpeg(const QString& args)
	{
		const std::string argsString = args.toStdString();

		SHELLEXECUTEINFOA ShExecInfo = { 0 };
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
		ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		ShExecInfo.hwnd = nullptr;
		ShExecInfo.lpVerb = nullptr;
		ShExecInfo.lpFile = "ffmpeg";
		ShExecInfo.lpParameters = argsString.data();
		ShExecInfo.lpDirectory = nullptr;
		ShExecInfo.nShow = SW_SHOW;
		ShExecInfo.hInstApp = nullptr;

		const bool result = ShellExecuteExA(&ShExecInfo);

		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		CloseHandle(ShExecInfo.hProcess);

		return result;
	}

    QString ConstructCmdArgs(const QueueItem* item, const QString& outputDirectory, EReEncodeQuality quality)
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
		const QString outputPath = QDir::toNativeSeparators(outputDirectory + QDir::separator() + item->VideoName);

        // Tells ffmpeg the encoding settings and output directory
        switch (quality)
        {
        case QUALITY_COPY:
            arguments += QString(" -c:v copy -c:a copy \"%1\"").arg(outputPath);
            break;

        case QUALITY_LOW:
            arguments += QString(" -c:v libx264 -crf 27 -preset fast -c:a copy \"%1\"").arg(outputPath);
            break;

        case QUALITY_MEDIUM:
            arguments += QString(" -c:v libx264 -crf 24 -preset fast -c:a copy \"%1\"").arg(outputPath);
            break;

        case QUALITY_HIGH:
            arguments += QString(" -c:v libx264 -crf 21 -preset fast -c:a copy \"%1\"").arg(outputPath);
            break;
        }

		return arguments;
	}

    void ProcessQueueItem(const QueueItem* item, const QString& outputDirectory, EReEncodeQuality quality)
	{
        const QString arguments = ConstructCmdArgs(item, outputDirectory, quality);
		ExecuteFFmpeg(arguments);
	}
}
