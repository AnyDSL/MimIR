#pragma once

#include <deque>
#include <memory>
#include <optional>

#include <fe/arena.h>
#include <fe/assert.h>
#include <fe/cast.h>

#include "thorin/driver.h"

#include "thorin/ast/tok.h"

namespace thorin::ast {

class LamDecl;
class Module;
class Scopes;
class Emitter;

template<class T> using Ptr  = fe::Arena::Ptr<const T>;
template<class T> using Ptrs = std::deque<Ptr<T>>;
/*             */ using Dbgs = std::deque<Dbg>;

class AST {
public:
    AST(Driver& driver)
        : driver_(driver)
        , sym_anon_(sym("_"))
        , sym_return_(sym("return")) {}

    Driver& driver() { return driver_; }

    /// @name Sym
    ///@{
    Sym sym(const char* s) { return driver().sym(s); }
    Sym sym(std::string_view s) { return driver().sym(s); }
    Sym sym(const std::string& s) { return driver().sym(s); }
    Sym sym_anon() const { return sym_anon_; }     ///< `"_"`.
    Sym sym_return() const { return sym_return_; } ///< `"return"`.
    ///@}

    template<class T, class... Args> auto ptr(Args&&... args) {
        return arena_.mk<const T>(std::forward<Args&&>(args)...);
    }

    /// @name Formatted Output
    ///@{
    /// Prefixes error message with `<location>: error: `.
    template<class... Args> void error(Loc loc, const char* fmt, Args&&... args) const {
        ++num_errors_;
        print(std::cerr, "{}{}: {}error: {}", rang::fg::yellow, loc, rang::fg::red, rang::fg::reset);
        println(std::cerr, fmt, std::forward<Args&&>(args)...);
    }
    template<class... Args> void warn(Loc loc, const char* fmt, Args&&... args) const {
        ++num_warnings_;
        print(std::cerr, "{}{}: {}warning: {}", rang::fg::yellow, loc, rang::fg::magenta, rang::fg::reset);
        println(std::cerr, fmt, std::forward<Args&&>(args)...);
    }
    template<class... Args> void note(Loc loc, const char* fmt, Args&&... args) const {
        print(std::cerr, "{}{}: {}note: {}", rang::fg::yellow, loc, rang::fg::green, rang::fg::reset);
        println(std::cerr, fmt, std::forward<Args&&>(args)...);
    }
    ///@}

private:
    mutable int num_errors_   = 0;
    mutable int num_warnings_ = 0;
    Driver& driver_;
    fe::Arena arena_;
    Sym sym_anon_;
    Sym sym_return_;
};

class Node : public fe::RuntimeCast<Node> {
protected:
    Node(Loc loc)
        : loc_(loc) {}
    virtual ~Node() {}

public:
    Loc loc() const { return loc_; }

    virtual std::ostream& stream(Tab&, std::ostream&) const = 0;
    void dump() const;

private:
    Loc loc_;
};

class Expr : public Node {
protected:
    Expr(Loc loc)
        : Node(loc) {}

public:
    virtual void bind(Scopes&) const = 0;
    virtual Ref emit(Emitter&) const = 0;
};

class Decl : public Node {
protected:
    Decl(Loc loc)
        : Node(loc) {}

public:
    Ref def() const { return def_; }

protected:
    mutable Ref def_ = nullptr;
};

class ValDecl : public Decl {
protected:
    ValDecl(Loc loc)
        : Decl(loc) {}

public:
    virtual void bind(Scopes&) const  = 0;
    virtual void emit(Emitter&) const = 0;
};

class DeclsBlock {
public:
    DeclsBlock(Ptrs<ValDecl>&& decls)
        : decls_(std::move(decls)) {}

    const auto& decls() const { return decls_; }
    const ValDecl* decl(size_t i) const { return decls_[i].get(); }
    size_t num_decls() const { return decls_.size(); }

