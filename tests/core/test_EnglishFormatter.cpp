#include <doctest/doctest.h>

#include "EnglishTypes.h"
#include "Formatter.h"
#include "FormatterDateTime.h"

namespace {
    using namespace PickCore::English;

    Plan makePlan(std::optional<std::string> heading = std::nullopt) {
        Query q;
        q.verb = Verb::LIST;
        q.fileName = "DATA";
        q.heading = std::move(heading);
        return Plan{q};
    }

    FormatterContext fixedCtx(int pickDay = 0, int secondsOfDay = 0, int page = 1) {
        FormatterContext ctx;
        ctx.pageNumber = page;
        ctx.currentPickDay = pickDay;
        ctx.currentSecondsOfDay = secondsOfDay;
        return ctx;
    }

    Row row(std::string id, std::vector<std::string> fields = {}) {
        Row r;
        r.id = std::move(id);
        r.projectedFields = std::move(fields);
        return r;
    }
} // namespace

TEST_CASE("formatter without heading is byte-identical to legacy projection") {
    const Plan plan = makePlan();
    std::vector<Row> rows;
    rows.push_back(row("R1", {"ALICE", "LONDON"}));
    rows.push_back(row("R2", {"BOB", "PARIS"}));
    rows.push_back(row("R3"));

    const Result r = format(plan, rows, {}, {"R1", "R2", "R3"}, fixedCtx());
    REQUIRE(r.lines.size() == 3);
    CHECK(r.lines[0] == "R1 ALICE LONDON");
    CHECK(r.lines[1] == "R2 BOB PARIS");
    CHECK(r.lines[2] == "R3");
    CHECK(r.selectedIds == std::vector<std::string>{"R1", "R2", "R3"});
}

TEST_CASE("formatter renders plain heading text verbatim") {
    const Plan plan = makePlan("Customers");
    std::vector<Row> rows;
    rows.push_back(row("R1", {"X"}));
    const Result r = format(plan, rows, {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 2);
    CHECK(r.lines[0] == "Customers");
    CHECK(r.lines[1] == "R1 X");
}

TEST_CASE("formatter substitutes @DATE from FormatterContext") {
    const Plan plan = makePlan("Date @DATE");
    const Result r = format(plan, {}, {}, {}, fixedCtx(/*pickDay=*/732));
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "Date 01 Jan 1970");
}

TEST_CASE("formatter substitutes @TIME from FormatterContext") {
    const Plan plan = makePlan("Time @TIME");
    const Result r = format(plan, {}, {}, {}, fixedCtx(0, /*secondsOfDay=*/3661));
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "Time 01:01:01");
}

TEST_CASE("formatter substitutes @PAGE from FormatterContext") {
    const Plan plan = makePlan("Page @PAGE");
    const Result r = format(plan, {}, {}, {}, fixedCtx(0, 0, /*page=*/7));
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "Page 7");
}

TEST_CASE("formatter substitution tokens are case-insensitive") {
    const Plan plan = makePlan("d=@date t=@Time p=@page");
    const Result r = format(plan, {}, {}, {}, fixedCtx(732, 3661, 1));
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "d=01 Jan 1970 t=01:01:01 p=1");
}

TEST_CASE("formatter @@ escapes to literal @ and disables substitution") {
    const Plan plan = makePlan("@@DATE");
    const Result r = format(plan, {}, {}, {}, fixedCtx(732));
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "@DATE");
}

TEST_CASE("formatter @<digits> renders empty (reserved for Stage 3-4)") {
    const Plan plan = makePlan("Attr @1");
    const Result r = format(plan, {}, {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "Attr ");
}

TEST_CASE("formatter unknown @<identifier> renders empty") {
    const Plan plan = makePlan("Unknown @FOO done");
    const Result r = format(plan, {}, {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "Unknown  done");
}

TEST_CASE("formatter bare @ without identifier is literal") {
    const Plan plan = makePlan("a @ b");
    const Result r = format(plan, {}, {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "a @ b");
}

TEST_CASE("formatter trailing @ at end of template is literal") {
    const Plan plan = makePlan("end@");
    const Result r = format(plan, {}, {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "end@");
}

TEST_CASE("formatter empty HEADING emits a blank first line") {
    const Plan plan = makePlan("");
    std::vector<Row> rows;
    rows.push_back(row("R1", {"X"}));
    const Result r = format(plan, rows, {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 2);
    CHECK(r.lines[0].empty());
    CHECK(r.lines[1] == "R1 X");
}

TEST_CASE("formatter appends trailing lines after rows") {
    Plan plan = makePlan("Top");
    plan.query.verb = Verb::SELECT;
    const Result r = format(plan, {}, {"3 records selected"}, {"A", "B", "C"}, fixedCtx());
    REQUIRE(r.lines.size() == 2);
    CHECK(r.lines[0] == "Top");
    CHECK(r.lines[1] == "3 records selected");
}

TEST_CASE("FormatterDateTime parity with BASIC OCONV \"D\" on day 732") {
    CHECK(formatDate(732) == "01 Jan 1970");
}

TEST_CASE("FormatterDateTime formatTimeOfDay HH:MM:SS") {
    CHECK(formatTimeOfDay(3661, true) == "01:01:01");
    CHECK(formatTimeOfDay(3661, false) == "01:01");
    CHECK(formatTimeOfDay(0, true) == "00:00:00");
    CHECK(formatTimeOfDay(86400, true) == "00:00:00"); // wraps
}
