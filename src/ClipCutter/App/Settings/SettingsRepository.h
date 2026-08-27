#ifndef CLIPCUTTER_APP_SETTINGS_SETTINGSREPOSITORY_H
#define CLIPCUTTER_APP_SETTINGS_SETTINGSREPOSITORY_H

#include "Core/Export/OutputDestination.h"

#include <QByteArray>
#include <QStringList>

#include <memory>

class QSettings;

namespace ClipCutter
{
struct ApplicationSettings
{
    QByteArray WindowGeometry;
    QByteArray WindowState;
    QByteArray MainSplitterState;
    int PreviewVolume = 100;
    QString LastImportDirectory;
    EOutputDestinationMode DestinationMode = EOutputDestinationMode::SourceRelative;
    QString SourceRelativeSubdirectory = QStringLiteral("ClipCutterOutput");
    QString LastFixedOutputDirectory;
    QString SelectedOutputProfile = QStringLiteral("fast-copy");
    bool PreserveMetadata = true;
    bool RecursiveFolderImport = false;
    QStringList SavedPrefixes;
    QString SelectedNamingTemplate = QStringLiteral("{prefix}{original}");
    QStringList RecentSessionFiles;
    bool TimelineAutoPlay = true;
};

class SettingsRepository final
{
public:
    SettingsRepository();
    explicit SettingsRepository(const QString& iniFilePath);
    ~SettingsRepository();

    ApplicationSettings Load() const;
    bool Save(const ApplicationSettings& settings, QString* error = nullptr);
    bool Reset(QString* error = nullptr);

    static QString OrganisationName();
    static QString ApplicationName();

private:
    std::unique_ptr<QSettings> Settings_;
};
} // namespace ClipCutter

#endif
