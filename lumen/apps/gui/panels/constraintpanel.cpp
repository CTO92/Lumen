/// @file constraintpanel.cpp
/// @brief Constraint configuration panel implementation

#include "constraintpanel.hpp"
#include "../applicationstate.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>

ConstraintPanel::ConstraintPanel(ApplicationState* state, QWidget* parent)
    : QWidget(parent)
    , state_(state)
{
    setupUI();
    setupConnections();
    onStateConstraintsChanged();
}

void ConstraintPanel::setupUI() {
    auto* layout = new QVBoxLayout(this);

    // Preset constraints
    presetsGroup_ = new QGroupBox(tr("Common Constraints"), this);
    auto* presetsLayout = new QVBoxLayout(presetsGroup_);

    noShortSellingCheck_ = new QCheckBox(tr("No short selling"), presetsGroup_);
    noShortSellingCheck_->setToolTip(tr("All positions must be >= 0"));
    noShortSellingCheck_->setChecked(true);
    presetsLayout->addWidget(noShortSellingCheck_);

    respectBandsCheck_ = new QCheckBox(tr("Respect allocation bands"), presetsGroup_);
    respectBandsCheck_->setToolTip(tr("Keep positions within min/max bounds"));
    presetsLayout->addWidget(respectBandsCheck_);

    taxAwareCheck_ = new QCheckBox(tr("Tax-aware optimization"), presetsGroup_);
    taxAwareCheck_->setToolTip(tr("Optimize for tax efficiency"));
    presetsLayout->addWidget(taxAwareCheck_);

    auto* limitsLayout = new QFormLayout();

    minPositionSpin_ = new QDoubleSpinBox(presetsGroup_);
    minPositionSpin_->setRange(0.0, 100.0);
    minPositionSpin_->setDecimals(1);
    minPositionSpin_->setSuffix("%");
    minPositionSpin_->setValue(0.0);
    minPositionSpin_->setToolTip(tr("Minimum allocation for any position"));
    limitsLayout->addRow(tr("Min position:"), minPositionSpin_);

    maxPositionSpin_ = new QDoubleSpinBox(presetsGroup_);
    maxPositionSpin_->setRange(0.0, 100.0);
    maxPositionSpin_->setDecimals(1);
    maxPositionSpin_->setSuffix("%");
    maxPositionSpin_->setValue(100.0);
    maxPositionSpin_->setToolTip(tr("Maximum allocation for any position"));
    limitsLayout->addRow(tr("Max position:"), maxPositionSpin_);

    maxTurnoverSpin_ = new QDoubleSpinBox(presetsGroup_);
    maxTurnoverSpin_->setRange(0.0, 200.0);
    maxTurnoverSpin_->setDecimals(1);
    maxTurnoverSpin_->setSuffix("%");
    maxTurnoverSpin_->setValue(100.0);
    maxTurnoverSpin_->setToolTip(tr("Maximum portfolio turnover"));
    limitsLayout->addRow(tr("Max turnover:"), maxTurnoverSpin_);

    presetsLayout->addLayout(limitsLayout);
    layout->addWidget(presetsGroup_);

    // Custom constraints
    auto* customGroup = new QGroupBox(tr("Custom Constraints"), this);
    auto* customLayout = new QVBoxLayout(customGroup);

    constraintList_ = new QListWidget(customGroup);
    constraintList_->setAlternatingRowColors(true);
    customLayout->addWidget(constraintList_);

    auto* buttonLayout = new QHBoxLayout();
    addBtn_ = new QPushButton(tr("Add..."), customGroup);
    removeBtn_ = new QPushButton(tr("Remove"), customGroup);
    removeBtn_->setEnabled(false);
    buttonLayout->addWidget(addBtn_);
    buttonLayout->addWidget(removeBtn_);
    buttonLayout->addStretch();
    customLayout->addLayout(buttonLayout);

    layout->addWidget(customGroup);

    // Apply button
    applyBtn_ = new QPushButton(tr("Apply Constraints"), this);
    layout->addWidget(applyBtn_);

    layout->addStretch();
}

void ConstraintPanel::setupConnections() {
    connect(state_, &ApplicationState::constraintsChanged,
            this, &ConstraintPanel::onStateConstraintsChanged);

    // Preset changes
    connect(noShortSellingCheck_, &QCheckBox::toggled, this, &ConstraintPanel::onPresetChanged);
    connect(respectBandsCheck_, &QCheckBox::toggled, this, &ConstraintPanel::onPresetChanged);
    connect(taxAwareCheck_, &QCheckBox::toggled, this, &ConstraintPanel::onPresetChanged);
    connect(minPositionSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ConstraintPanel::onPresetChanged);
    connect(maxPositionSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ConstraintPanel::onPresetChanged);
    connect(maxTurnoverSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ConstraintPanel::onPresetChanged);

    // Custom constraint buttons
    connect(addBtn_, &QPushButton::clicked, this, &ConstraintPanel::addCustomConstraint);
    connect(removeBtn_, &QPushButton::clicked, this, &ConstraintPanel::removeConstraint);
    connect(constraintList_, &QListWidget::itemSelectionChanged, this, [this]() {
        removeBtn_->setEnabled(constraintList_->currentRow() >= 0);
    });

    connect(applyBtn_, &QPushButton::clicked, this, &ConstraintPanel::applyConstraints);
}

void ConstraintPanel::onStateConstraintsChanged() {
    if (updatingUI_) return;
    updateConstraintList();
}

void ConstraintPanel::onPresetChanged() {
    // Mark that constraints need to be applied
    applyBtn_->setEnabled(true);
}

