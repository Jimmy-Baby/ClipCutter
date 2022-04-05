#pragma once

#include <filesystem>

#include <QtCore/QDir>
#include <VLCQtCore/Instance.h>
#include <VLCQtCore/Media.h>
#include <VLCQtCore/MediaPlayer.h>
#include "C_GuiBase.hpp"
#include "C_QueueItem.hpp"

#pragma comment(lib, "Qt5Core")
#pragma comment(lib, "Qt5Gui")
#pragma comment(lib, "Qt5Widgets")
#pragma comment(lib, "VLCQtCore")
#pragma comment(lib, "VLCQtWidgets")

class C_GuiApp final : public QMainWindow
{
	// Execution
public:
	explicit C_GuiApp(std::filesystem::path workingDirectory);
	C_GuiApp(const C_GuiApp& source) = default;
	C_GuiApp& operator=(const C_GuiApp& source) = default;
	C_GuiApp(C_GuiApp&&) = default;
	C_GuiApp& operator=(C_GuiApp&& source) = default;
	~C_GuiApp() override = default;

	void Show();
	bool LoadSettings();
	bool CreateSettings();
	static void InputThread();

	// Get/set
public:
	[[nodiscard]] auto WorkingDirectory() const { return m_WorkingDirectory; }

	// Protected functions
protected:
	void UpdateTime();
	void OpenSingleFile(const QString& fileString);
	QStringList GetFileList(const QString& folderString, const QString& filterString);
	void FirstListItem();
	void NextListItem();
	void SetStartPoint();
	void SetEndPoint();
	QString ConstructFFMpegArguments(const char* inputPath, const char* outputPath, int startTime, int endTime) const;
	void ProcessClips();
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
	std::vector<C_QueueItem> m_FFMpegQueueList;

	std::unique_ptr<C_GuiBase> m_GuiBase;
	std::unique_ptr<VlcInstance> m_VlcInstance;
	std::unique_ptr<VlcMedia> m_VlcMedia;
	std::unique_ptr<VlcMediaPlayer> m_VlcPlayer;
};
