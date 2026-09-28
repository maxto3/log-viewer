#pragma once

#include <QByteArray>
#include <QString>

namespace lv {

/// Converts legacy 8-bit encodings to Unicode with the platform facilities.
///
/// Windows uses the ANSI code pages (`MultiByteToWideChar`), Linux uses `iconv`
/// (glibc, supports GB18030, GBK, Big5, Shift-JIS, …). This is the only place
/// besides PlatformInfo that contains platform conditionals (spec.md CON-9).
class EncodingBackend
{
public:
    struct Result {
        QString text;
        bool valid = true;      ///< false when the bytes cannot be decoded
        int invalidBytes = 0;
    };

    /// Encodings tried when a file is not valid UTF-8, in this order.
    static QStringList candidateEncodings();

    /// Decodes \a raw with the named encoding ("GB18030", "Big5", …).
    static Result decode(const QByteArray &raw, const QString &encodingName);

    /// Decodes \a raw with the platform's local 8-bit encoding.
    static Result decodeLocal(const QByteArray &raw);

    /// Returns the first candidate that decodes the whole \a sample without
    /// invalid bytes, or an empty string when none applies.
    static QString detectLegacyEncoding(const QByteArray &sample);

    /// Human readable name for the platform local encoding.
    static QString localEncodingName();
};

} // namespace lv
