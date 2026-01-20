/// @file importdialog.cpp
/// @brief CSV import dialog implementation

#include "importdialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QSpinBox>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QStandardPaths>

ImportDialog::ImportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Import CSV"));
    setMinimumSize(700, 500);
    setupUI();
}

void ImportDialog::setupUI() {
    auto* layout = new QVBoxLayout(this);

    // File selection
    auto* fileGroup = new QGroupBox(tr("File"), this);
    auto* fileLayout = new QHBoxLayout(fileGroup);

    filePathEdit_ = new QLineEdit(fileGroup);
    filePathEdit_->setReadOnly(true);
    filePathEdit_->setPlaceholderText(tr("Select a CSV file..."));

    auto* browseBtn = new QPushButton(tr("Browse..."), fileGroup);
    connect(browseBtn, &QPushButton::clicked, this, &ImportDialog::browseFile);

    fileLayout->addWidget(filePathEdit_, 1);
    fileLayout->addWidget(browseBtn);
    layout->addWidget(fileGroup);

    // CSV Options
    auto* optionsGroup = new QGroupBox(tr("CSV Options"), this);
    auto* optionsLayout = new QFormLayout(optionsGroup);

    delimiterCombo_ = new QComboBox(optionsGroup);
    delimiterCombo_->addItem(tr("Comma (,)"), ",");
    delimiterCombo_->addItem(tr("Semicolon (;)"), ";");
    delimiterCombo_->addItem(tr("Tab"), "\t");
    delimiterCombo_->addItem(tr("Pipe (|)"), "|");
    connect(delimiterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportDialog::parseCSV);
    optionsLayout->addRow(tr("Delimiter:"), delimiterCombo_);

    hasHeaderCheck_ = new QCheckBox(tr("First row contains headers"), optionsGroup);
    hasHeaderCheck_->setChecked(true);
    connect(hasHeaderCheck_, &QCheckBox::toggled, this, &ImportDialog::parseCSV);
    optionsLayout->addRow(hasHeaderCheck_);

    skipRowsSpin_ = new QSpinBox(optionsGroup);
    skipRowsSpin_->setRange(0, 100);
    skipRowsSpin_->setValue(0);
    connect(skipRowsSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ImportDialog::parseCSV);
    optionsLayout->addRow(tr("Skip rows:"), skipRowsSpin_);

    layout->addWidget(optionsGroup);

    // Column mapping
    auto* mappingGroup = new QGroupBox(tr("Column Mapping"), this);
    auto* mappingLayout = new QFormLayout(mappingGroup);

    tickerColumnCombo_ = new QComboBox(mappingGroup);
    connect(tickerColumnCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportDialog::previewData);
    mappingLayout->addRow(tr("Ticker:"), tickerColumnCombo_);

    sharesColumnCombo_ = new QComboBox(mappingGroup);
    connect(sharesColumnCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportDialog::previewData);
    mappingLayout->addRow(tr("Shares:"), sharesColumnCombo_);

    priceColumnCombo_ = new QComboBox(mappingGroup);
    connect(priceColumnCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportDialog::previewData);
    mappingLayout->addRow(tr("Price:"), priceColumnCombo_);

    costBasisColumnCombo_ = new QComboBox(mappingGroup);
    connect(costBasisColumnCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportDialog::previewData);
    mappingLayout->addRow(tr("Cost Basis:"), costBasisColumnCombo_);

    assetClassColumnCombo_ = new QComboBox(mappingGroup);
    connect(assetClassColumnCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportDialog::previewData);
    mappingLayout->addRow(tr("Asset Class (optional):"), assetClassColumnCombo_);

    layout->addWidget(mappingGroup);

    // Preview
    auto* previewGroup = new QGroupBox(tr("Preview"), this);
    auto* previewLayout = new QVBoxLayout(previewGroup);

    previewTable_ = new QTableWidget(previewGroup);
    previewTable_->setAlternatingRowColors(true);
    previewTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    previewTable_->horizontalHeader()->setStretchLastSection(true);
    previewLayout->addWidget(previewTable_);

    statusLabel_ = new QLabel(previewGroup);
    previewLayout->addWidget(statusLabel_);

    layout->addWidget(previewGroup, 1);

    // Error label
    errorLabel_ = new QLabel(this);
    errorLabel_->setStyleSheet("color: red;");
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();
    layout->addWidget(errorLabel_);

    // Buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ImportDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

void ImportDialog::browseFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open CSV File"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("CSV Files (*.csv);;Text Files (*.txt);;All Files (*)"));

    if (!filePath.isEmpty()) {
        filePathEdit_->setText(filePath);
        detectDelimiter();
        parseCSV();
    }
}

