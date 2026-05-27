#include <doctest/doctest.h>

#include "EnglishTypes.h"
#include "Formatter.h"
#include "FormatterDateTime.h"

namespace {
    using namespace PickCore::English;

    Plan makePlan(std::optional<std::string> heading = std::nullopt,
                  std::optional<std::string> breakOnField = std::nullopt,
                  std::optional<std::string> totalField = std::nullopt) {
        Query q;
        q.verb = Verb::LIST;
        q.fileName = "DATA";
        q.heading = std::move(heading);
        q.breakOnField = std::move(breakOnField);
        q.totalField = std::move(totalField);
        return Plan{q};
    }

    std::string hyphenBreakLine() {
        return std::string(79, '-');
    }

    FormatterContext fixedCtx(int pickDay = 0, int secondsOfDay = 0, int page = 1, int pageLength = 24) {
        FormatterContext ctx;
        ctx.pageNumber = page;
        ctx.pageLength = pageLength;
        ctx.currentPickDay = pickDay;
        ctx.currentSecondsOfDay = secondsOfDay;
        return ctx;
    }

    std::vector<Row> makeRows(int count, const std::string &prefix = "R") {
        std::vector<Row> rows;
        rows.reserve(static_cast<std::size_t>(count));
        for (int i = 1; i <= count; ++i) {
            Row r;
            r.id = prefix + std::to_string(i);
            rows.push_back(std::move(r));
        }
        return rows;
    }

    Row row(std::string id,
            std::vector<std::string> fields = {},
            std::string breakKey = {},
            double totalAddend = 0,
            std::vector<std::string> headingAttrs = {}) {
        Row r;
        r.id = std::move(id);
        r.projectedFields = std::move(fields);
        r.breakKey = std::move(breakKey);
        r.totalAddend = totalAddend;
        r.headingAttrs = std::move(headingAttrs);
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

TEST_CASE("formatter @<digits> renders empty before any row") {
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

// --- Milestone 8 Stage 3: BREAK-ON ---

TEST_CASE("formatter without BREAK-ON emits no hyphen break lines") {
    const Plan plan = makePlan();
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, "A"));
    rows.push_back(row("R2", {}, "A"));
    rows.push_back(row("R3", {}, "B"));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 3);
    for (const std::string &line: r.lines) {
        CHECK(line != hyphenBreakLine());
    }
}

TEST_CASE("formatter BREAK-ON emits hyphen line when breakKey changes") {
    const Plan plan = makePlan(std::nullopt, "CITY");
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, "A"));
    rows.push_back(row("R2", {}, "A"));
    rows.push_back(row("R3", {}, "B"));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 4);
    CHECK(r.lines[0] == "R1");
    CHECK(r.lines[1] == "R2");
    CHECK(r.lines[2] == hyphenBreakLine());
    CHECK(r.lines[3] == "R3");
}

TEST_CASE("formatter BREAK-ON multiple key changes emit multiple hyphen lines") {
    const Plan plan = makePlan(std::nullopt, "CITY");
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, "A"));
    rows.push_back(row("R2", {}, "B"));
    rows.push_back(row("R3", {}, "C"));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 5);
    CHECK(r.lines[1] == hyphenBreakLine());
    CHECK(r.lines[3] == hyphenBreakLine());
}

TEST_CASE("formatter BREAK-ON single row emits no hyphen line") {
    const Plan plan = makePlan(std::nullopt, "CITY");
    const Result r = format(plan, {row("R1", {}, "A")}, {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "R1");
}

TEST_CASE("formatter BREAK-ON identical keys emit no hyphen line") {
    const Plan plan = makePlan(std::nullopt, "CITY");
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, "X"));
    rows.push_back(row("R2", {}, "X"));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 2);
    CHECK(r.lines[0] == "R1");
    CHECK(r.lines[1] == "R2");
}

// --- Milestone 8 Stage 4: TOTAL ---

TEST_CASE("formatter without TOTAL emits no total lines") {
    const Plan plan = makePlan();
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, {}, 10));
    rows.push_back(row("R2", {}, {}, 20));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 2);
    for (const std::string &line: r.lines) {
        CHECK(line.find("TOTAL ") == std::string::npos);
    }
}

TEST_CASE("formatter TOTAL emits grand total after rows") {
    const Plan plan = makePlan(std::nullopt, std::nullopt, "AMOUNT");
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, {}, 10));
    rows.push_back(row("R2", {}, {}, 20));
    rows.push_back(row("R3", {}, {}, 30));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 4);
    CHECK(r.lines[0] == "R1");
    CHECK(r.lines[1] == "R2");
    CHECK(r.lines[2] == "R3");
    CHECK(r.lines[3] == "TOTAL AMOUNT: 60");
}

