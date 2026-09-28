#pragma once

#include "core/LogLevel.h"
#include "core/Matcher.h"

#include <QDateTime>

namespace lv {

/// Everything the Filter box sends to the table model (spec.md REQ-FILTER).
/// All active conditions are combined with AND.
struct FilterSpec {
    /// Bit i set -> level i (logLevelIndex order) is shown. 0 = no level filter.
    int levelMask = 0;

    bool timeRangeActive = false;
    QDateTime timeFrom;
    QDateTime timeTo;
    /// Entries without a parsable timestamp: shown or hidden (REQ-FILTER-08).
    bool includeInvalidTime = false;

    /// Keyword filter; an empty matcher disables it.
    Matcher keyword;
    /// Inverted keyword filter: only rows that do *not* match \a keyword stay
    /// visible (REQ-FILTER-09). Ignored when the matcher is empty.
    bool invertKeyword = false;

    bool isActive() const
    {
        return levelMask != 0 || timeRangeActive || !keyword.isEmpty();
    }
};

} // namespace lv
