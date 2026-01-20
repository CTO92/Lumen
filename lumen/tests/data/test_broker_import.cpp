/// @file test_broker_import.cpp
/// @brief Unit tests for BrokerImporter classes

#include <gtest/gtest.h>
#include "lumen/data/broker_import.hpp"

#include <filesystem>
#include <fstream>
#include <chrono>

using namespace lumen::data;
namespace fs = std::filesystem;

// =============================================================================
// Test Fixtures
// =============================================================================

class BrokerImportTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        test_dir_ = fs::temp_directory_path() / "lumen_test_broker_import";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        // Clean up test files
        fs::remove_all(test_dir_);
    }

    std::string createTestFile(const std::string& filename, const std::string& content) {
        auto filepath = test_dir_ / filename;
        std::ofstream file(filepath);
        file << content;
        file.close();
        return filepath.string();
    }

    fs::path test_dir_;
};

// =============================================================================
// GenericCSVImporter Tests
// =============================================================================

TEST_F(BrokerImportTest, GenericCSVImporterParsesValidCSV) {
    std::string csv_content = R"(ticker,shares,cost_basis,purchase_date
AAPL,100,12000.00,2023-01-15
GOOGL,25,62500.00,2023-03-10
MSFT,50,15000.00,2023-06-01)";

    auto filepath = createTestFile("generic.csv", csv_content);

    GenericCSVImporter importer;
    auto result = importer.importFromFile(filepath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.lots.size(), 3);

    // Check first lot
    bool found_aapl = false;
    for (const auto& lot : result.lots) {
        if (lot.ticker == "AAPL") {
            EXPECT_DOUBLE_EQ(lot.shares, 100.0);
            EXPECT_DOUBLE_EQ(lot.cost_basis_per_share, 120.0);  // 12000 / 100
            found_aapl = true;
        }
    }
    EXPECT_TRUE(found_aapl);
}

TEST_F(BrokerImportTest, GenericCSVImporterHandlesMissingFile) {
    GenericCSVImporter importer;
    auto result = importer.importFromFile("/nonexistent/path/file.csv");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(BrokerImportTest, GenericCSVImporterHandlesEmptyFile) {
    auto filepath = createTestFile("empty.csv", "");

    GenericCSVImporter importer;
    auto result = importer.importFromFile(filepath);

    EXPECT_FALSE(result.success);
}

TEST_F(BrokerImportTest, GenericCSVImporterHandlesHeaderOnly) {
    std::string csv_content = "ticker,shares,cost_basis,purchase_date\n";
    auto filepath = createTestFile("header_only.csv", csv_content);

    GenericCSVImporter importer;
    auto result = importer.importFromFile(filepath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.lots.size(), 0);
}

TEST_F(BrokerImportTest, GenericCSVImporterWithCustomMapping) {
    std::string csv_content = R"(Symbol,Quantity,TotalCost,Date
VTI,75,16500.00,2023-02-20)";

    auto filepath = createTestFile("custom.csv", csv_content);

    CSVColumnMapping mapping;
    mapping.ticker_column = "Symbol";
    mapping.shares_column = "Quantity";
    mapping.cost_basis_column = "TotalCost";
    mapping.date_column = "Date";

    GenericCSVImporter importer(mapping);
    auto result = importer.importFromFile(filepath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.lots.size(), 1);
    EXPECT_EQ(result.lots[0].ticker, "VTI");
    EXPECT_DOUBLE_EQ(result.lots[0].shares, 75.0);
}

// =============================================================================
// SchwabImporter Tests
// =============================================================================

TEST_F(BrokerImportTest, SchwabImporterParsesValidFormat) {
    // Schwab format typically has different column names
    std::string csv_content = R"("Symbol","Quantity","Cost Basis","Date Acquired","Description"
"AAPL","100","$12,000.00","01/15/2023","APPLE INC"
"VTI","50","$11,000.00","03/20/2023","VANGUARD TOTAL STOCK")";

    auto filepath = createTestFile("schwab.csv", csv_content);

    SchwabImporter importer;
    auto result = importer.importFromFile(filepath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.lots.size(), 2);

    bool found_aapl = false;
    for (const auto& lot : result.lots) {
        if (lot.ticker == "AAPL") {
            EXPECT_DOUBLE_EQ(lot.shares, 100.0);
            found_aapl = true;
        }
    }
    EXPECT_TRUE(found_aapl);
}

