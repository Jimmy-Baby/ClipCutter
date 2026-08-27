#include <Windows.h>
#include <shellapi.h>

#include <QCoreApplication>
#include <QDir>

#include "FFmpeg.h"

namespace clipcutter::FFmpeg
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

        if (!result || shExecInfo.hProcess == nullptr)
        {
            return false;
        }

		WaitForSingleObject(shExecInfo.hProcess, INFINITE);
		CloseHandle(shExecInfo.hProcess);

		return result;
	}

    QString ConstructCmdArgs(const ExportSegment& segment, const QString& outputDirectory, ReEncodeQuality quality)
	{
		QString arguments;

		// Tells ffmpeg to overwrite output files
		arguments += " -y";

		// Tells ffmpeg the input path
        arguments += QString(" -i \"%1\"").arg(QDir::toNativeSeparators(segment.sourcePath));

        // Tells ffmpeg the start time
        arguments += QString(" -ss %1ms").arg(segment.range.start().count());

		// Tells ffmpeg the end time
        arguments += QString(" -to %1ms").arg(segment.range.end().count());

		// Construct output path
        const QString outputPath = QDir::toNativeSeparators(
            QDir(outputDirectory).absoluteFilePath(segment.outputFileName));


        // Tells ffmpeg the encoding settings and output directory
        switch (quality)
        {
        case ReEncodeQuality::Copy:
            arguments += QString(" -c:v copy -c:a copy \"%1\"").arg(outputPath);
            break;

        case ReEncodeQuality::Lowest:
            arguments += QString(" -c:v libx264 -crf 35 -preset faster -c:a copy \"%1\"").arg(outputPath);
            break;

        case ReEncodeQuality::Low:
            arguments += QString(" -c:v libx264 -crf 30 -preset fast -c:a copy \"%1\"").arg(outputPath);
            break;

        case ReEncodeQuality::Medium:
            arguments += QString(" -c:v libx264 -crf 25 -preset fast -c:a copy \"%1\"").arg(outputPath);
            break;

        case ReEncodeQuality::High:
            arguments += QString(" -c:v libx264 -crf 20 -preset medium -c:a copy \"%1\"").arg(outputPath);
            break;

        case ReEncodeQuality::Highest:
            arguments += QString(" -c:v libx264 -crf 15 -preset slow -c:a copy \"%1\"").arg(outputPath);
            break;
        }

		return arguments;
	}

    void ProcessSegment(
        const ExportSegment& segment,
        const QString& outputDirectory,
        ReEncodeQuality quality,
        bool showFfmpeg)
	{
        const QString arguments = ConstructCmdArgs(segment, outputDirectory, quality);
        ExecuteFFmpeg(arguments, showFfmpeg);
	}

    bool FFmpegTest()
    {
        return ExecuteFFmpeg("", false);
    }
}