TEST_CASE("formatter TOTAL single row emits grand total only") {
    const Plan plan = makePlan(std::nullopt, std::nullopt, "AMT");
    const Result r = format(plan, {row("R1", {}, {}, 42)}, {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 2);
    CHECK(r.lines[0] == "R1");
    CHECK(r.lines[1] == "TOTAL AMT: 42");
}

TEST_CASE("formatter BREAK-ON and TOTAL emit subtotal before hyphen then grand total") {
    const Plan plan = makePlan(std::nullopt, "CITY", "AMT");
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, "A", 1));
    rows.push_back(row("R2", {}, "A", 2));
    rows.push_back(row("R3", {}, "B", 4));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 6);
    CHECK(r.lines[0] == "R1");
    CHECK(r.lines[1] == "R2");
    CHECK(r.lines[2] == "TOTAL AMT: 3");
    CHECK(r.lines[3] == hyphenBreakLine());
    CHECK(r.lines[4] == "R3");
    CHECK(r.lines[5] == "TOTAL AMT: 7");
}

TEST_CASE("formatter TOTAL with zero rows emits grand total zero") {
    const Plan plan = makePlan(std::nullopt, std::nullopt, "AMOUNT");
    const Result r = format(plan, {}, {"5"}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 2);
    CHECK(r.lines[0] == "TOTAL AMOUNT: 0");
    CHECK(r.lines[1] == "5");
}

TEST_CASE("formatter HEADING @1 uses last emitted row attributes") {
    const Plan plan = makePlan("Item @1");
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, {}, 0, {"ALICE"}));
    rows.push_back(row("R2", {}, {}, 0, {"BOB"}));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx());
    REQUIRE(r.lines.size() == 3);
    CHECK(r.lines[0] == "Item ");
    CHECK(r.lines[1] == "R1");
    CHECK(r.lines[2] == "R2");
}

TEST_CASE("formatter HEADING @1 on page break uses last row on previous page") {
    const Plan plan = makePlan("Name @1", std::nullopt, std::nullopt);
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, {}, 0, {"ALICE"}));
    rows.push_back(row("R2", {}, {}, 0, {"BOB"}));
    rows.push_back(row("R3", {}, {}, 0, {"CAROL"}));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx(0, 0, 1, /*pageLength=*/2));
    // Page 1: heading + R1 (2 lines). Before R2: page break -> blank + heading with @1=ALICE
    REQUIRE(r.lines.size() >= 5);
    CHECK(r.lines[0] == "Name ");
    CHECK(r.lines[1] == "R1");
    CHECK(r.lines[2].empty());
    CHECK(r.lines[3] == "Name ALICE");
    CHECK(r.lines[4] == "R2");
}

TEST_CASE("formatter TOTAL line counts toward pagination") {
    const Plan plan = makePlan("Top", std::nullopt, "AMT");
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, {}, 1));
    rows.push_back(row("R2", {}, {}, 2));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx(0, 0, 1, /*pageLength=*/2));
    // Page 1: heading + R1. Before R2: page break. Page 2: R2 fills page; TOTAL triggers another break.
    REQUIRE(r.lines.size() >= 8);
    CHECK(r.lines[0] == "Top");
    CHECK(r.lines[1] == "R1");
    CHECK(r.lines[2].empty());
    CHECK(r.lines[3] == "Top");
    CHECK(r.lines[4] == "R2");
    CHECK(r.lines[5].empty());
    CHECK(r.lines[6] == "Top");
    CHECK(r.lines[7] == "TOTAL AMT: 3");
}

TEST_CASE("formatter BREAK-ON hyphen line counts toward pagination") {
    const Plan plan = makePlan("Top", "CITY");
    std::vector<Row> rows;
    rows.push_back(row("R1", {}, "A"));
    rows.push_back(row("R2", {}, "A"));
    rows.push_back(row("R3", {}, "B"));
    const Result r = format(plan, std::move(rows), {}, {}, fixedCtx(0, 0, 1, /*pageLength=*/3));
    // Page 1: heading + R1 + R2 (3 lines). Before break: page break -> blank + heading.
    // Page 2: hyphen + R3
    REQUIRE(r.lines.size() >= 5);
    CHECK(r.lines[0] == "Top");
    CHECK(r.lines[1] == "R1");
    CHECK(r.lines[2] == "R2");
    CHECK(r.lines[3].empty());
    CHECK(r.lines[4] == "Top");
    CHECK(r.lines[5] == hyphenBreakLine());
    CHECK(r.lines[6] == "R3");
}

// --- Milestone 8 Stage 2: pagination ---

TEST_CASE("formatter pagination is inactive without HEADING") {
    const Plan plan = makePlan(); // no HEADING
    const Result r = format(plan, makeRows(30), {}, {}, fixedCtx(0, 0, 1, /*pageLength=*/10));
    REQUIRE(r.lines.size() == 30);
    CHECK(r.lines[0] == "R1");
    CHECK(r.lines[29] == "R30");
}

