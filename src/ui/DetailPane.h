#pragma once

#include "core/LogEntry.h"
#include "core/Matcher.h"
#include "highlight/HighlightTheme.h"

#include <QColor>
#include <QGroupBox>

class QFormLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace lv {

class MessageTextHighlighter;

/// Detail pane (spec.md REQ-DETAIL): bold field names on top, the complete
/// (never truncated) message below, plus copy helpers.
class DetailPane : public QGroupBox
{
    Q_OBJECT

public:
    explicit DetailPane(QWidget *parent = nullptr);

    void setEntry(const LogEntry *entry, const QString &fileName);
    void clearEntry();
    void setMessageFont(const QFont &font);
    void setDarkTheme(bool dark);
    /// VSCode palette for embedded snippets (REQ-HL-06).
    void setHighlightTheme(const HighlightTheme *theme);
    /// Find highlight colours and pattern.
    void setFindHighlight(const Matcher &matcher, const QColor &background, const QColor &foreground);

    /// Diagnostics/test accessor for the message view.
    QPlainTextEdit *messageEdit() const { return m_message; }

public slots:
    void retranslateUi();

private:
    void rebuildFields();
    void updateButtons();
    void copyMessageToClipboard();
    void copyAllFieldsToClipboard();

    QFormLayout *m_form = nullptr;
    QLabel *m_messageLabel = nullptr;
    QLabel *m_placeholder = nullptr;
    QPlainTextEdit *m_message = nullptr;
    QPushButton *m_copyMessage = nullptr;
    QPushButton *m_copyAll = nullptr;

    LogEntry m_entry;
    QString m_fileName;
    bool m_hasEntry = false;
    bool m_dark = false;
    MessageTextHighlighter *m_highlighter = nullptr;
};

} // namespace lv
