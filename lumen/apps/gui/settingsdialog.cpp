/// @file settingsdialog.cpp
/// @brief Application settings dialog implementation

#include "settingsdialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(500, 450);

    auto* mainLayout = new QVBoxLayout(this);

    tabs_ = new QTabWidget(this);
    setupGeneralTab();
    setupOptimizationTab();
    setupQuantumTab();
    setupAppearanceTab();
    mainLayout->addWidget(tabs_);

    // Buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &SettingsDialog::resetToDefaults);
    mainLayout->addWidget(buttonBox);

    loadSettings();
}

void SettingsDialog::setupGeneralTab() {
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);

    // Data directory
    auto* dataGroup = new QGroupBox(tr("Data Storage"), tab);
    auto* dataLayout = new QHBoxLayout(dataGroup);
    dataDirectoryEdit_ = new QLineEdit(dataGroup);
    dataDirectoryEdit_->setReadOnly(true);
    auto* browseBtn = new QPushButton(tr("Browse..."), dataGroup);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::browseDataDir);
    dataLayout->addWidget(new QLabel(tr("Data Directory:"), dataGroup));
    dataLayout->addWidget(dataDirectoryEdit_, 1);
    dataLayout->addWidget(browseBtn);
    layout->addWidget(dataGroup);

    // Auto-save
    auto* saveGroup = new QGroupBox(tr("Auto-Save"), tab);
    auto* saveLayout = new QFormLayout(saveGroup);
    autoSaveCheck_ = new QCheckBox(tr("Enable auto-save"), saveGroup);
    saveLayout->addRow(autoSaveCheck_);
    autoSaveIntervalSpin_ = new QSpinBox(saveGroup);
    autoSaveIntervalSpin_->setRange(1, 60);
    autoSaveIntervalSpin_->setSuffix(tr(" minutes"));
    saveLayout->addRow(tr("Interval:"), autoSaveIntervalSpin_);
    layout->addWidget(saveGroup);

    // Confirmation
    auto* confirmGroup = new QGroupBox(tr("Confirmations"), tab);
    auto* confirmLayout = new QVBoxLayout(confirmGroup);
    confirmExitCheck_ = new QCheckBox(tr("Confirm before exiting with unsaved changes"), confirmGroup);
    confirmLayout->addWidget(confirmExitCheck_);
    layout->addWidget(confirmGroup);

    layout->addStretch();
    tabs_->addTab(tab, tr("General"));
}

void SettingsDialog::setupOptimizationTab() {
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);

    // Solver settings
    auto* solverGroup = new QGroupBox(tr("Solver"), tab);
    auto* solverLayout = new QFormLayout(solverGroup);

    defaultSolverCombo_ = new QComboBox(solverGroup);
    defaultSolverCombo_->addItem(tr("Classical (HiGHS)"), "classical");
    defaultSolverCombo_->addItem(tr("Hybrid (Classical + Quantum)"), "hybrid");
    defaultSolverCombo_->addItem(tr("Quantum Only"), "quantum");
    solverLayout->addRow(tr("Default Solver:"), defaultSolverCombo_);

    fallbackToClassicalCheck_ = new QCheckBox(tr("Fall back to classical if quantum fails"), solverGroup);
    solverLayout->addRow(fallbackToClassicalCheck_);

    layout->addWidget(solverGroup);

    // Timeouts
    auto* timeoutGroup = new QGroupBox(tr("Timeouts"), tab);
    auto* timeoutLayout = new QFormLayout(timeoutGroup);

    classicalTimeoutSpin_ = new QSpinBox(timeoutGroup);
    classicalTimeoutSpin_->setRange(5, 600);
    classicalTimeoutSpin_->setSuffix(tr(" seconds"));
    timeoutLayout->addRow(tr("Classical Solver:"), classicalTimeoutSpin_);

    quantumTimeoutSpin_ = new QSpinBox(timeoutGroup);
    quantumTimeoutSpin_->setRange(10, 600);
    quantumTimeoutSpin_->setSuffix(tr(" seconds"));
    timeoutLayout->addRow(tr("Quantum Solver:"), quantumTimeoutSpin_);

    layout->addWidget(timeoutGroup);

    // Cost control
    auto* costGroup = new QGroupBox(tr("Cost Control"), tab);
    auto* costLayout = new QFormLayout(costGroup);

    showCostEstimateCheck_ = new QCheckBox(tr("Show cost estimate before quantum optimization"), costGroup);
    costLayout->addRow(showCostEstimateCheck_);

    maxCostSpin_ = new QDoubleSpinBox(costGroup);
    maxCostSpin_->setRange(0.0, 100.0);
    maxCostSpin_->setDecimals(2);
    maxCostSpin_->setPrefix("$");
    maxCostSpin_->setSpecialValueText(tr("No limit"));
    costLayout->addRow(tr("Maximum cost per solve:"), maxCostSpin_);

    layout->addWidget(costGroup);

    layout->addStretch();
    tabs_->addTab(tab, tr("Optimization"));
}

