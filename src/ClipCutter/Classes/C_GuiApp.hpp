#pragma once

#include <filesystem>
#include <memory>

#include <QtCore/QDir>
#include <QtWidgets/QMainWindow>
#include <VLCQtCore/Instance.h>
#include <VLCQtCore/Media.h>
#include <VLCQtCore/MediaPlayer.h>
#include "C_QueueItem.hpp"

#pragma comment(lib, "Qt5Core")
#pragma comment(lib, "Qt5Gui")
#pragma comment(lib, "Qt5Widgets")
#pragma comment(lib, "VLCQtCore")
#pragma comment(lib, "VLCQtWidgets")

class C_GuiBase;

class C_GuiApp final : public QMainWindow
{
	// Execution
public:
	explicit C_GuiApp(std::filesystem::path workingDirectory);
	~C_GuiApp() override;

	void Show();
	//bool LoadSettings();
	//bool CreateSettings();

	void SetThreadShutdownFlag(std::atomic_bool& flag)
	{
		m_ExternalFlag = &flag;
	}

	void SetStartPoint();
	void SetEndPoint();
	void ProcessClips();
	void NextListItem();
	bool IsTyping() const;

	// Get/set
public:
	[[nodiscard]] auto WorkingDirectory() const { return m_WorkingDirectory; }

	// Protected functions
protected:
	bool EventFilter(QObject* Object, QEvent* event);
	void UpdateClipInfo() const;
	void UpdateNameLineEditRename();
	void UpdateFileName();
	void OpenSingleFile(const QString& fileString);
	QStringList GetFileList(const QString& folderString, const QString& filterString);
	void FirstListItem();
	void Quit();
	QString ConstructFfMpegArguments(const char* inputPath, const char* outputPath, int startTime,
	                                 int endTime) const;
	void OpenLocalFolder();

	// Variables
private:
	// External thread flag
	std::atomic_bool* m_ExternalFlag;

	// Program's working directory
	std::filesystem::path m_WorkingDirectory;

	// Current video directory
	QDir m_VideoDirectory;

	// Current video list
	QStringList m_VideoList;

	// Current list item index
	int32_t m_CurrentListItem;

	// Current FFMpeg queue list
	std::vector<C_QueueItem> m_FFMpegQueueList;

	C_GuiBase* m_GuiBase;
	std::unique_ptr<VlcInstance> m_VlcInstance;
	std::unique_ptr<VlcMedia> m_VlcMedia;
	std::unique_ptr<VlcMediaPlayer> m_VlcPlayer;
};
