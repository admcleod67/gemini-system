#include "EnglishParser.h"

#include <cctype>
#include <string>

namespace PickCore::English {
    namespace {
        bool ieq(const std::string_view a, const std::string_view b) {
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

        bool isBy(const std::string &t) {
            return ieq(t, "BY");
        }

        bool isByDsnd(const std::string &t) {
            return ieq(t, "BY-DSND");
        }

        bool isWith(const std::string &t) {
            return ieq(t, "WITH");
        }

        bool isHeading(const std::string &t) {
            return ieq(t, "HEADING");
        }

        bool isBreakOn(const std::string &t) {
            return ieq(t, "BREAK-ON");
        }

        bool isTotal(const std::string &t) {
            return ieq(t, "TOTAL");
        }

        /// True if the token is one of the ENGLISH structural keywords; used to reject
        /// `HEADING BY ...` and similar typos where the user clearly forgot the literal.
        bool isStructuralKeyword(const std::string &t) {
            return isBy(t) || isByDsnd(t) || isWith(t) || isHeading(t) || isBreakOn(t) || isTotal(t);
        }

        std::optional<Verb> verbFrom(const std::string &t) {
            if (ieq(t, "LIST")) {
                return Verb::LIST;
            }
            if (ieq(t, "COUNT")) {
                return Verb::COUNT;
            }
            if (ieq(t, "SELECT")) {
                return Verb::SELECT;
            }
            if (ieq(t, "SORT")) {
                return Verb::SORT;
            }
            return std::nullopt;
        }

        std::size_t findFirstBy(const std::vector<std::string> &tokens, std::size_t start) {
            for (std::size_t k = start; k < tokens.size(); ++k) {
                if (isBy(tokens[k])) {
                    return k;
                }
            }
            return tokens.size();
        }

        std::vector<SortKeySpec> parseSortSection(const std::vector<std::string> &tokens,
                                                  std::size_t start,
                                                  std::size_t end,
                                                  std::string &error) {
            std::vector<SortKeySpec> keys;
            bool nextDesc = false;
            for (std::size_t i = start; i < end;) {
                if (isBy(tokens[i])) {
                    ++i;
                    continue;
                }
                if (isByDsnd(tokens[i])) {
                    nextDesc = true;
                    ++i;
                    continue;
                }
                if (tokens[i].empty()) {
                    error = "Empty BY field";
                    return {};
                }
                keys.push_back(SortKeySpec{tokens[i], nextDesc});
                nextDesc = false;
                ++i;
            }
            if (nextDesc) {
                error = "BY-DSND without following field";
                return {};
            }
            return keys;
        }
    } // namespace

