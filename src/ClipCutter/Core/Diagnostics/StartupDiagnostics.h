#ifndef CLIPCUTTER_CORE_DIAGNOSTICS_STARTUPDIAGNOSTICS_H
#define CLIPCUTTER_CORE_DIAGNOSTICS_STARTUPDIAGNOSTICS_H

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QString>

namespace ClipCutter
{
struct ProfileSupport
{
    bool Supported = false;
    QString Reason;
};

struct StartupDiagnosticsResult
{
    bool FfmpegAvailable = false;
    bool FfprobeAvailable = false;
    QString FfmpegVersion;
    QString FfprobeVersion;
    QHash<QString, ProfileSupport> Profiles;
    QString Diagnostics;
};

class StartupDiagnostics final : public QObject
{
    Q_OBJECT

public:
    explicit StartupDiagnostics(QObject* parent = nullptr, QString ffmpegPath = {}, QString ffprobePath = {});
    void Start();

signals:
    void Completed(const ClipCutter::StartupDiagnosticsResult& result);

private:
    enum class EStep { FfmpegVersion, FfprobeVersion, Encoders, Muxers, Done };
    void RunStep();
    void FinishStep(int exitCode, QProcess::ExitStatus status);
    void Complete();
    static QString DefaultPath(const QString& executable);

    QString FfmpegPath_;
    QString FfprobePath_;
    EStep Step_ = EStep::FfmpegVersion;
    QProcess Process_;
    QByteArray Output_;
    QByteArray Error_;
    QString Encoders_;
    QString Muxers_;
    StartupDiagnosticsResult Result_;
};
} // namespace ClipCutter

Q_DECLARE_METATYPE(ClipCutter::StartupDiagnosticsResult)

#endif
