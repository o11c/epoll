// Copyright 2012 Ben Longbons
#ifndef CONST_ARRAY_HPP
#define CONST_ARRAY_HPP

#include <string>
#include <cstring>
#include <iterator>

template<class T>
class const_array
{
    const T *d;
    size_t n;
public:
    typedef const T *iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    const_array(const T *p, size_t z)
    : d(p), n(z)
    {}
    // All methods are non-const to "encourage" you
    // to always pass a const_array by value.
    // After all, "const const_array" looks funny.
    const T *data() { return d; }
    size_t size() { return n; }

    std::pair<const_array, const_array> cut(size_t o)
    {
        return {const_array(d, o), const_array(d + o, n - o)};
    }

    iterator begin() { return d; }
    iterator end() { return d + n; }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
};

// subclass just provides a simpler name and some conversions
class const_string : public const_array<char>
{
public:
    // Implicit conversion from C string.
    const_string(const char *z)
    : const_array<char>(z, strlen(z))
    {}
    // Same as parent constructor.
    const_string(const char *s, size_t n)
    : const_array<char>(s, n)
    {}
    // Implicit conversion from C++ string.
    const_string(const std::string& s)
    : const_array<char>(s.data(), s.size())
    {}
};

#endif // CONST_ARRAY_HPP
