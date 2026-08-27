#include "core/clip/Segment.h"

namespace clipcutter
{
QString Segment::outputFileName() const
{
    return prefix.value_or(QString()) + outputBaseName + outputExtension;
}
}