void SettingsDialog::setupQuantumTab() {
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);

    // General quantum settings
    auto* generalGroup = new QGroupBox(tr("Quantum Computing"), tab);
    auto* generalLayout = new QFormLayout(generalGroup);

    quantumEnabledCheck_ = new QCheckBox(tr("Enable quantum optimization"), generalGroup);
    generalLayout->addRow(quantumEnabledCheck_);

    quantumProviderCombo_ = new QComboBox(generalGroup);
    quantumProviderCombo_->addItem("D-Wave", "dwave");
    quantumProviderCombo_->addItem("IBM Quantum", "ibm");
    generalLayout->addRow(tr("Default Provider:"), quantumProviderCombo_);

    useMockCheck_ = new QCheckBox(tr("Use mock/simulator mode (no API costs)"), generalGroup);
    generalLayout->addRow(useMockCheck_);

    layout->addWidget(generalGroup);

    // D-Wave settings
    auto* dwaveGroup = new QGroupBox(tr("D-Wave"), tab);
    auto* dwaveLayout = new QFormLayout(dwaveGroup);

    dwaveApiKeyEdit_ = new QLineEdit(dwaveGroup);
    dwaveApiKeyEdit_->setEchoMode(QLineEdit::Password);
    dwaveApiKeyEdit_->setPlaceholderText(tr("Enter API key or set DWAVE_API_KEY env var"));
    dwaveLayout->addRow(tr("API Key:"), dwaveApiKeyEdit_);

    dwaveSolverCombo_ = new QComboBox(dwaveGroup);
    dwaveSolverCombo_->addItem("hybrid_binary_quadratic_model_version2");
    dwaveSolverCombo_->addItem("Advantage_system6.4");
    dwaveSolverCombo_->setEditable(true);
    dwaveLayout->addRow(tr("Solver:"), dwaveSolverCombo_);

    dwaveNumReadsSpin_ = new QSpinBox(dwaveGroup);
    dwaveNumReadsSpin_->setRange(10, 10000);
    dwaveLayout->addRow(tr("Number of reads:"), dwaveNumReadsSpin_);

    dwaveUseHybridCheck_ = new QCheckBox(tr("Use hybrid solver"), dwaveGroup);
    dwaveLayout->addRow(dwaveUseHybridCheck_);

    auto* dwaveTestLayout = new QHBoxLayout();
    dwaveTestBtn_ = new QPushButton(tr("Test Connection"), dwaveGroup);
    connect(dwaveTestBtn_, &QPushButton::clicked, this, &SettingsDialog::testDWaveConnection);
    dwaveStatusLabel_ = new QLabel(dwaveGroup);
    dwaveTestLayout->addWidget(dwaveTestBtn_);
    dwaveTestLayout->addWidget(dwaveStatusLabel_, 1);
    dwaveLayout->addRow(dwaveTestLayout);

    layout->addWidget(dwaveGroup);

    // IBM Quantum settings
    auto* ibmGroup = new QGroupBox(tr("IBM Quantum"), tab);
    auto* ibmLayout = new QFormLayout(ibmGroup);

    ibmApiKeyEdit_ = new QLineEdit(ibmGroup);
    ibmApiKeyEdit_->setEchoMode(QLineEdit::Password);
    ibmApiKeyEdit_->setPlaceholderText(tr("Enter API key or set IBM_QUANTUM_API_KEY env var"));
    ibmLayout->addRow(tr("API Key:"), ibmApiKeyEdit_);

    ibmBackendCombo_ = new QComboBox(ibmGroup);
    ibmBackendCombo_->addItem("ibm_brisbane");
    ibmBackendCombo_->addItem("ibm_kyoto");
    ibmBackendCombo_->addItem("simulator_statevector");
    ibmBackendCombo_->setEditable(true);
    ibmLayout->addRow(tr("Backend:"), ibmBackendCombo_);

    ibmShotsSpin_ = new QSpinBox(ibmGroup);
    ibmShotsSpin_->setRange(100, 10000);
    ibmLayout->addRow(tr("Shots:"), ibmShotsSpin_);

    ibmQaoaDepthSpin_ = new QSpinBox(ibmGroup);
    ibmQaoaDepthSpin_->setRange(1, 10);
    ibmLayout->addRow(tr("QAOA depth (p):"), ibmQaoaDepthSpin_);

    ibmOptimizerCombo_ = new QComboBox(ibmGroup);
    ibmOptimizerCombo_->addItem("SPSA");
    ibmOptimizerCombo_->addItem("COBYLA");
    ibmLayout->addRow(tr("Optimizer:"), ibmOptimizerCombo_);

    auto* ibmTestLayout = new QHBoxLayout();
    ibmTestBtn_ = new QPushButton(tr("Test Connection"), ibmGroup);
    connect(ibmTestBtn_, &QPushButton::clicked, this, &SettingsDialog::testIBMConnection);
    ibmStatusLabel_ = new QLabel(ibmGroup);
    ibmTestLayout->addWidget(ibmTestBtn_);
    ibmTestLayout->addWidget(ibmStatusLabel_, 1);
    ibmLayout->addRow(ibmTestLayout);

    layout->addWidget(ibmGroup);

    layout->addStretch();
    tabs_->addTab(tab, tr("Quantum"));
}

