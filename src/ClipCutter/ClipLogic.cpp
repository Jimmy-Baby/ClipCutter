#include "ClipLogic.h"

namespace ClipLogic
{
    QString NormalizeKeyword(const QString& keyword)
    {
        return keyword.trimmed();
    }

    bool ContainsKeyword(const QStringList& keywords, const QString& keyword)
    {
        const QString normalizedKeyword = NormalizeKeyword(keyword);

        for (const QString& existingKeyword : keywords)
        {
            if (KeywordsEqual(NormalizeKeyword(existingKeyword), normalizedKeyword))
            {
                return true;
            }
        }

        return false;
    }

    bool KeywordsEqual(const QString& left, const QString& right)
    {
        return left.compare(right, Qt::CaseInsensitive) == 0;
    }

    QString GetOutputName(const QString& videoName, const QString& keyword)
    {
        return keyword.isEmpty() ? videoName : keyword + videoName;
    }
}
