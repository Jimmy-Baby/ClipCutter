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
	connect(m_GuiBase->Button_Stop, &QPushButton::clicked, m_VlcPlayer.get(), &VlcMediaPlayer::stop);

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
	OpenSingleFile(m_VideoList.at(0));
}

void C_GuiApp::NextListItem()
{
	if (m_CurrentListItem + 1 > INT_MAX)
	{
		// TODO: Log numerical limit error
		return;
	}

	OpenSingleFile(m_VideoList.at(++m_CurrentListItem));
}

void C_GuiApp::OpenLocalFolder()
{
	// Folder Selection
	/*const QString folderString =
		QFileDialog::getExistingDirectory
		(
			this,
			tr("Open Directory"),
			"/home",
			QFileDialog::ShowDirsOnly
			| QFileDialog::DontResolveSymlinks
		);*/

	const QString folderString =
		QFileDialog::getOpenFileName(
			this,
			tr("Open XML File 2"), 
			"/home", 
			tr("XML Files (*.xml)")
		);

	// Check if get directory failed
	if (folderString.isEmpty())
		return;

	// Get all videos from folder with .mp4 extension
	m_VideoList = GetFileList(folderString, "*.mp4");

	// Set current video as first video file
	FirstListItem();
}
