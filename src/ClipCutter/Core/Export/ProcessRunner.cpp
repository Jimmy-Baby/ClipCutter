#include "Core/Export/ProcessRunner.h"

namespace ClipCutter
{
ProcessRunner::ProcessRunner(QObject* parent) : QObject(parent) {}

QProcessRunner::QProcessRunner(QObject* parent) : ProcessRunner(parent)
{
    connect(&Process_, &QProcess::started, this, &ProcessRunner::Started);
    connect(&Process_, &QProcess::readyReadStandardOutput, this,
            [this]() { emit StandardOutputReady(Process_.readAllStandardOutput()); });
    connect(&Process_, &QProcess::readyReadStandardError, this,
            [this]() { emit StandardErrorReady(Process_.readAllStandardError()); });
    connect(&Process_, &QProcess::errorOccurred, this, &ProcessRunner::ErrorOccurred);
    connect(&Process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](const int exitCode, const QProcess::ExitStatus exitStatus)
            {
                const QByteArray remainingOutput = Process_.readAllStandardOutput();
                const QByteArray remainingError = Process_.readAllStandardError();

                if (!remainingOutput.isEmpty())
                {
                    emit StandardOutputReady(remainingOutput);
                }

                if (!remainingError.isEmpty())
                {
                    emit StandardErrorReady(remainingError);
                }

                emit Finished(exitCode, exitStatus);
            });
}

void QProcessRunner::Start(const FfmpegCommand& command)
{
    Process_.setProgram(command.Program);
    Process_.setArguments(command.Arguments);
    Process_.setProcessChannelMode(QProcess::SeparateChannels);
    Process_.start();
}

void QProcessRunner::Terminate()
{
    Process_.terminate();
}

void QProcessRunner::Kill()
{
    Process_.kill();
}
} // namespace ClipCutter