TEST_CASE("formatter pagination: heading + 9 rows fits in one page at pageLength=10") {
    const Plan plan = makePlan("X");
    const Result r = format(plan, makeRows(9), {}, {}, fixedCtx(0, 0, 1, 10));
    REQUIRE(r.lines.size() == 10);
    CHECK(r.lines[0] == "X");
    CHECK(r.lines[1] == "R1");
    CHECK(r.lines[9] == "R9");
}

TEST_CASE("formatter pagination: heading + 10 rows overflows into a second page") {
    const Plan plan = makePlan("X");
    const Result r = format(plan, makeRows(10), {}, {}, fixedCtx(0, 0, 1, 10));
    REQUIRE(r.lines.size() == 13);
    CHECK(r.lines[0] == "X");
    CHECK(r.lines[1] == "R1");
    CHECK(r.lines[9] == "R9");
    CHECK(r.lines[10] == "");     // blank separator
    CHECK(r.lines[11] == "X");    // re-emitted heading on page 2
    CHECK(r.lines[12] == "R10");
}

TEST_CASE("formatter pagination: 30 rows + pageLength=10 yields 4 pages") {
    const Plan plan = makePlan("X");
    const Result r = format(plan, makeRows(30), {}, {}, fixedCtx(0, 0, 1, 10));
    REQUIRE(r.lines.size() == 37); // 10 + 11 + 11 + 5
    // Heading repeats at predictable positions.
    CHECK(r.lines[0] == "X");
    CHECK(r.lines[10] == "");
    CHECK(r.lines[11] == "X");
    CHECK(r.lines[21] == "");
    CHECK(r.lines[22] == "X");
    CHECK(r.lines[32] == "");
    CHECK(r.lines[33] == "X");
    // Boundary rows.
    CHECK(r.lines[9] == "R9");
    CHECK(r.lines[12] == "R10");
    CHECK(r.lines[20] == "R18");
    CHECK(r.lines[23] == "R19");
    CHECK(r.lines[31] == "R27");
    CHECK(r.lines[34] == "R28");
    CHECK(r.lines[36] == "R30");
}

TEST_CASE("formatter pagination: @PAGE is dynamic across pages") {
    const Plan plan = makePlan("Page @PAGE");
    const Result r = format(plan, makeRows(25), {}, {}, fixedCtx(0, 0, 1, 10));
    REQUIRE(r.lines.size() == 30); // 10 + 11 + 9
    CHECK(r.lines[0] == "Page 1");
    CHECK(r.lines[10] == "");
    CHECK(r.lines[11] == "Page 2");
    CHECK(r.lines[21] == "");
    CHECK(r.lines[22] == "Page 3");
    CHECK(r.lines[29] == "R25");
}

TEST_CASE("formatter pagination: trailing line counts toward the page") {
    Plan plan = makePlan("X");
    plan.query.verb = Verb::COUNT;
    // Heading (1) + trailing "5" (2) fits exactly into a pageLength=2 page.
    const Result r = format(plan, {}, {"5"}, {}, fixedCtx(0, 0, 1, 2));
    REQUIRE(r.lines.size() == 2);
    CHECK(r.lines[0] == "X");
    CHECK(r.lines[1] == "5");
}

TEST_CASE("formatter pagination: pageLength<=0 disables pagination as escape hatch") {
    const Plan plan = makePlan("X");
    const Result r = format(plan, makeRows(5), {}, {}, fixedCtx(0, 0, 1, /*pageLength=*/0));
    REQUIRE(r.lines.size() == 6);
    CHECK(r.lines[0] == "X");
    CHECK(r.lines[5] == "R5");
    // No blank separators and no repeat heading anywhere.
    for (std::size_t i = 1; i < r.lines.size(); ++i) {
        CHECK_FALSE(r.lines[i].empty());
        CHECK(r.lines[i] != "X");
    }
}

TEST_CASE("formatter pagination: pageLength=1 puts each row on its own page (degenerate)") {
    const Plan plan = makePlan("X");
    const Result r = format(plan, makeRows(2), {}, {}, fixedCtx(0, 0, 1, /*pageLength=*/1));
    // Page 1: "X" (overflow before row 1)
    // blank, "X", R1   (page 2; overflow before row 2)
    // blank, "X", R2   (page 3)
    REQUIRE(r.lines.size() == 7);
    CHECK(r.lines[0] == "X");
    CHECK(r.lines[1] == "");
    CHECK(r.lines[2] == "X");
    CHECK(r.lines[3] == "R1");
    CHECK(r.lines[4] == "");
    CHECK(r.lines[5] == "X");
    CHECK(r.lines[6] == "R2");
}

TEST_CASE("formatter pagination: empty rows + HEADING stays on page 1") {
    const Plan plan = makePlan("X");
    const Result r = format(plan, {}, {}, {}, fixedCtx(0, 0, 1, 10));
    REQUIRE(r.lines.size() == 1);
    CHECK(r.lines[0] == "X");
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
