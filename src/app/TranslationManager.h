#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>

namespace lv {

/// Loads and switches the interface translation at runtime (spec.md REQ-I18N).
class TranslationManager : public QObject
{
    Q_OBJECT

public:
    explicit TranslationManager(QObject *parent = nullptr);

    /// Language codes always offered by the UI ("en" is the source language).
    static QStringList availableLanguages();

    /// Human readable name of a language code, e.g. "简体中文".
    static QString displayName(const QString &code);

    QString language() const { return m_language; }

    /// Installs the translation for \a code ("en" removes the translation) and
    /// notifies all top level widgets so they can retranslate themselves.
    /// Returns false when a translation file was expected but could not be read.
    bool setLanguage(const QString &code);

signals:
    void languageChanged(const QString &code);

private:
    void notifyWidgets();

    QTranslator m_translator;
    QTranslator m_qtTranslator;
    QString m_language = QStringLiteral("en");
    bool m_installed = false;
    bool m_qtInstalled = false;
};

} // namespace lv
