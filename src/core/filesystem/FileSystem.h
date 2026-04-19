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
        explicit FileSystem(std::filesystem::path root = "filesystem");

        void setRoot(std::filesystem::path root);

        const std::filesystem::path &root() const { return catalog_.root(); }

        void createFile(const std::string &name);

        void deleteFile(const std::string &name);

        std::optional<Record> read(const std::string &fileName, const std::string &recordName) const;

        void write(const std::string &fileName, const Record &record);

        std::vector<std::string> listFiles() const;

        std::vector<std::string> listRecordNames(const std::string &fileName) const;

    private:
        struct StoredFile {
            std::string name;
            std::vector<std::pair<std::string, std::string>> records;
        };

        Catalog catalog_;

        StoredFile loadFile(const std::string &fileName) const;

        void saveFile(const StoredFile &stored) const;
    };
} // namespace PickFS

#endif // PICK_SYSTEM_FILESYSTEM_FILESYSTEM_H
