#include "Executor.h"

#include "correlatives/CorrelativeEvaluator.h"

#include <algorithm>
#include <cctype>
#include <numeric>
#include <optional>
#include <string>

namespace PickCore::English {
    namespace {
        enum class KeyKind {
            String,
            Number,
        };

        struct KeyPart {
            KeyKind kind{KeyKind::String};
            double num{0};
            std::string str;
        };

        void trimInPlace(std::string &s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
                s.erase(s.begin());
            }
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
                s.pop_back();
            }
        }

        std::optional<double> tryParseNumeric(const std::string &raw) {
            std::string s = raw;
            trimInPlace(s);
            for (char &c: s) {
                if (c == ',' || c == '$') {
                    c = ' ';
                }
            }
            trimInPlace(s);
            if (s.empty()) {
                return std::nullopt;
            }
            try {
                std::size_t pos = 0;
                const double v = std::stod(s, &pos);
                if (pos == 0) {
                    return std::nullopt;
                }
                return v;
            } catch (const std::exception &) {
                return std::nullopt;
            }
        }

        KeyPart makeSortPart(const std::string &rawFirstValue, const ConversionCode conv) {
            KeyPart p;
            if (conv == ConversionCode::None) {
                p.kind = KeyKind::String;
                p.str = rawFirstValue;
                return p;
            }
            if (const std::optional<double> n = tryParseNumeric(rawFirstValue)) {
                p.kind = KeyKind::Number;
                p.num = *n;
                return p;
            }
            p.kind = KeyKind::String;
            p.str = rawFirstValue;
            return p;
        }

        int comparePartsAsc(const KeyPart &a, const KeyPart &b) {
            if (a.kind == KeyKind::Number && b.kind == KeyKind::Number) {
                if (a.num < b.num) {
                    return -1;
                }
                if (a.num > b.num) {
                    return 1;
                }
                return 0;
            }
            if (a.str < b.str) {
                return -1;
            }
            if (a.str > b.str) {
                return 1;
            }
            return 0;
        }

        int comparePartsWithDesc(const KeyPart &a, const KeyPart &b, const bool desc) {
            const int c = comparePartsAsc(a, b);
            return desc ? -c : c;
        }

        /// Materialise the projected fields for one record. Mirrors the legacy
        /// `formatProjectLine` exactly: when the record is absent the projection
        /// list is empty (so the renderer emits only the id); when an attribute
        /// is unresolved the entry is the empty string (so the renderer still
        /// emits a placeholder space, byte-identical to the pre-formatter output).
        std::vector<std::string> materializeProjection(const std::optional<PickFS::Record> &rec,
                                                       const std::vector<FieldRef> &fields) {
            std::vector<std::string> out;
            if (!rec.has_value()) {
                return out;
            }
            const PickFS::StructuredRecord &structured = rec->structured();
            out.reserve(fields.size());
            for (const FieldRef &field: fields) {
                out.push_back(CorrelativeEvaluator::evaluateFieldCell(field, structured));
            }
            return out;
        }

        std::string materializeBreakKey(const std::optional<PickFS::Record> &rec, const FieldRef &breakRef) {
            if (!rec.has_value()) {
                return {};
            }
            return CorrelativeEvaluator::evaluateFieldCell(breakRef, rec->structured());
        }

        std::string materializeTotalCell(const std::optional<PickFS::Record> &rec, const FieldRef &totalRef) {
            if (!rec.has_value()) {
                return {};
            }
            return CorrelativeEvaluator::evaluateFieldCell(totalRef, rec->structured());
        }

        /// Largest 1-based attribute index referenced by `@<digits>` tokens in a report template.
        int maxTemplateAttributeIndex(const std::string &tpl) {
            int maxAttr = 0;
            const std::size_t n = tpl.size();
            for (std::size_t i = 0; i < n;) {
                if (tpl[i] != '@') {
                    ++i;
                    continue;
                }
                if (i + 1U >= n) {
                    ++i;
                    continue;
                }
                if (tpl[i + 1U] == '@') {
                    i += 2U;
                    continue;
                }
                const unsigned char nu = static_cast<unsigned char>(tpl[i + 1U]);
                if (!std::isdigit(nu)) {
                    ++i;
                    continue;
                }
                std::size_t j = i + 1U;
                bool digitsOnly = true;
                while (j < n) {
                    const unsigned char cj = static_cast<unsigned char>(tpl[j]);
                    if (!std::isalpha(cj) && !std::isdigit(cj)) {
                        break;
                    }
                    if (!std::isdigit(cj)) {
                        digitsOnly = false;
                    }
                    ++j;
                }
                if (digitsOnly) {
                    const std::string token = tpl.substr(i + 1U, j - (i + 1U));
                    try {
                        const int attrNo = std::stoi(token);
                        if (attrNo > maxAttr) {
                            maxAttr = attrNo;
                        }
                    } catch (const std::exception &) {
                        // ignore malformed digit runs
                    }
                }
                i = j;
            }
            return maxAttr;
        }

        void materializeHeadingAttrs(const PickFS::Record &rec,
                                     const int maxAttr,
                                     std::vector<std::string> &out) {
            if (maxAttr <= 0) {
                out.clear();
                return;
            }
            out.resize(static_cast<std::size_t>(maxAttr));
            for (int n = 1; n <= maxAttr; ++n) {
                if (rec.structured().hasAttribute(n)) {
                    out[static_cast<std::size_t>(n - 1)] =
                        rec.structured().attribute(n).firstValue();
                } else {
                    out[static_cast<std::size_t>(n - 1)].clear();
                }
            }
        }

        std::vector<KeyPart> materializeSortKeys(const PickFS::Record &rec, const std::vector<FieldRef> &refs) {
            std::vector<KeyPart> parts;
            parts.reserve(refs.size());
            const PickFS::StructuredRecord &structured = rec.structured();
            for (const FieldRef &ref: refs) {
                const std::string cell = CorrelativeEvaluator::evaluateFieldCell(ref, structured);
                parts.push_back(makeSortPart(cell, ref.conversion));
            }
            return parts;
        }

        std::vector<KeyPart> itemIdKey(const std::string &id) {
            KeyPart p;
            p.kind = KeyKind::String;
            p.str = id;
            return {p};
        }

        std::optional<std::string> firstUnresolvedFieldError(const std::vector<FieldRef> &refs,
                                                               const std::string &dataFile) {
            for (const FieldRef &r: refs) {
                if (!fieldRefIsResolved(r)) {
                    const std::string scoped = DictionaryResolver::scopedDictLogicalName(dataFile);
                    return std::optional<std::string>{
                        "Unknown ENGLISH field \"" + r.token + "\" (not in " + scoped + " nor DICT)"};
                }
            }
            return std::nullopt;
        }
    } // namespace

    void Executor::produceRows(PickFS::FileSystem &fs,
                               const Plan &plan,
                               const DictionaryResolver &dictResolver,
                               const EnglishRunOptions &opts,
                               std::vector<Row> &rows,
                               std::vector<std::string> &trailingLines,
                               std::vector<std::string> &selectedIds,
                               std::string &error) const {
        rows.clear();
        trailingLines.clear();
        selectedIds.clear();

        std::vector<std::string> ids;
        const std::string &fn = plan.query.fileName;

        if (opts.constrainRecordIds.has_value()) {
            ids = *opts.constrainRecordIds;
        } else {
            try {
                ids = fs.listRecordNames(fn);
            } catch (const std::exception &e) {
                error = e.what();
                return;
            }
        }

        if (plan.query.verb == Verb::COUNT) {
            trailingLines.push_back(std::to_string(ids.size()));
            return;
        }

        const std::vector<FieldRef> fields = dictResolver.resolveFields(fs, fn, plan.query.fields);

        if (!plan.query.fields.empty()) {
            if (const std::optional<std::string> err = firstUnresolvedFieldError(fields, fn)) {
                error = *err;
                return;
            }
        }

        std::optional<FieldRef> breakRef;
        if (plan.query.breakOnField.has_value()) {
            const FieldRef resolved =
                dictResolver.resolveField(fs, fn, *plan.query.breakOnField);
            if (const std::optional<std::string> err = firstUnresolvedFieldError({resolved}, fn)) {
                error = *err;
                return;
            }
            breakRef = resolved;
        }

        std::optional<FieldRef> totalRef;
        if (plan.query.totalField.has_value()) {
            const FieldRef resolved = dictResolver.resolveField(fs, fn, *plan.query.totalField);
            if (const std::optional<std::string> err = firstUnresolvedFieldError({resolved}, fn)) {
                error = *err;
                return;
            }
            totalRef = resolved;
        }

        int templateMaxAttr = 0;
        if (plan.query.heading.has_value()) {
            templateMaxAttr = std::max(templateMaxAttr, maxTemplateAttributeIndex(*plan.query.heading));
        }
        if (plan.query.footing.has_value()) {
            templateMaxAttr = std::max(templateMaxAttr, maxTemplateAttributeIndex(*plan.query.footing));
        }

        rows.reserve(ids.size());
        std::vector<std::optional<PickFS::Record>> records;
        records.reserve(ids.size());

        for (const std::string &id: ids) {
            selectedIds.push_back(id);
            if (plan.query.verb == Verb::SELECT) {
                continue;
            }
            try {
                std::optional<PickFS::Record> rec = fs.read(fn, id);
                Row row;
                row.id = id;
                row.projectedFields = materializeProjection(rec, fields);
                if (breakRef.has_value()) {
                    row.breakKey = materializeBreakKey(rec, *breakRef);
                }
                if (totalRef.has_value()) {
                    const std::string cell = materializeTotalCell(rec, *totalRef);
                    row.totalAddend = tryParseNumeric(cell).value_or(0.0);
                }
                if (templateMaxAttr > 0 && rec.has_value()) {
                    materializeHeadingAttrs(*rec, templateMaxAttr, row.headingAttrs);
                }
                rows.push_back(std::move(row));
                records.push_back(std::move(rec));
            } catch (const std::exception &e) {
                error = e.what();
                return;
            }
        }

        if (plan.query.verb == Verb::SELECT) {
            trailingLines.push_back(std::to_string(selectedIds.size()) + " records selected");
            return;
        }

        std::vector<std::size_t> order(rows.size());
        std::iota(order.begin(), order.end(), 0U);

        if (plan.query.verb == Verb::SORT) {
            std::vector<FieldRef> sortRefs;
            std::vector<bool> sortDesc;
            for (const SortKeySpec &sk: plan.query.sortKeys) {
                sortRefs.push_back(dictResolver.resolveField(fs, fn, sk.fieldToken));
                sortDesc.push_back(sk.descending);
            }

            if (const std::optional<std::string> err = firstUnresolvedFieldError(sortRefs, fn)) {
                error = *err;
                return;
            }

            std::vector<std::vector<KeyPart>> rowKeys;
            rowKeys.reserve(records.size());
            for (std::size_t r = 0; r < records.size(); ++r) {
                if (sortRefs.empty()) {
                    rowKeys.push_back(itemIdKey(ids[r]));
                } else if (records[r].has_value()) {
                    rowKeys.push_back(materializeSortKeys(*records[r], sortRefs));
                } else {
                    rowKeys.push_back(std::vector<KeyPart>(sortRefs.size(), KeyPart{}));
                }
            }

            const auto cmpRows = [&](const std::size_t a, const std::size_t b) {
                const std::vector<KeyPart> &ka = rowKeys[a];
                const std::vector<KeyPart> &kb = rowKeys[b];
                const std::size_t n = std::min(ka.size(), kb.size());
                for (std::size_t i = 0; i < n; ++i) {
                    const int c = comparePartsWithDesc(ka[i], kb[i], i < sortDesc.size() ? sortDesc[i] : false);
                    if (c != 0) {
                        return c < 0;
                    }
                }
                return ids[a] < ids[b];
            };
            std::stable_sort(order.begin(), order.end(), cmpRows);
        }

        if (order.size() != rows.size() || !std::is_sorted(order.begin(), order.end())) {
            std::vector<Row> reordered;
            reordered.reserve(order.size());
            for (const std::size_t idx: order) {
                reordered.push_back(std::move(rows[idx]));
            }
            rows = std::move(reordered);
        }
    }

    Result Executor::execute(PickFS::FileSystem &fs,
                             const Plan &plan,
                             const DictionaryResolver &dictResolver,
                             const EnglishRunOptions &opts,
                             std::string &error) const {
        std::vector<Row> rows;
        std::vector<std::string> trailingLines;
        std::vector<std::string> selectedIds;
        produceRows(fs, plan, dictResolver, opts, rows, trailingLines, selectedIds, error);
        if (!error.empty()) {
            return Result{};
        }
        FormatterContext ctx = defaultFormatterContext();
        if (opts.pageLength.has_value()) {
            ctx.pageLength = *opts.pageLength;
        }
        return format(plan,
                      std::move(rows),
                      std::move(trailingLines),
                      std::move(selectedIds),
                      ctx);
    }
} // namespace PickCore::English
