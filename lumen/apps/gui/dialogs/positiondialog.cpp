/// @file positiondialog.cpp
/// @brief Dialog for adding/editing portfolio positions implementation

#include "positiondialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QMessageBox>

PositionDialog::PositionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Position"));
    setMinimumWidth(400);
    setupUI();
}

void PositionDialog::setupUI() {
    auto* layout = new QVBoxLayout(this);

    // Position details group
    auto* detailsGroup = new QGroupBox(tr("Position Details"), this);
    auto* formLayout = new QFormLayout(detailsGroup);

    tickerEdit_ = new QLineEdit(detailsGroup);
    tickerEdit_->setPlaceholderText(tr("e.g., AAPL, MSFT, VTI"));
    tickerEdit_->setMaxLength(20);
    connect(tickerEdit_, &QLineEdit::textChanged, this, &PositionDialog::validateTicker);
    formLayout->addRow(tr("Ticker:"), tickerEdit_);

    sharesSpin_ = new QDoubleSpinBox(detailsGroup);
    sharesSpin_->setRange(0.0001, 1000000000.0);
    sharesSpin_->setDecimals(4);
    sharesSpin_->setValue(0.0);
    connect(sharesSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PositionDialog::updateValue);
    formLayout->addRow(tr("Shares:"), sharesSpin_);

    priceSpin_ = new QDoubleSpinBox(detailsGroup);
    priceSpin_->setRange(0.0001, 1000000.0);
    priceSpin_->setDecimals(4);
    priceSpin_->setPrefix("$");
    priceSpin_->setValue(0.0);
    connect(priceSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PositionDialog::updateValue);
    formLayout->addRow(tr("Current Price:"), priceSpin_);

    costBasisSpin_ = new QDoubleSpinBox(detailsGroup);
    costBasisSpin_->setRange(0.0001, 1000000.0);
    costBasisSpin_->setDecimals(4);
    costBasisSpin_->setPrefix("$");
    costBasisSpin_->setValue(0.0);
    connect(costBasisSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PositionDialog::updateValue);
    formLayout->addRow(tr("Cost Basis (per share):"), costBasisSpin_);

    assetClassCombo_ = new QComboBox(detailsGroup);
    assetClassCombo_->addItem(tr("Stocks"), static_cast<int>(lumen::core::AssetClass::STOCKS));
    assetClassCombo_->addItem(tr("Bonds"), static_cast<int>(lumen::core::AssetClass::BONDS));
    assetClassCombo_->addItem(tr("Cash"), static_cast<int>(lumen::core::AssetClass::CASH));
    assetClassCombo_->addItem(tr("Commodities"), static_cast<int>(lumen::core::AssetClass::COMMODITIES));
    assetClassCombo_->addItem(tr("Real Estate"), static_cast<int>(lumen::core::AssetClass::REAL_ESTATE));
    assetClassCombo_->addItem(tr("Crypto"), static_cast<int>(lumen::core::AssetClass::CRYPTO));
    assetClassCombo_->addItem(tr("Other"), static_cast<int>(lumen::core::AssetClass::OTHER));
    formLayout->addRow(tr("Asset Class:"), assetClassCombo_);

    purchaseDateEdit_ = new QDateEdit(detailsGroup);
    purchaseDateEdit_->setCalendarPopup(true);
    purchaseDateEdit_->setDate(QDate::currentDate());
    purchaseDateEdit_->setMaximumDate(QDate::currentDate());
    formLayout->addRow(tr("Purchase Date:"), purchaseDateEdit_);

    fractionalCheck_ = new QCheckBox(tr("Supports fractional shares"), detailsGroup);
    fractionalCheck_->setChecked(true);
    formLayout->addRow(fractionalCheck_);

    layout->addWidget(detailsGroup);

    // Calculated values group
    auto* calcGroup = new QGroupBox(tr("Calculated Values"), this);
    auto* calcLayout = new QFormLayout(calcGroup);

    valueLabel_ = new QLabel("$0.00", calcGroup);
    valueLabel_->setStyleSheet("font-weight: bold;");
    calcLayout->addRow(tr("Market Value:"), valueLabel_);

    gainLabel_ = new QLabel("$0.00 (0.00%)", calcGroup);
    calcLayout->addRow(tr("Unrealized Gain/Loss:"), gainLabel_);

    layout->addWidget(calcGroup);

    // Error label
    errorLabel_ = new QLabel(this);
    errorLabel_->setStyleSheet("color: red;");
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();
    layout->addWidget(errorLabel_);

    // Buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PositionDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

void PositionDialog::setPosition(const lumen::core::Position& position) {
    isEditing_ = true;
    originalTicker_ = QString::fromStdString(position.ticker);
    setWindowTitle(tr("Edit Position - %1").arg(originalTicker_));

    tickerEdit_->setText(originalTicker_);
    tickerEdit_->setReadOnly(true);  // Can't change ticker when editing

    sharesSpin_->setValue(position.shares);
    priceSpin_->setValue(position.current_price);
    costBasisSpin_->setValue(position.cost_basis);

    int assetIndex = assetClassCombo_->findData(static_cast<int>(position.asset_class));
    if (assetIndex >= 0) {
        assetClassCombo_->setCurrentIndex(assetIndex);
    }

    // Convert time_point to QDate
    auto timeT = std::chrono::system_clock::to_time_t(position.purchase_date);
    std::tm* tm = std::localtime(&timeT);
    if (tm) {
        purchaseDateEdit_->setDate(QDate(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday));
    }

    fractionalCheck_->setChecked(position.supports_fractional);

    updateValue();
}

lumen::core::Position PositionDialog::position() const {
    lumen::core::Position pos;

    pos.ticker = tickerEdit_->text().toUpper().toStdString();
    pos.shares = sharesSpin_->value();
    pos.current_price = priceSpin_->value();
    pos.cost_basis = costBasisSpin_->value();
    pos.asset_class = static_cast<lumen::core::AssetClass>(
        assetClassCombo_->currentData().toInt());

    // Convert QDate to time_point
    QDate date = purchaseDateEdit_->date();
    std::tm tm = {};
    tm.tm_year = date.year() - 1900;
    tm.tm_mon = date.month() - 1;
    tm.tm_mday = date.day();
    tm.tm_hour = 12;  // Noon to avoid timezone issues
    auto timeT = std::mktime(&tm);
    pos.purchase_date = std::chrono::system_clock::from_time_t(timeT);

    pos.supports_fractional = fractionalCheck_->isChecked();

    return pos;
}

void PositionDialog::validateTicker() {
    QString ticker = tickerEdit_->text().toUpper();
    tickerEdit_->setText(ticker);

    // Basic validation - alphanumeric only
    bool valid = !ticker.isEmpty() && ticker.length() <= 10;
    for (const QChar& c : ticker) {
        if (!c.isLetterOrNumber() && c != '.' && c != '-') {
            valid = false;
            break;
        }
    }

    if (!valid && !ticker.isEmpty()) {
        errorLabel_->setText(tr("Invalid ticker symbol. Use letters, numbers, dots, or dashes."));
        errorLabel_->show();
    } else {
        errorLabel_->hide();
    }
}

void PositionDialog::updateValue() {
    double shares = sharesSpin_->value();
    double price = priceSpin_->value();
    double costBasis = costBasisSpin_->value();

    double value = shares * price;
    double totalCost = shares * costBasis;
    double gain = value - totalCost;
    double gainPercent = (totalCost > 0) ? (gain / totalCost) * 100.0 : 0.0;

    valueLabel_->setText(QString("$%1").arg(value, 0, 'f', 2));

    QString gainText = QString("$%1 (%2%)")
        .arg(gain, 0, 'f', 2)
        .arg(gainPercent, 0, 'f', 2);

    if (gain >= 0) {
        gainLabel_->setText(gainText);
        gainLabel_->setStyleSheet("color: green;");
    } else {
        gainLabel_->setText(gainText);
        gainLabel_->setStyleSheet("color: red;");
    }
}

bool PositionDialog::validate() {
    QString ticker = tickerEdit_->text().trimmed();

    if (ticker.isEmpty()) {
        errorLabel_->setText(tr("Ticker symbol is required."));
        errorLabel_->show();
        tickerEdit_->setFocus();
        return false;
    }

    if (sharesSpin_->value() <= 0) {
        errorLabel_->setText(tr("Number of shares must be greater than zero."));
        errorLabel_->show();
        sharesSpin_->setFocus();
        return false;
    }

    if (priceSpin_->value() <= 0) {
        errorLabel_->setText(tr("Price must be greater than zero."));
        errorLabel_->show();
        priceSpin_->setFocus();
        return false;
    }

    if (costBasisSpin_->value() <= 0) {
        errorLabel_->setText(tr("Cost basis must be greater than zero."));
        errorLabel_->show();
        costBasisSpin_->setFocus();
        return false;
    }

    errorLabel_->hide();
    return true;
}

void PositionDialog::accept() {
    if (validate()) {
        QDialog::accept();
    }
}