TEST_F(BrokerImportTest, SchwabImporterHandlesDollarAmounts) {
    std::string csv_content = R"("Symbol","Quantity","Cost Basis","Date Acquired"
"MSFT","25","$7,500.00","06/01/2023")";

    auto filepath = createTestFile("schwab_dollar.csv", csv_content);

    SchwabImporter importer;
    auto result = importer.importFromFile(filepath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.lots.size(), 1);
    // $7,500 / 25 shares = $300 per share
    EXPECT_DOUBLE_EQ(result.lots[0].cost_basis_per_share, 300.0);
}

// =============================================================================
// FidelityImporter Tests
// =============================================================================

TEST_F(BrokerImportTest, FidelityImporterParsesValidFormat) {
    std::string csv_content = R"(Account Number,Symbol,Quantity,Cost Basis Total,Date Acquired
Z12345678,VOO,30,12000.00,2023-04-15
Z12345678,BND,100,10000.00,2023-05-20)";

    auto filepath = createTestFile("fidelity.csv", csv_content);

    FidelityImporter importer;
    auto result = importer.importFromFile(filepath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.lots.size(), 2);
}

// =============================================================================
// VanguardImporter Tests
// =============================================================================

TEST_F(BrokerImportTest, VanguardImporterParsesValidFormat) {
    std::string csv_content = R"(Fund Name,Symbol,Shares,Total Cost,Acquisition Date
Vanguard Total Stock,VTI,100,22000.00,03/15/2023
Vanguard Total Bond,BND,200,20000.00,04/20/2023)";

    auto filepath = createTestFile("vanguard.csv", csv_content);

    VanguardImporter importer;
    auto result = importer.importFromFile(filepath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.lots.size(), 2);

    bool found_vti = false;
    for (const auto& lot : result.lots) {
        if (lot.ticker == "VTI") {
            EXPECT_DOUBLE_EQ(lot.shares, 100.0);
            EXPECT_DOUBLE_EQ(lot.cost_basis_per_share, 220.0);  // 22000 / 100
            found_vti = true;
        }
    }
    EXPECT_TRUE(found_vti);
}

// =============================================================================
// BrokerImportFactory Tests
// =============================================================================

TEST_F(BrokerImportTest, FactoryCreatesCorrectImporter) {
    auto generic = BrokerImportFactory::create(BrokerFormat::GENERIC_CSV);
    auto schwab = BrokerImportFactory::create(BrokerFormat::SCHWAB);
    auto fidelity = BrokerImportFactory::create(BrokerFormat::FIDELITY);
    auto vanguard = BrokerImportFactory::create(BrokerFormat::VANGUARD);

    EXPECT_NE(generic, nullptr);
    EXPECT_NE(schwab, nullptr);
    EXPECT_NE(fidelity, nullptr);
    EXPECT_NE(vanguard, nullptr);
}

TEST_F(BrokerImportTest, FactoryAutoDetectsSchwabFormat) {
    std::string csv_content = R"("Symbol","Quantity","Cost Basis","Date Acquired","Description"
"AAPL","100","$12,000.00","01/15/2023","APPLE INC")";

    auto filepath = createTestFile("auto_schwab.csv", csv_content);

    auto importer = BrokerImportFactory::autoDetect(filepath);
    EXPECT_NE(importer, nullptr);

    auto result = importer->importFromFile(filepath);
    EXPECT_TRUE(result.success);
}

TEST_F(BrokerImportTest, FactoryAutoDetectsFidelityFormat) {
    std::string csv_content = R"(Account Number,Symbol,Quantity,Cost Basis Total,Date Acquired
Z12345678,VOO,30,12000.00,2023-04-15)";

    auto filepath = createTestFile("auto_fidelity.csv", csv_content);

    auto importer = BrokerImportFactory::autoDetect(filepath);
    EXPECT_NE(importer, nullptr);

    auto result = importer->importFromFile(filepath);
    EXPECT_TRUE(result.success);
}

