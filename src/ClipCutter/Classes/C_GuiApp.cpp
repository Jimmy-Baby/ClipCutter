#include <Windows.h>
#include <format>
#include <string>
#include <process.h>

#include <QtCore/QProcess>
#include <QtWidgets/QFileDialog>
#include <VLCQtCore/Common.h>
#include "C_GuiApp.hpp"
#include "C_GuiBase.hpp"

C_GuiApp::C_GuiApp(std::filesystem::path workingDirectory) :
	QMainWindow(nullptr), m_ExternalFlag(nullptr), m_WorkingDirectory(std::move(workingDirectory)),
	m_CurrentListItem(0), m_GuiBase(new C_GuiBase()), m_VlcMedia(nullptr)
{
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
	connect(m_GuiBase->Button_Pause, &QPushButton::clicked, m_GuiBase->actionPause, &QAction::toggle);
	connect(m_GuiBase->Button_Next, &QPushButton::clicked, this, &C_GuiApp::NextListItem);
	connect(m_GuiBase->Button_CutAll, &QPushButton::clicked, this, &C_GuiApp::ProcessClips);
	connect(m_GuiBase->Button_SetStart, &QPushButton::clicked, this, &C_GuiApp::SetStartPoint);
	connect(m_GuiBase->Button_SetEnd, &QPushButton::clicked, this, &C_GuiApp::SetEndPoint);
	connect(m_VlcPlayer.get(), &VlcMediaPlayer::timeChanged, this, &C_GuiApp::UpdateClipInfo);
	connect(m_GuiBase->Button_ToggleRenamePostfix, &QCheckBox::pressed, this, &C_GuiApp::UpdateNameLineEditRename);
	connect(m_GuiBase->LineEdit_RenameOrPostfix, &QLineEdit::textChanged, this, &C_GuiApp::UpdateFileName);
	connect(m_GuiBase->Action_Quit, &QAction::triggered, this, &C_GuiApp::Quit);

	/*if (LoadSettings() == false)
	{
		CreateSettings();
	}*/
}

C_GuiApp::~C_GuiApp()
{
	delete m_GuiBase;
}

void C_GuiApp::Show()
{
	this->show();
}

/*bool C_GuiApp::LoadSettings()
{
	// TOD: load settings file
	return false;
}

bool C_GuiApp::CreateSettings()
{
	// TOD: create settings file
	return LoadSettings();
}*/

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

void C_GuiApp::UpdateClipInfo() const
{
	const auto clipString =
		std::format(
			"[ Clip {} of {} ]: Start: {} - End: {}",
			m_CurrentListItem + 1,
			m_VideoList.size(),
			SecondsToHms(m_FFMpegQueueList[m_CurrentListItem].StartTime()),
			SecondsToHms(m_FFMpegQueueList[m_CurrentListItem].EndTime())
		);


	m_GuiBase->LineEdit_ClipInfo->setText(clipString.c_str());
}

void C_GuiApp::UpdateNameLineEditRename()
{
	if (m_CurrentListItem == 0 && m_FFMpegQueueList.empty())
		return;

	if (m_GuiBase->Button_ToggleRenamePostfix->text() == "Use File Postfix")
	{
		m_GuiBase->Button_ToggleRenamePostfix->setText("Use File Rename");
		m_GuiBase->LineEdit_RenameOrPostfix->setPlaceholderText("Postfix");
		m_FFMpegQueueList[m_CurrentListItem].SetFullRenameMode(false);
	}
	else if (m_GuiBase->Button_ToggleRenamePostfix->text() == "Use File Rename")
	{
		m_GuiBase->Button_ToggleRenamePostfix->setText("Use File Postfix");
		m_GuiBase->LineEdit_RenameOrPostfix->setPlaceholderText("Filename");
		m_FFMpegQueueList[m_CurrentListItem].SetFullRenameMode(true);
	}

	m_GuiBase->LineEdit_RenameOrPostfix->setText("");
}

