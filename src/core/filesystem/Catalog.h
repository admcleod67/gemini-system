//
// Created by Allan McLeod on 19/04/2026.
//

#ifndef PICK_SYSTEM_FILESYSTEM_CATALOG_H
#define PICK_SYSTEM_FILESYSTEM_CATALOG_H

#include <filesystem>
#include <string>

namespace PickFS {
    class Catalog {
    public:
        explicit Catalog(std::filesystem::path root = "filesystem");

        void setRoot(std::filesystem::path root);

        const std::filesystem::path &root() const { return root_; }

        std::filesystem::path filePath(const std::string &fileName) const;

        void ensureRootExists() const;

        static bool isValidName(const std::string &name);

    private:
        std::filesystem::path root_;
    };
} // namespace PickFS

#endif // PICK_SYSTEM_FILESYSTEM_CATALOG_H
