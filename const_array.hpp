// Copyright 2012 Ben Longbons
#ifndef CONST_ARRAY_HPP
#define CONST_ARRAY_HPP

#include <cstring>

#include <iterator>
#include <ostream>
#include <string>
#include <vector>

template<class T>
class const_array
{
    const T *d;
    size_t n;
public:
    typedef const T *iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;

    constexpr
    const_array(const T *p, size_t z)
    : d(p), n(z)
    {}

    constexpr
    const_array(const T *b, const T *e)
    : d(b), n(e - b)
    {}

    // Implicit conversion from std::vector
    const_array(const std::vector<T>& v)
    : d(v.data()), n(v.size())
    {}

    // but disallow conversion from a temporary
    const_array(std::vector<T>&&) = delete;

    // All methods are non-const to "encourage" you
    // to always pass a const_array by value.
    // After all, "const const_array" looks funny.
    constexpr
    const T *data() { return d; }
    constexpr
    size_t size() { return n; }
    constexpr
    bool empty() { return not n; }
    constexpr explicit
    operator bool() { return n; }

    constexpr
    std::pair<const_array, const_array> cut(size_t o)
    {
        return {const_array(d, o), const_array(d + o, n - o)};
    }

    constexpr
    const_array head(size_t o)
    {
        return cut(o).first;
    }

    constexpr
    const_array tail(size_t l)
    {
        return cut(size() - l).second;
    }

    constexpr
    iterator begin() { return d; }
    constexpr
    iterator end() { return d + n; }
    constexpr
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    constexpr
    reverse_iterator rend() { return reverse_iterator(begin()); }

    constexpr
    const T& front() { return *begin(); }
    constexpr
    const T& back() { return *rbegin(); }
};

// subclass just provides a simpler name and some conversions
class const_string : public const_array<char>
{
public:
    // Implicit conversion from C string.
    constexpr
    const_string(const char *z)
    : const_array<char>(z, z ? strlen(z) : 0)
    {}

    // Same as parent constructor.
    constexpr
    const_string(const char *s, size_t n)
    : const_array<char>(s, n)
    {}

    // Same as parent constructor.
    constexpr
    const_string(const char *b, const char *e)
    : const_array<char>(b, e)
    {}

    // Same as parent constructor.
    const_string(const std::vector<char> s)
    : const_array<char>(s)
    {}

    // Implicit conversion from C++ string.
    const_string(const std::string& s)
    : const_array<char>(s.data(), s.size())
    {}

    // but disallow converion from a temporary.
    const_string(std::string&&) = delete;

    // allow being sloppy
    constexpr
    const_string(const_array<char> a)
    : const_array<char>(a)
    {}
    const_string(const_array<unsigned char> a)
    : const_array<char>(reinterpret_cast<const char *>(a.data()), a.size())
    {}

    operator const_array<unsigned char>()
    {
        return const_array<unsigned char>(
                reinterpret_cast<const unsigned char *>(this->data()),
                this->size());
    }
};

inline
std::ostream& operator << (std::ostream& o, const_string s)
{
    return o.write(s.data(), s.size());
}

#endif // CONST_ARRAY_HPP
