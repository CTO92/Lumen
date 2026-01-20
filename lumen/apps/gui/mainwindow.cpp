/// @file mainwindow.cpp
/// @brief Main application window implementation

#include "mainwindow.hpp"
#include "applicationstate.hpp"
#include "settingsdialog.hpp"
#include "aboutdialog.hpp"
#include "views/portfoliotableview.hpp"
#include "views/allocationchartview.hpp"
#include "views/targeteditor.hpp"
#include "panels/constraintpanel.hpp"
#include "panels/resultspanel.hpp"
#include "widgets/progresswidget.hpp"
#include "workers/optimizationworker.hpp"
#include "dialogs/positiondialog.hpp"
#include "dialogs/importdialog.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QThread>
#include <QMimeData>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QStandardPaths>

MainWindow::MainWindow(ApplicationState* state, QWidget* parent)
    : QMainWindow(parent)
    , state_(state)
    , workerThread_(nullptr)
    , worker_(nullptr)
{
    setWindowTitle("Lumen - Portfolio Optimization");
    setMinimumSize(1024, 768);
    setAcceptDrops(true);

    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupCentralWidget();
    setupDockWidgets();
    setupConnections();
    loadSettings();

    // Apply initial theme
    applyTheme(state_->isDarkMode());
    updateWindowTitle();
}

MainWindow::~MainWindow() {
    saveSettings();

    // Clean up worker thread
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait();
        delete workerThread_;
    }
}

void MainWindow::setupMenuBar() {
    // File menu
    fileMenu_ = menuBar()->addMenu(tr("&File"));

    newAction_ = fileMenu_->addAction(tr("&New Portfolio"));
    newAction_->setShortcut(QKeySequence::New);
    newAction_->setStatusTip(tr("Create a new portfolio"));

    openAction_ = fileMenu_->addAction(tr("&Open..."));
    openAction_->setShortcut(QKeySequence::Open);
    openAction_->setStatusTip(tr("Open an existing portfolio"));

    fileMenu_->addSeparator();

    saveAction_ = fileMenu_->addAction(tr("&Save"));
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setStatusTip(tr("Save the current portfolio"));

    saveAsAction_ = fileMenu_->addAction(tr("Save &As..."));
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    saveAsAction_->setStatusTip(tr("Save the portfolio to a new file"));

    fileMenu_->addSeparator();

    importAction_ = fileMenu_->addAction(tr("&Import CSV..."));
    importAction_->setShortcut(Qt::CTRL | Qt::Key_I);
    importAction_->setStatusTip(tr("Import positions from a CSV file"));

    exportAction_ = fileMenu_->addAction(tr("&Export Results..."));
    exportAction_->setShortcut(Qt::CTRL | Qt::Key_E);
    exportAction_->setStatusTip(tr("Export optimization results to CSV"));
    exportAction_->setEnabled(false);

    fileMenu_->addSeparator();

    recentFilesMenu_ = fileMenu_->addMenu(tr("Recent Files"));
    updateRecentFilesMenu();

    fileMenu_->addSeparator();

    exitAction_ = fileMenu_->addAction(tr("E&xit"));
    exitAction_->setShortcut(QKeySequence::Quit);
    exitAction_->setStatusTip(tr("Exit the application"));

    // Edit menu
    editMenu_ = menuBar()->addMenu(tr("&Edit"));

    addPositionAction_ = editMenu_->addAction(tr("&Add Position..."));
    addPositionAction_->setShortcut(Qt::CTRL | Qt::Key_Plus);
    addPositionAction_->setStatusTip(tr("Add a new position to the portfolio"));

    editPositionAction_ = editMenu_->addAction(tr("&Edit Position..."));
    editPositionAction_->setShortcut(Qt::Key_Return);
    editPositionAction_->setStatusTip(tr("Edit the selected position"));
    editPositionAction_->setEnabled(false);

    removePositionAction_ = editMenu_->addAction(tr("&Remove Position"));
    removePositionAction_->setShortcut(Qt::Key_Delete);
    removePositionAction_->setStatusTip(tr("Remove the selected position"));
    removePositionAction_->setEnabled(false);

    // View menu
    viewMenu_ = menuBar()->addMenu(tr("&View"));

    darkModeAction_ = viewMenu_->addAction(tr("&Dark Mode"));
    darkModeAction_->setShortcut(Qt::CTRL | Qt::Key_D);
    darkModeAction_->setCheckable(true);
    darkModeAction_->setChecked(state_->isDarkMode());
    darkModeAction_->setStatusTip(tr("Toggle dark mode"));

    viewMenu_->addSeparator();

    settingsAction_ = viewMenu_->addAction(tr("&Settings..."));
    settingsAction_->setShortcut(QKeySequence::Preferences);
    settingsAction_->setStatusTip(tr("Open application settings"));

    viewMenu_->addSeparator();

    resetLayoutAction_ = viewMenu_->addAction(tr("&Reset Layout"));
    resetLayoutAction_->setStatusTip(tr("Reset window layout to default"));

    // Optimize menu
    optimizeMenu_ = menuBar()->addMenu(tr("&Optimize"));

    optimizeAction_ = optimizeMenu_->addAction(tr("&Run Optimization"));
    optimizeAction_->setShortcut(Qt::Key_F5);
    optimizeAction_->setStatusTip(tr("Run portfolio optimization"));
    optimizeAction_->setEnabled(false);

    cancelAction_ = optimizeMenu_->addAction(tr("&Cancel"));
    cancelAction_->setShortcut(Qt::Key_Escape);
    cancelAction_->setStatusTip(tr("Cancel running optimization"));
    cancelAction_->setEnabled(false);

    optimizeMenu_->addSeparator();

    costEstimateAction_ = optimizeMenu_->addAction(tr("Show Cost &Estimate..."));
    costEstimateAction_->setStatusTip(tr("Show estimated quantum computing cost"));
    costEstimateAction_->setEnabled(false);

    // Help menu
    helpMenu_ = menuBar()->addMenu(tr("&Help"));

    userGuideAction_ = helpMenu_->addAction(tr("&User Guide"));
    userGuideAction_->setShortcut(Qt::Key_F1);
    userGuideAction_->setStatusTip(tr("Open the user guide"));

    helpMenu_->addSeparator();

    aboutAction_ = helpMenu_->addAction(tr("&About Lumen..."));
    aboutAction_->setStatusTip(tr("About Lumen"));
}

