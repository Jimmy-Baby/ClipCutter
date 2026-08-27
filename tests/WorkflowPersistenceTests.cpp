#include "App/Models/ClipQueueFilterModel.h"
#include "App/Models/ClipQueueModel.h"
#include "App/Settings/SettingsRepository.h"
#include "Core/Export/OutputDestination.h"
#include "Core/Naming/NamingTemplate.h"
#include "Core/Session/SessionRepository.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace std::chrono_literals;

namespace
{
QString CreateFile(const QDir& directory, const QString& name)
{
    QFile file(directory.absoluteFilePath(name));
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write("video");
    file.close();
    return file.fileName();
}

ClipCutter::Clip MakeClip(const QString& path, const QString& base = QStringLiteral("source"))
{
    ClipCutter::Clip clip;
    clip.SourcePath = path;
    clip.OriginalFileName = QFileInfo(path).fileName();
    clip.MediaInformation.Duration = 10s;
    clip.MediaInformation.ProbeStatus = ClipCutter::EProbeStatus::Ready;
    clip.MediaInformation.HasVideo = true;
    clip.MediaInformation.VideoStreamIndices = {0};
    ClipCutter::Segment segment;
    segment.Range = *ClipCutter::TimeRange::Create(1s, 8s);
    segment.TrimRangeUserEdited = true;
    segment.OutputBaseName = base;
    segment.OutputExtension = QStringLiteral(".mp4");
    segment.ExportProfileId = QStringLiteral("accurate-balanced");
    clip.Segments.append(segment);
    return clip;
}
}

class WorkflowPersistenceTests final : public QObject
{
    Q_OBJECT

private slots:
    void NamingTokensAndPadding();
    void NamingMalformedSanitisedAndDuplicates();
    void DestinationSameAndMixedDirectories();
    void SettingsDefaultsRoundTripAndMalformedRecovery();
    void SessionRoundTripRelativeMissingAndFutureVersion();
    void DirtyAndRecoveryRules();
    void ProxyFilteringAndBatchOperations();
};

void WorkflowPersistenceTests::NamingTokensAndPadding()
{
    ClipCutter::NamingTemplateContext context;
    context.Original = QStringLiteral("Match");
    context.Prefix = QStringLiteral("best_");
    context.Index = 7;
    context.Date = QDate(2026, 8, 28);
    context.Profile = QStringLiteral("fast-copy");
    context.Segment = 2;
    const auto result = ClipCutter::NamingTemplate::Render(
        QStringLiteral("{prefix}{original}_{index}_{index:03}_{date}_{profile}_{segment}"), context);
    QVERIFY2(result.IsValid(), qPrintable(result.Error));
    QCOMPARE(result.Value, QStringLiteral("best_Match_7_007_2026-08-28_fast-copy_2"));
    QCOMPARE(ClipCutter::NamingTemplate::Render(QStringLiteral("{index:02}"), context).Value, QStringLiteral("07"));
    QCOMPARE(ClipCutter::NamingTemplate::Render(QStringLiteral("{index:010}"), context).Value,
             QStringLiteral("0000000007"));
}

void WorkflowPersistenceTests::NamingMalformedSanitisedAndDuplicates()
{
    ClipCutter::NamingTemplateContext context;
    context.Original = QStringLiteral("clip");
    QVERIFY(!ClipCutter::NamingTemplate::Validate(QStringLiteral("{unknown}")).isEmpty());
    QVERIFY(!ClipCutter::NamingTemplate::Validate(QStringLiteral("{index:3}")).isEmpty());
    QVERIFY(!ClipCutter::NamingTemplate::Validate(QStringLiteral("{original")).isEmpty());
    QVERIFY(!ClipCutter::NamingTemplate::Render(QStringLiteral("bad:{original}"), context).IsValid());
    const auto duplicate = ClipCutter::NamingTemplate::RenderBatch(
        QStringLiteral("{original}"), QVector<ClipCutter::NamingTemplateContext>{context, context});
    QVERIFY(!duplicate.IsValid());
    QCOMPARE(duplicate.Duplicates, QStringList{QStringLiteral("clip")});
}

void WorkflowPersistenceTests::DestinationSameAndMixedDirectories()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QDir root(temporary.path());
    QVERIFY(root.mkdir(QStringLiteral("a")));
    QVERIFY(root.mkdir(QStringLiteral("b")));
    const QString first = CreateFile(QDir(root.absoluteFilePath(QStringLiteral("a"))), QStringLiteral("one.mp4"));
    const QString second = CreateFile(QDir(root.absoluteFilePath(QStringLiteral("b"))), QStringLiteral("two.mp4"));
    ClipCutter::OutputDestinationSettings settings;
    QCOMPARE(ClipCutter::OutputDestination::DirectoryForSource(first, settings),
             QDir(QFileInfo(first).dir()).absoluteFilePath(QStringLiteral("ClipCutterOutput")));
    QVERIFY2(ClipCutter::OutputDestination::PrepareDirectories(settings, {first, second}), "prepare mixed output");
    QVERIFY(QFileInfo::exists(ClipCutter::OutputDestination::DirectoryForSource(first, settings)));
    QVERIFY(QFileInfo::exists(ClipCutter::OutputDestination::DirectoryForSource(second, settings)));
    QVERIFY(ClipCutter::OutputDestination::DirectoryForSource(first, settings) !=
            ClipCutter::OutputDestination::DirectoryForSource(second, settings));
    settings.Mode = ClipCutter::EOutputDestinationMode::FixedDirectory;
    settings.FixedDirectory = root.absoluteFilePath(QStringLiteral("fixed"));
    QVERIFY(ClipCutter::OutputDestination::PrepareDirectories(settings, {first, second}));
    QCOMPARE(ClipCutter::OutputDestination::DirectoryForSource(first, settings),
             ClipCutter::OutputDestination::DirectoryForSource(second, settings));
}

