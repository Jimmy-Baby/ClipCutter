#pragma once

class C_QueueItem
{
	// Execution
public:
	C_QueueItem() = default;

	C_QueueItem(const int start, const int end) :
		m_StartTime(start), m_EndTime(end)
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

	void SetStartTime(const int startTime) { m_StartTime = startTime; }
	void SetEndTime(const int endTime) { m_EndTime = endTime; }

	// Protected functions
protected:
	// Variables
private:
	int m_StartTime;
	int m_EndTime;
};
