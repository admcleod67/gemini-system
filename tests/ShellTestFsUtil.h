#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include "FileSystem.h"

namespace PickTests {

    [[nodiscard]] inline std::filesystem::path uniqueTempDir() {
        const auto base = std::filesystem::temp_directory_path();
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        return base / ("pick-shell-test-" + std::to_string(tick));
    }

    inline void seedVocAndProc(const std::filesystem::path &fsRoot) {
        PickFS::FileSystem fs(fsRoot);
        fs.createFile("VOC");
        fs.createFile("PROC");
    }

    inline void writeRecord(const std::filesystem::path &fsRoot,
                             const std::string &fileName,
                             const std::string &recordName,
                             const std::string &value) {
        PickFS::FileSystem fs(fsRoot);
        fs.write(fileName, PickFS::Record(recordName, value));
    }

    inline void writeProcScriptRecord(const std::filesystem::path &fsRoot,
                                      const std::string &scriptName,
                                      const std::string &scriptText) {
        writeRecord(fsRoot, "PROC", scriptName, scriptText);
    }

} // namespace PickTests
