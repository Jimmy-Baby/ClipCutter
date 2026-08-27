#include "App/Settings/SettingsRepository.h"

#include "Core/Export/OutputProfile.h"
#include "Core/Naming/NamingTemplate.h"

#include <QDir>
#include <QSettings>

namespace ClipCutter
{
namespace Keys
{
constexpr auto WindowGeometry = "window/geometry";
constexpr auto WindowState = "window/state";
constexpr auto Splitter = "window/mainSplitter";
constexpr auto Volume = "preview/volume";
constexpr auto LastImport = "import/lastDirectory";
constexpr auto DestinationMode = "output/destinationMode";
constexpr auto SourceSubdirectory = "output/sourceRelativeSubdirectory";
constexpr auto FixedDirectory = "output/lastFixedDirectory";
constexpr auto Profile = "output/profile";
constexpr auto Metadata = "output/preserveMetadata";
constexpr auto Recursive = "import/recursiveFolders";
constexpr auto Prefixes = "naming/savedPrefixes";
constexpr auto Template = "naming/template";
constexpr auto RecentSessions = "session/recentFiles";
constexpr auto TimelineAutoPlay = "timeline/autoPlay";
} // namespace Keys

namespace
{
bool BoolValue(const QVariant& value, const bool fallback)
{
    if (!value.isValid()) return fallback;
    const QString text = value.toString().trimmed().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1")) return true;
    if (text == QStringLiteral("false") || text == QStringLiteral("0")) return false;
    return fallback;
}

QStringList CleanPaths(const QStringList& paths)
{
    QStringList result;
    for (const QString& path : paths)
    {
        const QString clean = QDir::cleanPath(path.trimmed());
        if (!clean.isEmpty() && clean != QStringLiteral(".") && !result.contains(clean, Qt::CaseInsensitive))
            result.append(clean);
        if (result.size() == 10) break;
    }
    return result;
}

QString CleanOptionalPath(const QString& path)
{
    const QString trimmed = path.trimmed();
    return trimmed.isEmpty() ? QString() : QDir::cleanPath(trimmed);
}
} // namespace

SettingsRepository::SettingsRepository()
    : Settings_(std::make_unique<QSettings>(QSettings::NativeFormat, QSettings::UserScope,
                                             OrganisationName(), ApplicationName()))
{
}

SettingsRepository::SettingsRepository(const QString& iniFilePath)
    : Settings_(std::make_unique<QSettings>(iniFilePath, QSettings::IniFormat))
{
}

SettingsRepository::~SettingsRepository() = default;

QString SettingsRepository::OrganisationName() { return QStringLiteral("ClipCutter"); }
QString SettingsRepository::ApplicationName() { return QStringLiteral("ClipCutter"); }

ApplicationSettings SettingsRepository::Load() const
{
    ApplicationSettings result;
    if (Settings_->status() != QSettings::NoError) return result;
    result.WindowGeometry = Settings_->value(Keys::WindowGeometry).toByteArray();
    result.WindowState = Settings_->value(Keys::WindowState).toByteArray();
    result.MainSplitterState = Settings_->value(Keys::Splitter).toByteArray();
    bool volumeOk = false;
    const int volume = Settings_->value(Keys::Volume, result.PreviewVolume).toInt(&volumeOk);
    result.PreviewVolume = volumeOk ? qBound(0, volume, 100) : 100;
    result.LastImportDirectory = CleanOptionalPath(Settings_->value(Keys::LastImport).toString());

    bool modeOk = false;
    const int mode = Settings_->value(Keys::DestinationMode, 0).toInt(&modeOk);
    result.DestinationMode = modeOk && mode == 1 ? EOutputDestinationMode::FixedDirectory
                                                 : EOutputDestinationMode::SourceRelative;
    const QString subdirectory = Settings_->value(Keys::SourceSubdirectory, result.SourceRelativeSubdirectory).toString().trimmed();
    OutputDestinationSettings destination;
    destination.SourceRelativeSubdirectory = subdirectory;
    if (OutputDestination::Validate(destination, {}).isEmpty()) result.SourceRelativeSubdirectory = subdirectory;
    result.LastFixedOutputDirectory = CleanOptionalPath(Settings_->value(Keys::FixedDirectory).toString());

    const QString profile = Settings_->value(Keys::Profile, result.SelectedOutputProfile).toString();
    if (OutputProfiles::Find(profile) != nullptr) result.SelectedOutputProfile = profile;
    result.PreserveMetadata = BoolValue(Settings_->value(Keys::Metadata), true);
    result.RecursiveFolderImport = BoolValue(Settings_->value(Keys::Recursive), false);
    for (const QString& prefix : Settings_->value(Keys::Prefixes).toStringList())
    {
        const QString cleaned = prefix.trimmed();
        if (!cleaned.isEmpty() && !result.SavedPrefixes.contains(cleaned, Qt::CaseInsensitive)) result.SavedPrefixes.append(cleaned);
    }
    const QString namingTemplate = Settings_->value(Keys::Template, result.SelectedNamingTemplate).toString();
    if (NamingTemplate::Validate(namingTemplate).isEmpty()) result.SelectedNamingTemplate = namingTemplate;
    result.RecentSessionFiles = CleanPaths(Settings_->value(Keys::RecentSessions).toStringList());
    result.TimelineAutoPlay = BoolValue(Settings_->value(Keys::TimelineAutoPlay), true);
    return result;
}

bool SettingsRepository::Save(const ApplicationSettings& settings, QString* error)
{
    Settings_->setValue(Keys::WindowGeometry, settings.WindowGeometry);
    Settings_->setValue(Keys::WindowState, settings.WindowState);
    Settings_->setValue(Keys::Splitter, settings.MainSplitterState);
    Settings_->setValue(Keys::Volume, qBound(0, settings.PreviewVolume, 100));
    Settings_->setValue(Keys::LastImport, settings.LastImportDirectory);
    Settings_->setValue(Keys::DestinationMode, static_cast<int>(settings.DestinationMode));
    Settings_->setValue(Keys::SourceSubdirectory, settings.SourceRelativeSubdirectory);
    Settings_->setValue(Keys::FixedDirectory, settings.LastFixedOutputDirectory);
    Settings_->setValue(Keys::Profile, OutputProfiles::Find(settings.SelectedOutputProfile) != nullptr
                                         ? settings.SelectedOutputProfile : QStringLiteral("fast-copy"));
    Settings_->setValue(Keys::Metadata, settings.PreserveMetadata);
    Settings_->setValue(Keys::Recursive, settings.RecursiveFolderImport);
    Settings_->setValue(Keys::Prefixes, settings.SavedPrefixes);
    Settings_->setValue(Keys::Template, NamingTemplate::Validate(settings.SelectedNamingTemplate).isEmpty()
                                          ? settings.SelectedNamingTemplate : NamingTemplate::DefaultPattern());
    Settings_->setValue(Keys::RecentSessions, CleanPaths(settings.RecentSessionFiles));
    Settings_->setValue(Keys::TimelineAutoPlay, settings.TimelineAutoPlay);
    Settings_->sync();
    if (Settings_->status() == QSettings::NoError) return true;
    if (error != nullptr) *error = QStringLiteral("Unable to write application settings.");
    return false;
}

bool SettingsRepository::Reset(QString* error)
{
    Settings_->clear();
    Settings_->sync();
    if (Settings_->status() == QSettings::NoError) return true;
    if (error != nullptr) *error = QStringLiteral("Unable to reset application settings.");
    return false;
}
} // namespace ClipCutter
