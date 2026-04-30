#ifndef PICK_SYSTEM_TCL_VOCRESOLVER_H
#define PICK_SYSTEM_TCL_VOCRESOLVER_H

#pragma once

#include "FileSystem.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PickShell {
    class VocResolver {
    public:
        struct ProgramLocation {
            std::string fileName;
            std::string recordKey;
        };

        enum class EntryType {
            F,
            Q,
            V,
        };

        struct VocEntry {
            EntryType type;
            std::string attribute2;
            std::string attribute3;
        };

        explicit VocResolver(PickFS::FileSystem &fileSystem);

        void invalidate();

        ProgramLocation resolveProgramLocation(const std::string &programName);

        std::vector<std::string> listProgramFiles();

        const std::unordered_map<std::string, VocEntry> &table();

    private:
        PickFS::FileSystem &fileSystem_;
        std::unordered_map<std::string, VocEntry> table_;
        bool loaded_{false};

        void ensureLoaded();
        void loadVocTable();

        static std::string upperAscii(std::string text);
        static std::vector<std::string> parseAttributes(const std::string &value);
        static std::string stripAttributeLinePrefix(const std::string &line);
        static std::optional<VocEntry> parseVocEntry(const std::string &value);

        std::optional<std::string> resolveFileFromVocKey(const std::string &vocKey,
                                                         std::unordered_set<std::string> &visited) const;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_VOCRESOLVER_H
