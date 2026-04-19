#include "Catalog.h"

#include <stdexcept>

namespace PickFS {
    Catalog::Catalog(std::filesystem::path root)
        : root_(std::move(root)) {
    }

    void Catalog::setRoot(std::filesystem::path root) {
        root_ = std::move(root);
    }

    std::filesystem::path Catalog::filePath(const std::string &fileName) const {
        if (!isValidName(fileName)) {
            throw std::runtime_error("Invalid file name");
        }
        return root_ / (fileName + ".json");
    }

    void Catalog::ensureRootExists() const {
        std::error_code ec;
        std::filesystem::create_directories(root_, ec);
        if (ec) {
            throw std::runtime_error("Cannot create filesystem directory: " + root_.string());
        }
    }

    bool Catalog::isValidName(const std::string &name) {
        if (name.empty()) {
            return false;
        }
        for (const char c: name) {
            const bool isAlphaNum =
                    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
            if (!isAlphaNum && c != '_' && c != '-') {
                return false;
            }
        }
        return true;
    }
} // namespace PickFS
