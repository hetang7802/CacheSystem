#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>
#include <vector>
#include <optional>

using namespace std;

/**
 * Simple command structure
 */
struct Command {
    string operation;  // GET, SET, DEL, EXISTS, etc.
    vector<string> args;

    Command() = default;
    Command(const string& op) : operation(op) {}
};

/**
 * Parser for simple command format
 * Supports both in-memory and simple whitespace-separated formats
 */
class CommandParser {
public:
    /**
     * Parse a simple command string
     * Format: "OPERATION key value ttl"
     * Examples:
     *   "SET mykey myvalue"
     *   "SET mykey myvalue 3600"
     *   "GET mykey"
     *   "DEL mykey"
     *   "EXISTS mykey"
     * 
     * @param input The input string
     * @return Parsed command or nullopt if invalid
     */
    static optional<Command> parse(const string& input);

    /**
     * Validate if a command is valid
     * @param cmd The command to validate
     * @return true if valid
     */
    static bool validate(const Command& cmd);
};

#endif // COMMAND_PARSER_H
