#pragma once

#include <deque>
#include <ostream>

#include <absl/container/btree_set.h>

#include <mim/plug/clos/clos.h>
#include <mim/plug/math/math.h>
#include <mim/plug/mem/mem.h>

#include "mim/be/emitter.h"
#include "mim/util/print.h"

namespace mim {

class World;

namespace ll {

void emit(World&, std::ostream&);

int compile(World&, std::string name);
int compile(World&, std::string ll, std::string out);
int compile_and_run(World&, std::string name, std::string args = {});

struct BB {
    BB()                    = default;
    BB(const BB&)           = delete;
    BB(BB&& other) noexcept = default;
    BB& operator=(BB other) noexcept { return swap(*this, other), *this; }

    std::deque<std::ostringstream>& head() { return parts[0]; }
    std::deque<std::ostringstream>& body() { return parts[1]; }
    std::deque<std::ostringstream>& tail() { return parts[2]; }

    template<class... Args>
    std::string assign(std::string_view name, const char* s, Args&&... args) {
        print(print(body().emplace_back(), "{} = ", name), s, std::forward<Args>(args)...);
        return std::string(name);
    }

    template<class... Args>
    void tail(const char* s, Args&&... args) {
        print(tail().emplace_back(), s, std::forward<Args>(args)...);
    }

    friend void swap(BB& a, BB& b) noexcept {
        using std::swap;
        swap(a.phis, b.phis);
        swap(a.parts, b.parts);
    }

    DefMap<std::deque<std::pair<std::string, std::string>>> phis;
    std::array<std::deque<std::ostringstream>, 3> parts;
};

class Emitter : public mim::Emitter<std::string, std::string, BB, Emitter> {
public:
    using Super = mim::Emitter<std::string, std::string, BB, Emitter>;

    Emitter(World& world, std::ostream& ostream)
        : Super(world, "llvm_emitter", ostream) {}

    bool is_valid(std::string_view s) { return !s.empty(); }
    void start() override;
    void emit_imported(Lam*);
    virtual void emit_epilogue(Lam*);
    std::string emit_bb(BB&, const Def*);
    virtual std::string prepare();
    void finalize();

    virtual std::optional<std::string> isa_targetspecific_intrinsic(BB&, const Def*) { return std::nullopt; }
    std::string as_targetspecific_intrinsic(BB& bb, const Def* def) {
        if (auto res = isa_targetspecific_intrinsic(bb, def)) return res.value();
        error("target-specific intrinsic detected but not handled in LLVM backend: {} : {}", def, def->type());
    }

    template<class... Args>
    void declare(const char* s, Args&&... args) {
        std::ostringstream decl;
        print(decl << "declare ", s, std::forward<Args>(args)...);
        decls_.emplace(decl.str());
    }

protected:
    Emitter(World& world, std::string name, std::ostream& ostream)
        : Super(world, std::move(name), ostream) {}

    std::string id(const Def*, bool force_bb = false) const;
    virtual std::string convert(const Def*);
    std::string convert_ret_pi(const Pi*);

    absl::btree_set<std::string> decls_;
    std::ostringstream type_decls_;
    std::ostringstream vars_decls_;
    std::ostringstream func_decls_;
    std::ostringstream func_impls_;
};

} // namespace ll
} // namespace mim
