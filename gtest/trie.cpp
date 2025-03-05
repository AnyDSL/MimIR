#include <gtest/gtest.h>

#include <mim/util/trie.h>

TEST(Trie, Trie) {
    mim::Trie trie;

    auto s1     = trie.create(10);
    auto s14    = trie.insert(s1, 40);
    auto s13    = trie.insert(s1, 30);
    auto s134   = trie.insert(s13, 40);
    auto s1234  = trie.insert(s134, 20);
    auto s_134  = trie.insert(s134, 05);
    auto s_1234 = trie.insert(s1234, 05);
    auto s_234  = trie.erase(s_1234, 10);
    auto asdf   = trie.merge(trie.insert(trie.create(1), 17), s134);

    for (auto e : trie.range(asdf)) std::cout << e << std::endl;

    trie.dot();

    // for (auto elem :
}