void SettingsDialog::setupAppearanceTab() {
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);

    // Theme settings
    auto* themeGroup = new QGroupBox(tr("Theme"), tab);
    auto* themeLayout = new QFormLayout(themeGroup);

    themeCombo_ = new QComboBox(themeGroup);
    themeCombo_->addItem(tr("Light"), "light");
    themeCombo_->addItem(tr("Dark"), "dark");
    themeLayout->addRow(tr("Theme:"), themeCombo_);

    systemThemeCheck_ = new QCheckBox(tr("Follow system theme"), themeGroup);
    themeLayout->addRow(systemThemeCheck_);

    layout->addWidget(themeGroup);

    // Font settings
    auto* fontGroup = new QGroupBox(tr("Font"), tab);
    auto* fontLayout = new QFormLayout(fontGroup);

    fontSizeSpin_ = new QSpinBox(fontGroup);
    fontSizeSpin_->setRange(8, 24);
    fontSizeSpin_->setSuffix(" pt");
    fontLayout->addRow(tr("Font size:"), fontSizeSpin_);

    layout->addWidget(fontGroup);

    layout->addStretch();
    tabs_->addTab(tab, tr("Appearance"));
}

void SettingsDialog::loadSettings() {
    QSettings settings;

    settings.beginGroup("General");
    dataDirectoryEdit_->setText(settings.value("dataDirectory",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).toString());
    autoSaveCheck_->setChecked(settings.value("autoSave", true).toBool());
    autoSaveIntervalSpin_->setValue(settings.value("autoSaveInterval", 5).toInt());
    confirmExitCheck_->setChecked(settings.value("confirmExit", true).toBool());
    settings.endGroup();

    settings.beginGroup("Optimization");
    int solverIndex = defaultSolverCombo_->findData(
        settings.value("defaultSolver", "classical").toString());
    defaultSolverCombo_->setCurrentIndex(solverIndex >= 0 ? solverIndex : 0);
    fallbackToClassicalCheck_->setChecked(settings.value("fallbackToClassical", true).toBool());
    classicalTimeoutSpin_->setValue(settings.value("classicalTimeout", 30).toInt());
    quantumTimeoutSpin_->setValue(settings.value("quantumTimeout", 60).toInt());
    showCostEstimateCheck_->setChecked(settings.value("showCostEstimate", true).toBool());
    maxCostSpin_->setValue(settings.value("maxCost", 0.0).toDouble());
    settings.endGroup();

    settings.beginGroup("Quantum");
    quantumEnabledCheck_->setChecked(settings.value("enabled", false).toBool());
    int providerIndex = quantumProviderCombo_->findData(
        settings.value("provider", "dwave").toString());
    quantumProviderCombo_->setCurrentIndex(providerIndex >= 0 ? providerIndex : 0);
    useMockCheck_->setChecked(settings.value("useMock", true).toBool());
    settings.endGroup();

    settings.beginGroup("Quantum/DWave");
    dwaveApiKeyEdit_->setText(settings.value("apiKey").toString());
    dwaveSolverCombo_->setCurrentText(
        settings.value("solver", "hybrid_binary_quadratic_model_version2").toString());
    dwaveNumReadsSpin_->setValue(settings.value("numReads", 1000).toInt());
    dwaveUseHybridCheck_->setChecked(settings.value("useHybrid", true).toBool());
    settings.endGroup();

    settings.beginGroup("Quantum/IBM");
    ibmApiKeyEdit_->setText(settings.value("apiKey").toString());
    ibmBackendCombo_->setCurrentText(settings.value("backend", "ibm_brisbane").toString());
    ibmShotsSpin_->setValue(settings.value("shots", 1024).toInt());
    ibmQaoaDepthSpin_->setValue(settings.value("qaoaDepth", 2).toInt());
    ibmOptimizerCombo_->setCurrentText(settings.value("optimizer", "SPSA").toString());
    settings.endGroup();

    settings.beginGroup("Appearance");
    int themeIndex = themeCombo_->findData(settings.value("theme", "light").toString());
    themeCombo_->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    systemThemeCheck_->setChecked(settings.value("systemTheme", true).toBool());
    fontSizeSpin_->setValue(settings.value("fontSize", 10).toInt());
    settings.endGroup();
}