void C_GuiApp::UpdateFileName()
{
	if (m_CurrentListItem == 0 && m_FFMpegQueueList.empty())
		return;

	if (auto& currentListItem = m_FFMpegQueueList[m_CurrentListItem]; currentListItem.UseFullRename() == true)
	{
		currentListItem.SetOutputName(m_GuiBase->LineEdit_RenameOrPostfix->text());
	}
	else
	{
		currentListItem.SetOutputPostfix(m_GuiBase->LineEdit_RenameOrPostfix->text());
	}
}

bool C_GuiApp::IsTyping() const
{
	// Redundant
	return !m_GuiBase->LineEdit_RenameOrPostfix->isHidden();
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

	if (m_GuiBase->Button_ToggleRenamePostfix->text() == "Use File Postfix")
	{
		m_FFMpegQueueList[m_CurrentListItem].SetOutputName("");
		m_FFMpegQueueList[m_CurrentListItem].SetFullRenameMode(true);
	}
	else
	{
		m_FFMpegQueueList[m_CurrentListItem].SetOutputName(m_GuiBase->LineEdit_RenameOrPostfix->text());
		m_FFMpegQueueList[m_CurrentListItem].SetFullRenameMode(false);
	}

	UpdateClipInfo();
}

void C_GuiApp::NextListItem()
{
	if (m_CurrentListItem + 1 > INT_MAX || !(m_CurrentListItem + 1 < m_VideoList.size()))
		return;

	OpenSingleFile(m_VideoDirectory.absoluteFilePath(m_VideoList.at(++m_CurrentListItem)));

	m_FFMpegQueueList.emplace_back(C_QueueItem(0, static_cast<int>(m_VlcMedia->duration())));

	if (m_GuiBase->Button_ToggleRenamePostfix->text() == "Use File Postfix")
	{
		m_FFMpegQueueList[m_CurrentListItem].SetOutputName("");
		m_FFMpegQueueList[m_CurrentListItem].SetFullRenameMode(true);
	}
	else
	{
		m_FFMpegQueueList[m_CurrentListItem].SetOutputName(m_GuiBase->LineEdit_RenameOrPostfix->text());
		m_FFMpegQueueList[m_CurrentListItem].SetFullRenameMode(false);
	}

	UpdateClipInfo();
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

void C_GuiApp::Quit()
{
	m_ExternalFlag->store(true, std::memory_order_release);
	this->close();
}

QString C_GuiApp::ConstructFfMpegArguments(const char* inputPath, const char* outputPath, const int startTime,
                                           const int endTime) const
{
	// ffmpeg arguments
	QString arguments;

	arguments += " -y";
	arguments += QDir::toNativeSeparators(std::format(" -i \"{}\"", inputPath).c_str());
	arguments += std::format(" -ss {}", startTime).c_str();
	arguments += std::format(" -to {}", endTime).c_str();
	arguments += QDir::toNativeSeparators(std::format(" -c copy \"{}\"", outputPath).c_str());

	return arguments;
}

int ExecuteFFmpeg(const QString& args)
{
	const QString arguments = (QString("ffmpeg") + args).toStdString().c_str();
	char* szComSpec = nullptr;

	_dupenv_s(&szComSpec, nullptr, "COMSPEC");

	if (szComSpec == nullptr)
		return -1;

	char* szCmd = max(strrchr(szComSpec, '\\'), strrchr(szComSpec, '/'));

	if (szCmd == nullptr)
		szCmd = szComSpec;
	else
		szCmd++;

	const size_t lenCmdLine = strlen(szCmd) + 4 + arguments.size() + 1;

	auto szCmdLine = static_cast<char*>(malloc(lenCmdLine));

	if (szCmdLine == nullptr)
		return -1;

	strcpy_s(szCmdLine, lenCmdLine, szCmd);
	szCmd = strrchr(szCmdLine, '.');

	if (szCmd)
		*szCmd = 0;

	strcat_s(szCmdLine, lenCmdLine, " /C ");
	strcat_s(szCmdLine, lenCmdLine, arguments.toStdString().c_str());

	PROCESS_INFORMATION processInformation;
	STARTUPINFOA startupInformation{};

	startupInformation.cb = sizeof startupInformation;
	startupInformation.lpReserved = nullptr;
	startupInformation.dwFlags = STARTF_USESHOWWINDOW;
	startupInformation.wShowWindow = SW_SHOWDEFAULT;
	startupInformation.lpReserved2 = nullptr;
	startupInformation.cbReserved2 = 0;

	const BOOL createProcessResult =
		CreateProcessA(
			szComSpec,
			szCmdLine,
			nullptr,
			nullptr,
			TRUE,
			CREATE_NEW_PROCESS_GROUP,
			nullptr,
			nullptr,
			&startupInformation,
			&processInformation
		);

	free(szCmdLine);

	if (createProcessResult == FALSE)
	{
		CloseHandle(processInformation.hThread);
		return -1;
	}

	CloseHandle(processInformation.hThread);

	int outResult;

	_cwait(&outResult, reinterpret_cast<intptr_t>(processInformation.hProcess), 0);

	CloseHandle(processInformation.hProcess);

	return outResult;
}

void C_GuiApp::ProcessClips()
{
	m_VlcPlayer->pause();
	m_GuiBase->Button_CutAll->setEnabled(false);
	m_GuiBase->Button_Next->setEnabled(false);
	m_GuiBase->Button_Pause->setEnabled(false);
	m_GuiBase->Button_SetEnd->setEnabled(false);
	m_GuiBase->Button_SetStart->setEnabled(false);
	m_GuiBase->Button_ToggleRenamePostfix->setEnabled(false);

	int32_t listCount = 0;
	for (const C_QueueItem& item : m_FFMpegQueueList)
	{
		QFileInfo fileInfo = m_VideoDirectory.absoluteFilePath(m_VideoList[listCount]);
		QString outDirectory = m_VideoDirectory.absolutePath() + "/ClipCutterOutput/";

		if (item.UseFullRename())
		{
			ExecuteFFmpeg(
				ConstructFfMpegArguments(
					fileInfo.absoluteFilePath().toStdString().c_str(),
					(outDirectory + item.OutputName() + ".mp4").toStdString().c_str(),
					item.StartTime(),
					item.EndTime())
			);
		}
		else
		{
			ExecuteFFmpeg(
				ConstructFfMpegArguments(
					fileInfo.absoluteFilePath().toStdString().c_str(),
					(outDirectory + fileInfo.baseName() + "_" + item.OutputPostfix() + ".mp4").toStdString().c_str(),
					item.StartTime(),
					item.EndTime())
			);
		}

		if (m_GuiBase->Checkbox_DelOrig->checkState() == Qt::CheckState::Checked)
			std::filesystem::remove(m_VideoDirectory.absoluteFilePath(m_VideoList[listCount]).toStdString());

		listCount++;
	}

	m_GuiBase->Button_CutAll->setEnabled(true);
	m_GuiBase->Button_Next->setEnabled(true);
	m_GuiBase->Button_Pause->setEnabled(true);
	m_GuiBase->Button_SetEnd->setEnabled(true);
	m_GuiBase->Button_SetStart->setEnabled(true);
	m_GuiBase->Button_ToggleRenamePostfix->setEnabled(true);
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

	m_FFMpegQueueList.clear();

	// Get all videos from folder with .mp4 extension
	m_VideoList = GetFileList(folderString, "*.mp4");

	// Create output folder
	const QDir outDirectory(folderString);

	if (outDirectory.exists("ClipCutterOutput") == false)
		outDirectory.mkdir("ClipCutterOutput");

	// Set current video as first video file
	FirstListItem();
}
