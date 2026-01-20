/// @file aboutdialog.cpp
/// @brief About dialog implementation

#include "aboutdialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Lumen"));
    setFixedSize(450, 350);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(15);

    // Logo/Icon placeholder
    auto* iconLabel = new QLabel(this);
    iconLabel->setPixmap(QPixmap(":/icons/lumen.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // Application name and version
    auto* nameLabel = new QLabel(QString("<h1>Lumen</h1>"), this);
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    auto* versionLabel = new QLabel(
        tr("Version %1").arg(QApplication::applicationVersion()), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    // Description
    auto* descLabel = new QLabel(
        tr("Quantum-Enhanced Portfolio Optimization\n\n"
           "Lumen combines classical optimization with quantum computing\n"
           "to deliver optimal portfolio rebalancing with tax awareness\n"
           "and full explainability."),
        this);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // Features
    auto* featuresLabel = new QLabel(
        tr("Features:\n"
           "- Tax-aware lot selection (FIFO, LIFO, HIFO, Tax-Optimal)\n"
           "- D-Wave quantum annealing integration\n"
           "- IBM Quantum QAOA support\n"
           "- Full decision provenance and explainability"),
        this);
    featuresLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(featuresLabel);

    layout->addStretch();

    // Copyright
    auto* copyrightLabel = new QLabel(
        tr("Copyright 2024-2026 OAQL Labs\n"
           "Licensed under the MIT License"),
        this);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setStyleSheet("color: gray;");
    layout->addWidget(copyrightLabel);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    auto* websiteBtn = new QPushButton(tr("Website"), this);
    connect(websiteBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://oaql.ai"));
    });
    buttonLayout->addWidget(websiteBtn);

    auto* githubBtn = new QPushButton(tr("GitHub"), this);
    connect(githubBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/oaqlabs/lumen"));
    });
    buttonLayout->addWidget(githubBtn);

    buttonLayout->addStretch();

    auto* closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);

    layout->addLayout(buttonLayout);
}
