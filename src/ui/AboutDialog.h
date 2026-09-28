#pragma once

#include <QDialog>

class QLabel;
class QPushButton;

namespace lv {

/// Settings ▸ About: version, build information and licensing notes.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void retranslateUi();

    QLabel *m_title = nullptr;
    QLabel *m_version = nullptr;
    QLabel *m_build = nullptr;
    QLabel *m_description = nullptr;
    QLabel *m_license = nullptr;
    QPushButton *m_close = nullptr;
};

} // namespace lv
