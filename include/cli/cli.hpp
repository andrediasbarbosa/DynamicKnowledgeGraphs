#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <sstream>

namespace kg {

// Argument value holder
struct ArgValue {
    std::string value;
    bool is_set = false;

    operator bool() const { return is_set; }
    operator std::string() const { return value; }

    int as_int(int default_val = 0) const {
        if (!is_set) return default_val;
        try { return std::stoi(value); }
        catch (...) { return default_val; }
    }

    double as_double(double default_val = 0.0) const {
        if (!is_set) return default_val;
        try { return std::stod(value); }
        catch (...) { return default_val; }
    }

    std::vector<std::string> as_list(char delim = ',') const {
        std::vector<std::string> result;
        if (!is_set) return result;
        std::stringstream ss(value);
        std::string item;
        while (std::getline(ss, item, delim)) {
            if (!item.empty()) result.push_back(item);
        }
        return result;
    }

    std::vector<int> as_int_list(char delim = ',') const {
        std::vector<int> result;
        auto items = as_list(delim);
        for (const auto& item : items) {
            try { result.push_back(std::stoi(item)); }
            catch (...) {}
        }
        return result;
    }
};

// Parsed arguments container
class Args {
public:
    std::map<std::string, ArgValue> named;
    std::vector<std::string> positional;

    ArgValue get(const std::string& name, const std::string& default_val = "") const {
        auto it = named.find(name);
        if (it != named.end()) return it->second;
        return ArgValue{default_val, !default_val.empty()};
    }

    bool has(const std::string& name) const {
        auto it = named.find(name);
        return it != named.end() && it->second.is_set;
    }

    std::string require(const std::string& name) const {
        auto it = named.find(name);
        if (it == named.end() || !it->second.is_set) {
            throw std::runtime_error("Missing required argument: --" + name);
        }
        return it->second.value;
    }
};

// Argument definition
struct ArgDef {
    std::string name;
    std::string short_name;
    std::string description;
    std::string default_value;
    bool required = false;
    bool is_flag = false;  // If true, no value expected (presence = true)
};

// Command definition
struct Command {
    std::string name;
    std::string description;
    std::vector<ArgDef> args;
    std::function<int(const Args&)> handler;

    void print_help() const {
        std::cout << "\nUsage: kg " << name;
        for (const auto& arg : args) {
            if (arg.required) {
                std::cout << " --" << arg.name << " <value>";
            }
        }
        std::cout << " [options]\n\n";
        std::cout << description << "\n\n";
        std::cout << "Options:\n";
        for (const auto& arg : args) {
            std::cout << "  --" << arg.name;
            if (!arg.short_name.empty()) {
                std::cout << ", -" << arg.short_name;
            }
            if (!arg.is_flag) {
                std::cout << " <value>";
            }
            std::cout << "\n";
            std::cout << "      " << arg.description;
            if (!arg.default_value.empty()) {
                std::cout << " (default: " << arg.default_value << ")";
            }
            if (arg.required) {
                std::cout << " [required]";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
};

// Main CLI class
class CLI {
public:
    CLI(const std::string& program_name, const std::string& version)
        : program_name_(program_name), version_(version) {}

    void register_command(Command cmd) {
        commands_[cmd.name] = std::move(cmd);
    }

    int run(int argc, char** argv) {
        if (argc < 2) {
            print_help();
            return 1;
        }

        std::string cmd_name = argv[1];

        if (cmd_name == "--help" || cmd_name == "-h") {
            print_help();
            return 0;
        }

        if (cmd_name == "--version" || cmd_name == "-v") {
            std::cout << program_name_ << " version " << version_ << "\n";
            return 0;
        }

        auto it = commands_.find(cmd_name);
        if (it == commands_.end()) {
            std::cerr << "Unknown command: " << cmd_name << "\n";
            std::cerr << "Run '" << program_name_ << " --help' for available commands.\n";
            return 1;
        }

        const Command& cmd = it->second;

        // Check for command-level help
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                cmd.print_help();
                return 0;
            }
        }

        // Parse arguments
        Args args;
        try {
            args = parse_args(argc - 2, argv + 2, cmd);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            cmd.print_help();
            return 1;
        }

        // Run handler
        try {
            return cmd.handler(args);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    void print_help() const {
        std::cout << program_name_ << " - Knowledge Graph CLI\n\n";
        std::cout << "Usage: " << program_name_ << " <command> [options]\n\n";
        std::cout << "Commands:\n";
        for (const auto& [name, cmd] : commands_) {
            std::cout << "  " << name;
            // Pad to 16 chars
            for (size_t i = name.length(); i < 16; ++i) std::cout << " ";
            std::cout << cmd.description << "\n";
        }
        std::cout << "\nRun '" << program_name_ << " <command> --help' for command-specific options.\n";
        std::cout << "\nVersion: " << version_ << "\n";
    }

private:
    Args parse_args(int argc, char** argv, const Command& cmd) {
        Args result;

        // Build lookup maps
        std::map<std::string, const ArgDef*> by_name;
        std::map<std::string, const ArgDef*> by_short;
        for (const auto& arg : cmd.args) {
            by_name["--" + arg.name] = &arg;
            if (!arg.short_name.empty()) {
                by_short["-" + arg.short_name] = &arg;
            }
        }

        // Parse
        for (int i = 0; i < argc; ++i) {
            std::string arg = argv[i];

            const ArgDef* def = nullptr;
            if (arg.rfind("--", 0) == 0) {
                // Check for --name=value format
                auto eq_pos = arg.find('=');
                if (eq_pos != std::string::npos) {
                    std::string name_part = arg.substr(0, eq_pos);
                    std::string value_part = arg.substr(eq_pos + 1);
                    auto it = by_name.find(name_part);
                    if (it != by_name.end()) {
                        def = it->second;
                        result.named[def->name] = ArgValue{value_part, true};
                        continue;
                    }
                }
                auto it = by_name.find(arg);
                if (it != by_name.end()) def = it->second;
            } else if (arg.rfind("-", 0) == 0 && arg.length() == 2) {
                auto it = by_short.find(arg);
                if (it != by_short.end()) def = it->second;
            } else {
                result.positional.push_back(arg);
                continue;
            }

            if (!def) {
                throw std::runtime_error("Unknown argument: " + arg);
            }

            if (def->is_flag) {
                result.named[def->name] = ArgValue{"true", true};
            } else {
                if (i + 1 >= argc) {
                    throw std::runtime_error("Argument " + arg + " requires a value");
                }
                result.named[def->name] = ArgValue{argv[++i], true};
            }
        }

        // Apply defaults and check required
        for (const auto& arg : cmd.args) {
            if (result.named.find(arg.name) == result.named.end()) {
                if (arg.required) {
                    throw std::runtime_error("Missing required argument: --" + arg.name);
                }
                if (!arg.default_value.empty()) {
                    result.named[arg.name] = ArgValue{arg.default_value, true};
                }
            }
        }

        return result;
    }

    std::string program_name_;
    std::string version_;
    std::map<std::string, Command> commands_;
};

} // namespace kg
