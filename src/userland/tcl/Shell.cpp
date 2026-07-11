#include "Shell.h"

#include "BytecodeText.h"
#include "Catalog.h"
#include "DictItemClassifier.h"
#include "EnglishTypes.h"
#include "GeminiCatalog.h"
#include "GeminiSession.h"
#include "HelpTopics.h"
#include "LineRecordEditor.h"
#include "LoginService.h"
#include "LockRegistry.h"
#include "LanguageModuleBootLog.h"
#include "LanguageRegistry.h"
#include "LanguageRegistryFormat.h"

#include <pick_system/version.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace PickShell {
    namespace {
        // Set by executeCompiledBasicProgram while a program is running.
        // Accessed from the SIGINT handler, so must be an atomic pointer.
        std::atomic<PickVM::Runtime *> g_interruptRuntime{nullptr};

        struct SystemVarReaderScope {
            PickVM::Runtime &rt;
            explicit SystemVarReaderScope(PickVM::Runtime &r, GeminiSession &s) : rt(r) {
                rt.setSystemVariableReader([&s](const std::string_view n) -> std::optional<PickVM::Value> {
                    const std::optional<std::string> v = s.resolveSystemVariable(n);
                    if (!v.has_value()) {
                        return std::nullopt;
                    }
                    return PickVM::Value{*v};
                });
            }
            ~SystemVarReaderScope() { rt.setSystemVariableReader({}); }
        };

        void asciiUpperInPlace(std::string &s) {
            for (char &c: s) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
        }

        [[nodiscard]] std::string asciiUpperCopy(const std::string &s) {
            std::string o = s;
            asciiUpperInPlace(o);
            return o;
        }

        bool kwEq(const std::string &a, std::string_view b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (std::toupper(static_cast<unsigned char>(a[i])) !=
                    std::toupper(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }

        bool hasEnglishClauseKeywords(const std::vector<std::string> &tokens) {
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                if (kwEq(tokens[i], "WITH") || kwEq(tokens[i], "BY") || kwEq(tokens[i], "BY-DSND")) {
                    return true;
                }
            }
            return false;
        }

        bool activeListReady(const GeminiSession &session) {
            return !session.activeList().empty() && session.activeListSourceFile().has_value();
        }

        [[nodiscard]] bool logicalPickFileExists(const GeminiSession &session, const std::string &name) {
            try {
                (void) session.fileSystem().listRecordNames(name);
                return true;
            } catch (const PickFS::FileSystemError &) {
                return false;
            }
        }

        /// Returns empty optional on success, or error message when implicit scope is required but unavailable.
        [[nodiscard]] std::optional<std::string> englishImplicitSetup(const GeminiSession &session,
                                                                     const std::vector<std::string> &tokens,
                                                                     PickCore::English::ParseContext &pc,
                                                                     PickCore::English::EnglishRunOptions &eo) {
            if (tokens.empty()) {
                return std::nullopt;
            }
            bool wantImplicit = false;
            if (kwEq(tokens[0], "COUNT")) {
                wantImplicit = tokens.size() == 1U;
            } else if (kwEq(tokens[0], "LIST") || kwEq(tokens[0], "SORT")) {
                if (tokens.size() >= 3U && (kwEq(tokens[1], "BY") || kwEq(tokens[1], "WITH"))) {
                    wantImplicit = true;
                } else if (tokens.size() == 2U && !logicalPickFileExists(session, tokens[1])) {
                    wantImplicit = true;
                }
            }
            if (!wantImplicit) {
                return std::nullopt;
            }
            if (!activeListReady(session)) {
                return std::string{"No active list for implicit ENGLISH scope"};
            }
            pc.implicitFile = true;
            pc.imposedFileName = session.activeListSourceFile();
            eo.constrainRecordIds = session.activeList();
            return std::nullopt;
        }

        bool shellIsEnglishList(const std::vector<std::string> &t, GeminiSession &session) {
            if (t.empty() || !kwEq(t[0], "LIST")) {
                return false;
            }
            if (hasEnglishClauseKeywords(t)) {
                return true;
            }
            if (t.size() >= 3U) {
                return true;
            }
            if (t.size() == 2U && !logicalPickFileExists(session, t[1]) && activeListReady(session)) {
                return true;
            }
            return false;
        }

        bool shellIsEnglishCount(const std::vector<std::string> &t, GeminiSession &session) {
            if (t.empty() || !kwEq(t[0], "COUNT")) {
                return false;
            }
            if (t.size() >= 2U) {
                return true;
            }
            return activeListReady(session);
        }

        bool shellIsEnglishSort(const std::vector<std::string> &t, GeminiSession &session) {
            if (t.empty() || !kwEq(t[0], "SORT")) {
                return false;
            }
            if (hasEnglishClauseKeywords(t)) {
                return true;
            }
            if (t.size() >= 3U) {
                return true;
            }
            if (t.size() == 2U && logicalPickFileExists(session, t[1])) {
                return true;
            }
            if (t.size() == 2U && activeListReady(session) && !logicalPickFileExists(session, t[1])) {
                return true;
            }
            return false;
        }

        void expandSessionAtOperands(GeminiSession &sess, std::vector<std::string> &tokens) {
            if (tokens.size() < 2) {
                return;
            }
            const std::string verbUp = asciiUpperCopy(tokens[0]);
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                if ((verbUp == "SET" || verbUp == "GET" || verbUp == "UNSET") && i == 1) {
                    continue;
                }
                if ((verbUp == "RUN" || verbUp == "PROC") && i == 1) {
                    continue;
                }
                std::string &tok = tokens[i];
                if (tok.empty() || tok[0] != '@') {
                    continue;
                }
                if (!GeminiSession::isSessionSystemVariableName(tok)) {
                    continue;
                }
                const std::optional<std::string> v = sess.resolveSystemVariable(tok);
                if (v.has_value()) {
                    tok = *v;
                }
            }
        }

    } // namespace

    Shell::Shell(GeminiSession &session)
        : session_(session),
          vmDebugService_(session_),
          assemblerShell_(vmDebugService_) {
        if (!session_.hasSharedLockTable()) {
            session_.setSharedLockTable(PickCore::Locking::LockRegistry::instance().table());
        }
        session_.runtime().setFileSystem(&session_.fileSystem());
        basicShell_.setResolveProgramLocationFn([this](const std::string &programName) {
            const PickVoc::VocResolver::ProgramLocation resolved = session_.vocResolver().resolveProgramLocation(programName);
            return BasicShell::ProgramLocation{resolved.fileName, resolved.recordKey};
        });
        basicShell_.setReadRecordFn([this](const BasicShell::ProgramLocation &location, const bool objectRecord) {
            return readProgramRecord(location, objectRecord);
        });
        basicShell_.setWriteRecordFn(
            [this](const BasicShell::ProgramLocation &location, const bool objectRecord, const std::string &payload, std::string &error) {
                return writeProgramRecord(location, objectRecord, payload, error);
            });
        basicShell_.setExecuteProgramFn(
            [this](const std::vector<PickVM::Instruction> &program,
                   const std::vector<int> &sourceLinePerInstr,
                   const BasicProgram &sourceProgram,
                   std::ostream &out) {
                executeCompiledBasicProgram(program, sourceLinePerInstr, sourceProgram, out);
            });
        basicShell_.setRunLineRecordEditorFn([this](const BasicShell::ProgramLocation &location,
                                                    const std::optional<int> highlightPhysicalLine,
                                                    std::ostream &out) {
            runLineRecordEditorForLocation(location.fileName, location.recordKey, highlightPhysicalLine, out);
        });
        assemblerShell_.setRunCommandFn([this](const Tokens &tokens, std::ostream &out) { cmdRun(tokens, out); });
        assemblerShell_.setDumpStackFn([this](std::ostream &out) { cmdDumpStack(out); });
        registerCommands();
    }

    void Shell::setProgramsRoot(std::filesystem::path root) {
        session_.setProgramsRoot(std::move(root));
    }

    const std::filesystem::path &Shell::programsRoot() const {
        return session_.programsRoot();
    }

    void Shell::setFileSystemRoot(std::filesystem::path root) {
        session_.setFileSystemRoot(std::move(root));
    }

    const std::filesystem::path &Shell::fileSystemRoot() const {
        return session_.fileSystemRoot();
    }

    void Shell::setGeminiCatalogRoot(std::optional<std::filesystem::path> root) {
        session_.setGeminiCatalogRoot(std::move(root));
    }

    const std::optional<std::filesystem::path> &Shell::geminiCatalogRoot() const {
        return session_.geminiCatalogRoot();
    }

    void Shell::attachUserSession(const PickCore::UserSession &userSession) {
        session_.attach(userSession);
    }

    void Shell::setAdminQueries(ShellAdminQueries queries) {
        adminQueries_ = std::move(queries);
    }

    Shell::TokenizeResult Shell::tokenize(const std::string &line) {
        TokenizeResult result;
        std::string current;
        bool inQuotes = false;
        bool sawQuoteInCurrent = false;
        bool escaping = false;

        const auto flushToken = [&]() {
            if (!current.empty() || sawQuoteInCurrent) {
                result.tokens.push_back(current);
            }
            current.clear();
            sawQuoteInCurrent = false;
        };

        for (const char ch : line) {
            if (escaping) {
                current.push_back(ch);
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
                continue;
            }
            if (ch == '"') {
                inQuotes = !inQuotes;
                sawQuoteInCurrent = true;
                continue;
            }
            if (!inQuotes && std::isspace(static_cast<unsigned char>(ch)) != 0) {
                flushToken();
                continue;
            }
            current.push_back(ch);
        }

        if (escaping) {
            result.error = "Invalid escape sequence";
            return result;
        }
        if (inQuotes) {
            result.error = "Unterminated quoted string";
            return result;
        }
        flushToken();
        return result;
    }

    ShellRunResult Shell::runTclRepl() {
        std::signal(SIGINT, SIG_IGN); // Ctrl-C does nothing outside a running program
        sessionEndRequested_ = false;
        std::istream &in = input();
        std::ostream &out = output();
        // First stdout bytes here are banner text — no leading `\n` (successful login emits one flushed `\n` first).
        static constexpr std::string_view kBanner = "Gemini/TCL Developer Shell\n"
                                                    "Type HELP for usage; HELP-LIST lists HELP topics.\n";
        out.write(kBanner.data(), static_cast<std::streamsize>(kBanner.size()));
        out.flush();
        for (;;) {
            std::string line;
            if (!readPromptedInputLine(in, out, prompt(), line)) {
                return ShellRunResult::ExitProcess;
            }
            bool quit = false;
            handleLine(line, out, quit);
            if (quit) {
                return ShellRunResult::ExitProcess;
            }
            if (sessionEndRequested_) {
                return ShellRunResult::EndSession;
            }
        }
    }

    bool Shell::readPromptedInputLine(std::istream &in,
                                      std::ostream &out,
                                      const std::string_view promptText,
                                      std::string &line) {
        out.write(promptText.data(), static_cast<std::streamsize>(promptText.size()));
        out.flush();
        return static_cast<bool>(std::getline(in, line));
    }

    void Shell::handleLine(const std::string &line, std::ostream &out, bool &quit) {
        if (assemblerShell_.isActive() || basicShell_.isActive()) {
            const std::vector<std::string> tokens = tokenizeDebuggerLine(line);
            if (!tokens.empty()) {
                dispatch(tokens, quit, out);
            }
            return;
        }
        const TokenizeResult parsed = tokenize(line);
        if (parsed.error.has_value()) {
            out << "Error: " << *parsed.error << "\n";
            return;
        }
        if (!parsed.tokens.empty()) {
            dispatch(parsed.tokens, quit, out);
        }
    }

    void Shell::setInputStream(std::istream *in) {
        inputStream_ = in;
    }

    void Shell::setOutputStream(std::ostream *out) {
        outputStream_ = out;
    }

    std::istream &Shell::input() const {
        return inputStream_ != nullptr ? *inputStream_ : std::cin;
    }

    std::ostream &Shell::output() const {
        return outputStream_ != nullptr ? *outputStream_ : std::cout;
    }

    std::string Shell::prompt() const {
        if (assemblerShell_.isActive()) {
            return assemblerShell_.prompt();
        }
        if (basicShell_.isActive()) {
            return basicShell_.prompt();
        }
        return "TCL> ";
    }

    void Shell::registerCommands() {
        registerTclCommands();
    }

    void Shell::registerTclCommands() {
        tclCommands_["QUIT"] = [this](const Tokens &, std::ostream &out, bool &quit) {
            out << "Exiting shell\n";
            quit = true;
            session_.reset();
        };
        tclCommands_["HELP"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdHelpLookup(tokens, out); };
        tclCommands_["HELP-LIST"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdHelpList(tokens, out);
        };
        tclCommands_["HELP-EDIT"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdHelpEdit(tokens, out);
        };
        tclCommands_["VERSION"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "VERSION takes no arguments\n";
                return;
            }
            cmdVersion(out);
        };
        tclCommands_["SYSTEM"] = [this](const Tokens &tokens, std::ostream &out, bool &quit) {
            cmdSystem(tokens, out, quit);
        };
        tclCommands_["ABOUT"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "ABOUT takes no arguments\n";
                return;
            }
            bool ignoredQuit = false;
            cmdSystem(tokens, out, ignoredQuit);
        };
        tclCommands_["SHOW-MODULES"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "SHOW-MODULES takes no arguments\n";
                return;
            }
            cmdShowModules(out);
        };
        tclCommands_["WHO"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "WHO takes no arguments\n";
                return;
            }
            cmdWho(out);
        };
        tclCommands_["LISTSESSIONS"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "LISTSESSIONS takes no arguments\n";
                return;
            }
            cmdListSessions(out);
        };
        tclCommands_["STATUS"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "STATUS takes no arguments\n";
                return;
            }
            cmdStatus(out);
        };
        tclCommands_["KILLSESSION"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdKillSession(tokens, out);
        };
        tclCommands_["SHUTDOWN"] = [this](const Tokens &tokens, std::ostream &out, bool &quit) {
            if (tokens.size() != 1) {
                out << "SHUTDOWN takes no arguments\n";
                return;
            }
            cmdShutdown(out, quit);
        };
        tclCommands_["ECHO"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdEcho(tokens, out); };
        tclCommands_["RUN"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdRun(tokens, out); };
        tclCommands_["PROC"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdProc(tokens, out); };
        tclCommands_["DUMP-STACK"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "DUMP-STACK takes no arguments\n";
                return;
            }
            cmdDumpStack(out);
        };
        tclCommands_["LIST-PROGRAMS"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "LIST-PROGRAMS takes no arguments\n";
                return;
            }
            cmdListPrograms(out);
        };
        tclCommands_["CREATE-FILE"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdCreateFile(tokens, out);
        };
        tclCommands_["DELETE-FILE"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdDeleteFile(tokens, out);
        };
        tclCommands_["LIST-FILES"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdListFiles(tokens, out);
        };
        tclCommands_["LIST"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdList(tokens, out); };
        tclCommands_["COUNT"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdCount(tokens, out); };
        tclCommands_["SELECT"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdSelect(tokens, out); };
        tclCommands_["SORT"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdSort(tokens, out); };
        tclCommands_["LIST-LIST"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdListList(tokens, out); };
        tclCommands_["CLEAR-LIST"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdClearList(tokens, out); };
        tclCommands_["RESOLVE-FIELD"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdResolveField(tokens, out);
        };
        tclCommands_["DEFINE-FIELD"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdDefineField(tokens, out);
        };
        tclCommands_["LIST-DICT"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdListDict(tokens, out); };
        tclCommands_["CREATE-VOC"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdCreateVoc(tokens, out); };
        tclCommands_["DELETE-VOC"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdDeleteVoc(tokens, out); };
        tclCommands_["LIST-VOC"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdListVoc(tokens, out); };
        tclCommands_["READ"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdRead(tokens, out); };
        tclCommands_["WRITE"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdWrite(tokens, out); };
        tclCommands_["READU"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdReadU(tokens, out); };
        tclCommands_["WRITEU"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdWriteU(tokens, out); };
        tclCommands_["RELEASE"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdRelease(tokens, out); };
        tclCommands_["ASM"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 2) {
                out << "ASM takes at most one program name\n";
                return;
            }
            assemblerShell_.enter(out);
            if (tokens.size() == 2) {
                cmdRun({"RUN", tokens[1]}, out);
            }
        };
        tclCommands_["SET"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdSet(tokens, out); };
        tclCommands_["GET"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdGet(tokens, out); };
        tclCommands_["LIST-VARS"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdListVars(tokens, out);
        };
        tclCommands_["UNSET"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdUnset(tokens, out); };
        tclCommands_["BASIC"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 2) {
                out << "BASIC takes at most one program name\n";
                return;
            }
            std::optional<std::string> programName;
            if (tokens.size() == 2) {
                programName = tokens[1];
            }
            basicShell_.enter(programName, out);
        };
        tclCommands_["EDIT"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdEdit(tokens, out); };
        tclCommands_["LOGTO"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdLogto(tokens, out); };
        tclCommands_["LOGOFF"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdLogoff(tokens, out); };
    }

    void Shell::dispatch(const std::vector<std::string> &tokens, bool &quit, std::ostream &out) {
        if (assemblerShell_.isActive()) {
            bool leaveAssembler = false;
            assemblerShell_.handleCommand(tokens, out, leaveAssembler);
            return;
        }
        if (basicShell_.isActive()) {
            bool leaveBasic = false;
            basicShell_.handleCommand(tokens, out, leaveBasic);
            return;
        }
        handleTclCommand(tokens, quit, out);
    }

    void Shell::handleTclCommand(const Tokens &tokensIn, bool &quit, std::ostream &out) {
        Tokens work = tokensIn;
        if (!work.empty()) {
            work[0] = session_.vocResolver().resolveVerbName(work[0]);
            asciiUpperInPlace(work[0]);
        }
        expandSessionAtOperands(session_, work);
        const auto it = tclCommands_.find(work[0]);
        if (it == tclCommands_.end()) {
            out << "Unknown command: " << work[0] << "\n";
            return;
        }
        it->second(work, out, quit);
    }

    void Shell::executeCompiledBasicProgram(const std::vector<PickVM::Instruction> &program,
                                            const std::vector<int> &sourceLinePerInstr,
                                            const BasicProgram &sourceProgram,
                                            std::ostream &out) {
        basicBreakpoints_.clear();
        basicResumePastLine_.reset();
        basicTrace_ = false;

        session_.runtime().setOutputStream(&out);
        session_.runtime().setInputStream(inputStream_);
        session_.runtime().loadProgram(program, sourceLinePerInstr); // also clears interrupted_ flag

        const SystemVarReaderScope sysScope(session_.runtime(), session_);

        g_interruptRuntime.store(&session_.runtime());
        const auto previousHandler = std::signal(SIGINT, [](int) {
            if (auto *rt = g_interruptRuntime.load()) {
                rt->interrupt();
            }
        });

        try {
            session_.runtime().run();
        } catch (const PickVM::ChainRequest &req) {
            const std::string chainProgram = req.programName;
            if (chainProgram.empty()) {
                out << "\nRuntime error: CHAIN requires a program name\n";
            } else if (chainProgram.find('.') != std::string::npos) {
                out << "\nRuntime error: CHAIN expects a program name without extension\n";
            } else if (!ensureProgramObjectExistsForRun(chainProgram, out)) {
                // ensureProgramObjectExistsForRun writes concrete errors.
            } else {
                try {
                    const PickVoc::VocResolver::ProgramLocation resolved = session_.vocResolver().resolveProgramLocation(chainProgram);
                    const BasicShell::ProgramLocation location{resolved.fileName, resolved.recordKey};
                    const std::optional<std::string> objectText = readProgramRecord(location, true);
                    if (!objectText.has_value()) {
                        out << "\nRuntime error: CHAIN target has no object code: " << chainProgram << '\n';
                    } else {
                        PickVM::Parser parser;
                        std::istringstream bytecodeStream(*objectText);
                        PickVM::LoadedBytecode loaded = parser.parse(bytecodeStream);

                        BasicProgram chainedSource;
                        chainedSource.setName(chainProgram);
                        if (const std::optional<std::string> sourceText = readProgramRecord(location, false); sourceText.has_value()) {
                            std::istringstream in(*sourceText);
                            std::string line;
                            while (std::getline(in, line)) {
                                std::istringstream iss(line);
                                std::string lineNumberToken;
                                if (!(iss >> lineNumberToken)) {
                                    continue;
                                }
                                int lineNumber = 0;
                                try {
                                    lineNumber = std::stoi(lineNumberToken);
                                } catch (const std::exception &) {
                                    continue;
                                }
                                if (lineNumber <= 0) {
                                    continue;
                                }
                                std::string text;
                                std::getline(iss, text);
                                if (!text.empty() && text.front() == ' ') {
                                    text.erase(text.begin());
                                }
                                chainedSource.setLine(lineNumber, text);
                            }
                        }
                        executeCompiledBasicProgram(loaded.program, loaded.sourceLinePerInstr, chainedSource, out);
                    }
                } catch (const std::exception &e) {
                    out << "\nRuntime error: CHAIN failed: " << e.what() << '\n';
                }
            }
        } catch (const PickVM::UserInterrupt &) {
            out << "\nBreak\n";
            session_.runtime().clearInterrupt();
            bool done = false;
            while (!done && session_.runtime().instructionPointer() < program.size()) {
                std::string line;
                std::istream &in = input();
                if (!readPromptedInputLine(in, out, "* ", line)) {
                    session_.runtime().setInstructionPointer(program.size());
                    break;
                }
                const std::vector<std::string> tokens = tokenizeDebuggerLine(line);
                done = handleBasicDebuggerCommand(tokens, program, sourceLinePerInstr, sourceProgram, out);
            }
        } catch (const std::runtime_error &e) {
            out << "\nRuntime error: " << e.what() << '\n';
            bool done = false;
            while (!done && session_.runtime().instructionPointer() < program.size()) {
                std::string line;
                std::istream &in = input();
                if (!readPromptedInputLine(in, out, "* ", line)) {
                    session_.runtime().setInstructionPointer(program.size());
                    break;
                }
                const std::vector<std::string> tokens = tokenizeDebuggerLine(line);
                done = handleBasicDebuggerCommand(tokens, program, sourceLinePerInstr, sourceProgram, out);
            }
        }

        std::signal(SIGINT, previousHandler); // restores SIG_IGN
        g_interruptRuntime.store(nullptr);
        session_.runtime().setOutputStream(nullptr);
        session_.runtime().setInputStream(nullptr);
    }

    bool Shell::runBasicUntilStop(const std::vector<PickVM::Instruction> &program,
                                  const std::vector<int> &sourceLinePerInstr,
                                  std::ostream &out,
                                  const bool stopAtNextBasicLine) {
        const SystemVarReaderScope sysScope(session_.runtime(), session_);
        bool steppedAny = false;
        const int initialLine = session_.runtime().currentSourceLine();
        while (session_.runtime().instructionPointer() < program.size()) {
            const std::size_t ip = session_.runtime().instructionPointer();
            const int sourceLine = (ip < sourceLinePerInstr.size()) ? sourceLinePerInstr[ip] : 0;
            const bool skipLineBreakpointOnce =
                basicResumePastLine_.has_value() && sourceLine > 0 && *basicResumePastLine_ == sourceLine;
            if (skipLineBreakpointOnce) {
                basicResumePastLine_.reset();
            } else if (sourceLine > 0 && basicBreakpoints_.count(sourceLine) != 0U) {
                out << "Breakpoint hit at line " << sourceLine << '\n';
                return false;
            }

            PickVM::LoadedBytecode loaded{program, {}, sourceLinePerInstr};
            const VmDebugService::StepResult stepResult = vmDebugService_.stepRuntime(basicTrace_, out, &loaded);
            if (stepResult.outcome == VmDebugService::StepOutcome::Halted) {
                return true;
            }
            if (stepResult.outcome == VmDebugService::StepOutcome::Interrupted) {
                out << "\nBreak\n";
                session_.runtime().clearInterrupt();
                return false;
            }
            if (stepResult.outcome == VmDebugService::StepOutcome::RuntimeError) {
                out << "\nRuntime error: " << stepResult.runtimeError << '\n';
                return false;
            }

            steppedAny = true;
            if (stopAtNextBasicLine) {
                const int currentLine = session_.runtime().currentSourceLine();
                if (currentLine > 0 && currentLine != initialLine && steppedAny) {
                    return false;
                }
            }
        }
        return true;
    }

    std::optional<int> Shell::parsePositiveInt(const std::string &token) {
        try {
            const int value = std::stoi(token);
            if (value > 0) {
                return value;
            }
        } catch (const std::exception &) {
        }
        return std::nullopt;
    }

    std::vector<std::string> Shell::tokenizeDebuggerLine(const std::string &line) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::set<int> Shell::sourceLinesFromMetadata(const std::vector<int> &sourceLinePerInstr) {
        std::set<int> lines;
        for (const int line: sourceLinePerInstr) {
            if (line > 0) {
                lines.insert(line);
            }
        }
        return lines;
    }

    bool Shell::parseBasicListRange(const std::vector<std::string> &tokens, int &startLine, int &endLine) {
        if (tokens.size() == 1) {
            startLine = 1;
            endLine = std::numeric_limits<int>::max();
            return true;
        }
        if (tokens.size() != 2) {
            return false;
        }
        const std::string &arg = tokens[1];
        const std::size_t dashPos = arg.find('-');
        if (dashPos == std::string::npos) {
            const std::optional<int> line = parsePositiveInt(arg);
            if (!line) {
                return false;
            }
            startLine = *line;
            endLine = *line;
            return true;
        }
        const std::string startToken = arg.substr(0, dashPos);
        const std::string endToken = arg.substr(dashPos + 1);
        const std::optional<int> start = parsePositiveInt(startToken);
        const std::optional<int> end = parsePositiveInt(endToken);
        if (!start || !end || *start > *end) {
            return false;
        }
        startLine = *start;
        endLine = *end;
        return true;
    }

    void Shell::listBasicSourceRange(const BasicProgram &program,
                                     const int startLine,
                                     const int endLine,
                                     std::ostream &out) {
        for (const auto &[lineNumber, text]: program.lines()) {
            if (lineNumber < startLine || lineNumber > endLine) {
                continue;
            }
            out << lineNumber;
            if (!text.empty()) {
                out << ' ' << text;
            }
            out << '\n';
        }
    }

    std::optional<std::size_t> Shell::firstInstructionForSourceLine(const std::vector<int> &sourceLinePerInstr,
                                                                     const int sourceLine) {
        for (std::size_t i = 0; i < sourceLinePerInstr.size(); ++i) {
            if (sourceLinePerInstr[i] == sourceLine) {
                return i;
            }
        }
        return std::nullopt;
    }

    bool Shell::handleBasicDebuggerCommand(const std::vector<std::string> &tokens,
                                           const std::vector<PickVM::Instruction> &program,
                                           const std::vector<int> &sourceLinePerInstr,
                                           const BasicProgram &sourceProgram,
                                           std::ostream &out) {
        if (tokens.empty()) {
            return false;
        }

        const std::string &cmd = tokens[0];
        if (cmd == "CONT") {
            const bool finished = runBasicUntilStop(program, sourceLinePerInstr, out, false);
            return finished;
        }
        if (cmd == "STEP") {
            const bool finished = runBasicUntilStop(program, sourceLinePerInstr, out, true);
            return finished;
        }
        if (cmd == "BREAKPOINT") {
            if (tokens.size() != 2) {
                out << "BREAKPOINT requires a BASIC line number\n";
                return false;
            }
            const std::optional<int> line = parsePositiveInt(tokens[1]);
            if (!line) {
                out << "BREAKPOINT requires a BASIC line number\n";
                return false;
            }
            const std::set<int> knownLines = sourceLinesFromMetadata(sourceLinePerInstr);
            if (knownLines.count(*line) == 0U) {
                out << "No such BASIC line in compiled program\n";
                return false;
            }
            basicBreakpoints_.insert(*line);
            return false;
        }
        if (cmd == "BREAKPOINTS") {
            if (basicBreakpoints_.empty()) {
                out << "No breakpoints\n";
                return false;
            }
            std::vector<int> sorted(basicBreakpoints_.begin(), basicBreakpoints_.end());
            std::sort(sorted.begin(), sorted.end());
            out << "Breakpoints:";
            for (const int line: sorted) {
                out << ' ' << line;
            }
            out << '\n';
            return false;
        }
        if (cmd == "CLEAR-BREAKPOINT") {
            if (tokens.size() != 2) {
                out << "CLEAR-BREAKPOINT requires a BASIC line number\n";
                return false;
            }
            const std::optional<int> line = parsePositiveInt(tokens[1]);
            if (!line) {
                out << "CLEAR-BREAKPOINT requires a BASIC line number\n";
                return false;
            }
            if (basicBreakpoints_.erase(*line) == 0U) {
                out << "No such breakpoint\n";
            }
            return false;
        }
        if (cmd == "CLEAR-BREAKPOINTS") {
            basicBreakpoints_.clear();
            return false;
        }
        if (cmd == "TRACE") {
            if (tokens.size() != 2 || (tokens[1] != "ON" && tokens[1] != "OFF")) {
                out << "TRACE requires ON or OFF\n";
                return false;
            }
            basicTrace_ = (tokens[1] == "ON");
            return false;
        }
        if (cmd == "LIST") {
            int startLine = 1;
            int endLine = std::numeric_limits<int>::max();
            if (!parseBasicListRange(tokens, startLine, endLine)) {
                out << "LIST takes no args, LIST <line>, or LIST <start-end>\n";
                return false;
            }
            listBasicSourceRange(sourceProgram, startLine, endLine, out);
            return false;
        }
        if (cmd == "GOTO") {
            if (tokens.size() != 2) {
                out << "GOTO requires a BASIC line number\n";
                return false;
            }
            const std::optional<int> line = parsePositiveInt(tokens[1]);
            if (!line) {
                out << "GOTO requires a BASIC line number\n";
                return false;
            }
            const std::optional<std::size_t> ip = firstInstructionForSourceLine(sourceLinePerInstr, *line);
            if (!ip) {
                out << "No such BASIC line in compiled program\n";
                return false;
            }
            session_.runtime().setInstructionPointer(*ip);
            return false;
        }
        if (cmd == "QUIT" || cmd == "END") {
            session_.runtime().setInstructionPointer(program.size());
            return true;
        }
        if (cmd == "HELP") {
            out << "Debugger commands:\n";
            out << "  CONT\n";
            out << "  STEP\n";
            out << "  BREAKPOINT <line>\n";
            out << "  BREAKPOINTS\n";
            out << "  CLEAR-BREAKPOINT <line>\n";
            out << "  CLEAR-BREAKPOINTS\n";
            out << "  TRACE ON|OFF\n";
            out << "  LIST [line|start-end]\n";
            out << "  GOTO <line>\n";
            out << "  QUIT\n";
            return false;
        }

        out << "Unknown debugger command: " << cmd << '\n';
        return false;
    }

    namespace {
        bool echoVarNameChar(const char c) {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
        }
    } // namespace

    std::string Shell::expandEchoToken(const std::string &token) const {
        std::string out;
        out.reserve(token.size());
        for (std::size_t i = 0; i < token.size();) {
            if (token[i] == '$' && i + 1 < token.size() && token[i + 1] == '$') {
                out.push_back('$');
                i += 2;
                continue;
            }
            if (token[i] == '$' && i + 1 < token.size() && (echoVarNameChar(token[i + 1]) || token[i + 1] == '@')) {
                std::size_t j = i + 1;
                if (j < token.size() && token[j] == '@') {
                    ++j;
                }
                while (j < token.size() && echoVarNameChar(token[j])) {
                    ++j;
                }
                if (j > i + 1) {
                    const std::string name(token.data() + i + 1, j - i - 1);
                    const std::optional<std::string> ev = session_.env().get(name);
                    if (ev.has_value()) {
                        out += *ev;
                    } else if (GeminiSession::isSessionSystemVariableName(name)) {
                        out += session_.resolveSystemVariable(name).value_or("");
                    } else {
                        out += "";
                    }
                    i = j;
                    continue;
                }
            }
            if (token[i] == '$') {
                out.push_back('$');
                ++i;
                continue;
            }
            out.push_back(token[i]);
            ++i;
        }
        return out;
    }

    void Shell::cmdEcho(const std::vector<std::string> &tokens, std::ostream &out) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            out << expandEchoToken(tokens[i]);
            if (i + 1 < tokens.size()) {
                out << ' ';
            }
        }
        out << "\n";
    }

    void Shell::cmdRun(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() == 1) {
            if (!session_.suspended_) {
                out << "RUN requires a filename\n";
                return;
            }
            if (!session_.programImageLoaded()) {
                session_.suspended_ = false;
                out << "RUN requires a filename\n";
                return;
            }
            session_.resumePastBreakpointIp_ = session_.runtime().instructionPointer();
            session_.runtime().setOutputStream(&out);
            session_.runtime().setInputStream(inputStream_);
            {
                const SystemVarReaderScope sysScope(session_.runtime(), session_);
                session_.executeVmLoop(out);
            }
            session_.runtime().setOutputStream(nullptr);
            session_.runtime().setInputStream(nullptr);
            return;
        }
        if (tokens.size() != 2) {
            out << "RUN takes exactly one program name\n";
            return;
        }

        const std::string &programName = tokens[1];
        if (programName.empty()) {
            out << "RUN requires a program name\n";
            return;
        }
        if (programName.find('.') != std::string::npos) {
            out << "RUN expects a program name without extension\n";
            return;
        }
        if (!ensureProgramObjectExistsForRun(programName, out)) {
            return;
        }

        try {
            const PickVoc::VocResolver::ProgramLocation resolved = session_.vocResolver().resolveProgramLocation(programName);
            const BasicShell::ProgramLocation location{resolved.fileName, resolved.recordKey};
            const std::optional<std::string> objectText = readProgramRecord(location, true);
            if (!objectText.has_value()) {
                out << "Error: Cannot open bytecode file for program: " << programName << "\n";
                return;
            }
            PickVM::Parser parser;
            std::istringstream bytecodeStream(*objectText);
            PickVM::LoadedBytecode loaded = parser.parse(bytecodeStream);
            session_.pruneBreakpointsForProgram(loaded.program.size(), out);
            session_.lastLoaded_ = std::move(loaded);
            session_.runtime().setOutputStream(&out);
            session_.runtime().setInputStream(inputStream_);
            session_.runtime().loadProgram(session_.lastLoaded_->program, session_.lastLoaded_->sourceLinePerInstr);
            session_.suspended_ = false;
            session_.resumePastBreakpointIp_.reset();
            {
                const SystemVarReaderScope sysScope(session_.runtime(), session_);
                session_.executeVmLoop(out);
            }
            session_.runtime().setOutputStream(nullptr);
            session_.runtime().setInputStream(nullptr);
        } catch (const std::exception &e) {
            session_.runtime().setOutputStream(nullptr);
            session_.runtime().setInputStream(nullptr);
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdProc(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "PROC requires a program name\n";
            return;
        }
        if (tokens[1].empty() || tokens[1].find('.') != std::string::npos) {
            out << "PROC expects a program name without extension\n";
            return;
        }

        const PickVoc::VocResolver::ProgramLocation resolved = session_.vocResolver().resolveProcScriptLocation(tokens[1]);
        const BasicShell::ProgramLocation location{resolved.fileName, resolved.recordKey};
        const std::optional<std::string> scriptText = readProgramRecord(location, false);
        if (!scriptText.has_value()) {
            out << "Error: Cannot open PROC script: " << tokens[1] << "\n";
            return;
        }

        std::vector<std::string> lines;
        std::istringstream scriptStream(*scriptText);
        for (std::string line; std::getline(scriptStream, line);) {
            lines.push_back(line);
        }

        std::vector<std::string> params;
        params.reserve(tokens.size() > 2 ? tokens.size() - 2 : 0);
        for (std::size_t i = 2; i < tokens.size(); ++i) {
            params.push_back(tokens[i]);
        }
        resetProcReadNextCursor();

        const ProcInterpreter::SessionAtFn sessionAt = [this](const std::string_view n) {
            return session_.resolveSystemVariable(n);
        };
        (void) procInterpreter_.runScript(lines, params, inputStream_, out,
                                          [this](const std::string &tclLine, std::ostream &procOut) {
                                              executeProcTclCommand(tclLine, procOut);
                                          },
                                          sessionAt,
                                          [this](const std::string &fileName, std::string &error) {
                                              return procSelect(fileName, error);
                                          },
                                          [this](std::string &error) {
                                              return procReadNext(error);
                                          },
                                          [this](const std::string &fileName,
                                                 const std::string &recordKey,
                                                 std::string &recordBody,
                                                 std::string &hardError) {
                                              return procReadU(fileName, recordKey, recordBody, hardError);
                                          },
                                          [this](const std::string &fileName,
                                                 const std::string &recordKey,
                                                 const std::string &value,
                                                 std::string &hardError) {
                                              return procWriteU(fileName, recordKey, value, hardError);
                                          },
                                          [this](const std::string &fileName,
                                                 const std::string &recordKey,
                                                 std::string &hardError) {
                                              return procRelease(fileName, recordKey, hardError);
                                          });
    }

    ProcLockOutcome Shell::procReadU(const std::string &fileName,
                                     const std::string &recordKey,
                                     std::string &recordBody,
                                     std::string &hardError) {
        try {
            const std::optional<PickFS::Record> record =
                session_.fileSystem().readU(fileName, recordKey);
            if (!record.has_value()) {
                return ProcLockOutcome::MissingRecord;
            }
            recordBody = record->value();
            return ProcLockOutcome::Success;
        } catch (const PickFS::FileSystemError &ex) {
            if (std::string_view(ex.what()).find("RECORD LOCKED") != std::string_view::npos) {
                return ProcLockOutcome::Locked;
            }
            hardError = ex.what();
            return ProcLockOutcome::HardError;
        } catch (const std::exception &ex) {
            hardError = ex.what();
            return ProcLockOutcome::HardError;
        }
    }

    ProcLockOutcome Shell::procWriteU(const std::string &fileName,
                                      const std::string &recordKey,
                                      const std::string &value,
                                      std::string &hardError) {
        try {
            session_.fileSystem().writeU(fileName, PickFS::Record(recordKey, value));
            if (fileName == "VOC") {
                session_.vocResolver().invalidate();
            }
            return ProcLockOutcome::Success;
        } catch (const PickFS::FileSystemError &ex) {
            if (std::string_view(ex.what()).find("RECORD LOCKED") != std::string_view::npos) {
                return ProcLockOutcome::Locked;
            }
            hardError = ex.what();
            return ProcLockOutcome::HardError;
        } catch (const std::exception &ex) {
            hardError = ex.what();
            return ProcLockOutcome::HardError;
        }
    }

    bool Shell::procRelease(const std::string &fileName,
                            const std::string &recordKey,
                            std::string &hardError) {
        try {
            (void) session_.fileSystem().releaseRecord(fileName, recordKey);
            return true;
        } catch (const std::exception &ex) {
            hardError = ex.what();
            return false;
        }
    }

    bool Shell::procSelect(const std::string &fileName, std::string &error) {
        std::vector<std::string> tokens{"SELECT", fileName};
        PickCore::English::ParseContext pc;
        PickCore::English::EnglishRunOptions eo;
        const std::optional<PickCore::English::Result> result =
            englishService_.run(session_.fileSystem(), tokens, pc, eo, error);
        if (!result.has_value()) {
            return false;
        }
        session_.setActiveList(result->selectedIds, fileName);
        resetProcReadNextCursor();
        return true;
    }

    std::optional<std::string> Shell::procReadNext(std::string &error) {
        if (!session_.activeListSourceFile().has_value()) {
            error = "No active list";
            return std::nullopt;
        }
        if (procReadNextIndex_ >= session_.activeList().size()) {
            error.clear();
            return std::nullopt;
        }
        return session_.activeList()[procReadNextIndex_++];
    }

    void Shell::resetProcReadNextCursor() {
        procReadNextIndex_ = 0;
    }

    void Shell::executeProcTclCommand(const std::string &line, std::ostream &out) {
        const TokenizeResult parsed = tokenize(line);
        if (parsed.error.has_value()) {
            out << "Error: " << *parsed.error << "\n";
            return;
        }
        Tokens tokens = parsed.tokens;
        if (tokens.empty()) {
            return;
        }
        tokens[0] = session_.vocResolver().resolveVerbName(tokens[0]);
        bool quit = false;
        handleTclCommand(tokens, quit, out);
    }

    std::string Shell::programObjectRecordKey(const std::string &recordKey) {
        return recordKey + "_OBJ";
    }

    std::optional<std::string> Shell::readProgramRecord(const BasicShell::ProgramLocation &location,
                                                        const bool objectRecord) {
        const std::string recordKey = objectRecord ? programObjectRecordKey(location.recordKey) : location.recordKey;
        try {
            const std::optional<PickFS::Record> rec = session_.fileSystem().read(location.fileName, recordKey);
            if (!rec.has_value()) {
                return std::nullopt;
            }
            return rec->value();
        } catch (const PickFS::FileSystemError &) {
            return std::nullopt;
        }
    }

    bool Shell::writeProgramRecord(const BasicShell::ProgramLocation &location,
                                   const bool objectRecord,
                                   const std::string &payload,
                                   std::string &error) {
        const std::string recordKey = objectRecord ? programObjectRecordKey(location.recordKey) : location.recordKey;
        try {
            try {
                (void) session_.fileSystem().openFile(location.fileName);
            } catch (const PickFS::FileSystemError &openError) {
                if (std::string(openError.what()).find("File not found: ") != 0) {
                    throw;
                }
                session_.fileSystem().createFile(location.fileName);
            }
            session_.fileSystem().write(location.fileName, PickFS::Record(recordKey, payload));
            if (location.fileName == "VOC") {
                session_.vocResolver().invalidate();
            }
            return true;
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
    }

    bool Shell::ensureProgramObjectExistsForRun(const std::string &programName, std::ostream &out) {
        const PickVoc::VocResolver::ProgramLocation resolved = session_.vocResolver().resolveProgramLocation(programName);
        const BasicShell::ProgramLocation location{resolved.fileName, resolved.recordKey};
        if (readProgramRecord(location, true).has_value()) {
            return true;
        }

        const std::optional<std::string> sourceText = readProgramRecord(location, false);
        if (!sourceText.has_value()) {
            out << "Error: Cannot open bytecode file for program: " << programName << "\n";
            return false;
        }

        BasicProgram program;
        program.setName(programName);
        std::istringstream in(*sourceText);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream iss(line);
            std::string lineNumberToken;
            if (!(iss >> lineNumberToken)) {
                continue;
            }
            int lineNumber = 0;
            try {
                lineNumber = std::stoi(lineNumberToken);
            } catch (const std::exception &) {
                continue;
            }
            if (lineNumber <= 0) {
                continue;
            }
            std::string text;
            std::getline(iss, text);
            if (!text.empty() && text.front() == ' ') {
                text.erase(text.begin());
            }
            program.setLine(lineNumber, text);
        }

        BasicCompiler compiler;
        const BasicCompileResult compile = compiler.compile(program);
        if (!compile.success) {
            for (const auto &err: compile.errors) {
                out << "Error on line " << err.line << ": " << err.message << "\n";
            }
            out << "Compilation failed.\n";
            return false;
        }

        std::string error;
        if (!writeProgramRecord(location,
                                true,
                                PickVM::serializeBytecodeText(compile.program, compile.sourceLinePerInstr),
                                error)) {
            out << "Error: " << error << "\n";
            return false;
        }
        return true;
    }

    void Shell::cmdSet(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "SET requires a variable name\n";
            return;
        }
        // Typed system setting (M8 Stage 2): `SET PAGE-LENGTH n` configures the ENGLISH
        // report formatter's lines-per-page. Validated to a positive integer; stored on
        // the session, not in the env_ string store, so it does not appear in LIST-VARS.
        if (kwEq(tokens[1], "PAGE-LENGTH")) {
            if (tokens.size() != 3) {
                out << "SET PAGE-LENGTH requires a positive integer\n";
                return;
            }
            int n = 0;
            try {
                std::size_t pos = 0;
                n = std::stoi(tokens[2], &pos);
                if (pos != tokens[2].size() || n < 1) {
                    throw std::invalid_argument("");
                }
            } catch (...) {
                out << "SET PAGE-LENGTH requires a positive integer\n";
                return;
            }
            session_.setReportPageLength(n);
            return;
        }
        if (GeminiSession::isSessionSystemVariableName(tokens[1])) {
            out << "Read-only system variable\n";
            return;
        }
        std::string value;
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (i > 2) {
                value += ' ';
            }
            value += tokens[i];
        }
        if (!session_.env().set(tokens[1], std::move(value))) {
            out << "Invalid variable name\n";
        }
    }

    void Shell::cmdGet(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "GET requires a variable name\n";
            return;
        }
        if (kwEq(tokens[1], "PAGE-LENGTH")) {
            out << session_.reportPageLength() << '\n';
            return;
        }
        if (const std::optional<std::string> sys = session_.resolveSystemVariable(tokens[1])) {
            out << *sys << '\n';
            return;
        }
        const std::optional<std::string> val = session_.env().get(tokens[1]);
        if (!val) {
            out << "No such variable: " << tokens[1] << '\n';
            return;
        }
        out << *val << '\n';
    }

    void Shell::cmdListVars(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() > 1) {
            out << "LIST-VARS takes no arguments\n";
            return;
        }
        std::vector<std::string> names = session_.env().names();
        if (session_.loggedIn()) {
            names.push_back("@ACCOUNT");
            names.push_back("@LOGNAME");
            names.push_back("@USERNO");
        }
        if (session_.defaultDataFile().has_value()) {
            names.push_back("@DEFDATA");
        }
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        if (names.empty()) {
            out << "No variables\n";
            return;
        }
        out << "Variables:\n";
        for (const std::string &n: names) {
            out << "  " << n << '\n';
        }
    }

    void Shell::cmdUnset(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "UNSET requires a variable name\n";
            return;
        }
        if (GeminiSession::isSessionSystemVariableName(tokens[1])) {
            out << "Read-only system variable\n";
            return;
        }
        if (!session_.env().unset(tokens[1])) {
            out << "No such variable\n";
        }
    }

    void Shell::cmdHelpLookup(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() == 1) {
            if (const std::optional<std::string> zero =
                    HelpTopics::resolveHelpBody(session_.fileSystem(),
                                                session_.geminiCatalogRoot(),
                                                session_.fileSystemRoot(),
                                                "HELP")) {
                out << *zero;
                return;
            }
            out << HelpTopics::helpZeroArgumentFallbackLine();
            return;
        }

        const std::string display = HelpTopics::joinOperandsDisplay(tokens, 1);
        std::string canonical =
            HelpTopics::canonicalTopicKey(session_.vocResolver(), tokens, 1);
        if (tokens.size() == 2 && kwEq(tokens[1], "COMMANDS")) {
            canonical = "HELP COMMANDS";
        }
        std::optional<std::string> body = HelpTopics::resolveHelpBody(session_.fileSystem(),
                                                                      session_.geminiCatalogRoot(),
                                                                      session_.fileSystemRoot(),
                                                                      canonical);
        if (!body && canonical == "HELP COMMANDS") {
            body = generatedHelpCommandsBody();
        }
        if (body.has_value()) {
            out << *body;
            return;
        }
        out << "No help available for \"" << display << "\".\n";
    }

    std::string Shell::generatedHelpCommandsBody() {
        static constexpr std::string_view kIntro =
            "HELP COMMANDS lists the TCL command verbs available in this account.\n"
            "It shows the command names only; use HELP <command> for usage and details.\n"
            "This topic reflects the current VOC and command surface, and may differ\n"
            "between accounts depending on local overrides or site configuration.\n";

        std::set<std::string> names;
        for (const auto &entry: tclCommands_) {
            names.insert(entry.first);
        }
        for (const auto &vocPair: session_.vocResolver().table()) {
            const std::string &vocKey = vocPair.first;
            std::string resolved = session_.vocResolver().resolveVerbName(vocKey);
            asciiUpperInPlace(resolved);
            if (tclCommands_.find(resolved) != tclCommands_.end()) {
                names.insert(vocKey);
            }
        }

        std::string out{kIntro};
        for (const std::string &n: names) {
            out += n;
            out += '\n';
        }
        return out;
    }

    void Shell::cmdHelpList(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 1) {
            out << "HELP-LIST takes no arguments\n";
            return;
        }
        const std::vector<std::string> topics = HelpTopics::listCanonicalHelpTopicsSorted(session_.fileSystem());
        if (topics.empty()) {
            out << "No HELP topics\n";
            return;
        }
        for (const std::string &t: topics) {
            out << t << '\n';
        }
    }

    void Shell::cmdHelpEdit(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "HELP-EDIT requires a topic\n";
            return;
        }
        const std::string canonical =
            HelpTopics::canonicalTopicKey(session_.vocResolver(), tokens, 1);
        const std::string storage = HelpTopics::storageRecordIdFromCanonical(canonical);
        try {
            try {
                (void) session_.fileSystem().openFile("HELP");
            } catch (const PickFS::FileSystemError &openError) {
                const std::string msg = openError.what();
                if (msg.find("File not found: ") != 0U) {
                    throw;
                }
                session_.fileSystem().createFile("HELP");
            }
            runLineRecordEditorForLocation("HELP", storage, std::nullopt, out);
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdVersion(std::ostream &out) {
        out << pick_system::system_title << " " << pick_system::version_string << "\n";
        out << "Build: " << __DATE__ << "\n";
    }

    void Shell::cmdSystem(const Tokens &tokens, std::ostream &out, bool &quit) {
        if (tokens.size() == 1) {
            out << "System: " << pick_system::system_title << "\n";
            out << "Version: " << pick_system::version_string << "\n";
            out << "Build: " << __DATE__ << "\n";
            if (session_.loggedIn()) {
                out << "Session: " << session_.whoPort() << ' ' << session_.sessionUsername() << ' '
                    << session_.sessionAccount() << "\n";
            } else {
                out << "Session: 0 - -\n";
            }
            out << "Pick root: " << session_.fileSystemRoot().string() << "\n";
            out << "Account context: " << (session_.loggedIn() ? session_.sessionAccount() : "(none)") << "\n";
            out << "Catalogue root: "
                << (session_.geminiCatalogRoot().has_value() ? session_.geminiCatalogRoot()->string() : "(none)")
                << "\n";
            return;
        }

        if (tokens.size() >= 2 && tokens[1] == "LANGUAGES") {
            bool verbose = false;
            if (tokens.size() == 3) {
                if (tokens[2] == "VERBOSE") {
                    verbose = true;
                } else {
                    out << "SYSTEM: unknown subcommand \"" << tokens[2] << "\"\n";
                    return;
                }
            } else if (tokens.size() > 3) {
                out << "SYSTEM LANGUAGES takes at most one argument\n";
                return;
            }

            PickCore::Languages::formatLanguagesReport(out,
                                                         PickCore::Languages::LanguageRegistry::instance(),
                                                         PickCore::Languages::LanguageModuleBootLog::instance(),
                                                         verbose);
            return;
        }

        if (tokens.size() == 2 && tokens[1] == "STATUS") {
            cmdStatus(out);
            return;
        }

        if (tokens.size() == 2 && tokens[1] == "SHUTDOWN") {
            cmdShutdown(out, quit);
            return;
        }

        out << "SYSTEM: unknown subcommand \"" << tokens[1] << "\"\n";
    }

    void Shell::cmdShowModules(std::ostream &out) {
        PickCore::Languages::formatLanguagesReport(out,
                                                   PickCore::Languages::LanguageRegistry::instance(),
                                                   PickCore::Languages::LanguageModuleBootLog::instance(),
                                                   false);
    }

    void Shell::cmdWho(std::ostream &out) {
        if (!session_.loggedIn()) {
            out << "0 - -\n";
            return;
        }
        out << session_.whoPort() << ' ' << session_.sessionUsername() << ' ' << session_.sessionAccount() << '\n';
    }

    bool Shell::requireSysprog(std::ostream &out, const std::string_view command) const {
        if (session_.loggedIn() && session_.sessionAccount() == "SYSPROG") {
            return true;
        }
        out << command << " requires SYSPROG\n";
        return false;
    }

    const char *Shell::runStateLabel(const PickCore::SessionRunState state) {
        switch (state) {
            case PickCore::SessionRunState::Runnable:
                return "Runnable";
            case PickCore::SessionRunState::Running:
                return "Running";
            case PickCore::SessionRunState::WaitingForInput:
                return "WaitingForInput";
        }
        return "Runnable";
    }

    void Shell::formatListSessions(const std::vector<AdminSessionRow> &rows, std::ostream &out) const {
        out << "PORT BOUND USER ACCOUNT STATE\n";
        for (const AdminSessionRow &row : rows) {
            out << row.port << ' ' << (row.consoleBound ? "yes" : "no") << ' ';
            if (row.loggedIn) {
                out << row.username << ' ' << row.account;
            } else {
                out << "- -";
            }
            out << ' ' << runStateLabel(row.runState) << '\n';
        }
    }

    void Shell::formatStatus(const AdminDaemonStatus &status, std::ostream &out) const {
        out << "Gemini System " << status.version << '\n';
        out << "sessions=" << status.sessionCount << " maxSessions=" << status.maxSessions << '\n';
        out << "socket=";
        if (status.socketPath.empty()) {
            out << "none";
        } else {
            out << status.socketPath.string();
        }
        out << '\n';
    }

    std::vector<AdminSessionRow> Shell::fallbackAdminSessions() const {
        AdminSessionRow row;
        row.port = static_cast<PickCore::SessionId>(session_.whoPort());
        row.consoleBound = false;
        row.loggedIn = session_.loggedIn();
        if (row.loggedIn) {
            row.username = session_.sessionUsername();
            row.account = session_.sessionAccount();
        }
        row.runState = PickCore::SessionRunState::Runnable;
        return {std::move(row)};
    }

    AdminDaemonStatus Shell::fallbackAdminStatus() const {
        AdminDaemonStatus status;
        status.maxSessions = 1;
        status.sessionCount = 1;
        status.version = pick_system::version_string;
        return status;
    }

    void Shell::cmdListSessions(std::ostream &out) {
        if (!requireSysprog(out, "LISTSESSIONS")) {
            return;
        }
        const std::vector<AdminSessionRow> rows =
            adminQueries_.listSessions ? adminQueries_.listSessions() : fallbackAdminSessions();
        formatListSessions(rows, out);
    }

    void Shell::cmdStatus(std::ostream &out) {
        if (!requireSysprog(out, "STATUS")) {
            return;
        }
        const AdminDaemonStatus status = adminQueries_.status ? adminQueries_.status() : fallbackAdminStatus();
        formatStatus(status, out);
    }

    void Shell::cmdKillSession(const Tokens &tokens, std::ostream &out) {
        if (!requireSysprog(out, "KILLSESSION")) {
            return;
        }
        if (tokens.size() != 2) {
            out << "KILLSESSION requires a session port\n";
            return;
        }
        const std::optional<int> portValue = parsePositiveInt(tokens[1]);
        if (!portValue.has_value()) {
            out << "KILLSESSION requires a session port\n";
            return;
        }
        const auto port = static_cast<PickCore::SessionId>(*portValue);
        if (static_cast<int>(port) == session_.whoPort()) {
            out << "KILLSESSION: cannot kill current session\n";
            return;
        }
        if (!adminQueries_.killSession) {
            out << "KILLSESSION: not available\n";
            return;
        }
        std::string error;
        if (!adminQueries_.killSession(port, error)) {
            out << "KILLSESSION: " << error << '\n';
            return;
        }
        out << "Session " << port << " killed\n";
    }

    void Shell::cmdShutdown(std::ostream &out, bool &quit) {
        if (!requireSysprog(out, "SHUTDOWN")) {
            return;
        }
        if (!adminQueries_.requestShutdown) {
            out << "SHUTDOWN: not available\n";
            return;
        }
        adminQueries_.requestShutdown();
        out << "Shutting down\n";
        quit = true;
    }

    void Shell::cmdDumpStack(std::ostream &out) {
        session_.runtime().setOutputStream(&out);
        session_.runtime().dumpStack();
        session_.runtime().setOutputStream(nullptr);
    }

    void Shell::cmdListPrograms(std::ostream &out) {
        std::set<std::string> programNames;
        const std::vector<std::string> files = session_.vocResolver().listProgramFiles();
        for (const std::string &fileName: files) {
            try {
                const std::vector<std::string> records = session_.fileSystem().listRecordNames(fileName);
                for (const std::string &recordName: records) {
                    if (recordName.size() >= 4 && recordName.substr(recordName.size() - 4) == "_OBJ") {
                        continue;
                    }
                    programNames.insert(recordName);
                }
            } catch (const PickFS::FileSystemError &) {
            }
        }

        out << "Programs:\n";
        for (const auto &name: programNames) {
            out << "  " << name << "\n";
        }
    }

    void Shell::cmdCreateFile(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "CREATE-FILE requires a filename\n";
            return;
        }
        try {
            session_.fileSystem().createFile(tokens[1]);
            if (tokens[1] == "VOC") {
                session_.vocResolver().invalidate();
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdDeleteFile(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "DELETE-FILE requires a filename\n";
            return;
        }
        try {
            session_.fileSystem().deleteFile(tokens[1]);
            if (tokens[1] == "VOC") {
                session_.vocResolver().invalidate();
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdListFiles(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() > 1) {
            out << "LIST-FILES takes no arguments\n";
            return;
        }
        try {
            const std::vector<std::string> names = session_.fileSystem().listFiles();
            if (names.empty()) {
                out << "No files\n";
                return;
            }
            out << "Files:\n";
            for (const std::string &name: names) {
                out << "  " << name << '\n';
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdList(const std::vector<std::string> &tokens, std::ostream &out) {
        if (shellIsEnglishList(tokens, session_)) {
            PickCore::English::ParseContext pc;
            PickCore::English::EnglishRunOptions eo;
            eo.pageLength = session_.reportPageLength();
            if (const std::optional<std::string> impErr = englishImplicitSetup(session_, tokens, pc, eo)) {
                out << "Error: " << *impErr << "\n";
                return;
            }
            std::string error;
            const std::optional<PickCore::English::Result> result =
                englishService_.run(session_.fileSystem(), tokens, pc, eo, error);
            if (!result.has_value()) {
                out << "Error: " << error << "\n";
                return;
            }
            for (const std::string &line: result->lines) {
                out << line << '\n';
            }
            return;
        }
        if (tokens.size() != 2) {
            out << "LIST requires a filename\n";
            return;
        }
        try {
            const std::vector<std::string> names = session_.fileSystem().listRecordNames(tokens[1]);
            if (names.empty()) {
                out << "No records\n";
                return;
            }
            out << "Records:\n";
            for (const std::string &name: names) {
                out << "  " << name << '\n';
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdCount(const std::vector<std::string> &tokens, std::ostream &out) {
        if (!shellIsEnglishCount(tokens, session_)) {
            out << "COUNT requires a filename or active-list scope\n";
            return;
        }
        PickCore::English::ParseContext pc;
        PickCore::English::EnglishRunOptions eo;
        eo.pageLength = session_.reportPageLength();
        if (const std::optional<std::string> impErr = englishImplicitSetup(session_, tokens, pc, eo)) {
            out << "Error: " << *impErr << "\n";
            return;
        }
        std::string error;
        const std::optional<PickCore::English::Result> result =
            englishService_.run(session_.fileSystem(), tokens, pc, eo, error);
        if (!result.has_value()) {
            out << "Error: " << error << "\n";
            return;
        }
        for (const std::string &line: result->lines) {
            out << line << '\n';
        }
    }

    void Shell::cmdSelect(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "SELECT requires a filename\n";
            return;
        }
        PickCore::English::ParseContext pc;
        PickCore::English::EnglishRunOptions eo;
        eo.pageLength = session_.reportPageLength();
        std::string error;
        const std::optional<PickCore::English::Result> result =
            englishService_.run(session_.fileSystem(), tokens, pc, eo, error);
        if (!result.has_value()) {
            out << "Error: " << error << "\n";
            return;
        }
        session_.setActiveList(result->selectedIds, tokens[1]);
        for (const std::string &line: result->lines) {
            out << line << '\n';
        }
    }

    void Shell::cmdSort(const std::vector<std::string> &tokens, std::ostream &out) {
        if (!shellIsEnglishSort(tokens, session_)) {
            out << "SORT: ENGLISH syntax not detected; legacy Tcl SORT is not implemented\n";
            return;
        }
        PickCore::English::ParseContext pc;
        PickCore::English::EnglishRunOptions eo;
        eo.pageLength = session_.reportPageLength();
        if (const std::optional<std::string> impErr = englishImplicitSetup(session_, tokens, pc, eo)) {
            out << "Error: " << *impErr << "\n";
            return;
        }
        std::string error;
        const std::optional<PickCore::English::Result> result =
            englishService_.run(session_.fileSystem(), tokens, pc, eo, error);
        if (!result.has_value()) {
            out << "Error: " << error << "\n";
            return;
        }
        for (const std::string &line: result->lines) {
            out << line << '\n';
        }
    }

    void Shell::cmdListList(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() > 1) {
            out << "LIST-LIST takes no arguments\n";
            return;
        }
        if (session_.activeList().empty()) {
            out << "No active list\n";
            return;
        }
        out << "Active list:\n";
        for (const std::string &id: session_.activeList()) {
            out << "  " << id << '\n';
        }
    }

    void Shell::cmdClearList(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() > 1) {
            out << "CLEAR-LIST takes no arguments\n";
            return;
        }
        session_.clearActiveList();
        out << "Active list cleared\n";
    }

    void Shell::cmdResolveField(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 3) {
            out << "RESOLVE-FIELD takes <data-file> <field-token>\n";
            return;
        }
        const PickCore::English::DictionaryResolver &resolver = englishService_.dictionaryResolver();
        const PickCore::English::FieldRef ref =
            resolver.resolveField(session_.fileSystem(), tokens[1], tokens[2]);

        std::optional<PickFS::Record> dictRec;
        const std::string scopedDict =
            PickCore::English::DictionaryResolver::scopedDictLogicalName(tokens[1]);
        try {
            (void) session_.fileSystem().openFile(scopedDict);
            dictRec = session_.fileSystem().read(scopedDict, tokens[2]);
        } catch (const PickFS::FileSystemError &) {
            // fall through to global DICT
        }
        if (!dictRec.has_value()) {
            try {
                dictRec = session_.fileSystem().read("DICT", tokens[2]);
            } catch (const PickFS::FileSystemError &) {
                // no DICT row for this token
            }
        }

        out << "Data file: " << tokens[1] << '\n';
        out << "Field token: " << tokens[2] << '\n';
        out << "File-scoped dict: " << scopedDict << '\n';

        if (PickCore::English::fieldRefIsResolved(ref)) {
            out << "Field kind: " << PickCore::English::DictionaryResolver::describeFieldKind(ref) << '\n';
            if (ref.kind == PickCore::English::DictFieldKind::FCorrelative && ref.fCorrelative.has_value()) {
                out << "Source attribute: " << ref.fCorrelative->sourceAttributeNo << '\n';
                out << "Selector: "
                    << PickCore::English::DictionaryResolver::describeFSelector(*ref.fCorrelative) << '\n';
                out << "Tail (raw): " << ref.fCorrelative->tailRaw << '\n';
            } else if (ref.kind == PickCore::English::DictFieldKind::ICorrelative &&
                       ref.iCorrelative.has_value()) {
                out << "Expression: "
                    << PickCore::English::DictionaryResolver::describeIExpression(*ref.iCorrelative) << '\n';
            } else if (ref.attributeNo.has_value()) {
                out << "Resolved attribute: " << *ref.attributeNo << '\n';
            }
        } else if (dictRec.has_value()) {
            const PickCore::English::DictItemSummary summary =
                PickCore::English::DictItemClassifier::classify(dictRec->structured());
            const std::string kindLabel =
                summary.typeLabel == "INVALID" ? "unknown" : summary.typeLabel;
            out << "Field kind: " << kindLabel << '\n';
            if (!summary.valid) {
                out << "Validity: INVALID\n";
                if (!summary.error.empty()) {
                    out << "Error: " << summary.error << '\n';
                }
            }
            if (summary.typeLabel == "F") {
                out << "Source attribute: " << dictRec->structured().attribute(2).firstValue() << '\n';
                out << "Tail (raw): " << dictRec->structured().attribute(3).firstValue() << '\n';
            } else if (summary.typeLabel == "I") {
                const std::string expression = dictRec->structured().attribute(2).firstValue();
                out << "Expression: " << (expression.empty() ? "(empty)" : expression) << '\n';
            } else {
                out << "Resolved attribute: (none — unknown field)\n";
            }
        } else {
            out << "Field kind: " << PickCore::English::DictionaryResolver::describeFieldKind(ref) << '\n';
            out << "Resolved attribute: (none — unknown field)\n";
        }
        out << "Conversion hint (D/MD/MC): "
            << PickCore::English::DictionaryResolver::describeConversion(ref) << '\n';
    }

    void Shell::cmdDefineField(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 4) {
            out << "DEFINE-FIELD takes <dict-file> <field-name> <attribute-number>\n";
            return;
        }
        const std::string &dictFile = tokens[1];
        const std::string &fieldName = tokens[2];
        const std::string &attrToken = tokens[3];
        const std::optional<int> attrNo = parsePositiveInt(attrToken);
        if (!attrNo.has_value()) {
            out << "DEFINE-FIELD requires a positive integer attribute-number\n";
            return;
        }
        try {
            (void) session_.fileSystem().openFile(dictFile);
        } catch (const PickFS::FileSystemError &) {
            out << "DEFINE-FIELD requires dictionary file " << dictFile << "; use CREATE-FILE first\n";
            return;
        }
        const std::string body = std::string{"A\n"} + std::to_string(*attrNo) + '\n';
        try {
            session_.fileSystem().write(dictFile, PickFS::Record(fieldName, body));
        } catch (const PickFS::FileSystemError &e) {
            out << "Error: " << e.what() << '\n';
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << '\n';
        }
    }

    void Shell::cmdListDict(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "LIST-DICT takes <dict-file>\n";
            return;
        }
        const std::string &dictFile = tokens[1];
        try {
            const PickFS::FileSystem::FileHandle handle = session_.fileSystem().openFile(dictFile);
            std::vector<std::string> ids = session_.fileSystem().listRecords(handle);
            std::sort(ids.begin(), ids.end());
            for (const std::string &id: ids) {
                const std::optional<PickFS::Record> rec = session_.fileSystem().readRecord(handle, id);
                if (!rec.has_value()) {
                    out << id << " INVALID INVALID\n";
                    continue;
                }
                const PickCore::English::DictItemSummary summary =
                    PickCore::English::DictItemClassifier::classify(rec->structured());
                out << id << ' ' << summary.typeLabel << ' '
                    << (summary.valid ? "VALID" : "INVALID") << '\n';
            }
        } catch (const PickFS::FileSystemError &) {
            out << "LIST-DICT requires dictionary file " << dictFile << "; use CREATE-FILE first\n";
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    namespace {
        std::optional<char> canonicalVocType(const std::string &token) {
            if (token.empty()) {
                return std::nullopt;
            }
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
            if (token.size() == 1 && (c == 'A' || c == 'D' || c == 'F' || c == 'Q' || c == 'V' || c == 'X')) {
                return c;
            }
            return std::nullopt;
        }

        std::string detectVocTypeOrInvalid(const std::string &raw) {
            std::istringstream in(raw);
            std::string first;
            if (!std::getline(in, first)) {
                return "INVALID";
            }
            while (!first.empty() && (first.back() == '\r' || first.back() == '\n')) {
                first.pop_back();
            }
            std::istringstream firstIss(first);
            std::string firstToken;
            if (!(firstIss >> firstToken)) {
                return "INVALID";
            }
            if (const std::optional<char> t = canonicalVocType(firstToken)) {
                return std::string(1, *t);
            }
            if (firstToken.size() == 3 && std::isdigit(static_cast<unsigned char>(firstToken[0])) != 0 &&
                std::isdigit(static_cast<unsigned char>(firstToken[1])) != 0 &&
                std::isdigit(static_cast<unsigned char>(firstToken[2])) != 0) {
                std::string secondToken;
                if (firstIss >> secondToken) {
                    if (const std::optional<char> t2 = canonicalVocType(secondToken)) {
                        return std::string(1, *t2);
                    }
                }
            }
            return "INVALID";
        }
    } // namespace

    void Shell::cmdCreateVoc(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 4) {
            out << "CREATE-VOC requires <item-id> <type> <target...>\n";
            return;
        }
        const std::optional<char> type = canonicalVocType(tokens[2]);
        if (!type.has_value()) {
            out << "CREATE-VOC type must be one of A,D,F,Q,V,X\n";
            return;
        }
        try {
            const PickFS::FileSystem::FileHandle voc = session_.fileSystem().openFile("VOC");
            std::string value;
            value += *type;
            value += '\n';
            for (std::size_t i = 3; i < tokens.size(); ++i) {
                value += tokens[i];
                value += '\n';
            }
            session_.fileSystem().writeRecord(voc, PickFS::Record(tokens[1], value));
            session_.vocResolver().invalidate();
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdDeleteVoc(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "DELETE-VOC requires an item-id\n";
            return;
        }
        try {
            const PickFS::FileSystem::FileHandle voc = session_.fileSystem().openFile("VOC");
            session_.fileSystem().deleteRecord(voc, tokens[1]);
            session_.vocResolver().invalidate();
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdListVoc(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 1) {
            out << "LIST-VOC takes no arguments\n";
            return;
        }
        try {
            const PickFS::FileSystem::FileHandle voc = session_.fileSystem().openFile("VOC");
            const std::vector<std::string> ids = session_.fileSystem().listRecords(voc);
            for (const std::string &id: ids) {
                const std::optional<PickFS::Record> rec = session_.fileSystem().readRecord(voc, id);
                const std::string type = rec.has_value() ? detectVocTypeOrInvalid(rec->value()) : "INVALID";
                out << id << ' ' << type << '\n';
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    namespace {
        struct FileRecordReadTarget {
            std::string fileName;
            std::string recordKey;
        };

        struct FileRecordWriteTarget {
            std::string fileName;
            std::string recordKey;
            std::size_t valueStart{0};
        };

        [[nodiscard]] std::optional<FileRecordReadTarget> parseFileRecordReadOperands(
            const std::vector<std::string> &tokens,
            const std::optional<std::string> &defaultDataFile) {
            if (tokens.size() == 3) {
                return FileRecordReadTarget{tokens[1], tokens[2]};
            }
            if (tokens.size() == 2 && defaultDataFile.has_value()) {
                return FileRecordReadTarget{*defaultDataFile, tokens[1]};
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<FileRecordWriteTarget> parseFileRecordWriteOperands(
            const std::vector<std::string> &tokens,
            const std::optional<std::string> &defaultDataFile) {
            if (tokens.size() >= 4) {
                return FileRecordWriteTarget{tokens[1], tokens[2], 3};
            }
            if (tokens.size() == 3 && defaultDataFile.has_value()) {
                return FileRecordWriteTarget{*defaultDataFile, tokens[1], 2};
            }
            return std::nullopt;
        }

        [[nodiscard]] std::string fileRecordReadArityMessage(const std::string &verb) {
            return verb + " requires a file and record name (or " + verb +
                   " <record> when MD DEFDATA is set)\n";
        }

        [[nodiscard]] std::string fileRecordWriteArityMessage(const std::string &verb) {
            return verb + " requires a file, record name, and value (or " + verb +
                   " <record> <value> when MD DEFDATA is set)\n";
        }

        [[nodiscard]] std::string joinTokensFrom(const std::vector<std::string> &tokens, std::size_t start) {
            std::string value;
            for (std::size_t i = start; i < tokens.size(); ++i) {
                if (i > start) {
                    value += ' ';
                }
                value += tokens[i];
            }
            return value;
        }
    } // namespace

    void Shell::cmdRead(const std::vector<std::string> &tokens, std::ostream &out) {
        const std::optional<FileRecordReadTarget> target =
            parseFileRecordReadOperands(tokens, session_.defaultDataFile());
        if (!target.has_value()) {
            out << fileRecordReadArityMessage("READ");
            return;
        }
        try {
            const std::optional<PickFS::Record> record =
                session_.fileSystem().read(target->fileName, target->recordKey);
            if (!record) {
                out << "No such record\n";
                return;
            }
            out << record->value() << '\n';
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdWrite(const std::vector<std::string> &tokens, std::ostream &out) {
        const std::optional<FileRecordWriteTarget> target =
            parseFileRecordWriteOperands(tokens, session_.defaultDataFile());
        if (!target.has_value()) {
            out << fileRecordWriteArityMessage("WRITE");
            return;
        }
        try {
            session_.fileSystem().write(target->fileName,
                                       PickFS::Record(target->recordKey,
                                                        joinTokensFrom(tokens, target->valueStart)));
            if (target->fileName == "VOC") {
                session_.vocResolver().invalidate();
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdReadU(const std::vector<std::string> &tokens, std::ostream &out) {
        const std::optional<FileRecordReadTarget> target =
            parseFileRecordReadOperands(tokens, session_.defaultDataFile());
        if (!target.has_value()) {
            out << fileRecordReadArityMessage("READU");
            return;
        }
        try {
            const std::optional<PickFS::Record> record =
                session_.fileSystem().readU(target->fileName, target->recordKey);
            if (!record) {
                out << "No such record\n";
                return;
            }
            out << record->value() << '\n';
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdWriteU(const std::vector<std::string> &tokens, std::ostream &out) {
        const std::optional<FileRecordWriteTarget> target =
            parseFileRecordWriteOperands(tokens, session_.defaultDataFile());
        if (!target.has_value()) {
            out << fileRecordWriteArityMessage("WRITEU");
            return;
        }
        try {
            session_.fileSystem().writeU(target->fileName,
                                        PickFS::Record(target->recordKey,
                                                         joinTokensFrom(tokens, target->valueStart)));
            if (target->fileName == "VOC") {
                session_.vocResolver().invalidate();
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdRelease(const std::vector<std::string> &tokens, std::ostream &out) {
        const std::optional<FileRecordReadTarget> target =
            parseFileRecordReadOperands(tokens, session_.defaultDataFile());
        if (!target.has_value()) {
            out << fileRecordReadArityMessage("RELEASE");
            return;
        }
        try {
            (void) session_.fileSystem().releaseRecord(target->fileName, target->recordKey);
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::runLineRecordEditorForLocation(const std::string &fileName,
                                               const std::string &recordKey,
                                               const std::optional<int> &highlightPhysicalLine,
                                               std::ostream &out) {
        std::vector<std::string> lines;
        try {
            const std::optional<PickFS::Record> rec = session_.fileSystem().read(fileName, recordKey);
            if (rec.has_value()) {
                std::istringstream in(rec->value());
                std::string line;
                while (std::getline(in, line)) {
                    lines.push_back(std::move(line));
                }
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << '\n';
            return;
        }

        std::istream &in = input();
        LineRecordEditor editor(
            session_.fileSystem(),
            fileName,
            recordKey,
            std::move(lines),
            [this](const std::string &writtenFile) {
                if (writtenFile == "VOC") {
                    session_.vocResolver().invalidate();
                }
            });
        editor.run(in, out, highlightPhysicalLine);
    }

    void Shell::cmdEdit(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "EDIT requires a program name or a file and record name\n";
            return;
        }
        if (tokens.size() > 3) {
            out << "EDIT takes a program name or a file and record name\n";
            return;
        }

        std::string fileName;
        std::string recordKey;
        if (tokens.size() == 2) {
            const std::string &programName = tokens[1];
            if (programName.empty() || programName.find('.') != std::string::npos) {
                out << "EDIT expects a program name without extension\n";
                return;
            }
            const PickVoc::VocResolver::ProgramLocation resolved = session_.vocResolver().resolveProgramLocation(programName);
            fileName = resolved.fileName;
            recordKey = resolved.recordKey;
        } else {
            fileName = tokens[1];
            recordKey = tokens[2];
        }

        runLineRecordEditorForLocation(fileName, recordKey, std::nullopt, out);
    }

    namespace {
        bool iequalsAscii(std::string_view a, std::string_view b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    void Shell::cmdLogto(const std::vector<std::string> &tokens, std::ostream &out) {
        if (!session_.loggedIn()) {
            out << "Not logged in.\n";
            return;
        }
        if (tokens.size() != 2U) {
            out << "LOGTO requires an account name\n";
            return;
        }
        const auto &cat = session_.geminiCatalogRoot();
        if (!cat.has_value()) {
            out << "No Gemini catalogue configured.\n";
            return;
        }
        const auto accounts = PickCore::GeminiCatalog::loadAccounts(*cat / "ACCOUNTS.json");
        if (!accounts.has_value()) {
            out << "Cannot read ACCOUNTS.json.\n";
            return;
        }
        const std::string &accountName = tokens[1];
        const PickCore::GeminiAccountRow *acct = nullptr;
        for (const auto &a: *accounts) {
            if (iequalsAscii(a.name, accountName)) {
                acct = &a;
                break;
            }
        }
        if (acct == nullptr) {
            out << "Unknown account.\n";
            return;
        }
        std::istream &in = input();
        std::string password;
        if (!PickCore::LoginService::readPasswordLineIfAccountRequires(in, acct, password)) {
            out << "EOF\n";
            return;
        }
        std::ostringstream err;
        if (const auto sess = PickCore::LoginService::authenticateAccount(*cat, accountName, password, err)) {
            session_.attach(*sess);
            return;
        }
        out << err.str();
    }

    void Shell::cmdLogoff(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 1) {
            out << "LOGOFF takes no arguments\n";
            return;
        }
        if (!session_.loggedIn()) {
            out << "Not logged in.\n";
            return;
        }
        session_.detach();
        session_.reset();
        sessionEndRequested_ = true;
        out << "Logged off.\n";
    }
} // namespace PickShell
