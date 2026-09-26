#include "Dialogs.hpp"
#include "widgets/WindowChrome.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace Burner::UI::Dialogs {

namespace {

/// The button that closing the dialog (Escape, title bar close) counts as,
/// following QMessageBox's rules
QMessageBox::StandardButton escapeButton(QMessageBox::StandardButtons buttons) {
    for (auto button : {QMessageBox::Cancel, QMessageBox::Close, QMessageBox::Abort, QMessageBox::No}) {
        if (buttons.testFlag(button)) {
            return button;
        }
    }
    // Otherwise the first button, e.g. the OK of a single-button message
    for (uint bit = QMessageBox::FirstButton; bit <= QMessageBox::LastButton; bit <<= 1) {
        if (buttons.testFlag(static_cast<QMessageBox::StandardButton>(bit))) {
            return static_cast<QMessageBox::StandardButton>(bit);
        }
    }
    return QMessageBox::NoButton;
}

QPixmap messagePixmap(QWidget* dialog, QMessageBox::Icon icon) {
    QStyle::StandardPixmap which = QStyle::SP_MessageBoxInformation;
    switch (icon) {
        case QMessageBox::Warning:  which = QStyle::SP_MessageBoxWarning; break;
        case QMessageBox::Critical: which = QStyle::SP_MessageBoxCritical; break;
        case QMessageBox::Question: which = QStyle::SP_MessageBoxQuestion; break;
        default: break;
    }
    const int size = dialog->style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, dialog);
    return dialog->style()->standardIcon(which, nullptr, dialog)
        .pixmap(QSize(size, size), dialog->devicePixelRatioF());
}

/// Icon and text, laid out like QMessageBox
QHBoxLayout* messageRow(const QPixmap& pixmap, const QString& text) {
    auto* row = new QHBoxLayout;
    row->setSpacing(14);
    auto* icon = new QLabel;
    icon->setPixmap(pixmap);
    row->addWidget(icon, 0, Qt::AlignTop);
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(true);
    label->setMinimumWidth(360);
    row->addWidget(label, 1);
    return row;
}

QMessageBox::StandardButton showMessage(QMessageBox::Icon icon, QWidget* parent, const QString& title,
                                        const QString& text, QMessageBox::StandardButtons buttons,
                                        QMessageBox::StandardButton defaultButton) {
    if (!WindowChrome::integratedTitleBars()) {
        switch (icon) {
            case QMessageBox::Warning:  return QMessageBox::warning(parent, title, text, buttons, defaultButton);
            case QMessageBox::Critical: return QMessageBox::critical(parent, title, text, buttons, defaultButton);
            case QMessageBox::Question: return QMessageBox::question(parent, title, text, buttons, defaultButton);
            default: return QMessageBox::information(parent, title, text, buttons, defaultButton);
        }
    }

    ChromeDialog dialog(parent, title);
    dialog.body()->addLayout(messageRow(messagePixmap(&dialog, icon), text));
    dialog.body()->addSpacing(6);

    // Same buttons and default as QMessageBox: the requested default, else the
    // first accept button
    auto* buttonBox = new QDialogButtonBox;
    QPushButton* defaultPush = nullptr;
    for (uint bit = QMessageBox::FirstButton; bit <= QMessageBox::LastButton; bit <<= 1) {
        const auto button = static_cast<QMessageBox::StandardButton>(bit);
        if (!buttons.testFlag(button)) {
            continue;
        }
        QPushButton* push = buttonBox->addButton(static_cast<QDialogButtonBox::StandardButton>(bit));
        const bool isDefault = defaultButton == QMessageBox::NoButton
            ? buttonBox->buttonRole(push) == QDialogButtonBox::AcceptRole
            : button == defaultButton;
        if (!defaultPush && isDefault) {
            defaultPush = push;
        }
    }
    if (defaultPush) {
        defaultPush->setDefault(true);
        defaultPush->setFocus();
    }
    dialog.body()->addWidget(buttonBox);

    QMessageBox::StandardButton result = escapeButton(buttons);
    QObject::connect(buttonBox, &QDialogButtonBox::clicked, &dialog, [&](QAbstractButton* button) {
        result = static_cast<QMessageBox::StandardButton>(buttonBox->standardButton(button));
        dialog.accept();
    });
    dialog.exec();
    return result;
}

} // namespace

QMessageBox::StandardButton information(QWidget* parent, const QString& title, const QString& text,
                                        QMessageBox::StandardButtons buttons,
                                        QMessageBox::StandardButton defaultButton) {
    return showMessage(QMessageBox::Information, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton warning(QWidget* parent, const QString& title, const QString& text,
                                    QMessageBox::StandardButtons buttons,
                                    QMessageBox::StandardButton defaultButton) {
    return showMessage(QMessageBox::Warning, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton critical(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons,
                                     QMessageBox::StandardButton defaultButton) {
    return showMessage(QMessageBox::Critical, parent, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton question(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons,
                                     QMessageBox::StandardButton defaultButton) {
    return showMessage(QMessageBox::Question, parent, title, text, buttons, defaultButton);
}

void about(QWidget* parent, const QString& title, const QString& text) {
    if (!WindowChrome::integratedTitleBars()) {
        QMessageBox::about(parent, title, text);
        return;
    }

    ChromeDialog dialog(parent, title);
    // QMessageBox::about shows the window icon at 64px
    const QPixmap icon = dialog.windowIcon().pixmap(QSize(64, 64), dialog.devicePixelRatioF());
    dialog.body()->addLayout(messageRow(icon, text));
    dialog.body()->addSpacing(6);
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    dialog.body()->addWidget(buttonBox);
    buttonBox->button(QDialogButtonBox::Ok)->setFocus();
    dialog.exec();
}

QString getText(QWidget* parent, const QString& title, const QString& label,
                QLineEdit::EchoMode mode, const QString& text, bool* ok) {
    if (!WindowChrome::integratedTitleBars()) {
        return QInputDialog::getText(parent, title, label, mode, text, ok);
    }

    ChromeDialog dialog(parent, title);
    auto* prompt = new QLabel(label);
    auto* edit = new QLineEdit(text);
    edit->setEchoMode(mode);
    edit->setMinimumWidth(300);
    prompt->setBuddy(edit);
    dialog.body()->addWidget(prompt);
    dialog.body()->addWidget(edit);
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.body()->addWidget(buttonBox);
    edit->setFocus();
    edit->selectAll();

    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (ok) {
        *ok = accepted;
    }
    return accepted ? edit->text() : QString();
}

} // namespace Burner::UI::Dialogs
