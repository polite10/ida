// SHA-256: FIPS 180-4 örnekleri (NIST "SHA256.pdf" örnek değerleri) ve blok sınırları.
#include <string>

#include "deneme.hpp"
#include "ida/viz/sha256.hpp"

int main() {
  using ida::viz::onaltilik;
  using ida::viz::sha256;
  DENETLE(onaltilik(sha256("", 0)) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  DENETLE(onaltilik(sha256("abc", 3)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  const std::string iki_blok = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  DENETLE(onaltilik(sha256(iki_blok.data(), iki_blok.size())) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  // Parça parça ekleme tek seferlikle aynı.
  const std::string milyon(1'000'000, 'a');
  ida::viz::Sha256 s;
  for (std::size_t i = 0; i < milyon.size(); i += 777) {
    const std::size_t n = (milyon.size() - i < 777) ? milyon.size() - i : 777;
    s.ekle(milyon.data() + i, n);
  }
  DENETLE(onaltilik(s.bitir()) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  DENEME_SONUC("viz.sha256");
}
