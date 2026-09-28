#pragma once

#include "core/LogFormat.h"

#include <QVector>
#include <memory>

namespace lv {

/// Holds the built-in log formats and picks the best match for a file.
class LogFormatRegistry
{
public:
    static const LogFormatRegistry &instance();

    QVector<const ILogFormat *> formats() const;
    const ILogFormat *findById(const QString &id) const;

    /// Returns the format with the best probe score (>= 60) or the generic
    /// fallback when nothing scores high enough.
    const ILogFormat *detect(const QStringList &sampleLines) const;

private:
    LogFormatRegistry();
    std::vector<std::unique_ptr<ILogFormat>> m_formats;
};

} // namespace lv