    std::ostream& stream(Tab&, std::ostream&) const;
    void bind(Scopes&) const;
    void emit(Emitter&) const;

private:
    Ptrs<ValDecl> decls_;
};

/*
 * Ptrn
 */

class Ptrn : public Decl {
public:
    Ptrn(Loc loc, bool rebind, Dbg dbg)
        : Decl(loc)
        , dbg_(dbg)
        , rebind_(rebind) {}

    bool rebind() const { return rebind_; }
    Dbg dbg() const { return dbg_; }
    bool is_anon() const { return !dbg().sym || dbg().sym == '_'; }

    virtual void bind(Scopes&, bool quiet = false) const = 0;
    virtual Ref emit_value(Emitter&, Ref) const          = 0;
    Ref emit_type(Emitter& e) const { return thorin_type_ ? thorin_type_ : thorin_type_ = emit_type_(e); }

    [[nodiscard]] static Ptr<Expr> to_expr(AST&, Ptr<Ptrn>&&);
    [[nodiscard]] static Ptr<Ptrn> to_ptrn(Ptr<Expr>&&);

private:
    virtual Ref emit_type_(Emitter&) const = 0;

    Dbg dbg_;
    bool rebind_;
    mutable Ref thorin_type_ = nullptr;
};

/// `dbg: type`
class IdPtrn : public Ptrn {
public:
    IdPtrn(Loc loc, bool rebind, Dbg dbg, Ptr<Expr>&& type)
        : Ptrn(loc, rebind, dbg)
        , type_(std::move(type)) {}

    const Expr* type() const { return type_.get(); }

    static Ptr<IdPtrn> mk_type(AST& ast, Ptr<Expr>&& type) {
        auto loc = type->loc();
        return ast.ptr<IdPtrn>(loc, false, Dbg(loc, ast.sym_anon()), std::move(type));
    }

    void bind(Scopes&, bool quiet = false) const override;
    Ref emit_value(Emitter&, Ref) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ref emit_type_(Emitter&) const override;

    bool rebind_;
    Ptr<Expr> type_;
};

/// `dbg_0 ... dbg_n-2 id` where `id` = `dbg_n-1: type`
class GroupPtrn : public Ptrn {
public:
    GroupPtrn(Dbg dbg, const IdPtrn* id)
        : Ptrn(dbg.loc, false, dbg)
        , id_(id) {}

    const IdPtrn* id() const { return id_; }

    void bind(Scopes&, bool quiet = false) const override;
    Ref emit_value(Emitter&, Ref) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ref emit_type_(Emitter&) const override;

    const IdPtrn* id_;
};

/// `dbg::(ptrn_0, ..., ptrn_n-1)` or `dbg::[ptrn_0, ..., ptrn_n-1]`
class TuplePtrn : public Ptrn {
public:
    TuplePtrn(Loc loc, bool rebind, Dbg dbg, Tok::Tag delim_l, Ptrs<Ptrn>&& ptrns)
        : Ptrn(loc, rebind, dbg)
        , delim_l_(delim_l)
        , ptrns_(std::move(ptrns)) {}

    Tok::Tag delim_l() const { return delim_l_; }
    Tok::Tag delim_r() const { return Tok::delim_l2r(delim_l()); }
    bool is_paren() const { return delim_l() == Tok::Tag::D_paren_l; }
    bool is_brckt() const { return delim_l() == Tok::Tag::D_brckt_l; }

    const auto& ptrns() const { return ptrns_; }
    const Ptrn* ptrn(size_t i) const { return ptrns_[i].get(); }
    size_t num_ptrns() const { return ptrns().size(); }

    void bind(Scopes&, bool quiet = false) const override;
    Ref emit_value(Emitter&, Ref) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ref emit_type_(Emitter&) const override;

    Tok::Tag delim_l_;
    Ptrs<Ptrn> ptrns_;
};

/// Dummy Ptrn to introduce `return: type`.
/// @note ReturnPtrn::type is **not** owned.
class ReturnPtrn : public Ptrn {
public:
    ReturnPtrn(AST& ast, const Expr* type)
        : Ptrn(type->loc(), false, {type->loc(), ast.sym_return()})
        , type_(type) {}

