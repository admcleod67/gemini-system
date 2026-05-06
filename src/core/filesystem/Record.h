//
// Created by Allan McLeod on 19/04/2026.
//

#ifndef PICK_SYSTEM_FILESYSTEM_RECORD_H
#define PICK_SYSTEM_FILESYSTEM_RECORD_H

#include "StructuredRecord.h"

#include <string>

namespace PickFS {
    class Record {
    public:
        Record(std::string name, std::string value);
        Record(std::string name, StructuredRecord structured);

        const std::string &name() const { return name_; }

        const std::string &value() const { return value_; }

        const StructuredRecord &structured() const { return structured_; }

    private:
        std::string name_;
        std::string value_;
        StructuredRecord structured_;
    };
} // namespace PickFS

#endif // PICK_SYSTEM_FILESYSTEM_RECORD_H
