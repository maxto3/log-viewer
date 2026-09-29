#pragma once

#include <QCoreApplication>
#include <QString>
#include <QStringList>

namespace lv {

/// Parsed command line (see spec.md REQ-CLI).
struct CliOptions {
    QStringList files;                            ///< positional arguments
    QString language;                             ///< --lang, empty when not given
    QString formatId = QStringLiteral("auto");    ///< --format
    bool monitor = false;                         ///< --monitor
    bool demo = false;                            ///< --demo (synthetic preview data)
    bool registerAssociation = false;             ///< --register-association
    bool unregisterAssociation = false;           ///< --unregister-association
    bool forceAssociation = false;                ///< --force (with --register-association)
    bool helpRequested = false;                   ///< -h / --help
    bool versionRequested = false;                ///< -v / --version
    bool listFormats = false;                     ///< --format list
    QString error;                                ///< non-empty => usage error (exit code 2)
};

/// Pure command line parser: no UI dependency, unit tested in tst_cli.
class CliParser
{
    Q_DECLARE_TR_FUNCTIONS(CliParser)

public:
    static CliOptions parse(const QStringList &arguments);

    /// Localised usage text for --help.
    static QString usageText();

    /// Version string including the Qt version and build information.
    static QString versionText();

    /// Localised list of available format ids (used by --format list).
    static QString formatListText();

    /// Confirmation printed after --register-association (REQ-CLI-10).
    static QString associationRegisteredText(const QString &extension, const QString &exePath);

    /// Confirmation printed after --unregister-association (REQ-CLI-10).
    static QString associationRemovedText(const QString &extension);
};

} // namespace lv
