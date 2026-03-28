#pragma once

#include "command/ParserAdapter.h"

#include <memory>

namespace scrap::Command {

/**
 * @brief ParserAdapter implementation backed by CLI11.
 *
 * Completely hides the CLI11 library behind a PIMPL firewall.
 * No CLI11 headers are included in this file; all library
 * interaction is confined to the .cpp translation unit.
 *
 * The adapter is stateless with respect to parsing: configure()
 * stores the CommandSpec tree, and each parse() call constructs
 * a fresh CLI::App internally so that parse() can be const.
 */
class CLI11ParserAdapter : public ParserAdapter {
public:
    CLI11ParserAdapter();
    ~CLI11ParserAdapter() override;

    CLI11ParserAdapter(const CLI11ParserAdapter&) = delete;
    CLI11ParserAdapter& operator=(const CLI11ParserAdapter&) = delete;
    CLI11ParserAdapter(CLI11ParserAdapter&&) noexcept;
    CLI11ParserAdapter& operator=(CLI11ParserAdapter&&) noexcept;

    /**
     * @brief Store the command spec tree for subsequent parse() calls.
     *
     * Deep-copies the spec span into internal storage.  No CLI11
     * objects are created here; they are built fresh on each parse().
     *
     * @param specs Top-level CommandSpec entries (with subcommand trees).
     */
    auto configure(std::span<const CommandSpec> specs) -> void override;

    /**
     * @brief Parse command-line arguments against the configured specs.
     *
     * Constructs a temporary CLI::App, maps the stored CommandSpec
     * tree onto CLI11 subcommands and options, then parses @p argv.
     *
     * - Success yields a CommandInvocation with a dot-separated
     *   command path and the parsed option values.
     * - --help / --version are caught as ParseDirective.
     * - All other CLI11 exceptions become ParseFailure.
     *
     * @param argv Raw argument vector (argv[0] is the program name).
     * @return ParseResult indicating success, directive, or failure.
     */
    [[nodiscard]] auto parse(std::span<const char* const> argv) const -> ParseResult override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace scrap::Command
