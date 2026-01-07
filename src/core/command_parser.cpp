#include "command_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

optional<Command> CommandParser::parse(const string& input) {
    // Trim input
    string trimmed = input;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) {
        return nullopt;
    }

    istringstream iss(trimmed);
    string token;
    Command cmd;

    // Read operation (first token, uppercase)
    if (!(iss >> token)) {
        return nullopt;
    }

    // Convert to uppercase
    transform(token.begin(), token.end(), token.begin(), 
                   [](unsigned char c) { return toupper(c); });
    cmd.operation = token;

    // Read remaining arguments
    while (iss >> token) {
        cmd.args.push_back(token);
    }

    // Validate
    if (!validate(cmd)) {
        return nullopt;
    }

    return cmd;
}

bool CommandParser::validate(const Command& cmd) {
    // Simple validation: check operation and argument count
    if (cmd.operation.empty()) {
        return false;
    }

    if (cmd.operation == "CONFIG") {
        // CONFIG policy capacity
        return cmd.args.size() == 2;
    }
    else if (cmd.operation == "SET") {
        // SET key value [ttl]
        return cmd.args.size() >= 2 && cmd.args.size() <= 3;
    } 
    else if (cmd.operation == "GET" || cmd.operation == "DEL" || 
             cmd.operation == "EXISTS") {
        // GET/DEL/EXISTS key
        return cmd.args.size() == 1;
    }
    else if (cmd.operation == "CLEAR") {
        // CLEAR (no args)
        return cmd.args.empty();
    }
    else if (cmd.operation == "SIZE") {
        // SIZE (no args)
        return cmd.args.empty();
    }
    else if (cmd.operation == "CLEANUP") {
        // CLEANUP (no args)
        return cmd.args.empty();
    }
    else if (cmd.operation == "HELP") {
        // HELP (no args)
        return cmd.args.empty();
    }
    else if (cmd.operation == "STATUS") {
        // STATUS (no args)
        return cmd.args.empty();
    }
    else if (cmd.operation == "EXIT") {
        // EXIT (no args)
        return cmd.args.empty();
    }
    else if (cmd.operation == "ADDNODE") {
        // ADDNODE nodeId
        return cmd.args.size() == 1;
    }
    else if (cmd.operation == "REMOVENODE") {
        // REMOVENODE nodeId
        return cmd.args.size() == 1;
    }
    else if (cmd.operation == "FINDNODE") {
        // FINDNODE key
        return cmd.args.size() == 1;
    }
    else if (cmd.operation == "REPLICAS") {
        // REPLICAS key [count]
        return cmd.args.size() >= 1 && cmd.args.size() <= 2;
    }
    else if (cmd.operation == "NODES") {
        // NODES (no args)
        return cmd.args.empty();
    }
    else if (cmd.operation == "CLUSTERINFO") {
        // CLUSTERINFO (no args)
        return cmd.args.empty();
    }
    else if (cmd.operation == "CLUSTER") {
        // CLUSTER (no args) - switches to distributed mode
        return cmd.args.empty();
    }else if (cmd.operation == "FAILNODE"){
        return cmd.args.size()==1;
    }else if (cmd.operation == "FAILEDNODES"){
        return cmd.args.empty();
    }

    return false;
}