void ImportDialog::detectDelimiter() {
    QFile file(filePathEdit_->text());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString firstLine = in.readLine();
    file.close();

    // Count occurrences of common delimiters
    int commaCount = firstLine.count(',');
    int semicolonCount = firstLine.count(';');
    int tabCount = firstLine.count('\t');
    int pipeCount = firstLine.count('|');

    int maxCount = commaCount;
    int selectedIndex = 0;

    if (semicolonCount > maxCount) { maxCount = semicolonCount; selectedIndex = 1; }
    if (tabCount > maxCount) { maxCount = tabCount; selectedIndex = 2; }
    if (pipeCount > maxCount) { selectedIndex = 3; }

    delimiterCombo_->setCurrentIndex(selectedIndex);
}

void ImportDialog::parseCSV() {
    rawData_.clear();
    headers_.clear();
    errorLabel_->hide();

    QString filePath = filePathEdit_->text();
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showError(tr("Failed to open file: %1").arg(file.errorString()));
        return;
    }

    QString delimiter = delimiterCombo_->currentData().toString();
    QTextStream in(&file);
    int skipRows = skipRowsSpin_->value();
    int rowNum = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        if (rowNum < skipRows) {
            rowNum++;
            continue;
        }

        // Simple CSV parsing (doesn't handle quoted fields with delimiters)
        QStringList fields = line.split(delimiter);

        std::vector<std::string> row;
        for (const QString& field : fields) {
            // Remove quotes if present
            QString cleaned = field.trimmed();
            if (cleaned.startsWith('"') && cleaned.endsWith('"')) {
                cleaned = cleaned.mid(1, cleaned.length() - 2);
            }
            row.push_back(cleaned.toStdString());
        }

        if (rowNum == skipRows && hasHeaderCheck_->isChecked()) {
            for (const auto& h : row) {
                headers_.push_back(h);
            }
        } else {
            rawData_.push_back(row);
        }

        rowNum++;
    }

    file.close();

    updateColumnMappings();
    autoMapColumns();
    previewData();
}

void ImportDialog::updateColumnMappings() {
    // Block signals during update
    tickerColumnCombo_->blockSignals(true);
    sharesColumnCombo_->blockSignals(true);
    priceColumnCombo_->blockSignals(true);
    costBasisColumnCombo_->blockSignals(true);
    assetClassColumnCombo_->blockSignals(true);

    // Clear and repopulate
    tickerColumnCombo_->clear();
    sharesColumnCombo_->clear();
    priceColumnCombo_->clear();
    costBasisColumnCombo_->clear();
    assetClassColumnCombo_->clear();

    assetClassColumnCombo_->addItem(tr("(None)"), -1);

    if (!headers_.empty()) {
        for (size_t i = 0; i < headers_.size(); ++i) {
            QString header = QString::fromStdString(headers_[i]);
            tickerColumnCombo_->addItem(header, static_cast<int>(i));
            sharesColumnCombo_->addItem(header, static_cast<int>(i));
            priceColumnCombo_->addItem(header, static_cast<int>(i));
            costBasisColumnCombo_->addItem(header, static_cast<int>(i));
            assetClassColumnCombo_->addItem(header, static_cast<int>(i));
        }
    } else if (!rawData_.empty()) {
        for (size_t i = 0; i < rawData_[0].size(); ++i) {
            QString label = tr("Column %1").arg(i + 1);
            tickerColumnCombo_->addItem(label, static_cast<int>(i));
            sharesColumnCombo_->addItem(label, static_cast<int>(i));
            priceColumnCombo_->addItem(label, static_cast<int>(i));
            costBasisColumnCombo_->addItem(label, static_cast<int>(i));
            assetClassColumnCombo_->addItem(label, static_cast<int>(i));
        }
    }

    tickerColumnCombo_->blockSignals(false);
    sharesColumnCombo_->blockSignals(false);
    priceColumnCombo_->blockSignals(false);
    costBasisColumnCombo_->blockSignals(false);
    assetClassColumnCombo_->blockSignals(false);
}

void ImportDialog::autoMapColumns() {
    if (headers_.empty()) return;

    // Common header names for auto-mapping
    QStringList tickerNames = {"ticker", "symbol", "stock", "security", "name"};
    QStringList sharesNames = {"shares", "quantity", "qty", "units", "amount"};
    QStringList priceNames = {"price", "current_price", "market_price", "last", "close"};
    QStringList costBasisNames = {"cost_basis", "costbasis", "cost", "avg_cost", "purchase_price"};
    QStringList assetClassNames = {"asset_class", "assetclass", "class", "type", "category"};

    for (size_t i = 0; i < headers_.size(); ++i) {
        QString header = QString::fromStdString(headers_[i]).toLower();

        for (const QString& name : tickerNames) {
            if (header.contains(name)) {
                tickerColumnCombo_->setCurrentIndex(static_cast<int>(i));
                break;
            }
        }

        for (const QString& name : sharesNames) {
            if (header.contains(name)) {
                sharesColumnCombo_->setCurrentIndex(static_cast<int>(i));
                break;
            }
        }

        for (const QString& name : priceNames) {
            if (header.contains(name)) {
                priceColumnCombo_->setCurrentIndex(static_cast<int>(i));
                break;
            }
        }

        for (const QString& name : costBasisNames) {
            if (header.contains(name)) {
                costBasisColumnCombo_->setCurrentIndex(static_cast<int>(i));
                break;
            }
        }

        for (const QString& name : assetClassNames) {
            if (header.contains(name)) {
                assetClassColumnCombo_->setCurrentIndex(static_cast<int>(i) + 1);  // +1 for "(None)"
                break;
            }
        }
    }
}

