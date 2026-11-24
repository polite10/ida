// a3_istemci.hpp
#include "a3_istemci.hpp"

#include <chrono>
#include <cstring>

namespace ida::test::a3 {

namespace {

template <typename T>
T oku_le(const std::string& s, std::size_t k)
{
    T v{};
    std::memcpy(&v, s.data() + k, sizeof v);
    return v;
}

}  // namespace

bool Istemci::baglan(std::uint16_t port, const std::string& jeton, int bekle_ms)
{
    std::string hata;
    t_ = ida::kopru::tcp_baglan(port, hata);
    if (!t_) {
        return false;
    }
    const std::string istek = "GET /ws HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
                              "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Protocol: ida-a3.v1, ida-erisim." +
                              jeton + "\r\n\r\n";
    if (!t_->yaz(reinterpret_cast<const std::uint8_t*>(istek.data()), istek.size())) {
        return false;
    }
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(bekle_ms);
    bool baslik = false;
    while (std::chrono::steady_clock::now() < son) {
        std::size_t n = 0;
        const ida::kopru::OkumaSonucu r = t_->oku(okuma_.data(), okuma_.size(), 20'000, n);
        if (r == ida::kopru::OkumaSonucu::kapandi) {
            return false;
        }
        if (r == ida::kopru::OkumaSonucu::tamam) {
            tampon_.append(reinterpret_cast<const char*>(okuma_.data()), n);
        }
        if (!baslik) {
            const std::size_t e = tampon_.find("\r\n\r\n");
            if (e == std::string::npos) {
                continue;
            }
            if (tampon_.compare(0, 12, "HTTP/1.1 101") != 0) {
                return false;
            }
            tampon_.erase(0, e + 4);
            baslik = true;
        }
        while (cerceve_isle()) {
        }
        if (!metinler.empty()) {
            return metinler.front().find("\"merhaba\"") != std::string::npos;
        }
    }
    return false;
}

bool Istemci::yaz_(std::uint8_t opkod, const std::string& yuk)
{
    std::string c;
    c.push_back(static_cast<char>(0x80u | opkod));
    if (yuk.size() < 126u) {
        c.push_back(static_cast<char>(0x80u | yuk.size()));
    } else {
        c.push_back(static_cast<char>(0x80u | 126u));
        c.push_back(static_cast<char>((yuk.size() >> 8u) & 0xFFu));
        c.push_back(static_cast<char>(yuk.size() & 0xFFu));
    }
    const std::uint8_t m[4] = {0x21, 0x43, 0x65, 0x87};
    c.append(reinterpret_cast<const char*>(m), 4);
    for (std::size_t i = 0; i < yuk.size(); ++i) {
        c.push_back(static_cast<char>(static_cast<std::uint8_t>(yuk[i]) ^ m[i & 3u]));
    }
    return t_ && t_->yaz(reinterpret_cast<const std::uint8_t*>(c.data()), c.size());
}

bool Istemci::abone(const std::vector<std::pair<std::string, int>>& kanallar)
{
    std::string k;
    for (const auto& [ad, hz] : kanallar) {
        k += (k.empty() ? "" : ",") + std::string("{\"kanal\":\"") + ad + "\"" +
             (hz > 0 ? ",\"hz\":" + std::to_string(hz) : std::string()) + "}";
    }
    return yaz_(1u, "{\"tur\":\"abone_ol\",\"istek_no\":" + std::to_string(istek_no_++) + ",\"kanallar\":[" + k + "]}");
}

bool Istemci::oku(int bekle_ms)
{
    if (!t_ || kapandi) {
        return false;
    }
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(bekle_ms);
    do {
        std::size_t n = 0;
        const ida::kopru::OkumaSonucu r = t_->oku(okuma_.data(), okuma_.size(), 5'000, n);
        if (r == ida::kopru::OkumaSonucu::kapandi) {
            kapandi = true;
            return false;
        }
        if (r == ida::kopru::OkumaSonucu::tamam) {
            tampon_.append(reinterpret_cast<const char*>(okuma_.data()), n);
            while (cerceve_isle()) {
            }
        }
    } while (std::chrono::steady_clock::now() < son && !kapandi);
    return !kapandi;
}

void Istemci::kapat()
{
    if (t_) {
        t_->kapat();
        t_.reset();
    }
}

bool Istemci::cerceve_isle()
{
    if (tampon_.size() < 2u) {
        return false;
    }
    const auto b0 = static_cast<std::uint8_t>(tampon_[0]);
    const auto b1 = static_cast<std::uint8_t>(tampon_[1]);
    std::size_t k = 2;
    std::uint64_t boy = b1 & 0x7Fu;
    if (boy == 126u) {
        if (tampon_.size() < 4u) {
            return false;
        }
        boy = (static_cast<std::uint64_t>(static_cast<std::uint8_t>(tampon_[2])) << 8u) | static_cast<std::uint8_t>(tampon_[3]);
        k = 4;
    } else if (boy == 127u) {
        if (tampon_.size() < 10u) {
            return false;
        }
        boy = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            boy = (boy << 8u) | static_cast<std::uint8_t>(tampon_[2 + i]);
        }
        k = 10;
    }
    if (tampon_.size() < k + boy) {
        return false;
    }
    const auto op = static_cast<std::uint8_t>(b0 & 0x0Fu);
    const bool fin = (b0 & 0x80u) != 0u;
    std::string yuk = tampon_.substr(k, static_cast<std::size_t>(boy));
    tampon_.erase(0, k + static_cast<std::size_t>(boy));
    if (op == 9u) {
        (void)yaz_(10u, yuk);   // Ping → Pong (RFC 6455 §5.5.2)
        return true;
    }
    if (op == 8u) {
        kapandi = true;
        if (yuk.size() >= 2u) {
            kapanis_kodu = static_cast<std::uint16_t>((static_cast<std::uint8_t>(yuk[0]) << 8u) | static_cast<std::uint8_t>(yuk[1]));
        }
        return false;
    }
    if (op == 0u) {
        parca_ += yuk;
    } else if (op == 1u || op == 2u) {
        parca_ = yuk;
        parca_opkod_ = op;
    } else {
        return true;
    }
    if (!fin) {
        return true;
    }
    if (parca_opkod_ == 1u) {
        metinler.push_back(parca_);
    } else {
        a3_ayir(parca_);
    }
    parca_.clear();
    return true;
}

void Istemci::a3_ayir(const std::string& ileti)
{
    std::size_t k = 0;
    while (k + 24u <= ileti.size()) {
        const auto t_ns = oku_le<std::int64_t>(ileti, k);
        const auto boy = oku_le<std::uint32_t>(ileti, k + 12u);
        const auto kanal = oku_le<std::uint16_t>(ileti, k + 16u);
        if (k + 24u + boy > ileti.size()) {
            return;
        }
        const std::string yuk = ileti.substr(k + 24u, boy);
        ++cerceve_sayisi[kanal];
        if (kanal == 10u && yuk.size() >= 38u) {
            // yuk_fizik_durum: adim_no u64; govdeler sayacı u16; govde_id u16, tur u8, kutle u8, p_n f64[3]
            fizik.push_back({oku_le<std::uint64_t>(yuk, 0), t_ns, oku_le<double>(yuk, 30)});
        } else if (kanal == 3u && yuk.size() >= 22u) {
            // yuk_olay: tur u16, onem u8, kaynak u8, adim_no u64, komut_no u32, istemci u32, ayrinti metin(u16)
            const auto n = oku_le<std::uint16_t>(yuk, 20);
            if (22u + n <= yuk.size()) {
                olay_turleri.push_back(oku_le<std::uint16_t>(yuk, 0));
                olaylar.push_back(yuk.substr(22, n));
            }
        }
        k += 24u + boy;
    }
}

}  // namespace ida::test::a3