    const Expr* type() const { return type_; }

    void bind(Scopes&, bool quiet = false) const override;
    Ref emit_value(Emitter&, Ref) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ref emit_type_(Emitter&) const override;

    const Expr* type_;
};

/*
 * Expr
 */

/// `sym`
class IdExpr : public Expr {
public:
    IdExpr(Dbg dbg)
        : Expr(dbg.loc)
        , dbg_(dbg) {}

    Dbg dbg() const { return dbg_; }
    const Decl* decl() const { return decl_; }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Dbg dbg_;
    mutable const Decl* decl_ = nullptr;
};

/// `tag`
class PrimaryExpr : public Expr {
public:
    PrimaryExpr(Loc loc, Tok::Tag tag)
        : Expr(loc)
        , tag_(tag) {}
    PrimaryExpr(Tok tok)
        : Expr(tok.loc())
        , tag_(tok.tag()) {}

    Tok::Tag tag() const { return tag_; }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Tok::Tag tag_;
};

/// `tag:type`
class LitExpr : public Expr {
public:
    LitExpr(Loc loc, Dbg value, Ptr<Expr>&& type)
        : Expr(loc)
        , value_(value)
        , type_(std::move(type)) {}

    Dbg value() const { return value_; }
    const Expr* type() const { return type_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Dbg value_;
    Ptr<Expr> type_;
};

/// `tag:type`
class ExtremumExpr : public Expr {
public:
    ExtremumExpr(Loc loc, Tok::Tag tag, Ptr<Expr>&& type)
        : Expr(loc)
        , tag_(tag)
        , type_(std::move(type)) {}

    Tok::Tag tag() const { return tag_; }
    const Expr* type() const { return type_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Tok::Tag tag_;
    Ptr<Expr> type_;
};

/// `{ decl_0 ... decl_n-1 e }` or `decl_0 ... decl_n-1` (used internaly)
class BlockExpr : public Expr {
public:
    BlockExpr(Loc loc, bool has_braces, Ptrs<ValDecl>&& decls, Ptr<Expr>&& expr)
        : Expr(loc)
        , has_braces_(has_braces)
        , decls_(std::move(decls))
        , expr_(std::move(expr)) {}

    bool has_braces() const { return has_braces_; }
    const Expr* expr() const { return expr_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    bool has_braces_;
    DeclsBlock decls_;
    Ptr<Expr> expr_;
};

/// `.Type level`
class TypeExpr : public Expr {
public:
    TypeExpr(Loc loc, Ptr<Expr>&& level)
        : Expr(loc)
        , level_(std::move(level)) {}

    const Expr* level() const { return level_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> level_;
};

// lam

/// `dom -> codom`
class ArrowExpr : public Expr {
public:
    ArrowExpr(Loc loc, Ptr<Expr>&& dom, Ptr<Expr>&& codom)
        : Expr(loc)
        , dom_(std::move(dom))
        , codom_(std::move(codom)) {}

private:
    const Expr* dom() const { return dom_.get(); }
    const Expr* codom() const { return codom_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> dom_;
    Ptr<Expr> codom_;
};

/// One of:
/// * `Π   dom_0 ... dom_n-1 -> codom`
/// * `.Cn dom_0 ... dom_n-1`
/// * `.Fn dom_0 ... dom_n-1 -> codom`
class PiExpr : public Expr {
public:
    class Dom : public Node {
    public:
        Dom(Loc loc, bool is_implicit, Ptr<Ptrn>&& ptrn)
            : Node(loc)
            , is_implicit_(is_implicit)
            , ptrn_(std::move(ptrn)) {}

        bool is_implicit() const { return is_implicit_; }
        const Ptrn* ptrn() const { return ptrn_.get(); }

        void bind(Scopes&) const;
        Ref emit(Emitter&) const;
        std::ostream& stream(Tab&, std::ostream&) const override;

