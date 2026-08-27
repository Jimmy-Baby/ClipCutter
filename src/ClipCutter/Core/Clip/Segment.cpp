#include "Core/Clip/Segment.h"

namespace ClipCutter
{
QString Segment::OutputFileName() const
{
    return (NamingTemplatePattern.has_value() ? QString() : Prefix.value_or(QString())) + OutputBaseName + OutputExtension;
}
} // namespace ClipCutter
