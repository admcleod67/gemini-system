#include "Record.h"

namespace PickFS {
    Record::Record(std::string name, std::string value)
        : name_(std::move(name)),
          value_(std::move(value)),
          structured_(StructuredRecord::fromRaw(value_)) {
        value_ = structured_.toRaw();
    }

    Record::Record(std::string name, StructuredRecord structured)
        : name_(std::move(name)),
          value_(structured.toRaw()),
          structured_(std::move(structured)) {
    }
} // namespace PickFS