void MainWindow::setupToolBar() {
    mainToolBar_ = addToolBar(tr("Main Toolbar"));
    mainToolBar_->setObjectName("MainToolBar");
    mainToolBar_->setMovable(false);

    mainToolBar_->addAction(newAction_);
    mainToolBar_->addAction(openAction_);
    mainToolBar_->addAction(saveAction_);
    mainToolBar_->addSeparator();
    mainToolBar_->addAction(importAction_);
    mainToolBar_->addSeparator();
    mainToolBar_->addAction(addPositionAction_);
    mainToolBar_->addSeparator();
    mainToolBar_->addAction(optimizeAction_);
}

void MainWindow::setupStatusBar() {
    statusLabel_ = new QLabel(tr("Ready"));
    statusBar()->addWidget(statusLabel_, 1);

    portfolioValueLabel_ = new QLabel();
    statusBar()->addPermanentWidget(portfolioValueLabel_);

    progressWidget_ = new ProgressWidget(this);
    progressWidget_->hide();
    statusBar()->addPermanentWidget(progressWidget_);
}

void MainWindow::setupCentralWidget() {
    centralSplitter_ = new QSplitter(Qt::Horizontal, this);

    portfolioTable_ = new PortfolioTableView(state_, this);
    allocationChart_ = new AllocationChartView(this);

    centralSplitter_->addWidget(portfolioTable_);
    centralSplitter_->addWidget(allocationChart_);
    centralSplitter_->setStretchFactor(0, 2);
    centralSplitter_->setStretchFactor(1, 1);

    setCentralWidget(centralSplitter_);
}

void MainWindow::setupDockWidgets() {
    // Target allocation dock
    targetDock_ = new QDockWidget(tr("Target Allocation"), this);
    targetDock_->setObjectName("TargetDock");
    targetEditor_ = new TargetEditor(state_, this);
    targetDock_->setWidget(targetEditor_);
    addDockWidget(Qt::RightDockWidgetArea, targetDock_);

    // Constraints dock
    constraintDock_ = new QDockWidget(tr("Constraints"), this);
    constraintDock_->setObjectName("ConstraintDock");
    constraintPanel_ = new ConstraintPanel(state_, this);
    constraintDock_->setWidget(constraintPanel_);
    addDockWidget(Qt::RightDockWidgetArea, constraintDock_);

    // Results dock
    resultsDock_ = new QDockWidget(tr("Results"), this);
    resultsDock_->setObjectName("ResultsDock");
    resultsPanel_ = new ResultsPanel(state_, this);
    resultsDock_->setWidget(resultsPanel_);
    addDockWidget(Qt::BottomDockWidgetArea, resultsDock_);

    // Add dock widget toggles to View menu
    viewMenu_->addSeparator();
    viewMenu_->addAction(targetDock_->toggleViewAction());
    viewMenu_->addAction(constraintDock_->toggleViewAction());
    viewMenu_->addAction(resultsDock_->toggleViewAction());
}