TEST_F(BrokerImportTest, FactoryAutoDetectsVanguardFormat) {
    std::string csv_content = R"(Fund Name,Symbol,Shares,Total Cost,Acquisition Date
Vanguard Total Stock,VTI,100,22000.00,03/15/2023)";

    auto filepath = createTestFile("auto_vanguard.csv", csv_content);

    auto importer = BrokerImportFactory::autoDetect(filepath);
    EXPECT_NE(importer, nullptr);

    auto result = importer->importFromFile(filepath);
    EXPECT_TRUE(result.success);
}

TEST_F(BrokerImportTest, FactoryAutoDetectFallsBackToGeneric) {
    std::string csv_content = R"(ticker,shares,cost_basis,purchase_date
AAPL,100,12000.00,2023-01-15)";

    auto filepath = createTestFile("generic_auto.csv", csv_content);

    auto importer = BrokerImportFactory::autoDetect(filepath);
    EXPECT_NE(importer, nullptr);

    auto result = importer->importFromFile(filepath);
    EXPECT_TRUE(result.success);
}

// =============================================================================
// TaxLotExporter Tests
// =============================================================================

TEST_F(BrokerImportTest, ExporterExportsToCSV) {
    TaxLotManager manager;

    TaxLot lot1;
    lot1.id = "LOT_001";
    lot1.ticker = "AAPL";
    lot1.shares = 100.0;
    lot1.cost_basis_per_share = 120.0;
    lot1.purchase_date = std::chrono::system_clock::now();
    manager.addLot(lot1);

    TaxLot lot2;
    lot2.id = "LOT_002";
    lot2.ticker = "VTI";
    lot2.shares = 50.0;
    lot2.cost_basis_per_share = 220.0;
    lot2.purchase_date = std::chrono::system_clock::now();
    manager.addLot(lot2);

    auto filepath = (test_dir_ / "export.csv").string();

    TaxLotExporter exporter;
    bool success = exporter.exportToCSV(manager, filepath);

    EXPECT_TRUE(success);
    EXPECT_TRUE(fs::exists(filepath));

    // Read back and verify
    std::ifstream file(filepath);
    std::string line;
    std::getline(file, line);  // Header
    EXPECT_TRUE(line.find("ticker") != std::string::npos);

    int line_count = 0;
    while (std::getline(file, line)) {
        line_count++;
    }
    EXPECT_EQ(line_count, 2);  // Two lots
}

// =============================================================================
// ImportResult Tests
// =============================================================================

TEST(ImportResultTest, DefaultStateIsFailure) {
    ImportResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.lots.empty());
}

TEST(ImportResultTest, CanAddWarnings) {
    ImportResult result;
    result.success = true;
    result.warnings.push_back("Warning 1");
    result.warnings.push_back("Warning 2");

    EXPECT_EQ(result.warnings.size(), 2);
}

// =============================================================================
// Date Parsing Tests
// =============================================================================

TEST_F(BrokerImportTest, ParsesMultipleDateFormats) {
    // Test various date formats that should be parsed correctly
    std::string csv_content = R"(ticker,shares,cost_basis,purchase_date
AAPL,100,12000.00,2023-01-15
MSFT,50,15000.00,01/20/2023
GOOGL,25,62500.00,03-Feb-2023
VTI,75,16500.00,2023/04/10)";

    auto filepath = createTestFile("dates.csv", csv_content);

    GenericCSVImporter importer;
    auto result = importer.importFromFile(filepath);

    // Should parse at least some of these formats
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.lots.size(), 1);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(BrokerImportTest, HandlesInvalidNumbersGracefully) {
    std::string csv_content = R"(ticker,shares,cost_basis,purchase_date
AAPL,invalid,12000.00,2023-01-15
MSFT,50,not_a_number,2023-03-10)";

    auto filepath = createTestFile("invalid_numbers.csv", csv_content);

    GenericCSVImporter importer;
    auto result = importer.importFromFile(filepath);

    // Should have warnings about skipped rows
    EXPECT_GE(result.warnings.size(), 1);
}

TEST_F(BrokerImportTest, HandlesMissingColumnsGracefully) {
    std::string csv_content = R"(ticker,shares
AAPL,100
MSFT,50)";

    auto filepath = createTestFile("missing_columns.csv", csv_content);

    GenericCSVImporter importer;
    auto result = importer.importFromFile(filepath);

    // Should fail or warn about missing required columns
    EXPECT_FALSE(result.success && result.lots.size() == 2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
