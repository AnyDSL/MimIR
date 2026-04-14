# MimIR

[TOC]

<table  class="markdownTable">
    <tr class="markdownTableRowOdd">
        <td>Support</td>
        <td>
            <a href=https://anydsl.github.io/MimIR><img src="https://img.shields.io/badge/docs-master-green?logo=gitbook&logoColor=white" alt="Documentation"></a>
            <a href=https://discord.gg/FPp7hdj3fQ><img src="https://img.shields.io/discord/960975142459179068?color=green&logo=discord&logoColor=white" alt="Discord"></a>
        </td>
    </tr>
    <tr class="markdownTableRowEven">
        <td class="markdownTableBodyNone">
            License
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://github.com/AnyDSL/MimIR/blob/master/LICENSE.TXT"><img src="https://img.shields.io/github/license/anydsl/MimIR?&color=yellowgreen&logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH5gQFFyMP+ejbyAAAAVRJREFUOMuNkk0rhGEUhq+jETaajSxEKdvJShayRUlZWbCYjZr/4N+oyVjLwkdWkoWl2ShSllKmZKGZuiwcenuHyV2nzsd9n+e5ex4YAHVb3R7EqfwhnAM6wFbWZ0A1Iu7L3CiIloEVYAG4BWaBpRxfAY9ADbgBziLisnyDSaAHbEREV60CTznbjYiOOgzsJbfPwhvwEhHdrN+BtUJOLn5Jbp/vQ/W8UFfV54xqoX+uHn7XQ9mcALrAtbqYp3WAC+Aic3J2DXRT87UA2AEOgH2gPuDV6sk5SM3PghXgNCIegCl17BeLY8BUck5Tw5C6CbwCNXUeaAMNdaQgHgEaQDs5NeBV3UQ9sh9ttaK2MirZK+MIdVxtqh9qTz1Rp/PkltrKfDpnveQ21fGix2F1tOT7Z0GhN5ofajDUdbWTsc5/oc6oq+pdwetd9mb+s+DYv3Fc5n8Cd+5Qbrzh2X0AAAAASUVORK5CYII=" alt="License"></a>
        </td>
    </tr>
    <tr class="markdownTableRowEven">
        <td class="markdownTableBodyNone">
            Tests
        </td>
        <td class="markdownTableBodyNone">
            <a href="https://github.com/AnyDSL/MimIR/actions/workflows/linux.yml"><img src="https://img.shields.io/github/actions/workflow/status/anydsl/MimIR/linux.yml?logo=linux&label=linux&logoColor=white&branch=master" alt="Linux"></a>
            <a href="https://github.com/AnyDSL/MimIR/actions/workflows/windows.yml"><img src="https://img.shields.io/github/actions/workflow/status/anydsl/MimIR/windows.yml?logo=windows&label=windows&branch=master" alt="Windows"></a>
            <a href="https://github.com/AnyDSL/MimIR/actions/workflows/macos.yml"><img src="https://img.shields.io/github/actions/workflow/status/anydsl/MimIR/macos.yml?logo=apple&label=macos&branch=master" alt="macOS"></a>
            <a href="https://github.com/AnyDSL/MimIR/actions/workflows/doxygen.yml"><img src="https://img.shields.io/github/actions/workflow/status/anydsl/MimIR/doxygen.yml?logo=github&label=doxygen&branch=master" alt="Doxygen"></a>
        </td>
    </tr>
</table>

**MimIR** is a pure, graph-based, higher-order intermediate representation rooted in the **Calculus of Constructions**.
Terms, types, and type-level computations all live in the same program graph as ordinary expressions.
MimIR provides:

- **Dependent types**, **parametric polymorphism**, and **higher-order functions** out of the box
- **Extensible plugins** for domain-specific axioms, types, normalizers, and code generation
- **SSA without dominance**: a scopeless IR for higher-order programs based on free-variable nesting
- A **sea-of-nodes** style IR with on-the-fly normalization, type checking, and partial evaluation

Well suited for DSL compilers, tensor compilers, automatic differentiation, regex engines, and other systems that need high-performance code from high-level abstractions.

## 💡 Why MimIR?

