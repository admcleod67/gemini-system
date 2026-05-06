#include "FileSystem.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace PickFS {
    namespace {
        constexpr const char *kDefaultRecordExtension = ".item";
        constexpr const char *kBasicRecordExtension = ".bas";
        constexpr const char *kProcRecordExtension = ".proc";
    } // namespace

    FileSystem::FileSystem(std::filesystem::path root)
        : catalog_(std::move(root)) {
    }

    void FileSystem::setRoot(std::filesystem::path root) {
        catalog_.setRoot(std::move(root));
    }

    void FileSystem::createFile(const std::string &name) {
        if (!Catalog::isValidName(name)) {
            throw FileSystemError("Invalid file name");
        }

        const std::filesystem::path path = catalog_.filePath(name);
        catalog_.ensureRootExists();

        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            throw FileSystemError("Cannot inspect file: " + path.string());
        }
        if (exists) {
            throw FileSystemError("File already exists: " + name);
        }
        const bool created = std::filesystem::create_directory(path, ec);
        if (ec || !created) {
            throw FileSystemError("Cannot create file: " + name);
        }
    }

    void FileSystem::deleteFile(const std::string &name) {
        if (!Catalog::isValidName(name)) {
            throw FileSystemError("Invalid file name");
        }

        const std::filesystem::path path = catalog_.filePath(name);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            if (ec) {
                throw FileSystemError("Cannot inspect file: " + name);
            }
            throw FileSystemError("File not found: " + name);
        }
        if (!std::filesystem::is_directory(path, ec)) {
            if (ec) {
                throw FileSystemError("Cannot inspect file: " + name);
            }
            throw FileSystemError("Invalid file storage for: " + name);
        }
        const std::uintmax_t removedCount = std::filesystem::remove_all(path, ec);
        if (ec) {
            throw FileSystemError("Cannot delete file: " + name);
        }
        if (removedCount == 0) {
            throw FileSystemError("File not found: " + name);
        }
    }

    FileSystem::FileHandle FileSystem::openFile(const std::string &name) const {
        if (!Catalog::isValidName(name)) {
            throw FileSystemError("Invalid file name");
        }
        const std::filesystem::path path = catalog_.filePath(name);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            if (ec) {
                throw FileSystemError("Cannot inspect file: " + name);
            }
            throw FileSystemError("File not found: " + name);
        }
        if (!std::filesystem::is_directory(path, ec)) {
            if (ec) {
                throw FileSystemError("Cannot inspect file: " + name);
            }
            throw FileSystemError("Invalid file storage for: " + name);
        }
        return FileHandle{name, path};
    }

    std::vector<std::string> FileSystem::listRecords(const FileHandle &handle) const {
        std::vector<std::string> names;
        std::error_code ec;
        for (const auto &entry: std::filesystem::directory_iterator(handle.directoryPath, ec)) {
            if (ec) {
                throw FileSystemError("Cannot read file directory: " + handle.name);
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string extension = entry.path().extension().string();
            if (!isKnownRecordExtension(extension)) {
                continue;
            }
            const std::string recordName = entry.path().stem().string();
            if (!isValidRecordName(recordName)) {
                continue;
            }
            names.push_back(recordName);
        }
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        return names;
    }

    std::optional<Record> FileSystem::readRecord(const FileHandle &handle, const std::string &recordName) const {
        if (!isValidRecordName(recordName)) {
            throw FileSystemError("Invalid record name");
        }

        const std::vector<std::string> extensions = {
            preferredExtensionForFile(handle.name),
            kDefaultRecordExtension,
            kBasicRecordExtension,
            kProcRecordExtension,
        };
        std::filesystem::path selectedPath;
        for (const std::string &ext: extensions) {
            const std::filesystem::path candidate = handle.directoryPath / (recordName + ext);
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) {
                if (ec) {
                    throw FileSystemError("Cannot inspect record: " + recordName);
                }
                selectedPath = candidate;
                break;
            }
        }

        if (selectedPath.empty()) {
            return std::nullopt;
        }
        std::ifstream in(selectedPath, std::ios::binary);
        if (!in) {
            throw FileSystemError("Cannot open record: " + recordName);
        }
        const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return Record(recordName, content);
    }

    void FileSystem::writeRecord(const FileHandle &handle, const Record &record) {
        if (!isValidRecordName(record.name())) {
            throw FileSystemError("Invalid record name");
        }

        const std::filesystem::path path = resolveRecordPath(handle, record.name());
        const std::filesystem::path tempPath = path.string() + ".tmp";
        {
            std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
            if (!out) {
                throw FileSystemError("Cannot write record: " + record.name());
            }
            const std::string canonicalRaw = StructuredRecord::fromRaw(record.value()).toRaw();
            out.write(canonicalRaw.data(), static_cast<std::streamsize>(canonicalRaw.size()));
            if (!out) {
                throw FileSystemError("Cannot write record: " + record.name());
            }
        }

        std::error_code ec;
        std::filesystem::rename(tempPath, path, ec);
        if (ec) {
            std::filesystem::remove(tempPath);
            throw FileSystemError("Cannot finalize record: " + record.name());
        }
    }

    void FileSystem::deleteRecord(const FileHandle &handle, const std::string &recordName) {
        if (!isValidRecordName(recordName)) {
            throw FileSystemError("Invalid record name");
        }
        const std::filesystem::path path = resolveRecordPath(handle, recordName);
        std::error_code ec;
        const bool removed = std::filesystem::remove(path, ec);
        if (ec) {
            throw FileSystemError("Cannot delete record: " + recordName);
        }
        if (!removed) {
            throw FileSystemError("Record not found: " + recordName);
        }
    }

    std::optional<Record> FileSystem::read(const std::string &fileName, const std::string &recordName) const {
        return readRecord(openFile(fileName), recordName);
    }

    void FileSystem::write(const std::string &fileName, const Record &record) {
        writeRecord(openFile(fileName), record);
    }

    std::vector<std::string> FileSystem::listFiles() const {
        std::vector<std::string> names;

        std::error_code ec;
        if (!std::filesystem::exists(catalog_.root(), ec)) {
            return names;
        }
        if (ec) {
            throw FileSystemError("Cannot inspect filesystem root: " + catalog_.root().string());
        }

        for (const auto &entry: std::filesystem::directory_iterator(catalog_.root(), ec)) {
            if (ec) {
                throw FileSystemError("Cannot read filesystem root: " + catalog_.root().string());
            }
            if (!entry.is_directory()) {
                continue;
            }

            const std::string fileName = entry.path().filename().string();
            if (!Catalog::isValidName(fileName)) {
                continue;
            }
            names.push_back(fileName);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    std::vector<std::string> FileSystem::listRecordNames(const std::string &fileName) const {
        return listRecords(openFile(fileName));
    }

    bool FileSystem::isValidRecordName(const std::string &name) {
        return Catalog::isValidName(name);
    }

    std::string FileSystem::preferredExtensionForFile(const std::string &fileName) {
        if (fileName == "BP") {
            return kBasicRecordExtension;
        }
        if (fileName == "PROC" || fileName == "PROCS") {
            return kProcRecordExtension;
        }
        return kDefaultRecordExtension;
    }

    bool FileSystem::isKnownRecordExtension(const std::string &extension) {
        return extension == kDefaultRecordExtension
            || extension == kBasicRecordExtension
            || extension == kProcRecordExtension;
    }

    std::filesystem::path FileSystem::resolveRecordPath(const FileHandle &handle, const std::string &recordName) const {
        const std::string preferredExt = preferredExtensionForFile(handle.name);
        const std::filesystem::path preferredPath = handle.directoryPath / (recordName + preferredExt);
        std::error_code ec;
        if (std::filesystem::exists(preferredPath, ec)) {
            if (ec) {
                throw FileSystemError("Cannot inspect record: " + recordName);
            }
            return preferredPath;
        }

        const std::vector<std::string> fallbackExtensions = {
            kDefaultRecordExtension,
            kBasicRecordExtension,
            kProcRecordExtension,
        };
        for (const std::string &ext: fallbackExtensions) {
            const std::filesystem::path candidate = handle.directoryPath / (recordName + ext);
            if (std::filesystem::exists(candidate, ec)) {
                if (ec) {
                    throw FileSystemError("Cannot inspect record: " + recordName);
                }
                return candidate;
            }
        }
        return preferredPath;
    }
} // namespace PickFS