void ImportDialog::previewData() {
    previewTable_->clear();
    previewTable_->setRowCount(0);
    previewTable_->setColumnCount(0);

    if (rawData_.empty()) {
        statusLabel_->setText(tr("No data to preview"));
        return;
    }

    // Show up to 10 rows
    int previewRows = std::min(static_cast<int>(rawData_.size()), 10);

    previewTable_->setColumnCount(5);
    previewTable_->setHorizontalHeaderLabels({
        tr("Ticker"), tr("Shares"), tr("Price"), tr("Cost Basis"), tr("Asset Class")
    });
    previewTable_->setRowCount(previewRows);

    int tickerCol = tickerColumnCombo_->currentData().toInt();
    int sharesCol = sharesColumnCombo_->currentData().toInt();
    int priceCol = priceColumnCombo_->currentData().toInt();
    int costBasisCol = costBasisColumnCombo_->currentData().toInt();
    int assetClassCol = assetClassColumnCombo_->currentData().toInt();

    for (int row = 0; row < previewRows; ++row) {
        const auto& dataRow = rawData_[row];

        auto getField = [&dataRow](int col) -> QString {
            if (col >= 0 && col < static_cast<int>(dataRow.size())) {
                return QString::fromStdString(dataRow[col]);
            }
            return "";
        };

        previewTable_->setItem(row, 0, new QTableWidgetItem(getField(tickerCol)));
        previewTable_->setItem(row, 1, new QTableWidgetItem(getField(sharesCol)));
        previewTable_->setItem(row, 2, new QTableWidgetItem(getField(priceCol)));
        previewTable_->setItem(row, 3, new QTableWidgetItem(getField(costBasisCol)));
        previewTable_->setItem(row, 4, new QTableWidgetItem(getField(assetClassCol)));
    }

    statusLabel_->setText(tr("Found %1 rows").arg(rawData_.size()));

    // Enable OK button if we have data
    if (auto* buttonBox = findChild<QDialogButtonBox*>()) {
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!rawData_.empty());
    }
}

void ImportDialog::showError(const QString& message) {
    errorLabel_->setText(message);
    errorLabel_->show();
}

lumen::core::Portfolio ImportDialog::importedPortfolio() const {
    return importedPortfolio_;
}

void ImportDialog::accept() {
    importedPortfolio_ = lumen::core::Portfolio("Imported Portfolio");

    int tickerCol = tickerColumnCombo_->currentData().toInt();
    int sharesCol = sharesColumnCombo_->currentData().toInt();
    int priceCol = priceColumnCombo_->currentData().toInt();
    int costBasisCol = costBasisColumnCombo_->currentData().toInt();
    int assetClassCol = assetClassColumnCombo_->currentData().toInt();

    int successCount = 0;
    int errorCount = 0;

    for (const auto& row : rawData_) {
        try {
            auto getField = [&row](int col) -> std::string {
                if (col >= 0 && col < static_cast<int>(row.size())) {
                    return row[col];
                }
                return "";
            };

            std::string ticker = getField(tickerCol);
            if (ticker.empty()) continue;

            // Clean up numeric values (remove currency symbols, commas)
            auto cleanNumber = [](const std::string& s) -> double {
                std::string cleaned;
                for (char c : s) {
                    if (std::isdigit(c) || c == '.' || c == '-') {
                        cleaned += c;
                    }
                }
                return cleaned.empty() ? 0.0 : std::stod(cleaned);
            };

            lumen::core::Position pos;
            pos.ticker = ticker;
            pos.shares = cleanNumber(getField(sharesCol));
            pos.current_price = cleanNumber(getField(priceCol));
            pos.cost_basis = cleanNumber(getField(costBasisCol));

            if (pos.cost_basis <= 0) {
                pos.cost_basis = pos.current_price;  // Default to current price
            }

            // Parse asset class if provided
            if (assetClassCol >= 0) {
                std::string acStr = getField(assetClassCol);
                pos.asset_class = lumen::core::assetClassFromString(acStr);
            } else {
                pos.asset_class = lumen::core::AssetClass::STOCKS;
            }

            pos.purchase_date = std::chrono::system_clock::now();
            pos.supports_fractional = true;

            if (pos.shares > 0 && pos.current_price > 0) {
                importedPortfolio_.addPosition(pos);
                successCount++;
            }

        } catch (...) {
            errorCount++;
        }
    }

    if (successCount == 0) {
        showError(tr("No valid positions could be imported."));
        return;
    }

    QDialog::accept();
}