| Feature                                                                                                  | LLVM                    | MLIR                                | MimIR                                                                                                              |
| -------------------------------------------------------------------------------------------------------- | ----------------------- | ----------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| [Higher-order functions](https://en.wikipedia.org/wiki/Higher-order_function)                            | ❌                      | ⚠️ (regions only)                   | ✅ (first-class functions)                                                                                         |
| [Parametric polymorphism (System F)](https://en.wikipedia.org/wiki/System_F)                             | ❌                      | ❌                                  | ✅                                                                                                                 |
| [Type-level abstraction / polytypism (System Fω)](https://en.wikipedia.org/wiki/System_F#System_F%CF%89) | ❌                      | ❌                                  | ✅                                                                                                                 |
| [Dependent types (Calculus of Constructions)](https://en.wikipedia.org/wiki/Calculus_of_constructions)   | ❌                      | ❌                                  | ✅                                                                                                                 |
| Semantic extensibility                                                                                   | ❌                      | 🔧 (dialect-specific C++ semantics) | ✅ (typed axioms)                                                                                                  |
| Program representation                                                                                   | CFG + instruction lists | CFG/regions + instruction lists     | Arbitrary expressions (direct style + [CPS](https://en.wikipedia.org/wiki/Continuation-passing_style))             |
| Structural foundation                                                                                    | CFG + dominance         | CFG/regions + dominance             | Free variables + nesting                                                                                           |
| DSL embedding / semantics retention                                                                      | Low                     | High (dialects + lowering)          | High ([partial evaluation](https://en.wikipedia.org/wiki/Partial_evaluation), typed axioms, normalizers, lowering) |

@note The table compares native IR-level support and representation, not what can be emulated via custom IR extensions, closure conversion, lowering, or external analyses.

## 🚀 Quick Start

```sh
git clone --recursive git@github.com:AnyDSL/MimIR.git
cd MimIR
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 📦 Install (optional)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/my/local/install/prefix
cmake --build build -j$(nproc) -t install
```

See the full [🛠️ build options](@ref build_options) in the docs.

## 🔥 Key Innovations

### 🧩 Plugins — Your DSL Lives Here

Declare new types, operations, and normalizers in a single `.mim` file.
C++ provides the heavy lifting: optimization, lowering, and code generation.

### 🌲 SSA without Dominance

Forget CFG dominance.
MimIR uses free-variable nesting:

- The **nesting tree** replaces the dominator tree
- Data-flow dependencies remain precise, even for higher-order code
- Loop peeling and unrolling reduce to simple β-reduction
- Mutual recursion and higher-order functions are handled naturally

### 🌊 Sea-of-Nodes with On-the-Fly Everything

MimIR hits the [**sweet spot**](@ref mut) between a fully mutable IR, which is easy to construct, and a fully immutable IR, which enables safe sharing and hash-consing:

- Non-binder expressions are immutable
  - [Hash-consing](https://en.wikipedia.org/wiki/Hash_consing), normalization, type checking, and partial evaluation happen **automatically** during graph construction
- Binders support variables and recursion by “tying the knot” through in-place mutation

## 🐉 Naming: MimIR vs Mim

**MimIR** is a recursive acronym for _MimIR is my Intermediate Representation_.

In Norse mythology, [Mímir](https://en.wikipedia.org/wiki/M%C3%ADmir) was a being of immense wisdom. After he was beheaded in the Æsir–Vanir War, Odin preserved his head, which continued to recite secret knowledge and counsel.

Today, **you** have Mímir’s head at your fingertips.

- **MimIR** refers to the core graph-based intermediate representation and its C++ API.
- **Mim** is the friendly, high-level frontend language that compiles down to MimIR.

In the entire codebase we consistently use `mim` / `MIM` for namespaces, macros, CMake variables, etc.
If in doubt, just use **Mim**.

## 💬 Community

- 💬 **Discord** → [Join the chat](https://discord.gg/FPp7hdj3fQ)
- 📚 **Documentation** → <https://anydsl.github.io/MimIR>
- 💻 **Examples** → [`examples/`](https://github.com/AnyDSL/MimIR/tree/master/examples) and [`lit/`](https://github.com/AnyDSL/MimIR/tree/master/lit)

**Ready to build the next generation of DSL compilers?**

[⭐ Star MimIR on GitHub](https://github.com/AnyDSL/MimIR), join Discord, and let’s make high-performance DSLs easy.

## ⚖️ License

MimIR is licensed under the [MIT License](https://github.com/AnyDSL/MimIR/blob/master/LICENSE.TXT).

## 📖 Publications

<ul>
    <li>
        <strong>SSA without Dominance for Higher-Order Programs</strong><br>
        Roland Leißa, Johannes Griebler.<br>
        <em>Proceedings of the ACM on Programming Languages (PLDI), 2026</em>, 10(PLDI).<br>
        <a href="https://arxiv.org/abs/2604.09961">Preprint</a> ·
        <a href="https://zenodo.org/records/19069679">Artifact</a>
    </li>
    <li>
        <strong>MimIR: An Extensible and Type-Safe Intermediate Representation for the DSL Age</strong><br>
        Roland Leißa, Marcel Ullrich, Joachim Meyer, Sebastian Hack.<br>
        <em>Proceedings of the ACM on Programming Languages (POPL), 2025</em>, 9(POPL), 95–125.<br>
        <a href="https://youtu.be/2zKUa6b9XYc?si=3ZX68gEHarsCsO-R">Talk</a> ·
        <a href="https://dl.acm.org/doi/10.1145/3704840">ACM</a> ·
        <a href="https://doi.org/10.1145/3704840">DOI</a> ·
        <a href="https://arxiv.org/abs/2411.07443">Preprint</a> ·
        <a href="https://zenodo.org/records/13952579">Artifact</a> ·
        <a href="https://dblp.uni-trier.de/rec/journals/pacmpl/LeissaUMH25.html?view=bibtex">BibTeX</a>
    </li>
</ul>
