#ifndef CLI_HPP
#define CLI_HPP

#include <functional>
#include <map>

#include "const_array.hpp"

namespace cli
{

// Split off the first word of an argument string.
// This can be used repeatedly to get a fully split array,
// but doing it this way also allows "raw" commands.
//
// After skipping leading whitespace,
// A word is:
// * any sequence of non-whitespace, non-quote characters
// * any sequence of non-' characters, delimited by '
// * any sequence of non-" characters, delimited by "
//
// Note that there is not any way to have both " and ' in a string
// (doing so would require memory allocation).
// This could be solved by allowing an arbitrary delimiter.
// The C++ standard uses r"delimiter(contents ...)delimiter",
// but then, we don't have backslashes.
// Maybe delimiter'evil string'delimiter ?
//
// The effect of not following a quote by a whitespace or the end
// of the string, or of encountering a quote in an unquoted string,
// is hereby declared undefined.
//
// Returns:
//  * in pair.first, the first word
//  * in pair.second, everything after the first word
//
// If the input contains only whitespace (usually: is empty),
// pair.first.data() will be NULL and pair.second will be the input.
// If there is an error, pair.first.data() will be NULL
// and pair.second.data() will be NULL.
// Therefore, you should make sure the input string is not NULL.
std::pair<const_string, const_string> split_first(const_string);

// Return a trimmed view with no leading whitespace
const_string trim_initial(const_string);
// Return a trimmed view with no trailing whitespace
const_string trim_trailing(const_string);
// Return a trimmed view with no leading or trailing whitespace
inline
const_string trim(const_string s)
{
    return trim_initial(trim_trailing(s));
}

// status of a command
enum class Status
{
    // Was passed an empty string - everything is ok.
    EMPTY,
    // Everything is fine.
    NORMAL,
    // Failed to find a command by that name.
    NOT_FOUND,
    // Generic error
    ERROR,
};

// A single command.
// The first argument is the name of the command itself
// The second is the arguments, which should be parsed according to
// split_first() unless there's a reason to be raw.
typedef std::function<Status(const_string, const_string)> Command;

// A set of commands and an environment
//
class Shell
{
protected:
    std::map<std::string, Command> _commands;
    Command _default;
public:
    Status operator()(const_string line);
};

} // namespace cli

#endif // CLI_HPP
