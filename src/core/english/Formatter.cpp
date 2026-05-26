#include "Formatter.h"

#include "FormatterDateTime.h"

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
            enum class Kind { Heading, Row, Trailing };
            Kind kind{};
            const std::string *headingTemplate{nullptr};
            const Row *row{nullptr};
            const std::string *trailingText{nullptr};
        };

        /// Stage 1 LayoutPlanner is straight-through; later stages will inject break/total events.
        class LayoutPlanner {
        public:
            LayoutPlanner(const Plan &plan,
                          const std::vector<Row> &rows,
                          const std::vector<std::string> &trailingLines)
                : plan_(plan), rows_(rows), trailingLines_(trailingLines) {}

            [[nodiscard]] std::vector<Event> events() const {
                std::vector<Event> ev;
                ev.reserve((plan_.query.heading.has_value() ? 1U : 0U) + rows_.size() + trailingLines_.size());
                if (plan_.query.heading.has_value()) {
                    Event e{};
                    e.kind = Event::Kind::Heading;
                    e.headingTemplate = &*plan_.query.heading;
                    ev.push_back(e);
                }
                for (const Row &r: rows_) {
                    Event e{};
                    e.kind = Event::Kind::Row;
                    e.row = &r;
                    ev.push_back(e);
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

        /// Owns the current page number. Stage 1 is fixed; Stage 2 will track line counts and advance.
        class PageManager {
        public:
            explicit PageManager(const FormatterContext &ctx) : ctx_(ctx) {}

            [[nodiscard]] int pageNumber() const { return ctx_.pageNumber; }

        private:
            const FormatterContext &ctx_;
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

        /// Render the `id field1 field2 ...` form. Matches the legacy `formatProjectLine`
        /// byte-for-byte: when `projectedFields` is empty (no record loaded), only the id
        /// is emitted; otherwise each field is prefixed with a single space, including
        /// empty placeholder fields.
        std::string renderProjection(const Row &r) {
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
                                  const PageManager &pages) {
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
                    // `@<digits>` (attribute substitution) reserved for Stage 3/4 — empty for now.
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

        /// Stage 1 Renderer: walks the event stream and produces the line vector
        /// that ultimately becomes `Result.lines`. No pagination yet.
        class Renderer {
        public:
            Renderer(const FormatterContext &ctx, const PageManager &pages) : ctx_(ctx), pages_(pages) {}

            [[nodiscard]] std::vector<std::string> render(const std::vector<Event> &events) const {
                std::vector<std::string> lines;
                lines.reserve(events.size());
                for (const Event &e: events) {
                    switch (e.kind) {
                        case Event::Kind::Heading:
                            lines.push_back(renderHeading(*e.headingTemplate, ctx_, pages_));
                            break;
                        case Event::Kind::Row:
                            lines.push_back(renderProjection(*e.row));
                            break;
                        case Event::Kind::Trailing:
                            lines.push_back(*e.trailingText);
                            break;
                    }
                }
                return lines;
            }

        private:
            const FormatterContext &ctx_;
            const PageManager &pages_;
        };
    } // namespace

    FormatterContext defaultFormatterContext() {
        FormatterContext ctx;
        ctx.pageNumber = 1;
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
        const PageManager pages(ctx);
        const Renderer renderer(ctx, pages);
        Result result;
        result.lines = renderer.render(planner.events());
        result.selectedIds = std::move(selectedIds);
        return result;
    }
} // namespace PickCore::English