void WorkflowPersistenceTests::SettingsDefaultsRoundTripAndMalformedRecovery()
{
    QTemporaryDir temporary;
    const QString path = QDir(temporary.path()).absoluteFilePath(QStringLiteral("settings.ini"));
    ClipCutter::SettingsRepository repository(path);
    const auto defaults = repository.Load();
    QCOMPARE(defaults.PreviewVolume, 100);
    QCOMPARE(defaults.DestinationMode, ClipCutter::EOutputDestinationMode::SourceRelative);
    QCOMPARE(defaults.SourceRelativeSubdirectory, QStringLiteral("ClipCutterOutput"));
    QVERIFY(defaults.LastImportDirectory.isEmpty());
    QVERIFY(defaults.LastFixedOutputDirectory.isEmpty());
    QVERIFY(!defaults.RecursiveFolderImport);
    ClipCutter::ApplicationSettings settings = defaults;
    settings.PreviewVolume = 41;
    settings.DestinationMode = ClipCutter::EOutputDestinationMode::FixedDirectory;
    settings.LastFixedOutputDirectory = temporary.path();
    settings.SavedPrefixes = {QStringLiteral("best_")};
    settings.SelectedNamingTemplate = QStringLiteral("{original}_{index:03}");
    QVERIFY(repository.Save(settings));
    const auto loaded = repository.Load();
    QCOMPARE(loaded.PreviewVolume, 41);
    QCOMPARE(loaded.SavedPrefixes, settings.SavedPrefixes);
    QCOMPARE(loaded.SelectedNamingTemplate, settings.SelectedNamingTemplate);

    QFile malformed(path);
    QVERIFY(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    malformed.write("[preview]\nvolume=not-a-number\n[output]\ndestinationMode=999\nprofile=missing\n[naming]\ntemplate={oops}\n");
    malformed.close();
    ClipCutter::SettingsRepository malformedRepository(path);
    const auto recovered = malformedRepository.Load();
    QCOMPARE(recovered.PreviewVolume, 100);
    QCOMPARE(recovered.DestinationMode, ClipCutter::EOutputDestinationMode::SourceRelative);
    QCOMPARE(recovered.SelectedOutputProfile, QStringLiteral("fast-copy"));
    QCOMPARE(recovered.SelectedNamingTemplate, ClipCutter::NamingTemplate::DefaultPattern());
}

void WorkflowPersistenceTests::SessionRoundTripRelativeMissingAndFutureVersion()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QDir root(temporary.path());
    QVERIFY(root.mkdir(QStringLiteral("media")));
    const QString source = CreateFile(QDir(root.absoluteFilePath(QStringLiteral("media"))), QStringLiteral("source.mp4"));
    const QString sessionPath = root.absoluteFilePath(QStringLiteral("work.clipcutter.json"));
    ClipCutter::SessionData session;
    session.Clips.push_back(MakeClip(source));
    session.Destination.FixedDirectory = root.absoluteFilePath(QStringLiteral("out"));
    session.SavedPrefixes = {QStringLiteral("best_")};
    session.SelectedNamingTemplate = QStringLiteral("{prefix}{original}_{index:03}");
    QVERIFY(ClipCutter::SessionRepository::Save(sessionPath, session));
    QFile jsonFile(sessionPath);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    const QByteArray json = jsonFile.readAll();
    QVERIFY(json.contains("media/source.mp4"));
    const auto loaded = ClipCutter::SessionRepository::Load(sessionPath);
    QVERIFY2(loaded.IsValid(), qPrintable(loaded.Error));
    QCOMPARE(loaded.Session.Clips.size(), std::size_t{1});
    QCOMPARE(loaded.Session.Clips.front().SourcePath, QDir::cleanPath(source));
    QCOMPARE(loaded.Session.Clips.front().Segments.front().Range.Start(), 1s);
    QCOMPARE(loaded.Session.Clips.front().Segments.front().Id, session.Clips.front().Segments.front().Id);
    QVERIFY(QFile::remove(source));
    const auto missing = ClipCutter::SessionRepository::Load(sessionPath);
    QVERIFY(missing.IsValid());
    QCOMPARE(missing.Session.MissingSourcePaths.size(), 1);
    QCOMPARE(missing.Session.Clips.size(), std::size_t{1});

    const QString futurePath = root.absoluteFilePath(QStringLiteral("future.json"));
    QFile future(futurePath);
    QVERIFY(future.open(QIODevice::WriteOnly));
    future.write(QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 999}}).toJson());
    future.close();
    const auto rejected = ClipCutter::SessionRepository::Load(futurePath);
    QVERIFY(!rejected.IsValid());
    QVERIFY(rejected.Error.contains(QStringLiteral("newer")));
}

