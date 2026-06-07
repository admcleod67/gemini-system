#include "HelpTopics.h"

#include "FileSystem.h"
#include "VocResolver.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace PickShell::HelpTopics {
    namespace {
        constexpr const char kBuiltinHelpTopicHelp[] =
            "HELP [topic...]  Type HELP-LIST for topics.  See docs/tcl-shell.md.\n";

        constexpr const char kEnglishFormattingClausesHelp[] =
            "  Formatting clauses (any position after verb/file):\n"
            "    HEADING \"text\"  FOOTING \"text\" — @DATE @TIME @PAGE @<n>\n"
            "    BREAK-ON <field>  TOTAL <field>  ID-SUPP\n"
            "  FOOTING once at end without HEADING; per-page with HEADING + PAGE-LENGTH\n"
            "  Pagination: SET PAGE-LENGTH n | GET PAGE-LENGTH (when HEADING present)\n"
            "  See docs/english-formatting.md\n";

        void asciiUpperInPlace(std::string &s) {
            for (char &ch: s) {
                ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            }
        }

        void trimCollapseAsciiSpace(std::string &s) {
            std::size_t write = 0;
            bool inSpace = false;
            for (std::size_t read = 0; read < s.size(); ++read) {
                const unsigned char uch = static_cast<unsigned char>(s[read]);
                if (std::isspace(uch) != 0) {
                    inSpace = true;
                    continue;
                }
                if (inSpace && write > 0) {
                    s[write++] = ' ';
                }
                inSpace = false;
                s[write++] = s[read];
            }
            s.resize(write);
        }

        [[nodiscard]] std::optional<std::string> tryReadHelpRecordBody(const PickFS::FileSystem &fs,
                                                                     const std::string &storageId) {
            try {
                const std::optional<PickFS::Record> rec = fs.read("HELP", storageId);
                if (!rec.has_value()) {
                    return std::nullopt;
                }
                return rec->value();
            } catch (const PickFS::FileSystemError &) {
                return std::nullopt;
            }
        }

        [[nodiscard]] bool filesystemRootsAreEquivalent(const std::filesystem::path &a,
                                                       const std::filesystem::path &b) {
            std::error_code ec;
            const std::filesystem::path ca = std::filesystem::weakly_canonical(a, ec);
            if (ec) {
                return a == b;
            }
            const std::filesystem::path cb = std::filesystem::weakly_canonical(b, ec);
            if (ec) {
                return a == b;
            }
            return ca == cb;
        }
    } // namespace

    std::string joinOperandsDisplay(const std::vector<std::string> &tokens, const std::size_t operandStart) {
        std::string out;
        for (std::size_t i = operandStart; i < tokens.size(); ++i) {
            if (i > operandStart) {
                out.push_back(' ');
            }
            out += tokens[i];
        }
        return out;
    }

    std::string canonicalTopicKey(PickVoc::VocResolver &voc,
                                  const std::vector<std::string> &tokens,
                                  const std::size_t operandStart) {
        if (operandStart >= tokens.size()) {
            return {};
        }
        if (tokens.size() - operandStart == 1) {
            std::string resolved = voc.resolveVerbName(tokens[operandStart]);
            asciiUpperInPlace(resolved);
            return resolved;
        }
        std::string joined = joinOperandsDisplay(tokens, operandStart);
        trimCollapseAsciiSpace(joined);
        asciiUpperInPlace(joined);
        return joined;
    }

    std::string storageRecordIdFromCanonical(const std::string_view canonicalKey) {
        std::string out;
        out.reserve(canonicalKey.size());
        for (const char ch: canonicalKey) {
            if (ch == ' ') {
                out.push_back('_');
            } else {
                out.push_back(ch);
            }
        }
        return out;
    }

    std::string canonicalTopicFromStorageRecordId(const std::string_view storageId) {
        std::string out;
        out.reserve(storageId.size());
        for (const char ch: storageId) {
            if (ch == '_') {
                out.push_back(' ');
            } else {
                out.push_back(ch);
            }
        }
        return out;
    }

    std::optional<std::string> resolveHelpBody(const PickFS::FileSystem &sessionFs,
                                                 const std::optional<std::filesystem::path> &geminiCatalogRoot,
                                                 const std::filesystem::path &sessionPickRoot,
                                                 const std::string_view canonicalKey) {
        const std::string canonical{canonicalKey};
        const std::string storageId = storageRecordIdFromCanonical(canonicalKey);
        if (const std::optional<std::string> local = tryReadHelpRecordBody(sessionFs, storageId)) {
            return local;
        }

        if (geminiCatalogRoot.has_value()) {
            const std::filesystem::path systemPickRoot = *geminiCatalogRoot / "accounts" / "SYSPROG";
            if (!filesystemRootsAreEquivalent(sessionPickRoot, systemPickRoot)) {
                const PickFS::FileSystem systemFs(systemPickRoot);
                if (const std::optional<std::string> sys = tryReadHelpRecordBody(systemFs, storageId)) {
                    return sys;
                }
            }
        }

        const auto &builtins = builtinHelpBodies();
        if (const auto it = builtins.find(canonical); it != builtins.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    const std::unordered_map<std::string, std::string> &builtinHelpBodies() {
        static const std::unordered_map<std::string, std::string> kTable = [] {
            std::unordered_map<std::string, std::string> m;
            m.emplace("HELP", std::string{kBuiltinHelpTopicHelp});
            m.emplace("VERSION", "VERSION\n");
            m.emplace("SYSTEM", "SYSTEM\n");
            m.emplace("ABOUT", "ABOUT\n");
            m.emplace("WHO", "WHO\n");
            m.emplace("ECHO", "ECHO <text...>\n");
            m.emplace("SET", "SET <name> <words...>\n");
            m.emplace("GET", "GET <name>\n");
            m.emplace("LIST-VARS", "LIST-VARS\n");
            m.emplace("UNSET", "UNSET <name>\n");
            m.emplace("LOGTO", "LOGTO <account>\n");
            m.emplace("LOGOFF", "LOGOFF\n");
            m.emplace("BASIC", "BASIC [name]\n");
            m.emplace("ASM", "ASM [name]\n");
            m.emplace("RUN", "RUN <name>\n");
            m.emplace("PROC", "HELP PROC\n"
                             "  PROC <name> [args...]\n"
                             "  Host PROC interpreter with R83 aliases and long-form command support.\n"
                             "  See docs/proc.md for full statement and substitution semantics.\n");
            m.emplace("LIST-PROGRAMS", "LIST-PROGRAMS\n");
            m.emplace("CREATE-FILE", "CREATE-FILE <name>\n");
            m.emplace("DELETE-FILE", "DELETE-FILE <name>\n");
            m.emplace("LIST-FILES", "LIST-FILES\n");
            m.emplace("LIST", std::string{"HELP LIST\n"
                                          "  LIST <file> [fields... | WITH ...]\n"} +
                                      kEnglishFormattingClausesHelp);
            m.emplace("COUNT", "COUNT <file> [fields...] | COUNT\n");
            m.emplace("SELECT", std::string{"HELP SELECT\n"
                                            "  SELECT <file> [fields...]\n"} +
                                        kEnglishFormattingClausesHelp);
            m.emplace("SORT", std::string{"HELP SORT\n"
                                         "  SORT <file> [fields...] BY <field> [BY-DSND ...]\n"} +
                                     kEnglishFormattingClausesHelp);
            m.emplace("LIST-LIST", "LIST-LIST\n");
            m.emplace("CLEAR-LIST", "CLEAR-LIST\n");
            m.emplace("RESOLVE-FIELD",
                      "RESOLVE-FIELD <data-file> <field-token>\n"
                      "  Resolves A-, F-, and I-type DICT items; invalid F/I rows show Validity INVALID.\n");
            m.emplace("DEFINE-FIELD", "DEFINE-FIELD <dict-file> <field-name> <attribute-number>\n");
            m.emplace("LIST-DICT",
                      "LIST-DICT <dict-file>\n"
                      "  Lists DICT items sorted by id: <id> <type> VALID|INVALID.\n");
            m.emplace("CREATE-VOC", "CREATE-VOC <item-id> <type> <target...>\n");
            m.emplace("DELETE-VOC", "DELETE-VOC <item-id>\n");
            m.emplace("LIST-VOC", "LIST-VOC\n");
            m.emplace("READ", "READ <file> <record-name>\n");
            m.emplace("WRITE", "WRITE <file> <record-name> <value...>\n");
            m.emplace("EDIT", "EDIT <file> <record> | EDIT <programName>\n");
            m.emplace("DUMP-STACK", "DUMP-STACK\n");
            m.emplace("QUIT", "QUIT\n");
            m.emplace("TCL", "HELP TCL\n"
                             "  Tcl host shell command surface and tokenization behavior.\n"
                             "  Use HELP <command> for specific command usage.\n"
                             "  See docs/tcl-shell.md for full shell semantics.\n");
            m.emplace("VOC", "HELP VOC\n"
                             "  CREATE-VOC <item-id> <type> <target...>\n"
                             "  DELETE-VOC <item-id>\n"
                             "  LIST-VOC\n");
            m.emplace("HELP-LIST", "HELP-LIST\n");
            m.emplace("HELP-EDIT", "HELP-EDIT <topic...>\n");
            return m;
        }();
        return kTable;
    }

    std::string helpZeroArgumentFallbackLine() {
        return std::string{kBuiltinHelpTopicHelp};
    }

    std::vector<std::string> listCanonicalHelpTopicsSorted(const PickFS::FileSystem &sessionFs) {
        try {
            const std::vector<std::string> storageIds = sessionFs.listRecordNames("HELP");
            std::vector<std::string> canonical;
            canonical.reserve(storageIds.size());
            for (const std::string &id: storageIds) {
                canonical.push_back(canonicalTopicFromStorageRecordId(id));
            }
            std::sort(canonical.begin(), canonical.end());
            canonical.erase(std::unique(canonical.begin(), canonical.end()), canonical.end());
            return canonical;
        } catch (const PickFS::FileSystemError &) {
            return {};
        }
    }

} // namespace PickShell::HelpTopics