void ConstraintPanel::updateConstraintList() {
    constraintList_->clear();

    const auto& constraints = state_->constraints();
    for (const auto& constraint : constraints.getAll()) {
        QString text;
        switch (constraint.type) {
            case lumen::core::ConstraintType::MIN_WEIGHT:
                text = tr("Min weight %1: %2%")
                    .arg(QString::fromStdString(constraint.target))
                    .arg(constraint.value * 100.0, 0, 'f', 1);
                break;
            case lumen::core::ConstraintType::MAX_WEIGHT:
                text = tr("Max weight %1: %2%")
                    .arg(QString::fromStdString(constraint.target))
                    .arg(constraint.value * 100.0, 0, 'f', 1);
                break;
            case lumen::core::ConstraintType::SECTOR_LIMIT:
                text = tr("Sector limit %1: %2%")
                    .arg(QString::fromStdString(constraint.target))
                    .arg(constraint.value * 100.0, 0, 'f', 1);
                break;
            case lumen::core::ConstraintType::MAX_TURNOVER:
                text = tr("Max turnover: %1%")
                    .arg(constraint.value * 100.0, 0, 'f', 1);
                break;
            case lumen::core::ConstraintType::MIN_CASH:
                text = tr("Min cash: $%1")
                    .arg(constraint.value, 0, 'f', 2);
                break;
            case lumen::core::ConstraintType::NO_SHORT_SELLING:
                text = tr("No short selling");
                break;
            default:
                text = tr("Custom constraint");
                break;
        }
        constraintList_->addItem(text);
    }
}

void ConstraintPanel::addCustomConstraint() {
    QStringList types = {
        tr("Minimum weight"),
        tr("Maximum weight"),
        tr("Sector limit"),
        tr("Minimum cash")
    };

    bool ok;
    QString typeStr = QInputDialog::getItem(this, tr("Add Constraint"),
                                             tr("Constraint type:"), types, 0, false, &ok);
    if (!ok) return;

    QString target;
    double value = 0.0;
    lumen::core::ConstraintType type;

    int typeIndex = types.indexOf(typeStr);

    if (typeIndex == 0 || typeIndex == 1) {
        // Min/Max weight - need ticker
        target = QInputDialog::getText(this, tr("Add Constraint"),
                                        tr("Ticker symbol:"), QLineEdit::Normal, "", &ok);
        if (!ok || target.isEmpty()) return;

        value = QInputDialog::getDouble(this, tr("Add Constraint"),
                                         tr("Weight (%):"), 10.0, 0.0, 100.0, 1, &ok);
        if (!ok) return;

        type = (typeIndex == 0) ? lumen::core::ConstraintType::MIN_WEIGHT
                                : lumen::core::ConstraintType::MAX_WEIGHT;
    } else if (typeIndex == 2) {
        // Sector limit
        target = QInputDialog::getText(this, tr("Add Constraint"),
                                        tr("Sector name:"), QLineEdit::Normal, "", &ok);
        if (!ok || target.isEmpty()) return;

        value = QInputDialog::getDouble(this, tr("Add Constraint"),
                                         tr("Limit (%):"), 30.0, 0.0, 100.0, 1, &ok);
        if (!ok) return;

        type = lumen::core::ConstraintType::SECTOR_LIMIT;
    } else {
        // Min cash
        value = QInputDialog::getDouble(this, tr("Add Constraint"),
                                         tr("Minimum cash ($):"), 1000.0, 0.0, 1000000.0, 2, &ok);
        if (!ok) return;

        type = lumen::core::ConstraintType::MIN_CASH;
    }

    lumen::core::Constraint constraint;
    constraint.type = type;
    constraint.target = target.toStdString();
    constraint.value = (type == lumen::core::ConstraintType::MIN_CASH) ? value : value / 100.0;

    state_->constraints().add(constraint);
    state_->markUnsaved();
    emit state_->constraintsChanged();
    emit constraintsChanged();
}

void ConstraintPanel::removeConstraint() {
    int row = constraintList_->currentRow();
    if (row < 0) return;

    const auto& all = state_->constraints().getAll();
    if (row < static_cast<int>(all.size())) {
        // Remove from state (need to rebuild constraint set)
        lumen::core::ConstraintSet newSet;
        for (size_t i = 0; i < all.size(); ++i) {
            if (static_cast<int>(i) != row) {
                newSet.add(all[i]);
            }
        }
        state_->setConstraints(newSet);
        emit constraintsChanged();
    }
}

void ConstraintPanel::applyConstraints() {
    lumen::core::ConstraintSet newConstraints;

    // Add preset constraints
    if (noShortSellingCheck_->isChecked()) {
        lumen::core::Constraint c;
        c.type = lumen::core::ConstraintType::NO_SHORT_SELLING;
        c.value = 0.0;
        newConstraints.add(c);
    }

    if (minPositionSpin_->value() > 0) {
        lumen::core::Constraint c;
        c.type = lumen::core::ConstraintType::MIN_WEIGHT;
        c.target = "*";  // All positions
        c.value = minPositionSpin_->value() / 100.0;
        newConstraints.add(c);
    }

    if (maxPositionSpin_->value() < 100) {
        lumen::core::Constraint c;
        c.type = lumen::core::ConstraintType::MAX_WEIGHT;
        c.target = "*";  // All positions
        c.value = maxPositionSpin_->value() / 100.0;
        newConstraints.add(c);
    }

    if (maxTurnoverSpin_->value() < 200) {
        lumen::core::Constraint c;
        c.type = lumen::core::ConstraintType::MAX_TURNOVER;
        c.value = maxTurnoverSpin_->value() / 100.0;
        newConstraints.add(c);
    }

    // Preserve custom constraints from the list
    // (they were already added via addCustomConstraint)

    state_->setConstraints(newConstraints);
    applyBtn_->setEnabled(false);
    emit constraintsChanged();
}
