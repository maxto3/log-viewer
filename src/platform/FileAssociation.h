#pragma once

#include <QCoreApplication>
#include <QString>
#include <QStringList>

namespace lv {

/// Registers the ".log" file association in the current user's registry
/// (HKCU\Software\Classes) so that double-clicking a .log file starts this
/// application, no matter where the program folder was unpacked
/// (spec.md REQ-ASSOC-01 / REQ-ASSOC-06 / REQ-ASSOC-07).
///
/// The registration always refers to the executable that runs it
/// (QCoreApplication::applicationFilePath()), which is what makes a portable
/// installation work: unpack the archive anywhere, run the registration once.
///
/// This is the only place that talks to the Windows registry, so the platform
/// conditional lives here and nowhere else (CON-9 / REQ-PLAT-01). Linux keeps
/// using the xdg-mime helper script (REQ-ASSOC-03).
class FileAssociation
{
    Q_DECLARE_TR_FUNCTIONS(FileAssociation)

public:
    /// Outcome of a registration or removal, ready to be shown to the user.
    struct Result {
        bool ok = false;
        QString error;          ///< translated; set when ok == false
        QStringList warnings;   ///< translated, non-fatal notes (e.g. backup)
    };

    /// The only extension this application registers (REQ-ASSOC-01: never .txt).
    static QString defaultExtension();

    /// Canonical form of an executable path: absolute, junctions resolved and
    /// (on Windows) 8.3 short names expanded. This is what the registry stores
    /// and what isRegisteredForCurrentUser() compares against, so two spellings
    /// of the same file never look like two different installations.
    static QString canonicalExePath(const QString &exePath);

    /// True on Windows; the registry registration exists nowhere else.
    static bool isSupported();

    /// ProgID written to the registry, e.g. "LogViewer.log" for ".log".
    static QString progIdFor(const QString &extension);

    /// Shell "open" command: `"C:\path\log-viewer.exe" "%1"` (REQ-ASSOC-02).
    static QString commandLineFor(const QString &exePath);

    /// Explorer icon of the file type: `"C:\path\log-viewer.exe",0`.
    static QString defaultIconFor(const QString &exePath);

    /// Registers \a extension for the current user so that it opens with
    /// \a exePath. The previous value of the extension is backed up and
    /// restored again by unregisterForCurrentUser(). \a force only suppresses
    /// the warning shown when the extension currently points elsewhere.
    static Result registerForCurrentUser(const QString &extension, const QString &exePath,
                                         bool force = false);

    /// Removes everything registerForCurrentUser() wrote for \a extension and
    /// restores the association that was in place before
    /// (spec.md REQ-ASSOC-08).
    static Result unregisterForCurrentUser(const QString &extension);

    /// True when \a extension currently opens with \a exePath. Detecting a
    /// stale path (the folder was moved after registration) is what makes the
    /// Settings menu entry show the truth (REQ-UI-16).
    static bool isRegisteredForCurrentUser(const QString &extension, const QString &exePath);

    /// Explanation shown on platforms without a registry (REQ-ASSOC-03).
    static QString unsupportedText();
};

} // namespace lv