void SettingsDialog::saveSettings() {
    QSettings settings;

    settings.beginGroup("General");
    settings.setValue("dataDirectory", dataDirectoryEdit_->text());
    settings.setValue("autoSave", autoSaveCheck_->isChecked());
    settings.setValue("autoSaveInterval", autoSaveIntervalSpin_->value());
    settings.setValue("confirmExit", confirmExitCheck_->isChecked());
    settings.endGroup();

    settings.beginGroup("Optimization");
    settings.setValue("defaultSolver", defaultSolverCombo_->currentData().toString());
    settings.setValue("fallbackToClassical", fallbackToClassicalCheck_->isChecked());
    settings.setValue("classicalTimeout", classicalTimeoutSpin_->value());
    settings.setValue("quantumTimeout", quantumTimeoutSpin_->value());
    settings.setValue("showCostEstimate", showCostEstimateCheck_->isChecked());
    settings.setValue("maxCost", maxCostSpin_->value());
    settings.endGroup();

    settings.beginGroup("Quantum");
    settings.setValue("enabled", quantumEnabledCheck_->isChecked());
    settings.setValue("provider", quantumProviderCombo_->currentData().toString());
    settings.setValue("useMock", useMockCheck_->isChecked());
    settings.endGroup();

    settings.beginGroup("Quantum/DWave");
    if (!dwaveApiKeyEdit_->text().isEmpty()) {
        settings.setValue("apiKey", dwaveApiKeyEdit_->text());
    }
    settings.setValue("solver", dwaveSolverCombo_->currentText());
    settings.setValue("numReads", dwaveNumReadsSpin_->value());
    settings.setValue("useHybrid", dwaveUseHybridCheck_->isChecked());
    settings.endGroup();

    settings.beginGroup("Quantum/IBM");
    if (!ibmApiKeyEdit_->text().isEmpty()) {
        settings.setValue("apiKey", ibmApiKeyEdit_->text());
    }
    settings.setValue("backend", ibmBackendCombo_->currentText());
    settings.setValue("shots", ibmShotsSpin_->value());
    settings.setValue("qaoaDepth", ibmQaoaDepthSpin_->value());
    settings.setValue("optimizer", ibmOptimizerCombo_->currentText());
    settings.endGroup();

    settings.beginGroup("Appearance");
    settings.setValue("theme", themeCombo_->currentData().toString());
    settings.setValue("systemTheme", systemThemeCheck_->isChecked());
    settings.setValue("fontSize", fontSizeSpin_->value());
    settings.endGroup();
}

