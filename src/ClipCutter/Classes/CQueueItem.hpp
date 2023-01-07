#pragma once

#include <QtCore/QString>

class CQueueItem
{
	// Execution
public:
	CQueueItem()
		: CQueueItem(0, 0)
	{
	}


	CQueueItem(const int start, const int end)
		: m_StartTime(start), m_EndTime(end), m_UseFullRename(true), m_DeleteOriginal(false)
	{
	}


	~CQueueItem() = default;

	// Get/set
public:
	[[nodiscard]] auto StartTime() const
	{
		return m_StartTime;
	}


	[[nodiscard]] auto EndTime() const
	{
		return m_EndTime;
	}


	[[nodiscard]] auto UseFullRename() const
	{
		return m_UseFullRename;
	}


	[[nodiscard]] auto DeleteOriginal() const
	{
		return m_DeleteOriginal;
	}


	[[nodiscard]] auto OutputName() const
	{
		return m_NameOrPostfix;
	}


	[[nodiscard]] auto OutputPostfix() const
	{
		return m_NameOrPostfix;
	}


	void SetStartTime(const int startTime)
	{
		m_StartTime = startTime;
	}


	void SetEndTime(const int endTime)
	{
		m_EndTime = endTime;
	}


	void SetFullRenameMode(const bool value)
	{
		m_UseFullRename = value;
	}


	void SetDeleteOriginal(const bool value)
	{
		m_DeleteOriginal = value;
	}


	void SetOutputName(const QString& value)
	{
		m_NameOrPostfix = value;
	}


	void SetOutputPostfix(const QString& value)
	{
		m_NameOrPostfix = value;
	}


	// Protected functions
protected:
	// Variables
private:
	int m_StartTime;
	int m_EndTime;
	bool m_UseFullRename;
	bool m_DeleteOriginal;
	QString m_NameOrPostfix;
};
