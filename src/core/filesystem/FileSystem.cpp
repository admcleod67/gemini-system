#include "FileSystem.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace PickFS {
    namespace {
        using RecordsVector = std::vector<std::pair<std::string, std::string>>;

        class JsonCursor {
        public:
            explicit JsonCursor(const std::string &text) : text_(text) {
            }

            [[nodiscard]] bool eof() const {
                return pos_ >= text_.size();
            }

            void skipWs() {
                while (!eof() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
                    ++pos_;
                }
            }

            bool consume(const char c) {
                skipWs();
                if (!eof() && text_[pos_] == c) {
                    ++pos_;
                    return true;
                }
                return false;
            }

            void expect(const char c, const std::string &context) {
                if (!consume(c)) {
                    throw std::runtime_error("Expected '" + std::string(1, c) + "' " + context);
                }
            }

            std::string parseString() {
                skipWs();
                if (eof() || text_[pos_] != '"') {
                    throw std::runtime_error("Expected string");
                }
                ++pos_;

                std::string out;
                while (!eof()) {
                    const char c = text_[pos_++];
                    if (c == '"') {
                        return out;
                    }
                    if (c == '\\') {
                        if (eof()) {
                            throw std::runtime_error("Invalid escape sequence");
                        }
                        const char e = text_[pos_++];
                        switch (e) {
                            case '"':
                                out.push_back('"');
                                break;
                            case '\\':
                                out.push_back('\\');
                                break;
                            case '/':
                                out.push_back('/');
                                break;
                            case 'b':
                                out.push_back('\b');
                                break;
                            case 'f':
                                out.push_back('\f');
                                break;
                            case 'n':
                                out.push_back('\n');
                                break;
                            case 'r':
                                out.push_back('\r');
                                break;
                            case 't':
                                out.push_back('\t');
                                break;
                            default:
                                throw std::runtime_error("Unsupported escape sequence");
                        }
                        continue;
                    }
                    out.push_back(c);
                }

                throw std::runtime_error("Unterminated string");
            }

        private:
            const std::string &text_;
            std::size_t pos_{0};
        };

        std::string escapeJson(const std::string &input) {
            std::string out;
            out.reserve(input.size());
            for (const char c: input) {
                switch (c) {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\b':
                        out += "\\b";
                        break;
                    case '\f':
                        out += "\\f";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        out.push_back(c);
                        break;
                }
            }
            return out;
        }

        bool isValidRecordName(const std::string &name) {
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

        RecordsVector parseRecords(JsonCursor &cursor) {
            RecordsVector records;
            cursor.expect('{', "for records object");
            if (cursor.consume('}')) {
                return records;
            }

            while (true) {
                const std::string key = cursor.parseString();
                cursor.expect(':', "after record name");
                const std::string value = cursor.parseString();
                records.emplace_back(key, value);

                if (cursor.consume('}')) {
                    break;
                }
                cursor.expect(',', "between record entries");
            }
            return records;
        }

        struct ParsedDocument {
            std::string name;
            RecordsVector records;
        };

        ParsedDocument parseDocument(const std::string &content) {
            JsonCursor cursor(content);
            cursor.expect('{', "at root");

            bool seenName = false;
            bool seenRecords = false;
            ParsedDocument doc;

            if (cursor.consume('}')) {
                throw std::runtime_error("Root object cannot be empty");
            }

            while (true) {
                const std::string key = cursor.parseString();
                cursor.expect(':', "after object key");
                if (key == "name") {
                    if (seenName) {
                        throw std::runtime_error("Duplicate 'name' field");
                    }
                    doc.name = cursor.parseString();
                    seenName = true;
                } else if (key == "records") {
                    if (seenRecords) {
                        throw std::runtime_error("Duplicate 'records' field");
                    }
                    doc.records = parseRecords(cursor);
                    seenRecords = true;
                } else {
                    throw std::runtime_error("Unexpected field: " + key);
                }

                if (cursor.consume('}')) {
                    break;
                }
                cursor.expect(',', "between root fields");
            }

            cursor.skipWs();
            if (!cursor.eof()) {
                throw std::runtime_error("Trailing content after JSON object");
            }

            if (!seenName || !seenRecords) {
                throw std::runtime_error("Missing required fields (name, records)");
            }
            if (!Catalog::isValidName(doc.name)) {
                throw std::runtime_error("Invalid value for 'name'");
            }
            return doc;
        }

        std::string serializeDocument(const std::string &name, const RecordsVector &records) {
            std::ostringstream out;
            out << "{\n";
            out << "  \"name\": \"" << escapeJson(name) << "\",\n";
            out << "  \"records\": {\n";
            for (std::size_t i = 0; i < records.size(); ++i) {
                const auto &entry = records[i];
                out << "    \"" << escapeJson(entry.first) << "\": \"" << escapeJson(entry.second) << "\"";
                if (i + 1 < records.size()) {
                    out << ',';
                }
                out << '\n';
            }
            out << "  }\n";
            out << "}\n";
            return out.str();
        }
    } // namespace

    FileSystem::FileSystem(std::filesystem::path root)
        : catalog_(std::move(root)) {
    }

    void FileSystem::setRoot(std::filesystem::path root) {
        catalog_.setRoot(std::move(root));
    }

    void FileSystem::createFile(const std::string &name) {
        const std::filesystem::path path = catalog_.filePath(name);
        catalog_.ensureRootExists();

        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            throw FileSystemError("Cannot inspect file: " + path.string());
        }
        if (exists) {
            throw FileSystemError("File already exists: " + name);
        }

        saveFile(StoredFile{name, {}});
    }

    void FileSystem::deleteFile(const std::string &name) {
        const std::filesystem::path path = catalog_.filePath(name);
        std::error_code ec;
        const bool removed = std::filesystem::remove(path, ec);
        if (ec) {
            throw FileSystemError("Cannot delete file: " + name);
        }
        if (!removed) {
            throw FileSystemError("File not found: " + name);
        }
    }

    std::optional<Record> FileSystem::read(const std::string &fileName, const std::string &recordName) const {
        if (!isValidRecordName(recordName)) {
            throw FileSystemError("Invalid record name");
        }
        const StoredFile stored = loadFile(fileName);
        for (const auto &entry: stored.records) {
            if (entry.first == recordName) {
                return Record(entry.first, entry.second);
            }
        }
        return std::nullopt;
    }

    void FileSystem::write(const std::string &fileName, const Record &record) {
        if (!isValidRecordName(record.name())) {
            throw FileSystemError("Invalid record name");
        }
        StoredFile stored = loadFile(fileName);

        bool replaced = false;
        for (auto &entry: stored.records) {
            if (entry.first == record.name()) {
                entry.second = record.value();
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            stored.records.emplace_back(record.name(), record.value());
        }
        saveFile(stored);
    }

    std::vector<std::string> FileSystem::listFiles() const {
        std::vector<std::string> names;

        std::error_code ec;
        if (!std::filesystem::exists(catalog_.root(), ec)) {
            return names;
        }
        if (ec) {
            throw FileSystemError("Cannot inspect filesystem root: " + catalog_.root().string());
        }

        for (const auto &entry: std::filesystem::directory_iterator(catalog_.root(), ec)) {
            if (ec) {
                throw FileSystemError("Cannot read filesystem root: " + catalog_.root().string());
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension() != ".json") {
                continue;
            }

            const std::string fileName = entry.path().stem().string();
            const StoredFile stored = loadFile(fileName);
            names.push_back(stored.name);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    FileSystem::StoredFile FileSystem::loadFile(const std::string &fileName) const {
        const std::filesystem::path path = catalog_.filePath(fileName);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            throw FileSystemError("File not found: " + fileName);
        }
        if (ec) {
            throw FileSystemError("Cannot inspect file: " + fileName);
        }

        std::ifstream in(path);
        if (!in) {
            throw FileSystemError("Cannot open file: " + fileName);
        }
        const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        ParsedDocument parsed;
        try {
            parsed = parseDocument(content);
        } catch (const std::exception &e) {
            throw FileSystemError("Invalid JSON in " + path.string() + ": " + e.what());
        }

        if (parsed.name != fileName) {
            throw FileSystemError("File name mismatch in " + path.string());
        }

        return StoredFile{parsed.name, std::move(parsed.records)};
    }

    void FileSystem::saveFile(const StoredFile &stored) const {
        const std::filesystem::path path = catalog_.filePath(stored.name);
        catalog_.ensureRootExists();

        const std::filesystem::path tempPath = path.string() + ".tmp";
        {
            std::ofstream out(tempPath, std::ios::trunc);
            if (!out) {
                throw FileSystemError("Cannot write file: " + stored.name);
            }
            out << serializeDocument(stored.name, stored.records);
            if (!out) {
                throw FileSystemError("Cannot write file: " + stored.name);
            }
        }

        std::error_code ec;
        std::filesystem::rename(tempPath, path, ec);
        if (ec) {
            std::filesystem::remove(tempPath);
            throw FileSystemError("Cannot finalize file: " + stored.name);
        }
    }
} // namespace PickFS
