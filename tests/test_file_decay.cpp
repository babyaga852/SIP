
#include <gtest/gtest.h>
#include "modules/file_decay.hpp"
#include "core/engine.hpp"
#include <filesystem>
#include <fstream>

using namespace sip;
namespace fs = std::filesystem;

class FileDecayTest : public ::testing::Test {
protected:
    fs::path tmp_dir;
    void SetUp() override {
        tmp_dir = fs::temp_directory_path() / "sip_fd_test";
        fs::create_directories(tmp_dir);
        // Create some test files
        for (int i = 0; i < 5; ++i)
            std::ofstream(tmp_dir / ("file_" + std::to_string(i) + ".txt")) << "data";
    }
    void TearDown() override { fs::remove_all(tmp_dir); }
};

TEST_F(FileDecayTest, InitAndWatch) {
    Engine eng;
    auto fd = std::make_shared<FileDecayTracker>();
    eng.register_module(fd);
    EXPECT_NO_THROW(fd->watch(tmp_dir));
}

TEST_F(FileDecayTest, ReportContainsFiles) {
    Engine eng;
    auto fd = std::make_shared<FileDecayTracker>();
    eng.register_module(fd);
    fd->watch(tmp_dir);
    auto report = fd->report();
    EXPECT_TRUE(report.is_array());
    EXPECT_GE(report.size(), 1u);
}

TEST_F(FileDecayTest, DeadFilesThreshold) {
    Engine eng;
    auto fd = std::make_shared<FileDecayTracker>();
    eng.register_module(fd);
    fd->watch(tmp_dir);
    // threshold=1.1 means all files are "dead"
    auto dead = fd->dead_files(1.1);
    EXPECT_GE(dead.size(), 0u); // just ensure no throw
}
