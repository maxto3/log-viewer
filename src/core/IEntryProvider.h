#pragma once

#include "core/LogEntry.h"

#include <QString>
#include <QVector>

#include <memory>

namespace lv {

/// Metadata about the currently displayed document.
struct DocumentInfo {
    QString filePath;
    QString fileName;
    QString encodingName;
    QString formatId;
    QString formatName;
    qint64 fileSize = 0;
    int lineCount = 0;
    bool truncated = false;     ///< index stopped at the configured line limit
    bool multiFile = false;
    bool mergeContinuations = true;
};

/// Supplies rows to the table model. Implemented by the real file-backed
/// LogSource (and the merged LogDocument) as well as by the synthetic demo data
/// provider.
class IEntryProvider
{
public:
    virtual ~IEntryProvider() = default;

    virtual int rowCount() const = 0;
    virtual const LogEntry &entryAt(int row) const = 0;

    /// Level histogram indexed by logLevelIndex(). Empty when not available.
    virtual QVector<int> levelCounts() const = 0;

    virtual DocumentInfo documentInfo() const = 0;

    /// File name of the source an entry belongs to (File column, REQ-PARSE-03).
    virtual QString sourceName(int sourceIndex) const = 0;
};

using EntryProviderPtr = std::shared_ptr<IEntryProvider>;

} // namespace lv
