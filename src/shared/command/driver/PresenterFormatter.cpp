#include "PresenterFormatter.h"
#include "shared/presentation/Presenter.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace scrap {

PresenterFormatter::PresenterFormatter(std::shared_ptr<Presenter> presenter)
    : CLI::Formatter(), presenter_(presenter)
{
    // Set reasonable column widths for beautiful output
    column_width(30);
    right_column_width(50);
}

std::string PresenterFormatter::make_help(const CLI::App *app, std::string name,
                                         CLI::AppFormatMode mode) const
{
    if (!presenter_) {
        // Fallback to default formatter if no presenter
        return CLI::Formatter::make_help(app, name, mode);
    }

    std::stringstream out;

    // Description
    if (!app->get_description().empty()) {
        out << app->get_description() << "\n";
        out << "\n";
    }

    // Usage
    out << formatUsage(app, name);

    // Positionals
    std::string positionals = formatPositionals(app);
    if (!positionals.empty()) {
        out << "\n";
        out << "Arguments:\n";
        out << positionals;
    }

    // Options
    std::string options = formatOptions(app);
    if (!options.empty()) {
        out << "\n";
        out << "Options:\n";
        out << options;
    }

    // Subcommands
    if (mode != CLI::AppFormatMode::Sub) {
        std::string subcommands = formatSubcommands(app);
        if (!subcommands.empty()) {
            out << "\n";
            out << "Commands:\n";
            out << subcommands;
        }
    }

    // Footer
    if (!app->get_footer().empty()) {
        out << "\n";
        out << app->get_footer() << "\n";
    }

    return out.str();
}

std::string PresenterFormatter::formatUsage(const CLI::App *app, const std::string& name) const
{
    std::stringstream out;
    out << "Usage: ";

    // Build the proper command path
    std::string commandPath = "scrap";
    if (!name.empty()) {
        // Use provided name but ensure it starts with "scrap"
        if (name.find("scrap") != 0) {
            commandPath = "scrap " + name;
        } else {
            commandPath = name;
        }
    } else {
        // Build command path - always include "scrap" prefix
        if (!app->get_name().empty()) {
            commandPath += " " + app->get_name();
        }
    }

    out << commandPath;

    // Add positionals placeholder
    for (const auto* opt : app->get_options()) {
        if (opt->get_positional()) {
            out << " <" << opt->get_name(true, false) << ">";
        }
    }

    // Add options placeholder
    out << " [options]";

    // Add subcommand placeholder if has subcommands
    if (!app->get_subcommands({}).empty()) {
        out << " [command]";
    }

    out << "\n";
    return out.str();
}

std::string PresenterFormatter::formatPositionals(const CLI::App *app) const
{
    std::stringstream out;

    for (const auto* opt : app->get_options()) {
        if (!opt->get_positional()) {
            continue;
        }

        // Format: "  <name>  description"
        std::string name = "  <" + opt->get_name(true, false) + ">";
        out << std::setw(static_cast<int>(column_width_)) << std::left << name;

        std::string desc = opt->get_description();
        if (!desc.empty()) {
            out << desc;
        }

        out << "\n";
    }

    return out.str();
}

std::string PresenterFormatter::formatOptions(const CLI::App *app) const
{
    std::stringstream out;

    for (const auto* opt : app->get_options()) {
        if (opt->get_positional() || opt->get_group() == "HIDDEN") {
            continue;
        }

        // Build option string with short and long forms
        std::stringstream optStr;
        optStr << "  ";

        // Get all option names
        auto names = opt->get_name(false, true);

        // Add short options first
        bool first = true;
        if (!opt->get_snames().empty()) {
            for (const auto& sname : opt->get_snames()) {
                if (!first) optStr << ", ";
                optStr << "-" << sname;
                first = false;
            }
        }

        // Add long options
        if (!opt->get_lnames().empty()) {
            for (const auto& lname : opt->get_lnames()) {
                if (!first) optStr << ", ";
                optStr << "--" << lname;
                first = false;
            }
        }

        // Add value placeholder for non-flag options
        if (opt->get_expected() > 0) {
            // Get type name or use generic placeholder
            std::string typeName = "value";
            if (!opt->get_lnames().empty()) {
                typeName = opt->get_lnames()[0];
            } else if (!opt->get_snames().empty()) {
                typeName = opt->get_snames()[0];
            }
            optStr << "=<" << typeName << ">";
        }

        // Format with proper alignment
        std::string optString = optStr.str();
        out << std::setw(static_cast<int>(column_width_)) << std::left << optString;

        // Add description
        std::string desc = opt->get_description();

        // Add default value if present
        if (!opt->get_default_str().empty()) {
            desc += " [default: " + opt->get_default_str() + "]";
        }

        // Note: Choices are already shown in default_str for validators

        if (!desc.empty()) {
            out << desc;
        }

        out << "\n";
    }

    return out.str();
}

std::string PresenterFormatter::formatSubcommands(const CLI::App *app) const
{
    std::stringstream out;

    auto subcommands = app->get_subcommands({});
    for (const auto* sub : subcommands) {
        if (sub->get_name().empty() || sub->get_group() == "HIDDEN") {
            continue;
        }

        // Format: "  name  description"
        std::string name = "  " + sub->get_name();
        out << std::setw(static_cast<int>(column_width_)) << std::left << name;

        std::string desc = sub->get_description();
        if (!desc.empty()) {
            out << desc;
        }

        out << "\n";
    }

    return out.str();
}

} // namespace scrap
