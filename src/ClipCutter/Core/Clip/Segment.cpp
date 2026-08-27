#include "Core/Clip/Segment.h"

namespace ClipCutter
{
QString Segment::OutputFileName() const
{
    return Prefix.value_or(QString()) + OutputBaseName + OutputExtension;
}
} // namespace ClipCutter