    std::optional<Query> EnglishParser::parse(const std::vector<std::string> &rawTokens,
                                               const ParseContext &ctx,
                                               std::string &error) const {
        error.clear();
        if (rawTokens.empty()) {
            error = "Empty query";
            return std::nullopt;
        }
        const std::optional<Verb> verbOpt = verbFrom(rawTokens[0]);
        if (!verbOpt.has_value()) {
            error = "Unsupported ENGLISH verb";
            return std::nullopt;
        }
        const Verb verb = *verbOpt;

        // Strip formatting clauses (`HEADING`, `BREAK-ON`, `TOTAL`) so the rest of the parser
        // keeps working on a verb-and-fields token stream. Each may appear anywhere
        // after the verb; only one occurrence of each is allowed.
        std::vector<std::string> tokens;
        tokens.reserve(rawTokens.size());
        std::optional<std::string> heading;
        std::optional<std::string> breakOnField;
        std::optional<std::string> totalField;
        for (std::size_t k = 0; k < rawTokens.size(); ++k) {
            if (k > 0U && isHeading(rawTokens[k])) {
                if (heading.has_value()) {
                    error = "HEADING can only appear once";
                    return std::nullopt;
                }
                if (k + 1U >= rawTokens.size() || isStructuralKeyword(rawTokens[k + 1U])) {
                    error = "HEADING requires a quoted string";
                    return std::nullopt;
                }
                heading = rawTokens[k + 1U];
                ++k; // also skip the heading argument
                continue;
            }
            if (k > 0U && isBreakOn(rawTokens[k])) {
                if (breakOnField.has_value()) {
                    error = "BREAK-ON can only appear once";
                    return std::nullopt;
                }
                if (k + 1U >= rawTokens.size() || isStructuralKeyword(rawTokens[k + 1U])) {
                    error = "BREAK-ON requires a field";
                    return std::nullopt;
                }
                breakOnField = rawTokens[k + 1U];
                ++k; // also skip the break field token
                continue;
            }
            if (k > 0U && isTotal(rawTokens[k])) {
                if (totalField.has_value()) {
                    error = "TOTAL can only appear once";
                    return std::nullopt;
                }
                if (k + 1U >= rawTokens.size() || isStructuralKeyword(rawTokens[k + 1U])) {
                    error = "TOTAL requires a field";
                    return std::nullopt;
                }
                totalField = rawTokens[k + 1U];
                ++k; // also skip the total field token
                continue;
            }
            tokens.push_back(rawTokens[k]);
        }

        if (verb == Verb::COUNT && tokens.size() == 1U) {
            if (!ctx.implicitFile || !ctx.imposedFileName.has_value() || ctx.imposedFileName->empty()) {
                error = "COUNT requires filename or active-list scope";
                return std::nullopt;
            }
            Query countQuery{};
            countQuery.verb = verb;
            countQuery.fileName = *ctx.imposedFileName;
            countQuery.heading = std::move(heading);
            countQuery.breakOnField = std::move(breakOnField);
            countQuery.totalField = std::move(totalField);
            return countQuery;
        }

        Query q{};
        q.verb = verb;
        q.heading = std::move(heading);
        q.breakOnField = std::move(breakOnField);
        q.totalField = std::move(totalField);
        std::size_t i = 1U;
        if (ctx.implicitFile) {
            if (!ctx.imposedFileName.has_value() || ctx.imposedFileName->empty()) {
                error = "Internal: implicit scope requires imposed filename";
                return std::nullopt;
            }
            q.fileName = *ctx.imposedFileName;
        } else {
            if (tokens.size() < 2U) {
                error = "Command requires filename";
                return std::nullopt;
            }
            q.fileName = tokens[i++];
            if (q.fileName.empty()) {
                error = "Filename must be non-empty";
                return std::nullopt;
            }
        }

        const std::size_t byIdx = findFirstBy(tokens, i);
        const bool sawByClause = byIdx != tokens.size();
        if (sawByClause && verb != Verb::SORT) {
            error = "BY is only valid with SORT";
            return std::nullopt;
        }

        // Projection and optional WITH: order is FIELD... [WITH withTokens...], all before BY.
        std::size_t projEnd = byIdx;
        std::size_t withPos = projEnd;
        for (std::size_t k = i; k < byIdx; ++k) {
            if (isWith(tokens[k])) {
                withPos = k;
                break;
            }
        }

        if (withPos != projEnd) {
            for (std::size_t k = i; k < withPos; ++k) {
                q.fields.push_back(tokens[k]);
            }
            // Skip WITH keyword and absorption body until BY boundary (already at byIdx).
            for (std::size_t k = withPos + 1; k < byIdx; ++k) {
                (void) tokens[k];
                // Selection stub — not evaluated in this milestone (docs: WITH reserved).
            }
        } else {
            for (std::size_t k = i; k < byIdx; ++k) {
                q.fields.push_back(tokens[k]);
            }
        }

        if (verb == Verb::SORT && sawByClause) {
            q.sortKeys = parseSortSection(tokens, byIdx, tokens.size(), error);
            if (!error.empty()) {
                return std::nullopt;
            }
        }

        return q;
    }
} // namespace PickCore::English
