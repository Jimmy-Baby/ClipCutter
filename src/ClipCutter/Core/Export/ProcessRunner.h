#ifndef CLIPCUTTER_CORE_EXPORT_PROCESSRUNNER_H
#define CLIPCUTTER_CORE_EXPORT_PROCESSRUNNER_H

#include "Core/Export/FfmpegCommand.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>

namespace ClipCutter
{
class ProcessRunner : public QObject
{
    Q_OBJECT

public:
    explicit ProcessRunner(QObject* parent = nullptr);
    ~ProcessRunner() override = default;

    virtual void Start(const FfmpegCommand& command) = 0;
    virtual void Terminate() = 0;
    virtual void Kill() = 0;

signals:
    void Started();
    void StandardOutputReady(const QByteArray& data);
    void StandardErrorReady(const QByteArray& data);
    void ErrorOccurred(QProcess::ProcessError error);
    void Finished(int exitCode, QProcess::ExitStatus exitStatus);
};

class QProcessRunner final : public ProcessRunner
{
    Q_OBJECT

public:
    explicit QProcessRunner(QObject* parent = nullptr);

    void Start(const FfmpegCommand& command) override;
    void Terminate() override;
    void Kill() override;

private:
    QProcess Process_;
};
} // namespace ClipCutter

#endif
