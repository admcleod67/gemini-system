#include "MdDefaultDataFile.h"

#include "Catalog.h"
#include "FileSystem.h"

#include <sstream>

namespace PickCore {
    namespace {
        constexpr const char *kMdFile = "MD";
        constexpr const char *kDefDataRecord = "DEFDATA";

        void trimAsciiWsInPlace(std::string &s) {
            while (!s.empty()) {
                const unsigned char c = static_cast<unsigned char>(s.front());
                if (c == ' ' || c == '\t' || c == '\r') {
                    s.erase(s.begin());
                } else {
                    break;
                }
            }
            while (!s.empty()) {
                const unsigned char c = static_cast<unsigned char>(s.back());
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                    s.pop_back();
                } else {
                    break;
                }
            }
        }
    } // namespace

    std::optional<std::string> loadDefaultDataFileFromMd(const PickFS::FileSystem &fs) {
        try {
            const std::optional<PickFS::Record> rec = fs.read(kMdFile, kDefDataRecord);
            if (!rec.has_value()) {
                return std::nullopt;
            }
            std::istringstream in(rec->value());
            std::string line;
            while (std::getline(in, line)) {
                trimAsciiWsInPlace(line);
                if (line.empty()) {
                    continue;
                }
                if (!PickFS::Catalog::isValidName(line)) {
                    return std::nullopt;
                }
                return line;
            }
            return std::nullopt;
        } catch (const PickFS::FileSystemError &) {
            return std::nullopt;
        }
    }
} // namespace PickCore
