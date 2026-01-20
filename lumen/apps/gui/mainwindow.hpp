#pragma once

/// @file mainwindow.hpp
/// @brief Main application window

#include <QMainWindow>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTimer>

class ApplicationState;
class PortfolioTableView;
class AllocationChartView;
class TargetEditor;
class ConstraintPanel;
class ResultsPanel;
class ProgressWidget;
class OptimizationWorker;

/// Main application window for Lumen GUI
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(ApplicationState* state, QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Load a portfolio from file (called from command line or drag-drop)
    void loadPortfolioFromFile(const QString& filePath);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // File menu actions
    void newPortfolio();
    void openPortfolio();
    void importCSV();
    void savePortfolio();
    void savePortfolioAs();
    void exportResults();
    void recentFileTriggered();

    // Edit menu actions
    void addPosition();
    void removePosition();
    void editPosition();

    // View menu actions
    void toggleDarkMode();
    void showSettings();
    void resetLayout();

    // Optimization menu actions
    void runOptimization();
    void cancelOptimization();
    void showCostEstimate();

    // Help menu actions
    void showAbout();
    void showUserGuide();

    // State update slots
    void onPortfolioChanged();
    void onTargetChanged();
    void onConstraintsChanged();
    void onUnsavedChangesChanged(bool unsaved);
    void onOptimizingChanged(bool optimizing);
    void onDarkModeChanged(bool dark);
    void onResultAvailable(const lumen::core::SolverResult& result);
    void onOptimizationProgress(int percent, const QString& status);
    void onOptimizationError(const QString& message);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void setupDockWidgets();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void applyTheme(bool dark);
    void updateWindowTitle();
    void updateRecentFilesMenu();
    void addToRecentFiles(const QString& filePath);
    bool maybeSave();

    ApplicationState* state_;

    // Central widget components
    QSplitter* centralSplitter_;
    PortfolioTableView* portfolioTable_;
    AllocationChartView* allocationChart_;

    // Dock widgets
    QDockWidget* targetDock_;
    QDockWidget* constraintDock_;
    QDockWidget* resultsDock_;

    TargetEditor* targetEditor_;
    ConstraintPanel* constraintPanel_;
    ResultsPanel* resultsPanel_;

    // Menus
    QMenu* fileMenu_;
    QMenu* recentFilesMenu_;
    QMenu* editMenu_;
    QMenu* viewMenu_;
    QMenu* optimizeMenu_;
    QMenu* helpMenu_;

    // Actions
    QAction* newAction_;
    QAction* openAction_;
    QAction* saveAction_;
    QAction* saveAsAction_;
    QAction* importAction_;
    QAction* exportAction_;
    QAction* exitAction_;

    QAction* addPositionAction_;
    QAction* removePositionAction_;
    QAction* editPositionAction_;

    QAction* darkModeAction_;
    QAction* settingsAction_;
    QAction* resetLayoutAction_;

    QAction* optimizeAction_;
    QAction* cancelAction_;
    QAction* costEstimateAction_;

    QAction* aboutAction_;
    QAction* userGuideAction_;

    // Toolbar
    QToolBar* mainToolBar_;

    // Status bar widgets
    QLabel* statusLabel_;
    QLabel* portfolioValueLabel_;
    ProgressWidget* progressWidget_;

    // Optimization worker
    QThread* workerThread_;
    OptimizationWorker* worker_;

    // Recent files
    QStringList recentFiles_;
    static constexpr int MaxRecentFiles = 10;
};
