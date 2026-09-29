#include "platform/FileAssociation.h"

#include <QDir>
#include <QFileInfo>

#include <string>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <shlobj.h>
#endif

namespace lv {

namespace {

/// Same translation context as FileAssociation::tr; used by the file local
/// Win32 helpers, which cannot use the class scope.
QString tr(const char *text)
{
    return QCoreApplication::translate("FileAssociation", text);
}

#if defined(Q_OS_WIN)

constexpr auto kClassesRoot = "Software\\Classes";
constexpr auto kBackupKeyPath = "Software\\Classes\\LogViewer.Backup";

/// Registry keys need a value name, not a key path: the extension doubles as
/// the value name in the backup key and in Applications\...\SupportedTypes.
std::wstring toValueName(const QString &name)
{
    return QDir::toNativeSeparators(name).toStdWString();
}

std::wstring toKeyPath(const QString &path)
{
    return QDir::toNativeSeparators(path).toStdWString();
}

QString registryError(LSTATUS status)
{
    return tr("Windows registry access failed (error %1).").arg(int(status));
}

/// Reads a REG_SZ/REG_EXPAND_SZ value; \a found reports whether it exists.
/// Other value types are reported as "not found" — we never write them.
QString readStringValue(const QString &keyPath, const wchar_t *valueName, bool *found,
                        QString *error)
{
    *found = false;
    HKEY key = nullptr;
    const std::wstring path = toKeyPath(keyPath);
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_READ, &key);
    if (status == ERROR_FILE_NOT_FOUND)
        return QString();
    if (status != ERROR_SUCCESS) {
        *error = registryError(status);
        return QString();
    }

    DWORD type = 0;
    DWORD size = 0;
    status = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &size);
    if (status == ERROR_FILE_NOT_FOUND || (status == ERROR_SUCCESS && type != REG_SZ
                                           && type != REG_EXPAND_SZ)) {
        RegCloseKey(key);
        return QString();
    }
    if (status != ERROR_SUCCESS) {
        RegCloseKey(key);
        *error = registryError(status);
        return QString();
    }

    // RegQueryValueExW returns the size in bytes, including the terminator of
    // a REG_SZ value.
    std::wstring buffer(static_cast<size_t>(size) / sizeof(wchar_t) + 1, L'\0');
    DWORD bufferSize = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
    status = RegQueryValueExW(key, valueName, nullptr, &type,
                              reinterpret_cast<LPBYTE>(buffer.data()), &bufferSize);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        *error = registryError(status);
        return QString();
    }

    *found = true;
    return QString::fromWCharArray(buffer.c_str());
}

bool writeValue(const QString &keyPath, const wchar_t *valueName, DWORD type, const void *data,
                DWORD size, QString *error)
{
    HKEY key = nullptr;
    const std::wstring path = toKeyPath(keyPath);
    // RegCreateKeyExW creates every missing component of the path.
    LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_WRITE,
                                     nullptr, &key, nullptr);
    if (status == ERROR_SUCCESS)
        status = RegSetValueExW(key, valueName, 0, type, reinterpret_cast<const BYTE *>(data), size);
    if (key)
        RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        *error = registryError(status);
        return false;
    }
    return true;
}

bool writeStringValue(const QString &keyPath, const wchar_t *valueName, const QString &value,
                      QString *error)
{
    const std::wstring wide = value.toStdWString();
    return writeValue(keyPath, valueName, REG_SZ, wide.c_str(),
                      static_cast<DWORD>((wide.size() + 1) * sizeof(wchar_t)), error);
}

