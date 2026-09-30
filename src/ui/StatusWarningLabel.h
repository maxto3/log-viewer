#pragma once

#include <QLabel>

namespace lv {

/// Status bar indicator for persistent failure warnings (spec.md REQ-REL-01).
///
/// The label keeps the full message as tooltip and shows an elided version
/// (middle elision) so a long file path never stretches the status bar or the
/// main window. It is themed through the `#statusWarning` style sheet rule in
/// ThemeManager.
class StatusWarningLabel : public QLabel
{
    Q_OBJECT

public:
    explicit StatusWarningLabel(QWidget *parent = nullptr);

    /// Replaces the warning text; the label does not show itself.
    void setFullText(const QString &text);

    /// Unelided warning text (what the tooltip shows).
    QString fullText() const { return m_fullText; }

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void updateElidedText();

    QString m_fullText;
};

} // namespace lv
