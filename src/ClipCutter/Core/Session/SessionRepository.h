#ifndef CLIPCUTTER_CORE_SESSION_SESSIONREPOSITORY_H
#define CLIPCUTTER_CORE_SESSION_SESSIONREPOSITORY_H

#include "Core/Clip/Clip.h"
#include "Core/Export/OutputDestination.h"

#include <QDateTime>
#include <QStringList>

#include <vector>
#include <utility>

namespace ClipCutter
{
struct SessionData
{
    static constexpr int CurrentSchemaVersion = 1;

    int SchemaVersion = CurrentSchemaVersion;
    std::vector<Clip> Clips;
    OutputDestinationSettings Destination;
    QString SelectedOutputProfile = QStringLiteral("fast-copy");
    bool PreserveMetadata = true;
    bool RecursiveFolderImport = false;
    QStringList SavedPrefixes;
    QString SelectedNamingTemplate = QStringLiteral("{prefix}{original}");
    QStringList MissingSourcePaths;
    QDateTime SavedAtUtc;
    QString ExplicitSessionPath;
    QDateTime ExplicitSaveTimeUtc;
};

struct SessionLoadResult
{
    SessionData Session;
    QString Error;
    QStringList Warnings;

    bool IsValid() const noexcept { return Error.isEmpty(); }
};

class SessionRepository final
{
public:
    static QString DefaultRecoveryPath();
    static bool Save(const QString& filePath, const SessionData& session, QString* error = nullptr,
                     bool recoveryFile = false);
    static SessionLoadResult Load(const QString& filePath);
    static bool ShouldOfferRecovery(const QString& recoveryPath, const QString& explicitSessionPath = {});
    static bool RemoveRecovery(const QString& recoveryPath = DefaultRecoveryPath());
    static QString RelinkSource(const QString& missingSourcePath, const QString& replacementPath,
                                SessionData& session);
};

class SessionDirtyState final
{
public:
    bool IsDirty() const noexcept { return Dirty_; }
    QString FilePath() const { return FilePath_; }
    void MarkModified() noexcept { Dirty_ = true; }
    void MarkSaved(QString filePath) { FilePath_ = std::move(filePath); Dirty_ = false; }
    void Reset() { FilePath_.clear(); Dirty_ = false; }

private:
    QString FilePath_;
    bool Dirty_ = false;
};
} // namespace ClipCutter

#endif
