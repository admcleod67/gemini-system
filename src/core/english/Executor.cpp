#include "Executor.h"

#include <sstream>

namespace PickCore::English {
    Result Executor::execute(PickFS::FileSystem &fs,
                             const Plan &plan,
                             const DictionaryResolver &dictResolver,
                             std::string &error) const {
        Result result;
        std::vector<std::string> ids;
        try {
            ids = fs.listRecordNames(plan.query.fileName);
        } catch (const std::exception &e) {
            error = e.what();
            return result;
        }

        if (plan.query.verb == Verb::COUNT) {
            result.lines.push_back(std::to_string(ids.size()));
            return result;
        }

        const std::vector<FieldRef> fields = dictResolver.resolveFields(fs, plan.query.fileName, plan.query.fields);

        for (const std::string &id: ids) {
            result.selectedIds.push_back(id);
            if (plan.query.verb == Verb::SELECT) {
                continue;
            }
            std::ostringstream row;
            row << id;
            try {
                const std::optional<PickFS::Record> rec = fs.read(plan.query.fileName, id);
                if (rec.has_value()) {
                    for (const FieldRef &field: fields) {
                        row << ' ';
                        if (field.attributeNo.has_value()) {
                            row << rec->structured().attribute(*field.attributeNo).firstValue();
                        }
                    }
                }
            } catch (const std::exception &e) {
                error = e.what();
                return Result{};
            }
            result.lines.push_back(row.str());
        }

        if (plan.query.verb == Verb::SELECT) {
            result.lines.push_back(std::to_string(result.selectedIds.size()) + " records selected");
        }
        return result;
    }
} // namespace PickCore::English
