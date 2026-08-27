#ifndef CLIPCUTTER_CORE_NAMING_NAMINGTEMPLATE_H
#define CLIPCUTTER_CORE_NAMING_NAMINGTEMPLATE_H

#include <QDate>
#include <QString>
#include <QVector>

namespace ClipCutter
{
struct NamingTemplateContext
{
    QString Original;
    QString Prefix;
    int Index = 1;
    QDate Date = QDate::currentDate();
    QString Profile;
    int Segment = 1;
};

struct NamingTemplateResult
{
    QString Value;
    QString Error;

    bool IsValid() const noexcept { return Error.isEmpty(); }
};

struct NamingBatchResult
{
    QVector<QString> Values;
    QStringList Errors;
    QStringList Duplicates;

    bool IsValid() const noexcept { return Errors.isEmpty() && Duplicates.isEmpty(); }
};

class NamingTemplate final
{
public:
    static QString DefaultPattern();
    static QString Validate(const QString& pattern);
    static NamingTemplateResult Render(const QString& pattern, const NamingTemplateContext& context);
    static NamingBatchResult RenderBatch(const QString& pattern, const QVector<NamingTemplateContext>& contexts);
};
} // namespace ClipCutter

#endif