void MainWindow::setupConnections() {
    // File menu
    connect(newAction_, &QAction::triggered, this, &MainWindow::newPortfolio);
    connect(openAction_, &QAction::triggered, this, &MainWindow::openPortfolio);
    connect(saveAction_, &QAction::triggered, this, &MainWindow::savePortfolio);
    connect(saveAsAction_, &QAction::triggered, this, &MainWindow::savePortfolioAs);
    connect(importAction_, &QAction::triggered, this, &MainWindow::importCSV);
    connect(exportAction_, &QAction::triggered, this, &MainWindow::exportResults);
    connect(exitAction_, &QAction::triggered, this, &QMainWindow::close);

    // Edit menu
    connect(addPositionAction_, &QAction::triggered, this, &MainWindow::addPosition);
    connect(editPositionAction_, &QAction::triggered, this, &MainWindow::editPosition);
    connect(removePositionAction_, &QAction::triggered, this, &MainWindow::removePosition);

    // View menu
    connect(darkModeAction_, &QAction::triggered, this, &MainWindow::toggleDarkMode);
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::showSettings);
    connect(resetLayoutAction_, &QAction::triggered, this, &MainWindow::resetLayout);

    // Optimize menu
    connect(optimizeAction_, &QAction::triggered, this, &MainWindow::runOptimization);
    connect(cancelAction_, &QAction::triggered, this, &MainWindow::cancelOptimization);
    connect(costEstimateAction_, &QAction::triggered, this, &MainWindow::showCostEstimate);

    // Help menu
    connect(aboutAction_, &QAction::triggered, this, &MainWindow::showAbout);
    connect(userGuideAction_, &QAction::triggered, this, &MainWindow::showUserGuide);

    // State connections
    connect(state_, &ApplicationState::portfolioChanged,
            this, &MainWindow::onPortfolioChanged);
    connect(state_, &ApplicationState::targetChanged,
            this, &MainWindow::onTargetChanged);
    connect(state_, &ApplicationState::constraintsChanged,
            this, &MainWindow::onConstraintsChanged);
    connect(state_, &ApplicationState::unsavedChangesChanged,
            this, &MainWindow::onUnsavedChangesChanged);
    connect(state_, &ApplicationState::optimizingChanged,
            this, &MainWindow::onOptimizingChanged);
    connect(state_, &ApplicationState::darkModeChanged,
            this, &MainWindow::onDarkModeChanged);
    connect(state_, &ApplicationState::resultAvailable,
            this, &MainWindow::onResultAvailable);

    // Portfolio table selection
    connect(portfolioTable_, &PortfolioTableView::selectionChanged,
            this, [this](bool hasSelection) {
        editPositionAction_->setEnabled(hasSelection);
        removePositionAction_->setEnabled(hasSelection);
    });

    connect(portfolioTable_, &PortfolioTableView::positionDoubleClicked,
            this, &MainWindow::editPosition);

    // Progress widget
    connect(progressWidget_, &ProgressWidget::cancelRequested,
            this, &MainWindow::cancelOptimization);
}

void MainWindow::loadSettings() {
    QSettings settings;

    settings.beginGroup("MainWindow");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("state").toByteArray());
    settings.endGroup();

    settings.beginGroup("RecentFiles");
    recentFiles_ = settings.value("files").toStringList();
    settings.endGroup();

    updateRecentFilesMenu();
}

void MainWindow::saveSettings() {
    QSettings settings;

    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("state", saveState());
    settings.endGroup();

    settings.beginGroup("RecentFiles");
    settings.setValue("files", recentFiles_);
    settings.endGroup();
}

void MainWindow::applyTheme(bool dark) {
    QString stylePath = dark ? ":/styles/dark.qss" : ":/styles/light.qss";
    QFile styleFile(stylePath);

    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString style = QString::fromUtf8(styleFile.readAll());
        qApp->setStyleSheet(style);
        styleFile.close();
    }
}

