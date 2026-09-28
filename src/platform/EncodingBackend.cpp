#include "platform/EncodingBackend.h"

#include <QStringList>
#include <QVarLengthArray>

#if defined(Q_OS_WIN)
#  include <windows.h>
#else
#  include <errno.h>
#  include <iconv.h>
#endif

namespace lv {
namespace {

#if defined(Q_OS_WIN)
/// Codepage for a canonical encoding name (0 = not supported).
UINT codePageFor(const QString &encodingName)
{
    const QString name = encodingName.toUpper();
    if (name == QLatin1String("GB18030"))
        return 54936;
    if (name == QLatin1String("GBK") || name == QLatin1String("CP936"))
        return 936;
    if (name == QLatin1String("BIG5"))
        return 950;
    if (name == QLatin1String("SHIFT_JIS") || name == QLatin1String("SJIS"))
        return 932;
    if (name == QLatin1String("CP1252") || name == QLatin1String("WINDOWS-1252"))
        return 1252;
    return 0;
}

EncodingBackend::Result decodeCodePage(const QByteArray &raw, UINT codePage, bool strict)
{
    EncodingBackend::Result result;
    if (raw.isEmpty())
        return result;

    const DWORD flags = strict ? MB_ERR_INVALID_CHARS : 0;
    const int length = MultiByteToWideChar(codePage, flags, raw.constData(), raw.size(), nullptr, 0);
    if (length <= 0) {
        result.valid = false;
        result.invalidBytes = raw.size();
        return result;
    }

    QVarLengthArray<wchar_t, 4096> buffer(length);
    if (MultiByteToWideChar(codePage, flags, raw.constData(), raw.size(), buffer.data(), length) <= 0) {
        result.valid = false;
        result.invalidBytes = raw.size();
        return result;
    }
    result.text = QString::fromWCharArray(buffer.data(), length);
    return result;
}
#else
const char *iconvNameFor(const QString &encodingName)
{
    const QString name = encodingName.toUpper();
    if (name == QLatin1String("GB18030"))
        return "GB18030";
    if (name == QLatin1String("GBK") || name == QLatin1String("CP936"))
        return "GBK";
    if (name == QLatin1String("BIG5"))
        return "BIG5";
    if (name == QLatin1String("SHIFT_JIS") || name == QLatin1String("SJIS"))
        return "SHIFT_JIS";
    if (name == QLatin1String("CP1252") || name == QLatin1String("WINDOWS-1252"))
        return "CP1252";
    return nullptr;
}

EncodingBackend::Result decodeIconv(const QByteArray &raw, const char *encoding)
{
    EncodingBackend::Result result;
    if (raw.isEmpty() || !encoding)
        return result;

    iconv_t handle = iconv_open("UTF-8", encoding);
    if (handle == reinterpret_cast<iconv_t>(-1)) {
        result.valid = false;
        result.invalidBytes = raw.size();
        return result;
    }

    QByteArray output;
    output.resize(raw.size() * 4 + 16);
    char *inputPointer = const_cast<char *>(raw.constData());
    size_t inputLeft = static_cast<size_t>(raw.size());
    char *outputPointer = output.data();
    size_t outputLeft = static_cast<size_t>(output.size());

    const size_t converted = iconv(handle, &inputPointer, &inputLeft, &outputPointer, &outputLeft);
    iconv_close(handle);

    if (converted == static_cast<size_t>(-1)) {
        result.valid = false;
        result.invalidBytes = static_cast<int>(inputLeft);
        return result;
    }
    output.resize(output.size() - static_cast<int>(outputLeft));
    result.text = QString::fromUtf8(output);
    return result;
}
#endif

} // namespace

QStringList EncodingBackend::candidateEncodings()
{
    // Ordered by likelihood for the log files this application targets.
    return {QStringLiteral("GB18030"), QStringLiteral("Big5"), QStringLiteral("Shift_JIS"),
            QStringLiteral("CP1252")};
}

EncodingBackend::Result EncodingBackend::decode(const QByteArray &raw, const QString &encodingName)
{
#if defined(Q_OS_WIN)
    const UINT codePage = codePageFor(encodingName);
    if (codePage == 0)
        return decodeLocal(raw);
    return decodeCodePage(raw, codePage, false);
#else
    const char *name = iconvNameFor(encodingName);
    if (!name)
        return decodeLocal(raw);
    return decodeIconv(raw, name);
#endif
}

EncodingBackend::Result EncodingBackend::decodeLocal(const QByteArray &raw)
{
#if defined(Q_OS_WIN)
    return decodeCodePage(raw, CP_ACP, false);
#else
    EncodingBackend::Result result;
    result.text = QString::fromLocal8Bit(raw);
    return result;
#endif
}

QString EncodingBackend::detectLegacyEncoding(const QByteArray &sample)
{
    if (sample.isEmpty())
        return {};

    for (const QString &candidate : candidateEncodings()) {
        bool valid = false;
#if defined(Q_OS_WIN)
        const UINT codePage = codePageFor(candidate);
        if (codePage != 0)
            valid = decodeCodePage(sample, codePage, true).valid;
#else
        const char *name = iconvNameFor(candidate);
        if (name)
            valid = decodeIconv(sample, name).valid;
#endif
        if (valid)
            return candidate;
    }
    return {};
}

QString EncodingBackend::localEncodingName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("ANSI (CP%1)").arg(GetACP());
#else
    return QStringLiteral("local 8-bit");
#endif
}

} // namespace lv
