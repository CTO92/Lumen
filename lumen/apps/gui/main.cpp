/// @file main.cpp
/// @brief Lumen GUI application entry point

#include <QApplication>
#include <QStyleHints>
#include <QCommandLineParser>
#include <QFile>
#include <QDir>

#include "mainwindow.hpp"
#include "applicationstate.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Application metadata
    app.setOrganizationName("OAQL");
    app.setOrganizationDomain("oaql.ai");
    app.setApplicationName("Lumen");
    app.setApplicationVersion("1.0.0");
    app.setApplicationDisplayName("Lumen - Portfolio Optimization");

    // Command line parsing
    QCommandLineParser parser;
    parser.setApplicationDescription("Lumen - Quantum-Enhanced Portfolio Optimization");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption darkModeOption("dark", "Start in dark mode");
    parser.addOption(darkModeOption);

    QCommandLineOption portfolioOption("portfolio", "Open portfolio file on startup", "file");
    parser.addOption(portfolioOption);

    parser.process(app);

    // Create application state
    ApplicationState state;

    // Determine dark mode: command line > system preference
    bool isDark = parser.isSet(darkModeOption);
    if (!isDark) {
        // Check system preference (Qt 6.5+)
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        isDark = app.styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        // Fallback for older Qt versions
        isDark = false;
#endif
    }
    state.setDarkMode(isDark);

    // Create and show main window
    MainWindow window(&state);

    // Load portfolio from command line if specified
    if (parser.isSet(portfolioOption)) {
        QString portfolioPath = parser.value(portfolioOption);
        if (QFile::exists(portfolioPath)) {
            window.loadPortfolioFromFile(portfolioPath);
        }
    }

    window.show();

    return app.exec();
}
