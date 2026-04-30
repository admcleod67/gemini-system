//
// Line-oriented record editor (TCL EDIT session).
//

#ifndef PICK_SYSTEM_TCL_LINERECORDEDITOR_H
#define PICK_SYSTEM_TCL_LINERECORDEDITOR_H

#pragma once

#include "FileSystem.h"

#include <functional>
#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace PickShell {
    class LineRecordEditor {
    public:
        using PostWriteHook = std::function<void(const std::string &fileName)>;

        LineRecordEditor(PickFS::FileSystem &fileSystem,
                         std::string fileName,
                         std::string recordKey,
                         std::vector<std::string> lines,
                         PostWriteHook postWrite = {});

        void run(std::istream &in, std::ostream &out, std::optional<int> highlightPhysicalLine = std::nullopt);

    private:
        PickFS::FileSystem &fileSystem_;
        std::string fileName_;
        std::string recordKey_;
        std::vector<std::string> lines_;
        PostWriteHook postWrite_;

        static std::vector<std::string> tokenizeWs(const std::string &line);
        static std::string trimAscii(const std::string &text);
        static std::string upperAscii(std::string text);
        static std::string canonicalCommand(const std::string &token);
        static std::optional<int> parsePositiveInt(const std::string &token);
        static bool parseListRangeTokens(const std::vector<std::string> &tokens, int &startLine, int &endLine);

        void cmdList(const std::vector<std::string> &tokens, std::ostream &out) const;
        void cmdInsert(std::istream &in, const std::vector<std::string> &tokens, std::ostream &out);
        void cmdReplace(std::istream &in, const std::vector<std::string> &tokens, std::ostream &out);
        void cmdDelete(const std::vector<std::string> &tokens, std::ostream &out);
        void cmdSave(std::ostream &out);
        void cmdHelp(std::ostream &out) const;

        bool readPeriodTerminatedBlock(std::istream &in, std::vector<std::string> &outBlock, std::ostream &out);
        bool ensureFileExistsForWrite(std::ostream &out);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_LINERECORDEDITOR_H
