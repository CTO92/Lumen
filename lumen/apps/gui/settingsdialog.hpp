#pragma once

/// @file settingsdialog.hpp
/// @brief Application settings dialog

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

/// Settings dialog for configuring Lumen application
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void accept() override;
    void browseDataDir();
    void testDWaveConnection();
    void testIBMConnection();
    void resetToDefaults();

private:
    void setupGeneralTab();
    void setupOptimizationTab();
    void setupQuantumTab();
    void setupAppearanceTab();
    void loadSettings();
    void saveSettings();

    QTabWidget* tabs_;

    // General tab
    QLineEdit* dataDirectoryEdit_;
    QCheckBox* autoSaveCheck_;
    QSpinBox* autoSaveIntervalSpin_;
    QCheckBox* confirmExitCheck_;

    // Optimization tab
    QSpinBox* classicalTimeoutSpin_;
    QSpinBox* quantumTimeoutSpin_;
    QCheckBox* showCostEstimateCheck_;
    QDoubleSpinBox* maxCostSpin_;
    QComboBox* defaultSolverCombo_;
    QCheckBox* fallbackToClassicalCheck_;

    // Quantum tab
    QComboBox* quantumProviderCombo_;
    QCheckBox* quantumEnabledCheck_;
    QCheckBox* useMockCheck_;

    // D-Wave settings
    QLineEdit* dwaveApiKeyEdit_;
    QComboBox* dwaveSolverCombo_;
    QSpinBox* dwaveNumReadsSpin_;
    QCheckBox* dwaveUseHybridCheck_;
    QPushButton* dwaveTestBtn_;
    QLabel* dwaveStatusLabel_;

    // IBM Quantum settings
    QLineEdit* ibmApiKeyEdit_;
    QComboBox* ibmBackendCombo_;
    QSpinBox* ibmShotsSpin_;
    QSpinBox* ibmQaoaDepthSpin_;
    QComboBox* ibmOptimizerCombo_;
    QPushButton* ibmTestBtn_;
    QLabel* ibmStatusLabel_;

    // Appearance tab
    QComboBox* themeCombo_;
    QCheckBox* systemThemeCheck_;
    QSpinBox* fontSizeSpin_;
};
