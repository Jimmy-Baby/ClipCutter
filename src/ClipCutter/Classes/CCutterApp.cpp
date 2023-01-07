#include <Windows.h>
#include <format>
#include <string>
#include <process.h>

#include <QtCore/QProcess>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QFileDialog>
#include <VLCQtCore/Common.h>
#include "CCutterApp.hpp"
#include "CGuiBase.hpp"

#define CLIPCUTTER_ERROR_STRING "ClipCutter Error"


CCutterApp::CCutterApp(std::filesystem::path workingDirectory)
	: QMainWindow(nullptr), m_WorkingDirectory(std::move(workingDirectory)),
	  m_CurrentListItem(0), m_GuiBase(new CGuiBase()), m_VlcMedia(nullptr),
	  m_PlayerState(EPlayerState::STOPPED)
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

	connect(m_GuiBase->Action_OpenFolder, &QAction::triggered, this, &CCutterApp::OpenLocalFolder);
	connect(m_GuiBase->Action_Pause, &QAction::toggled, this, &CCutterApp::TryPlayPause);
	connect(m_GuiBase->Action_Stop, &QAction::triggered, m_VlcPlayer.get(), &VlcMediaPlayer::stop);
	connect(m_GuiBase->Button_Pause, &QPushButton::clicked, m_GuiBase->Action_Pause, &QAction::toggle);
	connect(m_GuiBase->Button_Next, &QPushButton::clicked, this, &CCutterApp::NextListItem);
	connect(m_GuiBase->Button_Skip, &QPushButton::clicked, this, &CCutterApp::SkipListItem);
	connect(m_GuiBase->Button_CutAll, &QPushButton::clicked, this, &CCutterApp::ProcessClips);
	connect(m_GuiBase->Button_SetStart, &QPushButton::clicked, this, &CCutterApp::SetStartPoint);
	connect(m_GuiBase->Button_SetEnd, &QPushButton::clicked, this, &CCutterApp::SetEndPoint);
	connect(m_GuiBase->Checkbox_DelOrig, &QCheckBox::toggled, this, &CCutterApp::UpdateDeleteOriginal);
	connect(m_GuiBase->Button_ToggleRenamePostfix, &QCheckBox::pressed, this, &CCutterApp::UpdateNameLineEditRename);
	connect(m_GuiBase->LineEdit_RenameOrPostfix, &QLineEdit::textChanged, this, &CCutterApp::UpdateFileName);
	connect(m_GuiBase->Action_Quit, &QAction::triggered, this, &CCutterApp::Quit);

	connect(m_VlcPlayer.get(), &VlcMediaPlayer::timeChanged, this, &CCutterApp::UpdateClipInfo);
	connect(m_VlcPlayer.get(), &VlcMediaPlayer::end, this, &CCutterApp::SetMediaStopped);
	connect(m_VlcPlayer.get(), &VlcMediaPlayer::stopped, this, &CCutterApp::SetMediaStopped);
	connect(m_VlcPlayer.get(), &VlcMediaPlayer::paused, this, &CCutterApp::SetMediaPaused);
	connect(m_VlcPlayer.get(), &VlcMediaPlayer::playing, this, &CCutterApp::SetMediaPlaying);

	// Set step bounds for progress bar
	m_GuiBase->ProgressBar_StatusBar->setMinimum(0);
	m_GuiBase->ProgressBar_StatusBar->setMaximum(100);
}


CCutterApp::~CCutterApp()
{
	delete m_GuiBase;
}


void CCutterApp::Show()
{
	this->show();
}


std::string SecondsToHms(const int totalSeconds)
{
	if (totalSeconds == -1)
	{
		return "00:00:00";
	}

	const int seconds = totalSeconds % 60;
	const int minutes = totalSeconds / 60 % 60;
	const int hours = totalSeconds / 60 / 60;

	const auto formatString = "{:02}:{:02}:{:02}";

	return std::format(formatString, hours, minutes, seconds);
}


bool CCutterApp::EventFilter(QObject*, QEvent* event)
{
	if (event->type() == QEvent::KeyPress)
	{
		const auto keyEvent = reinterpret_cast<QKeyEvent*>(event);

		switch (keyEvent->key())
		{
		case Qt::Key_Q:
			SetStartPoint();
			break;

		case Qt::Key_W:
			SetEndPoint();
			break;

		default:
			break;
		}
	}

	return false;
}


