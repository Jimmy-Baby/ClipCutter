#ifndef CLIPCUTTER_CORE_EXPORT_OUTPUTVERIFIER_H
#define CLIPCUTTER_CORE_EXPORT_OUTPUTVERIFIER_H

#include "Core/Export/ExportJob.h"
#include "Core/Export/OutputProfile.h"
#include "Core/Media/MediaInfo.h"

#include <QObject>

#include <functional>

namespace ClipCutter
{
struct VerificationResult
{
    bool Success = false;
    QString ErrorMessage;
    QString Diagnostics;
    MediaInfo OutputInfo;
};

class OutputVerifier : public QObject
{
public:
    using Callback = std::function<void(VerificationResult)>;

    explicit OutputVerifier(QObject* parent = nullptr) : QObject(parent) {}
    ~OutputVerifier() override = default;
    virtual void Verify(const ExportJob& job, const OutputProfile& profile, Callback callback) = 0;

    static std::chrono::milliseconds DurationTolerance(ETrimMode mode) noexcept;
    static VerificationResult Validate(const ExportJob& job, const OutputProfile& profile,
                                       const MediaInfo& outputInfo, const QString& diagnostics = {});
};

class FfprobeOutputVerifier final : public OutputVerifier
{
public:
    explicit FfprobeOutputVerifier(QObject* parent = nullptr, QString ffprobePath = {});
    void Verify(const ExportJob& job, const OutputProfile& profile, Callback callback) override;

private:
    QString ProgramPath_;
};
} // namespace ClipCutter

#endif
