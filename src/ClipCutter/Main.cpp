#include "MainWindow.h"
#include "App/Settings/SettingsRepository.h"
#include "ClipCutterVersion.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <iterator>
#include <cstdio>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <Psapi.h>
#endif

namespace
{
void WriteStandardOutput(const QByteArray& data)
{
#ifdef Q_OS_WIN
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != nullptr && output != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(output, data.constData(), static_cast<DWORD>(data.size()), &written, nullptr);
    }
#else
    std::fwrite(data.constData(), 1, static_cast<std::size_t>(data.size()), stdout);
    std::fflush(stdout);
#endif
}

QString RuntimeProgramPath(const QString& baseName)
{
#ifdef Q_OS_WIN
    const QString fileName = baseName + QStringLiteral(".exe");
#else
    const QString fileName = baseName;
#endif
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(fileName);
    return QFileInfo::exists(bundled) ? QFileInfo(bundled).canonicalFilePath() : baseName;
}

QJsonArray LoadedModulePaths()
{
    QJsonArray result;
#ifdef Q_OS_WIN
    HMODULE modules[1024];
    DWORD requiredBytes = 0;
    if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &requiredBytes) == FALSE) return result;
    const DWORD count = qMin<DWORD>(requiredBytes / sizeof(HMODULE), static_cast<DWORD>(std::size(modules)));
    for (DWORD index = 0; index < count; ++index)
    {
        wchar_t path[32768];
        const DWORD length = GetModuleFileNameExW(GetCurrentProcess(), modules[index], path,
                                                   static_cast<DWORD>(std::size(path)));
        if (length > 0) result.append(QDir::cleanPath(QString::fromWCharArray(path, static_cast<qsizetype>(length))));
    }
#endif
    return result;
}

QJsonObject RuntimeDiagnostics()
{
    QJsonArray libraryPaths;
    for (const QString& path : QCoreApplication::libraryPaths()) libraryPaths.append(QDir::cleanPath(path));
    return {
        {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion()},
        {QStringLiteral("applicationFilePath"), QDir::cleanPath(QCoreApplication::applicationFilePath())},
        {QStringLiteral("applicationDirPath"), QDir::cleanPath(QCoreApplication::applicationDirPath())},
        {QStringLiteral("ffmpegPath"), RuntimeProgramPath(QStringLiteral("ffmpeg"))},
        {QStringLiteral("ffprobePath"), RuntimeProgramPath(QStringLiteral("ffprobe"))},
        {QStringLiteral("qtPrefixPath"), QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::PrefixPath))},
        {QStringLiteral("qtPluginsPath"), QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::PluginsPath))},
        {QStringLiteral("qtRuntimeVersion"), QString::fromLatin1(qVersion())},
        {QStringLiteral("libraryPaths"), libraryPaths},
        {QStringLiteral("loadedModules"), LoadedModulePaths()}
    };
}
} // namespace

int main(int argumentCount, char* arguments[])
{
    QApplication application(argumentCount, arguments);
    QApplication::setOrganizationName(ClipCutter::SettingsRepository::OrganisationName());
    QApplication::setApplicationName(ClipCutter::SettingsRepository::ApplicationName());
    QApplication::setApplicationDisplayName(QStringLiteral("ClipCutter"));
    QApplication::setApplicationVersion(QStringLiteral(CLIPCUTTER_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Batch video clip editor"));
    parser.addHelpOption();
    const QCommandLineOption versionOption({QStringLiteral("v"), QStringLiteral("version")},
                                            QStringLiteral("Print the application version, then exit."));
    parser.addOption(versionOption);
    const QCommandLineOption runtimeDiagnostics(
        QStringLiteral("runtime-diagnostics"),
        QStringLiteral("Print package runtime paths and loaded modules as JSON, then exit."));
    parser.addOption(runtimeDiagnostics);
    parser.process(application);
    if (parser.isSet(versionOption))
    {
        WriteStandardOutput(QStringLiteral("ClipCutter %1\n").arg(QCoreApplication::applicationVersion()).toUtf8());
        return 0;
    }
    if (parser.isSet(runtimeDiagnostics))
    {
        WriteStandardOutput(QJsonDocument(RuntimeDiagnostics()).toJson(QJsonDocument::Compact) + '\n');
        return 0;
    }

    ClipCutter::MainWindow window;
    window.show();

    return QApplication::exec();
}