/// Deletes a value; a missing key or value is not an error (idempotent).
bool deleteValue(const QString &keyPath, const wchar_t *valueName)
{
    HKEY key = nullptr;
    const std::wstring path = toKeyPath(keyPath);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return true;
    const LSTATUS status = RegDeleteValueW(key, valueName);
    RegCloseKey(key);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

bool deleteKeyTree(const QString &keyPath)
{
    const std::wstring path = toKeyPath(keyPath);
    const LSTATUS status = RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str());
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

/// True when the key does not exist or holds neither values nor subkeys, so an
/// empty OpenWithProgids / backup key can be cleaned up.
bool keyIsEmpty(const QString &keyPath)
{
    HKEY key = nullptr;
    const std::wstring path = toKeyPath(keyPath);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return true;
    DWORD subKeys = 0;
    DWORD values = 0;
    const LSTATUS status = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subKeys, nullptr,
                                            nullptr, &values, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return status != ERROR_SUCCESS || (subKeys == 0 && values == 0);
}

/// Expands 8.3 short names (`C:\PROGRA~1\...`) to the form Explorer shows.
/// The file must exist; callers fall back to the input when it does not.
QString longPathName(const QString &path)
{
    const QString native = QDir::toNativeSeparators(path);
    const std::wstring wide = native.toStdWString();
    const DWORD size = GetLongPathNameW(wide.c_str(), nullptr, 0);
    if (size == 0)
        return native;
    std::wstring buffer(static_cast<size_t>(size), L'\0');
    const DWORD written = GetLongPathNameW(wide.c_str(), buffer.data(), size);
    if (written == 0 || written >= size)
        return native;
    return QString::fromWCharArray(buffer.c_str());
}

/// Absolute, canonical executable path: junctions and symlinks are resolved and
/// 8.3 short names expanded. Two spellings of the same file would break the
/// "is the association still pointing at this copy?" check (REQ-UI-16).
QString canonicalPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return longPathName(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

/// Executable of a shell command line: `"C:\x\y.exe" "%1"` -> `C:\x\y.exe`.
QString commandExecutable(const QString &command)
{
    if (command.startsWith(QLatin1Char('"'))) {
        const int end = command.indexOf(QLatin1Char('"'), 1);
        if (end > 1)
            return command.mid(1, end - 1);
    }
    const int space = command.indexOf(QLatin1Char(' '));
    return space > 0 ? command.left(space) : command;
}

QString extensionKeyPath(const QString &extension)
{
    return QLatin1String(kClassesRoot) + QLatin1Char('\\') + extension;
}

QString progIdKeyPath(const QString &extension)
{
    return QLatin1String(kClassesRoot) + QLatin1Char('\\') + FileAssociation::progIdFor(extension);
}

QString applicationKeyPath(const QString &exePath)
{
    return QLatin1String(kClassesRoot) + QLatin1String("\\Applications\\")
        + QFileInfo(exePath).fileName();
}

#endif // Q_OS_WIN

} // namespace

QString FileAssociation::defaultExtension()
{
    return QStringLiteral(".log");
}

bool FileAssociation::isSupported()
{
#if defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

QString FileAssociation::canonicalExePath(const QString &exePath)
{
#if defined(Q_OS_WIN)
    return canonicalPath(exePath);
#else
    return QFileInfo(exePath).absoluteFilePath();
#endif
}

QString FileAssociation::progIdFor(const QString &extension)
{
    QString suffix = extension;
    while (suffix.startsWith(QLatin1Char('.')))
        suffix.remove(0, 1);
    return QLatin1String("LogViewer.") + suffix;
}

QString FileAssociation::commandLineFor(const QString &exePath)
{
    return QLatin1Char('"') + QDir::toNativeSeparators(exePath) + QLatin1String("\" \"%1\"");
}

QString FileAssociation::defaultIconFor(const QString &exePath)
{
    return QLatin1Char('"') + QDir::toNativeSeparators(exePath) + QLatin1String("\",0");
}

QString FileAssociation::unsupportedText()
{
    return tr("File association is only available on Windows. On Linux use "
              "\"xdg-mime default log-viewer.desktop text/x-log\" instead.");
}

FileAssociation::Result FileAssociation::registerForCurrentUser(const QString &extension,
                                                               const QString &exePath, bool force)
{
    Result result;

    if (!extension.startsWith(QLatin1Char('.')) || extension.size() < 2) {
        result.error = tr("'%1' is not a file extension.").arg(extension);
        return result;
    }

#if defined(Q_OS_WIN)
    const QString exe = canonicalExePath(exePath);
    if (!QFileInfo::exists(exe)) {
        result.error = tr("Executable not found: %1").arg(exe);
        return result;
    }

    const QString progId = progIdFor(extension);
    const QString extensionKey = extensionKeyPath(extension);
    const QString progIdKey = progIdKeyPath(extension);
    const QString applicationKey = applicationKeyPath(exe);
    const QString backupKey = QLatin1String(kBackupKeyPath);
    const std::wstring valueName = toValueName(extension);

    QString error;
    bool found = false;
    const QString current = readStringValue(extensionKey, nullptr, &found, &error);
    if (!error.isEmpty()) {
        result.error = error;
        return result;
    }

    // Remember what the extension pointed at so -Unregister can restore it.
    if (found && !current.isEmpty() && current.compare(progId, Qt::CaseInsensitive) != 0) {
        if (!force) {
            result.warnings.append(
                tr("%1 currently opens with %2. That value was backed up and is restored when the "
                   "association is removed.").arg(extension, current));
        }
        if (!writeStringValue(backupKey, valueName.c_str(), current, &error)) {
            result.error = error;
            return result;
        }
    }

    const bool written =
        writeStringValue(progIdKey, nullptr, tr("Log file"), &error)
        && writeStringValue(progIdKey, L"FriendlyTypeName", tr("Log File"), &error)
        && writeStringValue(progIdKey + QLatin1String("\\DefaultIcon"), nullptr,
                            defaultIconFor(exe), &error)
        && writeStringValue(progIdKey + QLatin1String("\\shell\\open\\command"), nullptr,
                            commandLineFor(exe), &error)
        && writeStringValue(extensionKey, nullptr, progId, &error)
        && writeValue(extensionKey + QLatin1String("\\OpenWithProgids"), toValueName(progId).c_str(),
                      REG_NONE, nullptr, 0, &error)
        && writeStringValue(applicationKey, L"FriendlyAppName", tr("Log Viewer"), &error)
        && writeStringValue(applicationKey + QLatin1String("\\SupportedTypes"), valueName.c_str(),
                            QString(), &error)
        && writeStringValue(applicationKey + QLatin1String("\\shell\\open\\command"), nullptr,
                            commandLineFor(exe), &error);
    if (!written) {
        result.error = error;
        return result;
    }

    // Tell Explorer that the association changed; without this the new handler
    // is only picked up after the next sign-in in some cases.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    result.ok = true;
    return result;
#else
    Q_UNUSED(exePath);
    Q_UNUSED(force);
    result.error = unsupportedText();
    return result;
#endif
}

FileAssociation::Result FileAssociation::unregisterForCurrentUser(const QString &extension)
{
    Result result;

    if (!extension.startsWith(QLatin1Char('.')) || extension.size() < 2) {
        result.error = tr("'%1' is not a file extension.").arg(extension);
        return result;
    }

#if defined(Q_OS_WIN)
    const QString exe = canonicalExePath(QCoreApplication::applicationFilePath());
    const QString progId = progIdFor(extension);
    const QString extensionKey = extensionKeyPath(extension);
    const QString progIdKey = progIdKeyPath(extension);
    const QString applicationKey = applicationKeyPath(exe);
    const QString backupKey = QLatin1String(kBackupKeyPath);
    const QString openWithProgidsKey = extensionKey + QLatin1String("\\OpenWithProgids");
    const std::wstring valueName = toValueName(extension);

    QString error;
    bool found = false;
    const QString backup = readStringValue(backupKey, valueName.c_str(), &found, &error);
    if (!error.isEmpty()) {
        result.error = error;
        return result;
    }

    // Restore the previous handler, or drop our value when there was none.
    if (found && !backup.isEmpty()) {
        if (!writeStringValue(extensionKey, nullptr, backup, &error)) {
            result.error = error;
            return result;
        }
    } else {
        deleteValue(extensionKey, nullptr);
    }

    deleteValue(openWithProgidsKey, toValueName(progId).c_str());
    if (keyIsEmpty(openWithProgidsKey))
        deleteKeyTree(openWithProgidsKey);

    deleteKeyTree(progIdKey);

    // The Applications key belongs to the file name, not to a path: a second
    // copy of the program in another folder must not be unregistered here. A
    // stale one is removed though -- otherwise "Open with" keeps offering a Log
    // Viewer that was moved or deleted.
    bool commandFound = false;
    const QString command = readStringValue(applicationKey + QLatin1String("\\shell\\open\\command"),
                                            nullptr, &commandFound, &error);
    const bool isOurCopy = command.compare(commandLineFor(exe), Qt::CaseInsensitive) == 0;
    const bool isStaleCopy = !commandExecutable(command).isEmpty()
        && !QFileInfo::exists(commandExecutable(command));
    if (commandFound && (isOurCopy || isStaleCopy)) {
        deleteKeyTree(applicationKey);
    } else {
        deleteValue(applicationKey + QLatin1String("\\SupportedTypes"), valueName.c_str());
    }

    deleteValue(backupKey, valueName.c_str());
    if (keyIsEmpty(backupKey))
        deleteKeyTree(backupKey);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    result.ok = true;
    return result;
#else
    result.error = unsupportedText();
    return result;
#endif
}

bool FileAssociation::isRegisteredForCurrentUser(const QString &extension, const QString &exePath)
{
#if defined(Q_OS_WIN)
    QString error;
    bool found = false;
    const QString progId = progIdFor(extension);
    const QString current = readStringValue(extensionKeyPath(extension), nullptr, &found, &error);
    if (!found || current.compare(progId, Qt::CaseInsensitive) != 0)
        return false;

    bool commandFound = false;
    const QString command = readStringValue(progIdKeyPath(extension)
                                                + QLatin1String("\\shell\\open\\command"),
                                            nullptr, &commandFound, &error);
    return commandFound
        && command.compare(commandLineFor(canonicalExePath(exePath)), Qt::CaseInsensitive) == 0;
#else
    Q_UNUSED(extension);
    Q_UNUSED(exePath);
    return false;
#endif
}

} // namespace lv