    private:
        bool is_implicit_;
        Ptr<Ptrn> ptrn_;
    };

    PiExpr(Loc loc, Tok::Tag tag, Ptrs<Dom>&& doms, Ptr<Expr>&& codom)
        : Expr(loc)
        , tag_(tag)
        , doms_(std::move(doms))
        , codom_(std::move(codom)) {}

private:
    Tok::Tag tag() const { return tag_; }
    const auto& doms() const { return doms_; }
    const Dom* dom(size_t i) const { return doms_[i].get(); }
    size_t num_doms() const { return doms_.size(); }
    const Expr* codom() const { return codom_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Tok::Tag tag_;
    Ptrs<Dom> doms_;
    Ptr<Expr> codom_;
};

/// Wraps a LamDecl as Expr.
class LamExpr : public Expr {
public:
    LamExpr(Ptr<LamDecl>&& lam);

    const LamDecl* lam() const { return lam_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<LamDecl> lam_;
};

/// `callee arg`
class AppExpr : public Expr {
public:
    AppExpr(Loc loc, bool is_explicit, Ptr<Expr>&& callee, Ptr<Expr>&& arg)
        : Expr(loc)
        , is_explicit_(is_explicit)
        , callee_(std::move(callee))
        , arg_(std::move(arg)) {}

    bool is_explicit() const { return is_explicit_; }
    const Expr* callee() const { return callee_.get(); }
    const Expr* arg() const { return arg_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    bool is_explicit_;
    Ptr<Expr> callee_;
    Ptr<Expr> arg_;
};

/// `.ret ptrn = callee $ arg; body`
class RetExpr : public Expr {
public:
    RetExpr(Loc loc, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& callee, Ptr<Expr>&& arg, Ptr<Expr> body)
        : Expr(loc)
        , ptrn_(std::move(ptrn))
        , callee_(std::move(callee))
        , arg_(std::move(arg))
        , body_(std::move(body)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }
    const Expr* callee() const { return callee_.get(); }
    const Expr* arg() const { return arg_.get(); }
    const Expr* body() const { return body_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Ptrn> ptrn_;
    Ptr<Expr> callee_;
    Ptr<Expr> arg_;
    Ptr<Expr> body_;
};
// tuple

/// Just wraps TuplePtrn as Expr.
class SigmaExpr : public Expr {
public:
    SigmaExpr(Ptr<TuplePtrn>&& ptrn)
        : Expr(ptrn->loc())
        , ptrn_(std::move(ptrn)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<TuplePtrn> ptrn_;

    friend Ptr<Ptrn> Ptrn::to_ptrn(Ptr<Expr>&&);
};

/// `(elem_0, ..., elem_n-1)`
class TupleExpr : public Expr {
public:
    TupleExpr(Loc loc, Ptrs<Expr>&& elems)
        : Expr(loc)
        , elems_(std::move(elems)) {}

