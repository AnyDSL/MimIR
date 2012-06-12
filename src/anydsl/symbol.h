#ifndef ANYDSL_SYMBOL_HEADER
#define ANYDSL_SYMBOL_HEADER

#include <cstring>
#include <set>
#include <string>

#include <boost/functional/hash.hpp>
#include <boost/unordered_set.hpp>

#include "anydsl/symbolmemory.h"
#include "anydsl/util/singleton.h"

/*
 * TODO make index more typesafe by introducing a class Symbol::Handle
 * TODO operator << (ostream, Symbol) should just emit the str; objections?
 */

namespace anydsl {

/**
 * @brief Abstracts C-strings.
 *
 * Every \p Symbol's \p index_ points to a \em unique string in the \p SymbolTable.
 */
class Symbol {
public:

    /*
     * constructors
     */

    /// this will consult the symbol table
    inline Symbol(const char* str);
    /// this will consult the symbol table
    inline Symbol(const std::string& str);


    /// \p index parameter must be returned from some other Symbol::index()
    explicit Symbol(size_t index) : str_((const char*)index) {}

    /// create the empty-string-symbol
    Symbol() : str_(SymbolMemory::nullStr) {}

    /*
     * getters
     */

    /**
     * @brief Returns the index into the \p SymbolTable's internal array.
     *
     * \remark index will never invalidate
     *
     * @return The index
     */
    size_t index() const { return (size_t)str_; }

    /**
     * @brief Return the string which this \p Symbol represents.
     *
     * \warning Pointer is valid only temporarily.
     *
     * @return The string.
     */
    const char* str() const { return str_; }

    /// Is this the empty string?
    bool isEmpty() const { return str_==SymbolMemory::nullStr; }

    /*
     * comparisons
     */

    /// Are two given \p Symbol%s identical?
    bool operator == (Symbol b) const { return index() == b.index(); }

    /// Are two given \p Symbol%s not identical?
    bool operator !=( Symbol b) const { return index() != b.index(); }

private:

    const char* str_;

    friend class SymbolTable;
};

inline size_t hash_value(const Symbol sym) { return boost::hash_value(sym.index()); }

//------------------------------------------------------------------------------

class SymbolTable : public Singleton<SymbolTable> {
private:

    /*
     * constructor
     */

    SymbolTable();

public:

    /*
     * further methods
     */

    /// Equivalent to \c Symbol::Symbol(str).
    Symbol get(const char* str);
    Symbol get(const std::string& str) { return get(str.c_str()); }

private:

    Symbol store(const char* str);

    SymbolMemory memory_;

    typedef boost::unordered_set<Symbol> SymbolSet;
    SymbolSet symbolSet_;

    friend class Singleton<SymbolTable>;
};

//------------------------------------------------------------------------------

inline Symbol::Symbol(const char* str)
    : str_(SymbolTable::This().get(str).str()) {}
inline Symbol::Symbol(const std::string& str)
    : str_(SymbolTable::This().get(str).str()) {}

//------------------------------------------------------------------------------

std::ostream& operator << (std::ostream& o, Symbol s);

//------------------------------------------------------------------------------

} // namespace anydsl

#endif