void SettingsDialog::accept() {
    saveSettings();
    QDialog::accept();
}

void SettingsDialog::browseDataDir() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Data Directory"),
        dataDirectoryEdit_->text(),
        QFileDialog::ShowDirsOnly);

    if (!dir.isEmpty()) {
        dataDirectoryEdit_->setText(dir);
    }
}

void SettingsDialog::testDWaveConnection() {
    dwaveStatusLabel_->setText(tr("Testing..."));
    dwaveTestBtn_->setEnabled(false);

    // TODO: Actually test D-Wave connection
    // For now, just simulate
    QTimer::singleShot(1000, this, [this]() {
        if (useMockCheck_->isChecked()) {
            dwaveStatusLabel_->setText(tr("Mock mode: OK"));
            dwaveStatusLabel_->setStyleSheet("color: green;");
        } else if (dwaveApiKeyEdit_->text().isEmpty()) {
            dwaveStatusLabel_->setText(tr("No API key configured"));
            dwaveStatusLabel_->setStyleSheet("color: orange;");
        } else {
            dwaveStatusLabel_->setText(tr("Connection test not implemented"));
            dwaveStatusLabel_->setStyleSheet("color: orange;");
        }
        dwaveTestBtn_->setEnabled(true);
    });
}

void SettingsDialog::testIBMConnection() {
    ibmStatusLabel_->setText(tr("Testing..."));
    ibmTestBtn_->setEnabled(false);

    // TODO: Actually test IBM Quantum connection
    // For now, just simulate
    QTimer::singleShot(1000, this, [this]() {
        if (useMockCheck_->isChecked()) {
            ibmStatusLabel_->setText(tr("Mock mode: OK"));
            ibmStatusLabel_->setStyleSheet("color: green;");
        } else if (ibmApiKeyEdit_->text().isEmpty()) {
            ibmStatusLabel_->setText(tr("No API key configured"));
            ibmStatusLabel_->setStyleSheet("color: orange;");
        } else {
            ibmStatusLabel_->setText(tr("Connection test not implemented"));
            ibmStatusLabel_->setStyleSheet("color: orange;");
        }
        ibmTestBtn_->setEnabled(true);
    });
}

void SettingsDialog::resetToDefaults() {
    QMessageBox::StandardButton ret = QMessageBox::question(
        this, tr("Reset Settings"),
        tr("Are you sure you want to reset all settings to defaults?"),
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        QSettings settings;
        settings.clear();
        loadSettings();
    }
}
