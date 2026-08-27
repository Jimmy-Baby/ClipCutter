#ifndef CLIPCUTTER_CLIPLOGIC_H
#define CLIPCUTTER_CLIPLOGIC_H

#include <QString>
#include <QStringList>

namespace ClipCutter::ClipLogic
{
QString NormalizeKeyword(const QString& keyword);
bool ContainsKeyword(const QStringList& keywords, const QString& keyword);
bool KeywordsEqual(const QString& left, const QString& right);
QString GetOutputName(const QString& videoName, const QString& keyword);
} // namespace ClipCutter::ClipLogic

#endif
