#pragma once

#include <QtCore/QString>

enum EReEncodeQuality
{
	QUALITY_VERY_HIGH,
	QUALITY_HIGH,
	QUALITY_MEDIUM,
	QUALITY_LOW
};

struct CQueueItem
{
	// Execution
	CQueueItem()
		: CQueueItem(0, 0)
	{
	}


	CQueueItem(const int64_t start, const int64_t end)
		: StartTimeMs(start),
		  EndTimeMs(end),
		  UseFullRename(true),
		  DeleteOriginal(false),
		  ReEncode(false),
		  ReEncodeQuality(QUALITY_HIGH),
		  ShouldProcess(false)
	{
	}


	[[nodiscard]] auto OutputName() const
	{
		return NameOrPostfix;
	}


	[[nodiscard]] auto OutputPostfix() const
	{
		return NameOrPostfix;
	}


	void SetOutputName(const QString& value)
	{
		NameOrPostfix = value;
	}


	void SetOutputPostfix(const QString& value)
	{
		NameOrPostfix = value;
	}


	// Variables
	int64_t StartTimeMs;
	int64_t EndTimeMs;
	bool UseFullRename;
	bool DeleteOriginal;
	QString NameOrPostfix;
	bool ReEncode;
	EReEncodeQuality ReEncodeQuality;

	bool ShouldProcess;
};
