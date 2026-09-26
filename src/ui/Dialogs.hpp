#pragma once

#include <QLineEdit>
#include <QMessageBox>
#include <QString>

class QWidget;

/// Drop-in replacements for the QMessageBox / QInputDialog static functions.
/// With integrated title bars they show a dialog with Burner's title bar;
/// otherwise they show the standard Qt dialog.
namespace Burner::UI::Dialogs {

QMessageBox::StandardButton information(QWidget* parent, const QString& title, const QString& text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

QMessageBox::StandardButton warning(QWidget* parent, const QString& title, const QString& text,
                                    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

QMessageBox::StandardButton critical(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                     QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

QMessageBox::StandardButton question(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

void about(QWidget* parent, const QString& title, const QString& text);

QString getText(QWidget* parent, const QString& title, const QString& label,
                QLineEdit::EchoMode mode = QLineEdit::Normal, const QString& text = QString(),
                bool* ok = nullptr);

} // namespace Burner::UI::Dialogs
