#pragma once

#include <QtCore/QString>

class C_QueueItem
{
	// Execution
public:
	C_QueueItem() = default;

	C_QueueItem(const int start, const int end) :
		m_StartTime(start), m_EndTime(end), m_UseFullRename(true)
	{
	}

	C_QueueItem(const C_QueueItem& source) = default;
	C_QueueItem& operator=(const C_QueueItem& source) = default;
	C_QueueItem(C_QueueItem&&) = default;
	C_QueueItem& operator=(C_QueueItem&& source) = default;
	~C_QueueItem() = default;

	// Get/set
public:
	[[nodiscard]] auto StartTime() const { return m_StartTime; }
	[[nodiscard]] auto EndTime() const { return m_EndTime; }
	[[nodiscard]] auto UseFullRename() const { return m_UseFullRename; }
	[[nodiscard]] auto OutputName() const { return m_NameOrPostfix; }
	[[nodiscard]] auto OutputPostfix() const { return m_NameOrPostfix; }

	void SetStartTime(const int startTime) { m_StartTime = startTime; }
	void SetEndTime(const int endTime) { m_EndTime = endTime; }
	void SetFullRenameMode(const bool value) { m_UseFullRename = value; }
	void SetOutputName(const QString& value) { m_NameOrPostfix = value; }
	void SetOutputPostfix(const QString& value) { m_NameOrPostfix = value; }

	// Protected functions
protected:
	// Variables
private:
	int m_StartTime;
	int m_EndTime;
	bool m_UseFullRename;
	QString m_NameOrPostfix;
};