void MainWindow::updateWindowTitle() {
    QString title = "Lumen";

    if (!state_->currentFilePath().isEmpty()) {
        QFileInfo fi(state_->currentFilePath());
        title = fi.fileName() + " - Lumen";
    } else if (state_->hasPortfolio()) {
        title = state_->portfolio().getName().c_str();
        title += " - Lumen";
    }

    if (state_->hasUnsavedChanges()) {
        title = "* " + title;
    }

    setWindowTitle(title);
}

void MainWindow::updateRecentFilesMenu() {
    recentFilesMenu_->clear();

    if (recentFiles_.isEmpty()) {
        QAction* noFiles = recentFilesMenu_->addAction(tr("No recent files"));
        noFiles->setEnabled(false);
        return;
    }

    for (const QString& filePath : recentFiles_) {
        QFileInfo fi(filePath);
        QAction* action = recentFilesMenu_->addAction(fi.fileName());
        action->setData(filePath);
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, &MainWindow::recentFileTriggered);
    }

    recentFilesMenu_->addSeparator();
    QAction* clearAction = recentFilesMenu_->addAction(tr("Clear Recent Files"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        recentFiles_.clear();
        updateRecentFilesMenu();
    });
}

void MainWindow::addToRecentFiles(const QString& filePath) {
    recentFiles_.removeAll(filePath);
    recentFiles_.prepend(filePath);

    while (recentFiles_.size() > MaxRecentFiles) {
        recentFiles_.removeLast();
    }

    updateRecentFilesMenu();
}

bool MainWindow::maybeSave() {
    if (!state_->hasUnsavedChanges()) {
        return true;
    }

    QMessageBox::StandardButton ret = QMessageBox::warning(
        this, tr("Unsaved Changes"),
        tr("The portfolio has been modified.\nDo you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (ret) {
        case QMessageBox::Save:
            savePortfolio();
            return !state_->hasUnsavedChanges();
        case QMessageBox::Discard:
            return true;
        case QMessageBox::Cancel:
        default:
            return false;
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (state_->isOptimizing()) {
        QMessageBox::StandardButton ret = QMessageBox::question(
            this, tr("Optimization Running"),
            tr("An optimization is currently running. Cancel it and exit?"),
            QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::No) {
            event->ignore();
            return;
        }
        cancelOptimization();
    }

    if (maybeSave()) {
        saveSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString path = urls.first().toLocalFile();
            if (path.endsWith(".lumen", Qt::CaseInsensitive) ||
                path.endsWith(".json", Qt::CaseInsensitive) ||
                path.endsWith(".csv", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString path = urls.first().toLocalFile();

        if (path.endsWith(".csv", Qt::CaseInsensitive)) {
            // Import CSV
            if (maybeSave()) {
                loadPortfolioFromFile(path);
            }
        } else {
            // Load portfolio file
            if (maybeSave()) {
                loadPortfolioFromFile(path);
            }
        }
    }
}

void MainWindow::loadPortfolioFromFile(const QString& filePath) {
    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        // Import CSV file
        try {
            auto portfolio = lumen::core::Portfolio::fromCSV(filePath.toStdString());
            state_->setPortfolio(portfolio);
            statusLabel_->setText(tr("Imported %1 positions from CSV")
                                      .arg(portfolio.getPositionCount()));
        } catch (const std::exception& e) {
            QMessageBox::critical(this, tr("Import Error"),
                                  tr("Failed to import CSV: %1").arg(e.what()));
        }
    } else {
        // Load .lumen or .json file
        if (state_->loadFromFile(filePath)) {
            addToRecentFiles(filePath);
            statusLabel_->setText(tr("Loaded portfolio from %1").arg(filePath));
        }
    }
}

// File menu slots
void MainWindow::newPortfolio() {
    if (!maybeSave()) return;

    state_->clearPortfolio();
    statusLabel_->setText(tr("Created new portfolio"));
}

void MainWindow::openPortfolio() {
    if (!maybeSave()) return;

    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open Portfolio"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Lumen Files (*.lumen *.json);;All Files (*)"));

    if (!filePath.isEmpty()) {
        loadPortfolioFromFile(filePath);
    }
}

void MainWindow::importCSV() {
    ImportDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        auto portfolio = dialog.importedPortfolio();
        if (portfolio.getPositionCount() > 0) {
            state_->setPortfolio(portfolio);
            statusLabel_->setText(tr("Imported %1 positions")
                                      .arg(portfolio.getPositionCount()));
        }
    }
}

