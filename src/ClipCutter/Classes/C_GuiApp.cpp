#include <format>
#include <string>

#include <QtWidgets/QFileDialog>
#include <VLCQtCore/Common.h>
#include "C_GuiApp.hpp"

C_GuiApp::C_GuiApp(std::filesystem::path workingDirectory) :
	QMainWindow(nullptr), m_WorkingDirectory(std::move(workingDirectory)), m_VlcMedia(nullptr)
{
	m_GuiBase = std::make_unique<C_GuiBase>();
	m_GuiBase->SetupUserInterface(this);

	m_VlcInstance = std::make_unique<VlcInstance>(VlcCommon::args(), this);
	m_VlcPlayer = std::make_unique<VlcMediaPlayer>(m_VlcInstance.get());
	m_VlcPlayer->setVideoWidget(m_GuiBase->VideoFrame);

	m_GuiBase->VideoFrame->setMediaPlayer(m_VlcPlayer.get());
	m_GuiBase->VolumeSlider->setMediaPlayer(m_VlcPlayer.get());
	m_GuiBase->VolumeSlider->setVolume(125);
	m_GuiBase->VolumeSlider->hide();
	m_GuiBase->VideoSeek->setMediaPlayer(m_VlcPlayer.get());

	connect(m_GuiBase->Action_OpenFolder, &QAction::triggered, this, &C_GuiApp::OpenLocalFolder);
	connect(m_GuiBase->actionPause, &QAction::toggled, m_VlcPlayer.get(), &VlcMediaPlayer::togglePause);
	connect(m_GuiBase->actionStop, &QAction::triggered, m_VlcPlayer.get(), &VlcMediaPlayer::stop);
	connect(m_GuiBase->Button_Pause, &QPushButton::toggled, m_GuiBase->actionPause, &QAction::toggle);
	connect(m_GuiBase->Button_Next, &QPushButton::clicked, this, &C_GuiApp::NextListItem);
	connect(m_VlcPlayer.get(), &VlcMediaPlayer::timeChanged, this, &C_GuiApp::UpdateTime);

	if (LoadSettings() == false)
	{
		CreateSettings();
	}
}


void C_GuiApp::Show()
{
	this->show();
}

bool C_GuiApp::LoadSettings()
{
	// TODO: load settings file
	return false;
}

bool C_GuiApp::CreateSettings()
{
	// TODO: create settings file
	return LoadSettings();
}

std::string SecondsToHms(const int totalSeconds)
{
	if (totalSeconds == -1)
		return "00:00:00";

	const int seconds = totalSeconds % 60;
	const int minutes = totalSeconds / 60 % 60;
	const int hours = totalSeconds / 60 / 60;

	const auto formatString = "{:02}:{:02}:{:02}";

	return std::format(formatString, hours, minutes, seconds);
}

void C_GuiApp::UpdateTime()
{
	const auto formatString = "[ Start: {} - End: {} ]";

	m_GuiBase->Label_Times->setText(
		std::format(
			formatString,
			SecondsToHms(m_FFMpegQueueList[m_CurrentListItem].StartTime()),
			SecondsToHms(m_FFMpegQueueList[m_CurrentListItem].EndTime())
		).c_str()
	);
}

void C_GuiApp::OpenSingleFile(const QString& fileString)
{
	m_VlcMedia.reset(new VlcMedia(fileString, true, m_VlcInstance.get()));
	m_VlcPlayer->open(m_VlcMedia.get());
}

QStringList C_GuiApp::GetFileList(const QString& folderString, const QString& filterString)
{
	const QStringList filter(filterString);
	m_VideoDirectory = QDir(folderString);

	return m_VideoDirectory.entryList(filter, QDir::Files);
}

void C_GuiApp::FirstListItem()
{
	m_CurrentListItem = 0;
	OpenSingleFile(m_VideoDirectory.absoluteFilePath(m_VideoList.at(0)));

	m_FFMpegQueueList.emplace_back(C_QueueItem());
}

void C_GuiApp::NextListItem()
{
	if (++m_CurrentListItem > INT_MAX || !(m_CurrentListItem < m_VideoList.size()))
	{
		// TODO: Log numerical limit error
		return;
	}

	OpenSingleFile(m_VideoDirectory.absoluteFilePath(m_VideoList.at(m_CurrentListItem)));

	m_FFMpegQueueList.emplace_back(C_QueueItem(0, static_cast<int>(m_VlcMedia->duration())));
}

void C_GuiApp::SetStartPoint()
{
	const auto videoTimeMs = m_VlcPlayer->time();
	const auto videoTimeSeconds = videoTimeMs / 1000;

	if (videoTimeMs == -1)
		return; // No media loaded into player, return.

	if (m_CurrentListItem >= static_cast<int32_t>(m_FFMpegQueueList.size()))
		m_FFMpegQueueList.emplace_back(C_QueueItem());

	m_FFMpegQueueList[m_CurrentListItem].SetStartTime(videoTimeSeconds);
}

void C_GuiApp::SetEndPoint()
{
	const auto videoTimeMs = m_VlcPlayer->time();
	const auto videoTimeSeconds = videoTimeMs / 1000;

	if (videoTimeMs == -1)
		return; // No media loaded into player, return.

	if (m_CurrentListItem >= static_cast<int32_t>(m_FFMpegQueueList.size()))
		m_FFMpegQueueList.emplace_back(C_QueueItem());

	m_FFMpegQueueList[m_CurrentListItem].SetEndTime(videoTimeSeconds);
}

QString C_GuiApp::ConstructFFMpegArguments(const char* inputPath, const char* outputPath, const int startTime,
                                           const int endTime) const
{
	const auto baseArguments = "ffmpeg -i %s -ss %d -to %d -c copy %s";
	char outString[1024]{};

	sprintf_s(outString, baseArguments, inputPath, startTime, endTime, outputPath);

	return {outString};
}

void C_GuiApp::ProcessClips()
{
}

void C_GuiApp::OpenLocalFolder()
{
	// Folder Selection
	const QString folderString =
		QFileDialog::getExistingDirectory
		(
			this,
			tr("Open Directory"),
			"/home",
			QFileDialog::ShowDirsOnly
			| QFileDialog::DontResolveSymlinks
		);

	// Check if get directory failed
	if (folderString.isEmpty())
		return;

	// Get all videos from folder with .mp4 extension
	m_VideoList = GetFileList(folderString, "*.mp4");

	// Set current video as first video file
	FirstListItem();
}

void C_GuiApp::InputThread()
{
}
