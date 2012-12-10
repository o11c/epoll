// Copyright 2012 Ben Longbons
// GPL3+
#include <sstream>

namespace cli
{

inline
bool extract1(const_string word, const_string *p)
{
    *p = word;
    return true;
}

template<class T>
inline
bool extract1(const_string word, T *p)
{
    std::istringstream in(std::string(word.begin(), word.end()));
    char x;
    in >> std::noskipws;
    return in >> *p and (in >> x).eof();
}

// Base case succeeds only if line is empty and not error.
inline
bool do_extract(const_string line)
{
    auto pair = cli::split_first(line);
    return pair.first.data() == nullptr and pair.second.data() != nullptr;
}

template<class F, class... R>
inline
bool do_extract(const_string line, F *f, R *... r)
{
    auto pair = cli::split_first(line);
    return pair.first.data()
        && extract1(pair.first, f)
        && do_extract(pair.second, r...);
}

template<class... P>
inline
bool extract(const_string line, P *... p)
{
    return do_extract(line, p...);
}

} // namespace cli
