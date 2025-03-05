#include <gtest/gtest.h>

#include <mim/driver.h>

#include <mim/util/trie.h>

TEST(Trie, Trie) {
    mim::Driver driver;
    auto& w  = driver.world();
    auto l01 = w.lit_nat(1);
    auto l05 = w.lit_nat(5);
    auto l10 = w.lit_nat(10);
    auto l17 = w.lit_nat(17);
    auto l20 = w.lit_nat(20);
    auto l30 = w.lit_nat(30);
    auto l40 = w.lit_nat(40);

    mim::Trie<const mim::Def*> trie;
    auto s1     = trie.create(l10);
    auto s14    = trie.insert(s1, l40);
    auto s13    = trie.insert(s1, l30);
    auto s134   = trie.insert(s13, l40);
    auto s1234  = trie.insert(s134, l20);
    auto s_134  = trie.insert(s134, l05);
    auto s_1234 = trie.insert(s1234, l05);
    auto s_234  = trie.erase(s_1234, l10);
    auto asdf   = trie.merge(trie.insert(trie.create(l01), l17), s134);

    for (auto e : s_1234) std::cout << e << std::endl;

    trie.dot();

    // for (auto elem :
}
