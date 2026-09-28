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
};

} // namespace lv