void MainWindow::savePortfolio() {
    if (state_->currentFilePath().isEmpty()) {
        savePortfolioAs();
    } else {
        if (state_->saveToFile(state_->currentFilePath())) {
            statusLabel_->setText(tr("Saved to %1").arg(state_->currentFilePath()));
        }
    }
}

void MainWindow::savePortfolioAs() {
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Portfolio"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Lumen Files (*.lumen);;JSON Files (*.json)"));

    if (!filePath.isEmpty()) {
        // Add extension if not present
        if (!filePath.endsWith(".lumen", Qt::CaseInsensitive) &&
            !filePath.endsWith(".json", Qt::CaseInsensitive)) {
            filePath += ".lumen";
        }

        if (state_->saveToFile(filePath)) {
            addToRecentFiles(filePath);
            statusLabel_->setText(tr("Saved to %1").arg(filePath));
        }
    }
}

void MainWindow::exportResults() {
    if (!state_->hasResult()) return;

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export Results"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("CSV Files (*.csv);;JSON Files (*.json)"));

    if (!filePath.isEmpty()) {
        resultsPanel_->exportToFile(filePath);
        statusLabel_->setText(tr("Exported results to %1").arg(filePath));
    }
}

void MainWindow::recentFileTriggered() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action && maybeSave()) {
        loadPortfolioFromFile(action->data().toString());
    }
}

// Edit menu slots
void MainWindow::addPosition() {
    PositionDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        auto position = dialog.position();
        state_->portfolio().addPosition(position);
        state_->markUnsaved();
        emit state_->portfolioChanged();
        statusLabel_->setText(tr("Added position: %1").arg(position.ticker.c_str()));
    }
}

void MainWindow::removePosition() {
    QString ticker = portfolioTable_->selectedTicker();
    if (ticker.isEmpty()) return;

    QMessageBox::StandardButton ret = QMessageBox::question(
        this, tr("Remove Position"),
        tr("Are you sure you want to remove %1?").arg(ticker),
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        state_->portfolio().removePosition(ticker.toStdString());
        state_->markUnsaved();
        emit state_->portfolioChanged();
        statusLabel_->setText(tr("Removed position: %1").arg(ticker));
    }
}

void MainWindow::editPosition() {
    QString ticker = portfolioTable_->selectedTicker();
    if (ticker.isEmpty()) return;

    try {
        auto position = state_->portfolio().getPosition(ticker.toStdString());
        PositionDialog dialog(this);
        dialog.setPosition(position);

        if (dialog.exec() == QDialog::Accepted) {
            auto updatedPosition = dialog.position();

            // Remove old and add updated
            state_->portfolio().removePosition(ticker.toStdString());
            state_->portfolio().addPosition(updatedPosition);
            state_->markUnsaved();
            emit state_->portfolioChanged();
            statusLabel_->setText(tr("Updated position: %1")
                                      .arg(updatedPosition.ticker.c_str()));
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Failed to edit position: %1").arg(e.what()));
    }
}

// View menu slots
void MainWindow::toggleDarkMode() {
    state_->setDarkMode(darkModeAction_->isChecked());
}

void MainWindow::showSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        statusLabel_->setText(tr("Settings saved"));
    }
}

void MainWindow::resetLayout() {
    // Reset dock widgets to default positions
    removeDockWidget(targetDock_);
    removeDockWidget(constraintDock_);
    removeDockWidget(resultsDock_);

    addDockWidget(Qt::RightDockWidgetArea, targetDock_);
    addDockWidget(Qt::RightDockWidgetArea, constraintDock_);
    addDockWidget(Qt::BottomDockWidgetArea, resultsDock_);

    targetDock_->show();
    constraintDock_->show();
    resultsDock_->show();

    // Reset splitter
    centralSplitter_->setSizes({width() * 2 / 3, width() / 3});

    statusLabel_->setText(tr("Layout reset to default"));
}

