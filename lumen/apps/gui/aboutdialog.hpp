#pragma once

/// @file aboutdialog.hpp
/// @brief About dialog

#include <QDialog>

/// About dialog showing application information
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
};
