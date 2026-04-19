#include "Record.h"

namespace PickFS {
    Record::Record(std::string name, std::string value)
        : name_(std::move(name)), value_(std::move(value)) {
    }
} // namespace PickFS
