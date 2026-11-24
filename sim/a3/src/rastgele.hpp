// rastgele.hpp — işletim sisteminin kriptografik rastgele kaynağı (A3-K04, K94).
// macOS/Linux: getentropy(3); Windows: BCryptGenRandom (sistem tercihli RNG).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ida::a3 {

// Başarısızsa false (çağıran başlamayı reddeder; zayıf kaynağa düşülmez).
bool kripto_rastgele(std::uint8_t* hedef, std::size_t n);

// n bayt rastgele değerin küçük harfli onaltılık gösterimi; kaynak başarısızsa boş dize.
std::string rastgele_onaltilik(std::size_t bayt);

}  // namespace ida::a3