void CCutterApp::UpdateClipInfo() const
{
	const auto clipString = std::format(
		"[ Clip {} of {} ]: Start: {} - End: {}",
		m_CurrentListItem + 1,
		m_VideoList.size(),
		SecondsToHms(m_FFMpegQueueList[m_CurrentListItem].StartTime()),
		SecondsToHms(m_FFMpegQueueList[m_CurrentListItem].EndTime())
	);


	m_GuiBase->LineEdit_ClipInfo->setText(clipString.c_str());
}


void CCutterApp::UpdateDeleteOriginal()
{
	if (m_GuiBase->Checkbox_DelOrig->checkState() == Qt::CheckState::Checked)
	{
		m_FFMpegQueueList[m_CurrentListItem].SetDeleteOriginal(true);
	}
	else
	{
		m_FFMpegQueueList[m_CurrentListItem].SetDeleteOriginal(false);
	}
}


void CCutterApp::UpdateNameLineEditRename()
{
	if (m_CurrentListItem == 0 && m_FFMpegQueueList.empty())
	{
		return;
	}

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


void CCutterApp::UpdateFileName()
{
	if (m_CurrentListItem == 0 && m_FFMpegQueueList.empty())
	{
		return;
	}

	if (auto& currentListItem = m_FFMpegQueueList[m_CurrentListItem]; currentListItem.UseFullRename() == true)
	{
		currentListItem.SetOutputName(m_GuiBase->LineEdit_RenameOrPostfix->text());
	}
	else
	{
		currentListItem.SetOutputPostfix(m_GuiBase->LineEdit_RenameOrPostfix->text());
	}
}


void CCutterApp::OpenSingleFile(const QString& fileString)
{
	m_VlcMedia.reset(new VlcMedia(fileString, true, m_VlcInstance.get()));
	m_VlcPlayer->open(m_VlcMedia.get());
}


void CCutterApp::TryPlayPause()
{
	const auto player = m_VlcPlayer.get();

	switch (GetPlayerState())
	{
	case EPlayerState::STOPPED:
		{
			// Reload current video into media object
			m_VlcMedia.reset(new VlcMedia(m_VideoDirectory.absoluteFilePath(m_VideoList.at(m_CurrentListItem)), true, m_VlcInstance.get()));

			// Reload our media object into player
			player->open(m_VlcMedia.get());

			break;
		}

	case EPlayerState::PAUSED:
		{
			player->play();
			break;
		}

	case EPlayerState::PLAYING:
		{
			player->pause();
			break;
		}
	}
}


QStringList CCutterApp::GetFileList(const QString& folderString, const QString& filterString)
{
	const QStringList filter(filterString);
	m_VideoDirectory = QDir(folderString);

	return m_VideoDirectory.entryList(filter, QDir::Files);
}


void CCutterApp::FirstListItem()
{
	m_CurrentListItem = 0;
	OpenSingleFile(m_VideoDirectory.absoluteFilePath(m_VideoList.at(0)));

	m_FFMpegQueueList.emplace_back(CQueueItem());

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


void CCutterApp::NextListItem()
{
	if (m_CurrentListItem + 1 >= m_VideoList.size())
	{
		EndOfList();
		return;
	}

	// Very rudimentary way of telling if we are in full file naming mode
	if (m_GuiBase->Button_ToggleRenamePostfix->text() == "Use File Postfix")
	{
		// Check if file name is blank
		if (m_GuiBase->LineEdit_RenameOrPostfix->text() == "")
		{
			MessageBoxA(nullptr, "Invalid File Name", CLIPCUTTER_ERROR_STRING, MB_ICONWARNING);
			return;
		}
	}

	// Check that end point is after start point, otherwise won't process properly
	if (!(m_FFMpegQueueList[m_CurrentListItem].StartTime() < m_FFMpegQueueList[m_CurrentListItem].EndTime()))
	{
		MessageBoxA(nullptr, "Invalid start/end points", CLIPCUTTER_ERROR_STRING, MB_ICONWARNING);
		return;
	}

	OpenSingleFile(m_VideoDirectory.absoluteFilePath(m_VideoList.at(++m_CurrentListItem)));

	m_FFMpegQueueList.emplace_back(CQueueItem(0, static_cast<int>(m_VlcMedia->duration())));

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

	// Set the 'delete original' checkbox to false for each video by default
	m_GuiBase->Checkbox_DelOrig->setCheckState(Qt::CheckState::Unchecked);
}


void CCutterApp::SkipListItem()
{
	// Set start and end time to 0 so that the file is not processed by ffmpeg
	m_FFMpegQueueList[m_CurrentListItem].SetStartTime(0);
	m_FFMpegQueueList[m_CurrentListItem].SetEndTime(0);

	if (m_CurrentListItem + 1 >= m_VideoList.size())
	{
		EndOfList();
		return;
	}

	OpenSingleFile(m_VideoDirectory.absoluteFilePath(m_VideoList.at(++m_CurrentListItem)));

	m_FFMpegQueueList.emplace_back(CQueueItem(0, static_cast<int>(m_VlcMedia->duration())));

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

	// Set the 'delete original' checkbox to false for each video by default
	m_GuiBase->Checkbox_DelOrig->setCheckState(Qt::CheckState::Unchecked);
}


void CCutterApp::EndOfList() const
{
	m_VlcPlayer->stop();

	m_GuiBase->Button_Next->setEnabled(false);
	m_GuiBase->Button_Skip->setEnabled(false);
	m_GuiBase->Button_Pause->setEnabled(false);
	m_GuiBase->Button_SetEnd->setEnabled(false);
	m_GuiBase->Button_SetStart->setEnabled(false);
	m_GuiBase->Button_ToggleRenamePostfix->setEnabled(false);
	m_GuiBase->Label_StatusBar->setText("End of List");
	QApplication::processEvents();
}


void CCutterApp::OpenedNewList() const
{
	m_GuiBase->Button_Next->setEnabled(true);
	m_GuiBase->Button_Skip->setEnabled(true);
	m_GuiBase->Button_Pause->setEnabled(true);
	m_GuiBase->Button_SetEnd->setEnabled(true);
	m_GuiBase->Button_SetStart->setEnabled(true);
	m_GuiBase->Button_ToggleRenamePostfix->setEnabled(true);
	m_GuiBase->Label_StatusBar->setText("Ready");
	QApplication::processEvents();
}


void CCutterApp::SetStartPoint()
{
	const auto videoTimeMs = m_VlcPlayer->time();
	const auto videoTimeSeconds = videoTimeMs / 1000;

	if (videoTimeMs == -1)
	{
		return; // No media loaded into player, return.
	}

	if (videoTimeSeconds >= m_FFMpegQueueList[m_CurrentListItem].EndTime())
	{
		MessageBoxA(nullptr, "Start point must be before end point", CLIPCUTTER_ERROR_STRING, MB_ICONWARNING);
		return;
	}

	if (m_CurrentListItem >= static_cast<int32_t>(m_FFMpegQueueList.size()))
	{
		m_FFMpegQueueList.emplace_back(CQueueItem());
	}

	m_FFMpegQueueList[m_CurrentListItem].SetStartTime(videoTimeSeconds);

	UpdateClipInfo();
}


void CCutterApp::SetEndPoint()
{
	const auto videoTimeMs = m_VlcPlayer->time();
	const auto videoTimeSeconds = videoTimeMs / 1000;

	if (videoTimeMs == -1)
	{
		return; // No media loaded into player, return.
	}

	if (videoTimeSeconds <= m_FFMpegQueueList[m_CurrentListItem].StartTime())
	{
		MessageBoxA(nullptr, "End point must be after start point", CLIPCUTTER_ERROR_STRING, MB_ICONWARNING);
		return;
	}

	if (m_CurrentListItem >= static_cast<int32_t>(m_FFMpegQueueList.size()))
	{
		m_FFMpegQueueList.emplace_back(CQueueItem());
	}

	m_FFMpegQueueList[m_CurrentListItem].SetEndTime(videoTimeSeconds);

	UpdateClipInfo();
}


void CCutterApp::Quit()
{
	this->close();
}


void CCutterApp::UpdateProgressBar(const int clipsProcessed, const int clipsTotal) const
{
	const int numberOfSteps = clipsProcessed / clipsTotal * 100;
	m_GuiBase->ProgressBar_StatusBar->setValue(numberOfSteps);
}


QString CCutterApp::ConstructFfMpegArguments(const char* inputPath, const char* outputPath, const int startTime, const int endTime) const
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
	{
		return -1;
	}

	char* szCmd = max(strrchr(szComSpec, '\\'), strrchr(szComSpec, '/'));

	if (szCmd == nullptr)
	{
		szCmd = szComSpec;
	}
	else
	{
		szCmd++;
	}

	const size_t lenCmdLine = strlen(szCmd) + 4 + arguments.size() + 1;

	const auto szCmdLine = static_cast<char*>(malloc(lenCmdLine));

	if (szCmdLine == nullptr)
	{
		return -1;
	}

	strcpy_s(szCmdLine, lenCmdLine, szCmd);
	szCmd = strrchr(szCmdLine, '.');

	if (szCmd)
	{
		*szCmd = 0;
	}

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


void CCutterApp::ProcessClips()
{
	m_VlcPlayer->pause();
	m_GuiBase->Button_CutAll->setEnabled(false);
	m_GuiBase->Button_Next->setEnabled(false);
	m_GuiBase->Button_Pause->setEnabled(false);
	m_GuiBase->Button_SetEnd->setEnabled(false);
	m_GuiBase->Button_SetStart->setEnabled(false);
	m_GuiBase->Button_ToggleRenamePostfix->setEnabled(false);
	m_GuiBase->Label_StatusBar->setText("Processing..");
	QApplication::processEvents();

	int32_t listCount = 0;

	for (const CQueueItem& item : m_FFMpegQueueList)
	{
		QFileInfo fileInfo = m_VideoDirectory.absoluteFilePath(m_VideoList[listCount]);
		QString outDirectory = m_VideoDirectory.absolutePath() + "/ClipCutterOutput/";

		if (item.UseFullRename())
		{
			ExecuteFFmpeg(ConstructFfMpegArguments(fileInfo.absoluteFilePath().toStdString().c_str(),
			                                       (outDirectory + item.OutputName() + ".mp4").toStdString().c_str(),
			                                       item.StartTime(),
			                                       item.EndTime()));
		}
		else
		{
			ExecuteFFmpeg(ConstructFfMpegArguments(fileInfo.absoluteFilePath().toStdString().c_str(),
			                                       (outDirectory + fileInfo.baseName() + "_" + item.OutputPostfix() + ".mp4").toStdString().c_str(),
			                                       item.StartTime(),
			                                       item.EndTime()));
		}

		if (item.DeleteOriginal())
		{
			std::filesystem::remove(m_VideoDirectory.absoluteFilePath(m_VideoList[listCount]).toStdString());
		}

		listCount++;
	}

	m_GuiBase->Button_CutAll->setEnabled(true);
	m_GuiBase->Button_Next->setEnabled(true);
	m_GuiBase->Button_Pause->setEnabled(true);
	m_GuiBase->Button_SetEnd->setEnabled(true);
	m_GuiBase->Button_SetStart->setEnabled(true);
	m_GuiBase->Button_ToggleRenamePostfix->setEnabled(true);
	m_GuiBase->Label_StatusBar->setText("Ready");
	QApplication::processEvents();
}


void CCutterApp::OpenLocalFolder()
{
	// Folder Selection
	const QString folderString = QFileDialog::getExistingDirectory(this,
	                                                               tr("Open Directory"),
	                                                               "/home",
	                                                               QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	// Check if get directory failed
	if (folderString.isEmpty())
	{
		return;
	}

	m_FFMpegQueueList.clear();

	// Get all videos from folder with .mp4 extension
	m_VideoList = GetFileList(folderString, "*.mp4");

	// Create output folder
	const QDir outDirectory(folderString);

	if (outDirectory.exists("ClipCutterOutput") == false)
	{
		[[maybe_unused]] bool directoryMade = outDirectory.mkdir("ClipCutterOutput");
	}

	// Set current video as first video file
	FirstListItem();
}
