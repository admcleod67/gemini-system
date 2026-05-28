#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>

#include "DictionaryResolver.h"
#include "FileSystem.h"
#include "StructuredRecord.h"
#include "correlatives/CorrelativeEvaluator.h"
#include "correlatives/DictFItemParser.h"

namespace {
    using namespace PickCore::English;

    PickFS::StructuredRecord dictFromRaw(const std::string &raw) {
        return PickFS::Record("X", raw).structured();
    }

    std::filesystem::path uniqueTempDir() {
        const auto base = std::filesystem::temp_directory_path();
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        return base / ("pick-correlative-test-" + std::to_string(tick));
    }

    constexpr char kVm = static_cast<char>(0xFD);
} // namespace

TEST_CASE("DictFItemParser accepts classic F DICT attrs 1-3") {
    std::string error;
    const auto def = DictFItemParser::parse(dictFromRaw("F\n2\n3\n"), error);
    REQUIRE(def.has_value());
    CHECK(error.empty());
    CHECK(def->sourceAttributeNo == 2);
    CHECK(def->tailRaw == "3");
    CHECK(def->selectorKind == FSelectorKind::ValueIndex);
    CHECK(def->valueIndex == 3);
}

TEST_CASE("DictFItemParser classifies L and R selectors") {
    std::string error;
    const auto left = DictFItemParser::parse(dictFromRaw("F\n2\nL\n"), error);
    REQUIRE(left.has_value());
    CHECK(left->selectorKind == FSelectorKind::Leftmost);

    const auto right = DictFItemParser::parse(dictFromRaw("F\n2\nr\n"), error);
    REQUIRE(right.has_value());
    CHECK(right->selectorKind == FSelectorKind::Rightmost);
}

TEST_CASE("DictFItemParser stores conversion tail as ConversionRaw") {
    std::string error;
    const auto def = DictFItemParser::parse(dictFromRaw("F\n2\nD2/\n"), error);
    REQUIRE(def.has_value());
    CHECK(def->selectorKind == FSelectorKind::ConversionRaw);
    CHECK(def->tailRaw == "D2/");
}

TEST_CASE("DictFItemParser rejects invalid F DICT items") {
    std::string error;

    CHECK_FALSE(DictFItemParser::parse(dictFromRaw("F\n\n3\n"), error).has_value());
    CHECK(error == "Invalid F-type DICT item: missing source attribute");

    error.clear();
    CHECK_FALSE(DictFItemParser::parse(dictFromRaw("F\nX\n3\n"), error).has_value());
    CHECK(error == "Invalid F-type DICT item: bad source attribute number");

    error.clear();
    CHECK_FALSE(DictFItemParser::parse(dictFromRaw("F\n2\n\n"), error).has_value());
    CHECK(error == "Invalid F-type DICT item: missing selector");
}

TEST_CASE("CorrelativeEvaluator value index extracts multi-value cell") {
    PickFS::StructuredRecord data;
    data.setAttribute(2, PickFS::RecordAttribute(std::string("A") + kVm + "B" + kVm + "C"));

    FCorrelativeDef def;
    def.sourceAttributeNo = 2;
    def.selectorKind = FSelectorKind::ValueIndex;
    def.valueIndex = 3;
    def.tailRaw = "3";

    std::string error;
    const auto value = CorrelativeEvaluator::evaluateF(def, data, error);
    REQUIRE(value.has_value());
    CHECK(error.empty());
    CHECK(*value == "C");
}

TEST_CASE("CorrelativeEvaluator L and R selectors") {
    PickFS::StructuredRecord data;
    data.setAttribute(2, PickFS::RecordAttribute(std::string("A") + kVm + "B" + kVm + "C"));

    FCorrelativeDef left;
    left.sourceAttributeNo = 2;
    left.selectorKind = FSelectorKind::Leftmost;
    left.tailRaw = "L";

    FCorrelativeDef right;
    right.sourceAttributeNo = 2;
    right.selectorKind = FSelectorKind::Rightmost;
    right.tailRaw = "R";

    std::string error;
    CHECK(CorrelativeEvaluator::evaluateF(left, data, error) == "A");
    error.clear();
    CHECK(CorrelativeEvaluator::evaluateF(right, data, error) == "C");
}

TEST_CASE("CorrelativeEvaluator missing attribute error") {
    PickFS::StructuredRecord data;
    FCorrelativeDef def;
    def.sourceAttributeNo = 9;
    def.selectorKind = FSelectorKind::Leftmost;
    def.tailRaw = "L";

    std::string error;
    CHECK_FALSE(CorrelativeEvaluator::evaluateF(def, data, error).has_value());
    CHECK(error == "F-type: attribute 9 missing");
}

TEST_CASE("CorrelativeEvaluator conversion tail not supported in stage 1") {
    PickFS::StructuredRecord data;
    data.setAttribute(2, PickFS::RecordAttribute("100"));

    FCorrelativeDef def;
    def.sourceAttributeNo = 2;
    def.selectorKind = FSelectorKind::ConversionRaw;
    def.tailRaw = "D2/";

    std::string error;
    CHECK_FALSE(CorrelativeEvaluator::evaluateF(def, data, error).has_value());
    CHECK(error == "F-type conversion not supported");
}

TEST_CASE("CorrelativeEvaluator empty attribute yields empty string for L") {
    PickFS::StructuredRecord data;
    data.setAttribute(2, PickFS::RecordAttribute{});

    FCorrelativeDef def;
    def.sourceAttributeNo = 2;
    def.selectorKind = FSelectorKind::Leftmost;
    def.tailRaw = "L";

    std::string error;
    const auto value = CorrelativeEvaluator::evaluateF(def, data, error);
    REQUIRE(value.has_value());
    CHECK(*value == "");
}

TEST_CASE("DictionaryResolver resolves F-type DICT item") {
    const auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("BALANCE", "F\n2\n3\n"));

    DictionaryResolver resolver;
    const FieldRef ref = resolver.resolveField(fs, "DATA", "BALANCE");

    CHECK(ref.kind == DictFieldKind::FCorrelative);
    REQUIRE(ref.fCorrelative.has_value());
    CHECK(ref.fCorrelative->sourceAttributeNo == 2);
    CHECK(ref.fCorrelative->valueIndex == 3);
    CHECK_FALSE(ref.attributeNo.has_value());
    CHECK(fieldRefIsResolved(ref));
}

TEST_CASE("DictionaryResolver invalid F DICT item stays unresolved") {
    const auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("BAD", "F\n2\n\n"));

    DictionaryResolver resolver;
    const FieldRef ref = resolver.resolveField(fs, "DATA", "BAD");

    CHECK(ref.kind == DictFieldKind::Attribute);
    CHECK_FALSE(ref.attributeNo.has_value());
    CHECK_FALSE(fieldRefIsResolved(ref));
}

TEST_CASE("DictionaryResolver A-type unchanged") {
    const auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));

    DictionaryResolver resolver;
    const FieldRef ref = resolver.resolveField(fs, "DATA", "NAME");

    CHECK(ref.kind == DictFieldKind::Attribute);
    REQUIRE(ref.attributeNo.has_value());
    CHECK(*ref.attributeNo == 1);
    CHECK(fieldRefIsResolved(ref));
}
