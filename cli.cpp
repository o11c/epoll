#include "cli.hpp"

#include <algorithm>

namespace cli
{
    std::pair<const_string, const_string> split_first(const_string s)
    {
        s = trim_initial(s);
        if (not s)
            return {nullptr, s};

        switch(char c = s.front())
        {
        default:
        {
            // std::isspace has overloads that confuse the template
            // is there no way to resolve that?
            auto sp = std::find_if(s.begin(), s.end(), ::isspace);
            if (std::find(s.begin(), sp, '\'') != sp
                or std::find(s.begin(), sp, '"') != sp)
                return {nullptr, nullptr};
            return {{s.begin(), sp}, {sp, s.end()}};
        }
        case '\'':
        case '"':
        {
            auto q = std::find(s.begin() + 1, s.end(), c);
            if (q == s.end())
                return {nullptr, nullptr};
            if (q + 1 != s.end()
                and not std::isspace(*(q+1)))
                return {nullptr, nullptr};
            return {{s.begin() + 1, q}, {q + 1, s.end()}};
        }
        }
    }

    const_string trim_initial(const_string s)
    {
        while(s and std::isspace(s.front()))
            s = s.tail(s.size() - 1);
        return s;
    }

    const_string trim_trailing(const_string s)
    {
        while(s and std::isspace(s.back()))
            s = s.head(s.size() - 1);
        return s;
    }

    Status Shell::operator()(const_string line)
    {
        auto pair = split_first(line);
        if (pair.first.data() == nullptr)
            return Status::EMPTY;
        auto cmd = pair.first;
        auto args = pair.second;
        auto it = _commands.find(std::string(cmd.begin(), cmd.end()));
        if (it == _commands.end())
        {
            if (_default)
                return _default(cmd, args);
            return Status::NOT_FOUND;
        }
        return it->second(cmd, args);

    }
}
