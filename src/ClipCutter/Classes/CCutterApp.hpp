#pragma once

#include <filesystem>
#include <memory>

#include <QtCore/QDir>
#include <QtWidgets/QMainWindow>
#include <VLCQtCore/Instance.h>
#include <VLCQtCore/Media.h>
#include <VLCQtCore/MediaPlayer.h>
#include "CQueueItem.hpp"

#pragma comment(lib, "Qt5Core")
#pragma comment(lib, "Qt5Gui")
#pragma comment(lib, "Qt5Widgets")
#pragma comment(lib, "VLCQtCore")
#pragma comment(lib, "VLCQtWidgets")

class CGuiBase;

enum class EPlayerState
{
	STOPPED,
	PLAYING,
	PAUSED
};

class CCutterApp final : public QMainWindow
{
	// Execution
public:
	explicit CCutterApp(std::filesystem::path workingDirectory);
	~CCutterApp() override;

	void Show();
	void SetStartPoint();
	void SetEndPoint();
	void ProcessClips();
	void NextListItem();
	void SkipListItem();
	void EndOfList() const;
	void OpenedNewList() const;


	void SetMediaStopped()
	{
		SetPlayerState(EPlayerState::STOPPED);
	}


	void SetMediaPlaying()
	{
		SetPlayerState(EPlayerState::PLAYING);
	}


	void SetMediaPaused()
	{
		SetPlayerState(EPlayerState::PAUSED);
	}


	void SetPlayerState(const EPlayerState state)
	{
		m_PlayerState = state;
	}


	[[nodiscard]] EPlayerState GetPlayerState() const
	{
		return m_PlayerState;
	}


	// Get/set
public:
	[[nodiscard]] auto WorkingDirectory() const
	{
		return m_WorkingDirectory;
	}


	// Protected functions
protected:
	bool EventFilter(QObject* object, QEvent* event);
	void UpdateClipInfo() const;
	void UpdateDeleteOriginal();
	void UpdateReEncode();
	void UpdateReEncodeQuality();
	void UpdateNameLineEditRename();
	void UpdateFileName();
	void OpenSingleFile(const QString& fileString);
	void TryPlayPause();
	QStringList GetFileList(const QString& folderString, const QString& filterString);
	void FirstListItem();
	void Quit();
	void UpdateProgressBar(int clipsProcessed, int clipsTotal) const;
	QString ConstructFfMpegArguments(const char* inputPath,
	                                 const char* outputPath,
	                                 int64_t startTimeMs,
	                                 int64_t endTimeMs,
	                                 bool reEncode,
	                                 EReEncodeQuality reEncodeQuality) const;
	void OpenLocalFolder();

	// Variables
private:
	// Program's working directory
	std::filesystem::path m_WorkingDirectory;

	// Current video directory
	QDir m_VideoDirectory;

	// Current video list
	QStringList m_VideoList;

	// Current list item index
	int32_t m_CurrentListItem;

	// Current FFMpeg queue list
	std::vector<CQueueItem> m_FFMpegQueueList;

	CGuiBase* m_GuiBase;
	std::unique_ptr<VlcInstance> m_VlcInstance;
	std::unique_ptr<VlcMedia> m_VlcMedia;
	std::unique_ptr<VlcMediaPlayer> m_VlcPlayer;
	EPlayerState m_PlayerState;
};
