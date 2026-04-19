//
// Created by Allan McLeod on 19/04/2026.
//

#ifndef PICK_SYSTEM_FILESYSTEM_RECORD_H
#define PICK_SYSTEM_FILESYSTEM_RECORD_H

#include <string>

namespace PickFS {
    class Record {
    public:
        Record(std::string name, std::string value);

        const std::string &name() const { return name_; }

        const std::string &value() const { return value_; }

    private:
        std::string name_;
        std::string value_;
    };
} // namespace PickFS

#endif // PICK_SYSTEM_FILESYSTEM_RECORD_H
