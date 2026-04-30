#include "VocResolver.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace PickShell {
    namespace {
        constexpr const char *kDefaultProgramFile = "BP";
    } // namespace

    VocResolver::VocResolver(PickFS::FileSystem &fileSystem)
        : fileSystem_(fileSystem) {
    }

    void VocResolver::invalidate() {
        loaded_ = false;
        table_.clear();
    }

    VocResolver::ProgramLocation VocResolver::resolveProgramLocation(const std::string &programName) {
        ensureLoaded();
        ProgramLocation location{kDefaultProgramFile, programName};
        std::unordered_set<std::string> visited;
        if (const std::optional<std::string> resolvedFile = resolveFileFromVocKey(programName, visited)) {
            location.fileName = *resolvedFile;
        }
        return location;
    }

    std::vector<std::string> VocResolver::listProgramFiles() {
        ensureLoaded();
        std::unordered_set<std::string> files;
        files.insert(kDefaultProgramFile);
        for (const auto &[key, entry]: table_) {
            if (entry.type == EntryType::F) {
                if (!entry.attribute2.empty()) {
                    files.insert(entry.attribute2);
                }
                continue;
            }
            if (entry.type != EntryType::Q) {
                continue;
            }
            std::unordered_set<std::string> visited;
            if (const std::optional<std::string> resolved = resolveFileFromVocKey(key, visited)) {
                files.insert(*resolved);
            }
        }
        std::vector<std::string> out(files.begin(), files.end());
        std::sort(out.begin(), out.end());
        return out;
    }

    const std::unordered_map<std::string, VocResolver::VocEntry> &VocResolver::table() {
        ensureLoaded();
        return table_;
    }

    void VocResolver::ensureLoaded() {
        if (loaded_) {
            return;
        }
        loadVocTable();
        loaded_ = true;
    }

    void VocResolver::loadVocTable() {
        table_.clear();
        PickFS::FileSystem::FileHandle vocFile;
        try {
            vocFile = fileSystem_.openFile("VOC");
        } catch (const PickFS::FileSystemError &) {
            return;
        }

        const std::vector<std::string> recordNames = fileSystem_.listRecords(vocFile);
        for (const std::string &recordName: recordNames) {
            const std::optional<PickFS::Record> record = fileSystem_.readRecord(vocFile, recordName);
            if (!record.has_value()) {
                continue;
            }
            const std::optional<VocEntry> entry = parseVocEntry(record->value());
            if (!entry.has_value()) {
                continue;
            }
            table_[upperAscii(recordName)] = *entry;
        }
    }

    std::string VocResolver::upperAscii(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return text;
    }

    std::vector<std::string> VocResolver::parseAttributes(const std::string &value) {
        std::vector<std::string> attrs;
        std::stringstream in(value);
        std::string line;
        while (std::getline(in, line)) {
            attrs.push_back(stripAttributeLinePrefix(line));
        }
        if (!value.empty() && value.back() == '\n') {
            attrs.emplace_back("");
        }
        return attrs;
    }

    std::string VocResolver::stripAttributeLinePrefix(const std::string &line) {
        std::size_t pos = 0;
        while (pos < line.size() && std::isdigit(static_cast<unsigned char>(line[pos])) != 0) {
            ++pos;
        }
        if (pos > 0 && pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])) != 0) {
            while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])) != 0) {
                ++pos;
            }
            return line.substr(pos);
        }
        return line;
    }

    std::optional<VocResolver::VocEntry> VocResolver::parseVocEntry(const std::string &value) {
        const std::vector<std::string> attrs = parseAttributes(value);
        if (attrs.empty()) {
            return std::nullopt;
        }
        const std::string type = upperAscii(attrs[0]);
        if (type == "F") {
            if (attrs.size() < 2 || attrs[1].empty()) {
                return std::nullopt;
            }
            return VocEntry{EntryType::F, attrs[1], attrs.size() > 2 ? attrs[2] : ""};
        }
        if (type == "Q") {
            if (attrs.size() < 2 || attrs[1].empty()) {
                return std::nullopt;
            }
            return VocEntry{EntryType::Q, attrs[1], attrs.size() > 2 ? attrs[2] : ""};
        }
        if (type == "V") {
            return VocEntry{EntryType::V, attrs.size() > 1 ? attrs[1] : "", attrs.size() > 2 ? attrs[2] : ""};
        }
        return std::nullopt;
    }

    std::optional<std::string> VocResolver::resolveFileFromVocKey(const std::string &vocKey,
                                                                   std::unordered_set<std::string> &visited) const {
        const std::string normalized = upperAscii(vocKey);
        if (!visited.insert(normalized).second) {
            return std::nullopt;
        }
        const auto it = table_.find(normalized);
        if (it == table_.end()) {
            return std::nullopt;
        }
        const VocEntry &entry = it->second;
        if (entry.type == EntryType::F) {
            if (entry.attribute2.empty()) {
                return std::nullopt;
            }
            return entry.attribute2;
        }
        if (entry.type == EntryType::Q) {
            if (entry.attribute2.empty()) {
                return std::nullopt;
            }
            return resolveFileFromVocKey(entry.attribute2, visited);
        }
        return std::nullopt;
    }
} // namespace PickShell
