#ifndef CLIPLOGIC_H
#define CLIPLOGIC_H

#include <QString>
#include <QStringList>

namespace ClipLogic
{
    QString NormalizeKeyword(const QString& keyword);
    bool ContainsKeyword(const QStringList& keywords, const QString& keyword);
    bool KeywordsEqual(const QString& left, const QString& right);
    QString GetOutputName(const QString& videoName, const QString& keyword);
}

#endif // CLIPLOGIC_H