void WorkflowPersistenceTests::DirtyAndRecoveryRules()
{
    ClipCutter::SessionDirtyState state;
    QVERIFY(!state.IsDirty());
    state.MarkModified();
    QVERIFY(state.IsDirty());
    state.MarkSaved(QStringLiteral("session.json"));
    QVERIFY(!state.IsDirty());
    QCOMPARE(state.FilePath(), QStringLiteral("session.json"));
    state.MarkModified();
    state.Reset();
    QVERIFY(!state.IsDirty());
    QVERIFY(state.FilePath().isEmpty());

    QTemporaryDir temporary;
    const QString recovery = QDir(temporary.path()).absoluteFilePath(QStringLiteral("recovery.json"));
    ClipCutter::SessionData session;
    QVERIFY(ClipCutter::SessionRepository::Save(recovery, session, nullptr, true));
    QVERIFY(ClipCutter::SessionRepository::ShouldOfferRecovery(recovery));
    const QString explicitPath = QDir(temporary.path()).absoluteFilePath(QStringLiteral("explicit.json"));
    QVERIFY(ClipCutter::SessionRepository::Save(explicitPath, session));
    session.ExplicitSessionPath = explicitPath;
    session.ExplicitSaveTimeUtc = QDateTime::currentDateTimeUtc().addDays(1);
    QVERIFY(ClipCutter::SessionRepository::Save(recovery, session, nullptr, true));
    QVERIFY(!ClipCutter::SessionRepository::ShouldOfferRecovery(recovery, explicitPath));
    QVERIFY(ClipCutter::SessionRepository::RemoveRecovery(recovery));
    QVERIFY(!ClipCutter::SessionRepository::ShouldOfferRecovery(recovery));
}

void WorkflowPersistenceTests::ProxyFilteringAndBatchOperations()
{
    ClipCutter::ClipQueueModel model;
    model.AddClip(MakeClip(QStringLiteral("C:/clips/Alpha.mp4"), QStringLiteral("alpha")));
    model.AddClip(MakeClip(QStringLiteral("C:/clips/Beta.mp4"), QStringLiteral("beta")));
    const QUuid first = model.SegmentIdAtRow(0);
    const QUuid second = model.SegmentIdAtRow(1);
    QVERIFY(model.ApplyPrefix(first, QStringLiteral("hero_")));
    model.UpdateSkipState(second, true);
    ClipCutter::ClipQueueFilterModel proxy;
    proxy.setSourceModel(&model);
    proxy.SetSearchText(QStringLiteral("hero"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(ClipCutter::ClipQueueModel::SegmentIdRole).toUuid(), first);
    proxy.SetSearchText(QStringLiteral("skip"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(ClipCutter::ClipQueueModel::SegmentIdRole).toUuid(), second);
    proxy.ClearFilters();
    QCOMPARE(proxy.rowCount(), 2);
    proxy.SetSkipped(true);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(ClipCutter::ClipQueueModel::SegmentIdRole).toUuid(), second);
    proxy.SetSkipped(std::nullopt);
    model.UpdateExportState(first, ClipCutter::EExportState::Failed);
    proxy.SetExportState(ClipCutter::EExportState::Failed);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data(ClipCutter::ClipQueueModel::SegmentIdRole).toUuid(), first);
    proxy.ClearFilters();

    QVERIFY(model.UpdateSkipStates({}, false));
    QVERIFY(!model.FindSegment(second)->Skipped);
    QVERIFY(model.InvertSkipStates({first}));
    QVERIFY(model.FindSegment(first)->Skipped);
    QVERIFY(model.ApplyPrefixTo({first, second}, QStringLiteral("batch_")));
    QVERIFY(model.ClearPrefixes({second}));
    QVERIFY(model.ApplyExportProfile({first}, QStringLiteral("compact")));
    QCOMPARE(model.FindSegment(first)->ExportProfileId, QStringLiteral("compact"));
    QVERIFY(model.ResetTrimRanges({first}));
    QCOMPARE(model.FindSegment(first)->Range, *ClipCutter::TimeRange::Create(0ms, 10s));
    QString error;
    QVERIFY(model.ApplyNamingTemplate({first, second}, QStringLiteral("{original}_{index:03}"), QDate(2026, 8, 28), &error));
    QCOMPARE(model.FindSegment(first)->OutputBaseName, QStringLiteral("Alpha_001"));
    QVERIFY(model.RemoveEntries({second}));
    QCOMPARE(model.rowCount(), 1);
    model.ClearClips();
    QCOMPARE(model.rowCount(), 0);
}

QTEST_APPLESS_MAIN(WorkflowPersistenceTests)

#include "WorkflowPersistenceTests.moc"
