//
// Created by Allan McLeod on 19/04/2026.
//

#ifndef PICK_SYSTEM_FILESYSTEM_FILESYSTEM_H
#define PICK_SYSTEM_FILESYSTEM_FILESYSTEM_H

#include "Catalog.h"
#include "Record.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace PickFS {
    class FileSystemError final : public std::runtime_error {
    public:
        explicit FileSystemError(const std::string &what) : std::runtime_error(what) {
        }
    };

    class FileSystem {
    public:
        struct FileHandle {
            std::string name;
            std::filesystem::path directoryPath;
        };

        explicit FileSystem(std::filesystem::path root = "filesystem");

        void setRoot(std::filesystem::path root);

        const std::filesystem::path &root() const { return catalog_.root(); }

        void createFile(const std::string &name);

        void deleteFile(const std::string &name);

        FileHandle openFile(const std::string &name) const;

        std::vector<std::string> listRecords(const FileHandle &handle) const;

        std::optional<Record> readRecord(const FileHandle &handle, const std::string &recordName) const;

        void writeRecord(const FileHandle &handle, const Record &record);

        void deleteRecord(const FileHandle &handle, const std::string &recordName);

        std::optional<Record> read(const std::string &fileName, const std::string &recordName) const;

        void write(const std::string &fileName, const Record &record);

        std::vector<std::string> listFiles() const;

        std::vector<std::string> listRecordNames(const std::string &fileName) const;

        std::optional<std::string> readAttributeValue(const std::string &fileName,
                                                      const std::string &recordName,
                                                      int attributeNo) const;

        std::optional<std::string> readSubvalue(const std::string &fileName,
                                                const std::string &recordName,
                                                int attributeNo,
                                                int valueIndex) const;

        void writeAttributeValue(const std::string &fileName,
                                 const std::string &recordName,
                                 int attributeNo,
                                 const std::string &value);

        void writeSubvalue(const std::string &fileName,
                           const std::string &recordName,
                           int attributeNo,
                           int valueIndex,
                           const std::string &value);

    private:
        Catalog catalog_;

        static bool isValidRecordName(const std::string &name);

        static std::string preferredExtensionForFile(const std::string &fileName);

        static bool isKnownRecordExtension(const std::string &extension);

        std::filesystem::path resolveRecordPath(const FileHandle &handle, const std::string &recordName) const;
    };
} // namespace PickFS

#endif // PICK_SYSTEM_FILESYSTEM_FILESYSTEM_H
