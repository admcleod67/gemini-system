#include "Formatter.h"

#include "FormatterDateTime.h"

#include <cmath>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace PickCore::English {
    namespace {
        /// Skeletal layout-plan event. The Stage 1 stream is just `[heading?, row*, trailing*]`;
        /// the type lives here so Stage 2-4 (pagination / break-on / totals) can extend it without
        /// touching the public Formatter API.
        struct Event {
            enum class Kind { Heading, TotalLine, BreakLine, Row, Trailing };
            Kind kind{};
            const std::string *headingTemplate{nullptr};
            const std::string *totalFieldToken{nullptr};
            double totalValue{0};
            const Row *row{nullptr};
            const std::string *trailingText{nullptr};
        };

        constexpr int kReportWidth = 79;

        /// Stage 1 LayoutPlanner is straight-through; later stages will inject break/total events.
        class LayoutPlanner {
        public:
            LayoutPlanner(const Plan &plan,
                          const std::vector<Row> &rows,
                          const std::vector<std::string> &trailingLines)
                : plan_(plan), rows_(rows), trailingLines_(trailingLines) {}

            void pushTotalLine(std::vector<Event> &ev, const double value) const {
                Event e{};
                e.kind = Event::Kind::TotalLine;
                e.totalFieldToken = &*plan_.query.totalField;
                e.totalValue = value;
                ev.push_back(e);
            }

            [[nodiscard]] std::vector<Event> events() const {
                const bool breakOn = plan_.query.breakOnField.has_value();
                const bool totalOn = plan_.query.totalField.has_value();
                std::vector<Event> ev;
                ev.reserve((plan_.query.heading.has_value() ? 1U : 0U) + rows_.size() +
                           (breakOn ? rows_.size() : 0U) + (totalOn ? rows_.size() + 1U : 0U) +
                           trailingLines_.size());
                if (plan_.query.heading.has_value()) {
                    Event e{};
                    e.kind = Event::Kind::Heading;
                    e.headingTemplate = &*plan_.query.heading;
                    ev.push_back(e);
                }
                double groupSum = 0;
                double grandSum = 0;
                for (std::size_t ri = 0; ri < rows_.size(); ++ri) {
                    if (breakOn && ri > 0U && rows_[ri].breakKey != rows_[ri - 1U].breakKey) {
                        if (totalOn) {
                            pushTotalLine(ev, groupSum);
                            groupSum = 0;
                        }
                        Event e{};
                        e.kind = Event::Kind::BreakLine;
                        ev.push_back(e);
                    }
                    if (totalOn) {
                        groupSum += rows_[ri].totalAddend;
                        grandSum += rows_[ri].totalAddend;
                    }
                    Event e{};
                    e.kind = Event::Kind::Row;
                    e.row = &rows_[ri];
                    ev.push_back(e);
                }
                if (totalOn) {
                    pushTotalLine(ev, grandSum);
                }
                for (const std::string &line: trailingLines_) {
                    Event e{};
                    e.kind = Event::Kind::Trailing;
                    e.trailingText = &line;
                    ev.push_back(e);
                }
                return ev;
            }

        private:
            const Plan &plan_;
            const std::vector<Row> &rows_;
            const std::vector<std::string> &trailingLines_;
        };

        /// Tracks emitted-line count and the live page number. Pagination only fires when
        /// the query has a HEADING (otherwise the page boundary would be invisible) and
        /// `pageLength > 0` (escape hatch for callers that want to disable it).
        class PageManager {
        public:
            PageManager(const FormatterContext &ctx, bool hasHeading)
                : pageLength_(ctx.pageLength), hasHeading_(hasHeading),
                  pageNumber_(ctx.pageNumber), linesOnPage_(0) {}

            [[nodiscard]] int pageNumber() const { return pageNumber_; }
            [[nodiscard]] int pageLength() const { return pageLength_; }
            [[nodiscard]] int linesOnPage() const { return linesOnPage_; }
            [[nodiscard]] bool paginationActive() const { return hasHeading_ && pageLength_ > 0; }

            void onLineEmitted() { ++linesOnPage_; }
            void advancePage() {
                ++pageNumber_;
                linesOnPage_ = 0;
            }

        private:
            int pageLength_;
            bool hasHeading_;
            int pageNumber_;
            int linesOnPage_;
        };

        bool ieqLower(char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        }

        bool ieq(const std::string &a, const char *b) {
            const std::size_t bLen = std::char_traits<char>::length(b);
            if (a.size() != bLen) {
                return false;
            }
            for (std::size_t i = 0; i < bLen; ++i) {
                if (!ieqLower(a[i], b[i])) {
                    return false;
                }
            }
            return true;
        }

        /// Full-width hyphen break line (Milestone 8 Stage 3).
        std::string renderBreakLine() {
            return std::string(static_cast<std::size_t>(kReportWidth), '-');
        }

        std::string formatTotalValue(const double value) {
            if (value == std::trunc(value)) {
                return std::to_string(static_cast<long long>(value));
            }
            return std::to_string(value);
        }

        std::string renderTotalLine(const std::string &fieldToken, const double value) {
            return "TOTAL " + fieldToken + ": " + formatTotalValue(value);
        }

        /// Render the `id field1 field2 ...` form (or fields only when `idSupp`).
        /// Matches the legacy `formatProjectLine` byte-for-byte when `idSupp` is false.
        std::string renderProjection(const Row &r, const bool idSupp) {
            if (idSupp) {
                std::ostringstream out;
                bool first = true;
                for (const std::string &f: r.projectedFields) {
                    if (!first) {
                        out << ' ';
                    }
                    first = false;
                    out << f;
                }
                return out.str();
            }
            std::ostringstream out;
            out << r.id;
            for (const std::string &f: r.projectedFields) {
                out << ' ' << f;
            }
            return out.str();
        }

        /// Expand `@`-tokens in the heading template using Pick-classic rules.
        std::string renderHeading(const std::string &tpl,
                                  const FormatterContext &ctx,
                                  const PageManager &pages,
                                  const std::vector<std::string> *headingAttrs) {
            std::string out;
            out.reserve(tpl.size());
            const std::size_t n = tpl.size();
            for (std::size_t i = 0; i < n;) {
                const char c = tpl[i];
                if (c != '@') {
                    out.push_back(c);
                    ++i;
                    continue;
                }
                // c == '@'
                if (i + 1U >= n) {
                    out.push_back('@');
                    ++i;
                    continue;
                }
                const char next = tpl[i + 1U];
                if (next == '@') {
                    out.push_back('@');
                    i += 2U;
                    continue;
                }
                const unsigned char nu = static_cast<unsigned char>(next);
                if (!std::isalpha(nu) && !std::isdigit(nu)) {
                    // Bare `@` not followed by an identifier or digit → emit literal `@`
                    // and let the next iteration handle whatever follows.
                    out.push_back('@');
                    ++i;
                    continue;
                }
                // Scan identifier-like token: letters and digits.
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
                const std::string token = tpl.substr(i + 1U, j - (i + 1U));
                if (digitsOnly) {
                    if (headingAttrs != nullptr) {
                        try {
                            const int attrNo = std::stoi(token);
                            if (attrNo >= 1 &&
                                attrNo <= static_cast<int>(headingAttrs->size())) {
                                out += (*headingAttrs)[static_cast<std::size_t>(attrNo - 1)];
                            }
                        } catch (const std::exception &) {
                            // malformed digit token — empty
                        }
                    }
                } else if (ieq(token, "DATE")) {
                    out += formatDate(ctx.currentPickDay);
                } else if (ieq(token, "TIME")) {
                    out += formatTimeOfDay(ctx.currentSecondsOfDay, true);
                } else if (ieq(token, "PAGE")) {
                    out += std::to_string(pages.pageNumber());
                } else {
                    // Unknown `@<identifier>` reserved for later stages — empty for now.
                }
                i = j;
            }
            return out;
        }

        /// Stage 2 Renderer: walks the event stream and produces the final line vector.
        /// Before every Row/Trailing emission, asks PageManager whether the current page
        /// has filled; on overflow it injects exactly one blank separator line and a
        /// re-rendered heading (with the live `@PAGE`) before the next data line.
        class Renderer {
        public:
            Renderer(const FormatterContext &ctx, PageManager &pages, const std::string *headingTemplate,
                     const bool idSupp)
                : ctx_(ctx), pages_(pages), headingTemplate_(headingTemplate), idSupp_(idSupp) {}

            void emitWithPagination(std::vector<std::string> &lines, const std::string &line) {
                if (pages_.paginationActive() && pages_.linesOnPage() >= pages_.pageLength()) {
                    lines.emplace_back(); // single blank separator (not counted on new page)
                    pages_.advancePage();
                    if (headingTemplate_ != nullptr) {
                        lines.push_back(renderHeading(*headingTemplate_, ctx_, pages_, lastHeadingAttrs_));
                        pages_.onLineEmitted();
                    }
                }
                lines.push_back(line);
                pages_.onLineEmitted();
            }

            [[nodiscard]] std::vector<std::string> render(const std::vector<Event> &events) {
                std::vector<std::string> lines;
                lines.reserve(events.size());
                for (const Event &e: events) {
                    if (e.kind == Event::Kind::Heading) {
                        lines.push_back(renderHeading(*e.headingTemplate, ctx_, pages_, lastHeadingAttrs_));
                        pages_.onLineEmitted();
                        continue;
                    }
                    if (e.kind == Event::Kind::TotalLine) {
                        emitWithPagination(lines, renderTotalLine(*e.totalFieldToken, e.totalValue));
                        continue;
                    }
                    if (e.kind == Event::Kind::BreakLine) {
                        emitWithPagination(lines, renderBreakLine());
                        continue;
                    }
                    if (e.kind == Event::Kind::Row) {
                        emitWithPagination(lines, renderProjection(*e.row, idSupp_));
                        lastHeadingAttrs_ = &e.row->headingAttrs;
                        continue;
                    }
                    if (e.kind == Event::Kind::Trailing) {
                        emitWithPagination(lines, *e.trailingText);
                    }
                }
                return lines;
            }

        private:
            const FormatterContext &ctx_;
            PageManager &pages_;
            const std::string *headingTemplate_;
            const std::vector<std::string> *lastHeadingAttrs_{nullptr};
            bool idSupp_;
        };
    } // namespace

    FormatterContext defaultFormatterContext() {
        FormatterContext ctx;
        ctx.pageNumber = 1;
        ctx.pageLength = 24;
        ctx.currentPickDay = currentPickDay();
        ctx.currentSecondsOfDay = currentSecondsOfDay();
        return ctx;
    }

    Result format(const Plan &plan,
                  std::vector<Row> rows,
                  std::vector<std::string> trailingLines,
                  std::vector<std::string> selectedIds,
                  const FormatterContext &ctx) {
        const LayoutPlanner planner(plan, rows, trailingLines);
        const bool hasHeading = plan.query.heading.has_value();
        PageManager pages(ctx, hasHeading);
        const std::string *headingTemplate = hasHeading ? &*plan.query.heading : nullptr;
        Renderer renderer(ctx, pages, headingTemplate, plan.query.idSupp);
        Result result;
        result.lines = renderer.render(planner.events());
        result.selectedIds = std::move(selectedIds);
        return result;
    }
} // namespace PickCore::English