    const auto& elems() const { return elems_; }
    const Expr* elem(size_t i) const { return elems_[i].get(); }
    size_t num_elems() const { return elems().size(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptrs<Expr> elems_;
};

/// `«dbg: shape; body»` or `‹dbg: shape; body›`
template<bool arr> class ArrOrPackExpr : public Expr {
public:
    ArrOrPackExpr(Loc loc, Dbg dbg, Ptr<Expr>&& shape, Ptr<Expr>&& body)
        : Expr(loc)
        , dbg_(dbg)
        , shape_(std::move(shape))
        , body_(std::move(body)) {}

    Dbg dbg() const { return dbg_; }
    const Expr* shape() const { return shape_.get(); }
    const Expr* body() const { return body_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Dbg dbg_;
    Ptr<Expr> shape_;
    Ptr<Expr> body_;
};

using ArrExpr  = ArrOrPackExpr<true>;
using PackExpr = ArrOrPackExpr<false>;

/// `tuple#index`
class ExtractExpr : public Expr {
public:
    ExtractExpr(Loc loc, Ptr<Expr>&& tuple, Ptr<Expr>&& index)
        : Expr(loc)
        , tuple_(std::move(tuple))
        , index_(std::move(index)) {}
    ExtractExpr(Loc loc, Ptr<Expr>&& tuple, Dbg index)
        : Expr(loc)
        , tuple_(std::move(tuple))
        , index_(index) {}

    const Expr* tuple() const { return tuple_.get(); }
    const auto& index() const { return index_; }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> tuple_;
    std::variant<Ptr<Expr>, Dbg> index_;
};

/// `.ins(tuple, index, value)`
class InsertExpr : public Expr {
public:
    InsertExpr(Loc loc, Ptr<Expr>&& tuple, Ptr<Expr>&& index, Ptr<Expr>&& value)
        : Expr(loc)
        , tuple_(std::move(tuple))
        , index_(std::move(index))
        , value_(std::move(value)) {}

    const Expr* tuple() const { return tuple_.get(); }
    const Expr* index() const { return index_.get(); }
    const Expr* value() const { return value_.get(); }

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Expr> tuple_;
    Ptr<Expr> index_;
    Ptr<Expr> value_;
};

class ErrorExpr : public Expr {
public:
    ErrorExpr(Loc loc)
        : Expr(loc) {}

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;
};

class InferExpr : public Expr {
public:
    InferExpr(Loc loc)
        : Expr(loc) {}

    void bind(Scopes&) const override;
    Ref emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;
};

/*
 * Decls
 */

/// `.let ptrn: type = value;`
class LetDecl : public ValDecl {
public:
    LetDecl(Loc loc, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& value)
        : ValDecl(loc)
        , ptrn_(std::move(ptrn))
        , value_(std::move(value)) {}

    const Ptrn* ptrn() const { return ptrn_.get(); }
    const Expr* value() const { return value_.get(); }

    void bind(Scopes&) const override;
    void emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Ptr<Ptrn> ptrn_;
    Ptr<Expr> value_;
};

/// `.ax ptrn: type = value;`
class AxiomDecl : public ValDecl {
public:
    AxiomDecl(Loc loc, Dbg dbg, std::deque<Dbgs>&& subs, Ptr<Expr>&& type, Dbg normalizer, Dbg curry, Dbg trip)
        : ValDecl(loc)
        , dbg_(dbg)
        , subs_(std::move(subs))
        , type_(std::move(type))
        , normalizer_(normalizer)
        , curry_(curry)
        , trip_(trip) {}

    Dbg dbg() const { return dbg_; }
    const auto& subs() const { return subs_; }
    size_t num_subs() const { return subs_.size(); }
    const auto& sub(size_t i) const { return subs_[i]; }
    const Expr* type() const { return type_.get(); }
    Dbg normalizer() const { return normalizer_; }
    Dbg curry() const { return curry_; }
    Dbg trip() const { return trip_; }

    void bind(Scopes&) const override;
    void emit(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Dbg dbg_;
    std::deque<Dbgs> subs_;
    Ptr<Expr> type_;
    Dbg normalizer_;
    Dbg curry_, trip_;
};

/// `.rec dbg: type = body`
class RecDecl : public ValDecl {
public:
    RecDecl(Loc loc, Dbg dbg, Ptr<Expr>&& type, Ptr<Expr>&& body)
        : ValDecl(loc)
        , dbg_(dbg)
        , type_(std::move(type))
        , body_(std::move(body)) {}

    Dbg dbg() const { return dbg_; }
    const Expr* type() const { return type_.get(); }
    const Expr* body() const { return body_.get(); }

    void bind(Scopes&) const override;
    void emit(Emitter&) const override;
    virtual void bind_rec(Scopes&) const;
    virtual void emit_rec(Emitter&) const;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Dbg dbg_;
    Ptr<Expr> type_;
    Ptr<Expr> body_;
};

/// One of:
/// * `λ   dom_0 ... dom_n-1 -> codom`
/// * `.cn dom_0 ... dom_n-1`
/// * `.fn dom_0 ... dom_n-1 -> codom`
/// * `.lam dbg dom_0 ... dom_n-1 -> codom`
/// * `.con dbg dom_0 ... dom_n-1`
/// * `.fun dbg dom_0 ... dom_n-1 -> codom`
class LamDecl : public RecDecl {
public:
    class Dom : public PiExpr::Dom {
    public:
        Dom(Loc loc, bool has_bang, bool is_implicit, Ptr<Ptrn>&& ptrn, Ptr<Expr>&& filter)
            : PiExpr::Dom(loc, is_implicit, std::move(ptrn))
            , has_bang_(has_bang)
            , filter_(std::move(filter)) {}

        bool has_bang() const { return has_bang_; }
        const Expr* filter() const { return filter_.get(); }

        void bind(Scopes&, bool quiet = false) const;
        Ref emit(Emitter&) const;
        std::ostream& stream(Tab&, std::ostream&) const override;

    private:
        bool has_bang_;
        Ptr<Expr> filter_;
    };

    LamDecl(Loc loc, Tok::Tag tag, bool is_external, Dbg dbg, Ptrs<Dom>&& doms, Ptr<Expr>&& codom, Ptr<Expr>&& body)
        : RecDecl(loc, dbg, nullptr, std::move(body))
        , tag_(tag)
        , is_external_(is_external)
        , doms_(std::move(doms))
        , codom_(std::move(codom)) {}

    Tok::Tag tag() const { return tag_; }
    bool is_external() const { return is_external_; }
    const auto& doms() const { return doms_; }
    const Dom* dom(size_t i) const { return doms_[i].get(); }
    size_t num_doms() const { return doms_.size(); }
    const Expr* codom() const { return codom_.get(); }
    const ReturnPtrn* ret() const { return ret_.get(); }

    void bind(Scopes&) const override;
    void bind_rec(Scopes&) const override;
    void emit(Emitter&) const override;
    void emit_rec(Emitter&) const override;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    Tok::Tag tag_;
    bool is_external_;
    Ptrs<Dom> doms_;
    Ptr<Expr> codom_;
    mutable Ptr<ReturnPtrn> ret_;
};

/*
 * Module
 */

class Import {
public:
    Import& operator=(const Import&) = delete;
    Import(const Import&)            = delete;
    Import() noexcept                = default;

    Import(Import&& other)
        : Import() {
        swap(*this, other);
    }
    Import(Dbg dbg, Tok::Tag tag, Ptr<Module>&& module) noexcept
        : dbg_(dbg)
        , tag_(tag)
        , module_(std::move(module)) {}

    Dbg dbg() const { return dbg_; }
    Tok::Tag tag() const { return tag_; }
    const Module* module() const { return module_.get(); }

    void bind(Scopes&) const;
    void emit(Emitter&) const;
    std::ostream& stream(Tab&, std::ostream&) const;

    friend void swap(Import& i1, Import& i2) noexcept {
        using std::swap;
        swap(i1.dbg_, i2.dbg_);
        swap(i1.tag_, i2.tag_);
        swap(i1.module_, i2.module_);
    }

private:
    Dbg dbg_;
    Tok::Tag tag_;
    Ptr<Module> module_;
};

class Module : public Node {
public:
    Module(Loc loc, std::deque<Import>&& imports, Ptrs<ValDecl>&& decls)
        : Node(loc)
        , imports_(std::move(imports))
        , decls_(std::move(decls)) {}

    const auto& imports() const { return imports_; }

    void compile(AST&, World&) const;
    void bind(AST&) const;
    void bind(Scopes&) const;
    void emit(AST&, World&) const;
    void emit(Emitter&) const;
    std::ostream& stream(Tab&, std::ostream&) const override;

private:
    std::deque<Import> imports_;
    DeclsBlock decls_;
};

} // namespace thorin::ast