// Optimization menu slots
void MainWindow::runOptimization() {
    if (state_->isOptimizing()) return;
    if (!state_->hasPortfolio()) {
        QMessageBox::information(this, tr("No Portfolio"),
                                 tr("Please create or open a portfolio first."));
        return;
    }

    // Validate target allocation
    if (!state_->targetAllocation().validate()) {
        auto errors = state_->targetAllocation().getValidationErrors();
        QString errorMsg = tr("Target allocation is invalid:\n");
        for (const auto& error : errors) {
            errorMsg += QString::fromStdString(error) + "\n";
        }
        QMessageBox::warning(this, tr("Invalid Target"), errorMsg);
        return;
    }

    state_->setOptimizing(true);
    state_->clearResult();

    // Create worker thread if needed
    if (!workerThread_) {
        workerThread_ = new QThread(this);
        worker_ = new OptimizationWorker();
        worker_->moveToThread(workerThread_);

        connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
        connect(worker_, &OptimizationWorker::progress,
                this, &MainWindow::onOptimizationProgress);
        connect(worker_, &OptimizationWorker::finished,
                this, [this](const lumen::core::SolverResult& result) {
            state_->setLastResult(result);
            state_->setOptimizing(false);
        });
        connect(worker_, &OptimizationWorker::error,
                this, &MainWindow::onOptimizationError);

        workerThread_->start();
    }

    // Configure and start optimization
    worker_->configure(state_->portfolio(), state_->targetAllocation(),
                       state_->constraints());
    QMetaObject::invokeMethod(worker_, "process", Qt::QueuedConnection);

    statusLabel_->setText(tr("Running optimization..."));
}

void MainWindow::cancelOptimization() {
    if (worker_ && state_->isOptimizing()) {
        worker_->cancel();
        statusLabel_->setText(tr("Cancelling optimization..."));
    }
}

void MainWindow::showCostEstimate() {
    // TODO: Implement cost estimate dialog
    QMessageBox::information(this, tr("Cost Estimate"),
                             tr("Cost estimation not yet implemented."));
}

// Help menu slots
void MainWindow::showAbout() {
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::showUserGuide() {
    QString docsPath = QApplication::applicationDirPath() + "/../docs/USER_GUIDE.md";
    if (QFile::exists(docsPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(docsPath));
    } else {
        QDesktopServices::openUrl(QUrl("https://github.com/oaqlabs/lumen/docs/USER_GUIDE.md"));
    }
}

// State update slots
void MainWindow::onPortfolioChanged() {
    updateWindowTitle();

    bool hasPortfolio = state_->hasPortfolio();
    optimizeAction_->setEnabled(hasPortfolio);
    costEstimateAction_->setEnabled(hasPortfolio);

    // Update portfolio value in status bar
    if (hasPortfolio) {
        double value = state_->portfolio().getTotalValue();
        portfolioValueLabel_->setText(tr("Portfolio Value: $%1")
                                          .arg(value, 0, 'f', 2));
    } else {
        portfolioValueLabel_->clear();
    }

    // Update allocation chart
    allocationChart_->setPortfolio(state_->portfolio());
}

void MainWindow::onTargetChanged() {
    allocationChart_->setTargetAllocation(state_->targetAllocation());
}

void MainWindow::onConstraintsChanged() {
    // Constraints panel updates itself via signal
}

void MainWindow::onUnsavedChangesChanged(bool /* unsaved */) {
    updateWindowTitle();
}

void MainWindow::onOptimizingChanged(bool optimizing) {
    optimizeAction_->setEnabled(!optimizing && state_->hasPortfolio());
    cancelAction_->setEnabled(optimizing);

    if (optimizing) {
        progressWidget_->start();
        progressWidget_->show();
    } else {
        progressWidget_->stop();
        progressWidget_->hide();
    }
}

void MainWindow::onDarkModeChanged(bool dark) {
    darkModeAction_->setChecked(dark);
    applyTheme(dark);
}

void MainWindow::onResultAvailable(const lumen::core::SolverResult& result) {
    exportAction_->setEnabled(true);

    if (result.success) {
        statusLabel_->setText(tr("Optimization complete - %1 trades recommended")
                                  .arg(result.trades.size()));
    } else {
        statusLabel_->setText(tr("Optimization failed: %1")
                                  .arg(result.status.c_str()));
    }
}

void MainWindow::onOptimizationProgress(int percent, const QString& status) {
    progressWidget_->setProgress(percent, status);
}

void MainWindow::onOptimizationError(const QString& message) {
    state_->setOptimizing(false);
    QMessageBox::critical(this, tr("Optimization Error"), message);
    statusLabel_->setText(tr("Optimization failed"));
}
