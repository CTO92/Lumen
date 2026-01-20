#pragma once

/// @file broker_import.hpp
/// @brief Broker-specific tax lot import functionality
///
/// This module provides parsers for importing tax lot data from various
/// broker export formats including Schwab, Fidelity, and Vanguard.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumen/data/tax_lot.hpp"

namespace lumen::data {

/// Supported broker formats
enum class BrokerFormat {
    GENERIC_CSV,    ///< Generic CSV with configurable columns
    SCHWAB,         ///< Charles Schwab cost basis export
    FIDELITY,       ///< Fidelity Investments tax lot export
    VANGUARD,       ///< Vanguard cost basis export
    AUTO_DETECT     ///< Automatically detect format from file contents
};

/// Convert BrokerFormat to string
std::string brokerFormatToString(BrokerFormat format);

/// Parse BrokerFormat from string
BrokerFormat brokerFormatFromString(const std::string& str);

/// Column mapping for generic CSV import
struct CSVColumnMapping {
    int ticker_column = 0;           ///< Column index for ticker symbol
    int shares_column = 1;           ///< Column index for share quantity
    int cost_basis_column = 2;       ///< Column index for cost basis per share
    int purchase_date_column = 3;    ///< Column index for purchase date
    int lot_id_column = -1;          ///< Column index for lot ID (-1 = auto-generate)
    int sale_date_column = -1;       ///< Column index for sale date (-1 = not present)
    int sale_price_column = -1;      ///< Column index for sale price (-1 = not present)

    bool has_header = true;          ///< Whether first row is header
    char delimiter = ',';            ///< Field delimiter
    std::string date_format = "%Y-%m-%d";  ///< Date format string

    nlohmann::json toJSON() const;
    static CSVColumnMapping fromJSON(const nlohmann::json& j);
};

/// Result of an import operation
struct ImportResult {
    bool success = false;
    std::vector<TaxLot> lots;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    int rows_processed = 0;
    int rows_imported = 0;
    int rows_skipped = 0;
    BrokerFormat detected_format = BrokerFormat::GENERIC_CSV;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Abstract base class for broker import parsers
class BrokerImporter {
public:
    virtual ~BrokerImporter() = default;

    /// Get the broker format this importer handles
    virtual BrokerFormat getFormat() const = 0;

    /// Get human-readable name of the broker
    virtual std::string getBrokerName() const = 0;

    /// Check if this importer can handle the given file
    virtual bool canHandle(const std::string& file_path) const = 0;

    /// Import tax lots from file
    virtual ImportResult importFromFile(const std::string& file_path) = 0;

    /// Import tax lots from string content
    virtual ImportResult importFromString(const std::string& content) = 0;
};

/// Generic CSV importer with configurable column mapping
class GenericCSVImporter : public BrokerImporter {
public:
    GenericCSVImporter();
    explicit GenericCSVImporter(const CSVColumnMapping& mapping);
    ~GenericCSVImporter() override;

    BrokerFormat getFormat() const override { return BrokerFormat::GENERIC_CSV; }
    std::string getBrokerName() const override { return "Generic CSV"; }
    bool canHandle(const std::string& file_path) const override;
    ImportResult importFromFile(const std::string& file_path) override;
    ImportResult importFromString(const std::string& content) override;

    /// Set column mapping
    void setColumnMapping(const CSVColumnMapping& mapping);

    /// Get current column mapping
    const CSVColumnMapping& getColumnMapping() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/// Charles Schwab cost basis importer
class SchwabImporter : public BrokerImporter {
public:
    SchwabImporter();
    ~SchwabImporter() override;

    BrokerFormat getFormat() const override { return BrokerFormat::SCHWAB; }
    std::string getBrokerName() const override { return "Charles Schwab"; }
    bool canHandle(const std::string& file_path) const override;
    ImportResult importFromFile(const std::string& file_path) override;
    ImportResult importFromString(const std::string& content) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/// Fidelity Investments tax lot importer
class FidelityImporter : public BrokerImporter {
public:
    FidelityImporter();
    ~FidelityImporter() override;

    BrokerFormat getFormat() const override { return BrokerFormat::FIDELITY; }
    std::string getBrokerName() const override { return "Fidelity Investments"; }
    bool canHandle(const std::string& file_path) const override;
    ImportResult importFromFile(const std::string& file_path) override;
    ImportResult importFromString(const std::string& content) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/// Vanguard cost basis importer
class VanguardImporter : public BrokerImporter {
public:
    VanguardImporter();
    ~VanguardImporter() override;

    BrokerFormat getFormat() const override { return BrokerFormat::VANGUARD; }
    std::string getBrokerName() const override { return "Vanguard"; }
    bool canHandle(const std::string& file_path) const override;
    ImportResult importFromFile(const std::string& file_path) override;
    ImportResult importFromString(const std::string& content) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/// Factory for creating broker importers
class BrokerImportFactory {
public:
    /// Create importer for specified format
    static std::unique_ptr<BrokerImporter> create(BrokerFormat format);

    /// Create importer by auto-detecting format from file
    static std::unique_ptr<BrokerImporter> createForFile(const std::string& file_path);

    /// Get all supported formats
    static std::vector<BrokerFormat> getSupportedFormats();

    /// Import from file with auto-detection
    static ImportResult importFile(const std::string& file_path,
                                   BrokerFormat format = BrokerFormat::AUTO_DETECT);

    /// Import from file and add to TaxLotManager
    static ImportResult importToManager(const std::string& file_path,
                                        TaxLotManager& manager,
                                        BrokerFormat format = BrokerFormat::AUTO_DETECT);
};

/// Export tax lots to CSV format
class TaxLotExporter {
public:
    /// Export lots to CSV string
    static std::string toCSV(const std::vector<TaxLot>& lots);

    /// Export lots to CSV file
    static bool toFile(const std::vector<TaxLot>& lots, const std::string& file_path);

    /// Export TaxLotManager contents to CSV file
    static bool exportManager(const TaxLotManager& manager, const std::string& file_path);
};

}  // namespace lumen::data
