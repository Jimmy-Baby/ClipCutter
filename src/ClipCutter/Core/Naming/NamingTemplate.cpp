#include "Core/Naming/NamingTemplate.h"

#include "Core/Export/OutputPathPlanner.h"

#include <QRegularExpression>
#include <QSet>

namespace ClipCutter
{
namespace
{
const QRegularExpression& TokenExpression()
{
    static const QRegularExpression expression(QStringLiteral(R"(\{([^{}]*)\})"));
    return expression;
}

QString RenderToken(const QString& token, const NamingTemplateContext& context, QString& error)
{
    if (token == QStringLiteral("original")) return context.Original;
    if (token == QStringLiteral("prefix")) return context.Prefix;
    if (token == QStringLiteral("date")) return context.Date.toString(QStringLiteral("yyyy-MM-dd"));
    if (token == QStringLiteral("profile")) return context.Profile;
    if (token == QStringLiteral("segment")) return QString::number(context.Segment);
    if (token == QStringLiteral("index")) return QString::number(context.Index);

    static const QRegularExpression paddedIndex(QStringLiteral(R"(^index:(0[1-9][0-9]*)$)"));
    const QRegularExpressionMatch match = paddedIndex.match(token);
    if (match.hasMatch())
    {
        bool ok = false;
        const int width = match.captured(1).toInt(&ok);
        if (!ok || width < 1 || width > 64)
        {
            error = QStringLiteral("Invalid index padding token {%1}; width must be between 1 and 64.").arg(token);
            return {};
        }
        return QStringLiteral("%1").arg(context.Index, width, 10, QLatin1Char('0'));
    }

    error = QStringLiteral("Unknown or malformed naming token {%1}.").arg(token);
    return {};
}
} // namespace

QString NamingTemplate::DefaultPattern()
{
    return QStringLiteral("{prefix}{original}");
}

QString NamingTemplate::Validate(const QString& pattern)
{
    if (pattern.isEmpty()) return QStringLiteral("Naming template cannot be empty.");

    int cursor = 0;
    auto iterator = TokenExpression().globalMatch(pattern);
    while (iterator.hasNext())
    {
        const QRegularExpressionMatch match = iterator.next();
        const QString between = pattern.mid(cursor, match.capturedStart() - cursor);
        if (between.contains(QLatin1Char('{')) || between.contains(QLatin1Char('}')))
            return QStringLiteral("Naming template contains an unmatched brace near position %1.").arg(cursor + 1);
        QString error;
        NamingTemplateContext context;
        RenderToken(match.captured(1), context, error);
        if (!error.isEmpty()) return error;
        cursor = match.capturedEnd();
    }
    const QString remainder = pattern.mid(cursor);
    if (remainder.contains(QLatin1Char('{')) || remainder.contains(QLatin1Char('}')))
        return QStringLiteral("Naming template contains an unmatched brace near position %1.").arg(cursor + 1);

    return {};
}

NamingTemplateResult NamingTemplate::Render(const QString& pattern, const NamingTemplateContext& context)
{
    NamingTemplateResult result;
    result.Error = Validate(pattern);
    if (!result.Error.isEmpty()) return result;

    int cursor = 0;
    auto iterator = TokenExpression().globalMatch(pattern);
    while (iterator.hasNext())
    {
        const QRegularExpressionMatch match = iterator.next();
        result.Value += pattern.mid(cursor, match.capturedStart() - cursor);
        result.Value += RenderToken(match.captured(1), context, result.Error);
        if (!result.Error.isEmpty()) return result;
        cursor = match.capturedEnd();
    }
    result.Value += pattern.mid(cursor);
    result.Error = OutputPathPlanner::ValidateBaseName(result.Value);
    if (!result.Error.isEmpty()) result.Error = QStringLiteral("Rendered output name is invalid: %1").arg(result.Error);
    return result;
}

NamingBatchResult NamingTemplate::RenderBatch(const QString& pattern,
                                               const QVector<NamingTemplateContext>& contexts)
{
    NamingBatchResult result;
    QSet<QString> seen;
    for (int index = 0; index < contexts.size(); ++index)
    {
        const NamingTemplateResult rendered = Render(pattern, contexts.at(index));
        result.Values.append(rendered.Value);
        if (!rendered.IsValid())
        {
            result.Errors.append(QStringLiteral("Item %1: %2").arg(index + 1).arg(rendered.Error));
            continue;
        }
        const QString key = rendered.Value.toCaseFolded();
        if (seen.contains(key) && !result.Duplicates.contains(rendered.Value, Qt::CaseInsensitive))
            result.Duplicates.append(rendered.Value);
        seen.insert(key);
    }
    return result;
}
} // namespace ClipCutter
