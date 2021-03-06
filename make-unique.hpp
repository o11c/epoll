#ifndef MAKE_UNIQUE_HPP
#define MAKE_UNIQUE_HPP
// Copyright 2012 Ben Longbons
// GPL3+

# include <memory>

template<class T, class... A>
std::unique_ptr<T> make_unique(A&&... a)
{
    return std::unique_ptr<T>(new T(std::forward<A>(a)...));
}

#endif // MAKE_UNIQUE_HPP
