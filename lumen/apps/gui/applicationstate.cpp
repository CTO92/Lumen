/// @file applicationstate.cpp
/// @brief Implementation of centralized application state

#include "applicationstate.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

ApplicationState::ApplicationState(QObject* parent)
    : QObject(parent)
    , portfolio_("Untitled Portfolio")
    , target_(lumen::core::AllocationMode::PERCENTAGE)
{
}

ApplicationState::~ApplicationState() = default;

lumen::core::Portfolio& ApplicationState::portfolio() {
    return portfolio_;
}

const lumen::core::Portfolio& ApplicationState::portfolio() const {
    return portfolio_;
}

void ApplicationState::setPortfolio(const lumen::core::Portfolio& portfolio) {
    portfolio_ = portfolio;
    markUnsaved();
    emit portfolioChanged();
}

void ApplicationState::clearPortfolio() {
    portfolio_ = lumen::core::Portfolio("Untitled Portfolio");
    target_ = lumen::core::TargetAllocation(lumen::core::AllocationMode::PERCENTAGE);
    constraints_ = lumen::core::ConstraintSet();
    clearResult();
    currentFilePath_.clear();
    unsavedChanges_ = false;
    emit portfolioChanged();
    emit targetChanged();
    emit constraintsChanged();
    emit unsavedChangesChanged(false);
}

lumen::core::TargetAllocation& ApplicationState::targetAllocation() {
    return target_;
}

const lumen::core::TargetAllocation& ApplicationState::targetAllocation() const {
    return target_;
}

void ApplicationState::setTargetAllocation(const lumen::core::TargetAllocation& target) {
    target_ = target;
    markUnsaved();
    emit targetChanged();
}

lumen::core::ConstraintSet& ApplicationState::constraints() {
    return constraints_;
}

const lumen::core::ConstraintSet& ApplicationState::constraints() const {
    return constraints_;
}

void ApplicationState::setConstraints(const lumen::core::ConstraintSet& constraints) {
    constraints_ = constraints;
    markUnsaved();
    emit constraintsChanged();
}

const lumen::core::SolverResult& ApplicationState::lastResult() const {
    return lastResult_;
}

void ApplicationState::setLastResult(const lumen::core::SolverResult& result) {
    lastResult_ = result;
    hasResult_ = true;
    emit resultAvailable(result);
}

bool ApplicationState::hasResult() const {
    return hasResult_;
}

void ApplicationState::clearResult() {
    lastResult_ = lumen::core::SolverResult();
    hasResult_ = false;
}

bool ApplicationState::hasPortfolio() const {
    return portfolio_.getPositionCount() > 0;
}

void ApplicationState::setDarkMode(bool dark) {
    if (darkMode_ != dark) {
        darkMode_ = dark;
        emit darkModeChanged(dark);
    }
}

void ApplicationState::markUnsaved() {
    if (!unsavedChanges_) {
        unsavedChanges_ = true;
        emit unsavedChangesChanged(true);
    }
}

void ApplicationState::markSaved(const QString& filePath) {
    currentFilePath_ = filePath;
    if (unsavedChanges_) {
        unsavedChanges_ = false;
        emit unsavedChangesChanged(false);
    }
}

void ApplicationState::setOptimizing(bool optimizing) {
    if (optimizing_ != optimizing) {
        optimizing_ = optimizing;
        emit optimizingChanged(optimizing);
    }
}

bool ApplicationState::saveToFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred(tr("Failed to open file for writing: %1").arg(filePath));
        return false;
    }

    try {
        QJsonObject root;

        // Save portfolio
        auto portfolioJson = portfolio_.toJSON();
        root["portfolio"] = QJsonDocument::fromJson(
            QString::fromStdString(portfolioJson.dump()).toUtf8()).object();

        // Save target allocation
        auto targetJson = target_.toJSON();
        root["target"] = QJsonDocument::fromJson(
            QString::fromStdString(targetJson.dump()).toUtf8()).object();

        // Save constraints
        auto constraintsJson = constraints_.toJSON();
        root["constraints"] = QJsonDocument::fromJson(
            QString::fromStdString(constraintsJson.dump()).toUtf8()).object();

        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        markSaved(filePath);
        return true;

    } catch (const std::exception& e) {
        emit errorOccurred(tr("Failed to save file: %1").arg(e.what()));
        return false;
    }
}

bool ApplicationState::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Failed to open file: %1").arg(filePath));
        return false;
    }

    try {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            emit errorOccurred(tr("Invalid JSON file format"));
            return false;
        }

        QJsonObject root = doc.object();

        // Load portfolio
        if (root.contains("portfolio")) {
            QJsonDocument portfolioDoc(root["portfolio"].toObject());
            auto portfolioJson = nlohmann::json::parse(
                portfolioDoc.toJson(QJsonDocument::Compact).toStdString());
            portfolio_ = lumen::core::Portfolio::fromJSON(portfolioJson);
        }

        // Load target allocation
        if (root.contains("target")) {
            QJsonDocument targetDoc(root["target"].toObject());
            auto targetJson = nlohmann::json::parse(
                targetDoc.toJson(QJsonDocument::Compact).toStdString());
            target_ = lumen::core::TargetAllocation::fromJSON(targetJson);
        }

        // Load constraints
        if (root.contains("constraints")) {
            QJsonDocument constraintsDoc(root["constraints"].toObject());
            auto constraintsJson = nlohmann::json::parse(
                constraintsDoc.toJson(QJsonDocument::Compact).toStdString());
            constraints_ = lumen::core::ConstraintSet::fromJSON(constraintsJson);
        }

        clearResult();
        currentFilePath_ = filePath;
        unsavedChanges_ = false;

        emit portfolioChanged();
        emit targetChanged();
        emit constraintsChanged();
        emit unsavedChangesChanged(false);

        return true;

    } catch (const std::exception& e) {
        emit errorOccurred(tr("Failed to load file: %1").arg(e.what()));
        return false;
    }
}
