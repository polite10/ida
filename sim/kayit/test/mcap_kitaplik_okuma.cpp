// mcap_kitaplik_okuma.cpp — `foxglove/mcap` kitaplığının okuyucusuyla dosya dökümü (yalnız testler).
//
// Kitaplığın tamamı (okuyucu dahil) bu çeviri biriminde, AYRI bir ad alanında derlenir: ürünün
// sarmalayıcısı (`ida_kayit_mcap`) kitaplığın yazıcı uygulamasını `mcap` ad alanında zaten taşır ve
// başlık-yalnız kitaplığın uygulaması bir programda yalnız bir kez bulunabilir. Ad alanı burada
// makroyla yeniden adlandırılır; böylece testin kullandığı okuyucu, ürüne hiçbir şey eklemeden,
// kitaplığın kendi kodudur.
//
// Makroyla ad alanı değiştirmek kırılgandır (W5-kayit incelemesi, bulgu 9) ama denenen öteki yollar
// kapalıdır, bu yüzden bırakıldı:
//   - Bu birimde yalnız okuyucu uygulamasını (`reader.inl`) derleyip tip uygulamasını (`types.inl`)
//     ürünün biriminden almak: `reader.inl`, `types.inl`'de tanımlanan `constexpr OpCodeString`'i
//     kullanır; `constexpr` işlev örtük `inline`'dır ve kullanıldığı her birimde tanımlı olmalıdır.
//   - Kitaplığın tamamını tek bir test biriminde, ürünün birimi olmadan derlemek: test programı
//     `ida_kayit`'e bağlanır ve o da `ida_kayit_mcap` nesnesini getirir (yinelenen simge); nesneyi
//     dışarıda bırakmak testi ürünün derlenmiş sarmalayıcısından koparır.
// Makronun etki alanı bu birimle sınırlıdır: başlık (`mcap_kitaplik_okuma.hpp`) mcap tiplerini
// göstermez ve makrodan sonra içerilen tek İDA başlığı odur. Kitaplık sürümü `vcpkg.json`'da sabittir.
#define mcap mcap_sinama_kopyasi
#define MCAP_IMPLEMENTATION
#include <mcap/mcap.hpp>

#include "mcap_kitaplik_okuma.hpp"

namespace ida::kayit::test {

KitaplikDokumu kitaplik_ile_oku(const std::string& yol)
{
    KitaplikDokumu d;
    mcap::McapReader okuyucu;
    mcap::Status st = okuyucu.open(yol);
    if (!st.ok()) {
        d.hata = "open: " + st.message;
        return d;
    }
    const auto sorun = [&](const mcap::Status& s) { d.sorunlar.push_back(s.message); };
    st = okuyucu.readSummary(mcap::ReadSummaryMethod::NoFallbackScan, sorun);
    if (!st.ok()) {
        d.hata = "readSummary: " + st.message;
        return d;
    }
    if (okuyucu.header().has_value()) {
        d.profil = okuyucu.header()->profile;
        d.kitaplik = okuyucu.header()->library;
    }
    for (const auto& [id, s] : okuyucu.schemas()) {
        KitaplikSema k;
        k.id = id;
        k.ad = s->name;
        k.kodlama = s->encoding;
        k.veri.assign(reinterpret_cast<const char*>(s->data.data()), s->data.size());
        d.semalar.push_back(std::move(k));
    }
    for (const auto& [id, c] : okuyucu.channels()) {
        KitaplikKanal k;
        k.id = id;
        k.sema_id = c->schemaId;
        k.konu = c->topic;
        k.ileti_kodlamasi = c->messageEncoding;
        for (const auto& [a, v] : c->metadata) {
            k.metadata.emplace(a, v);
        }
        d.kanallar.push_back(std::move(k));
    }
    if (okuyucu.statistics().has_value()) {
        d.istatistik_ileti = okuyucu.statistics()->messageCount;
        d.istatistik_parca = okuyucu.statistics()->chunkCount;
    }
    {
        auto gorunum = okuyucu.readMessages(sorun);
        for (auto it = gorunum.begin(); it != gorunum.end(); it++) {
            KitaplikIleti i;
            i.konu = it->channel->topic;
            i.sira = it->message.sequence;
            i.log_time = it->message.logTime;
            i.publish_time = it->message.publishTime;
            const auto* p = reinterpret_cast<const std::uint8_t*>(it->message.data);
            i.veri.assign(p, p + it->message.dataSize);
            d.iletiler.push_back(std::move(i));
        }
    }
    // Üst veri, ek ve parça kayıtları: veri bölümünün kayıt kayıt taranması.
    {
        mcap::TypedRecordReader tr(*okuyucu.dataSource(), 8);
        tr.onMetadata = [&](const mcap::Metadata& m, mcap::ByteOffset) {
            std::map<std::string, std::string> icerik;
            for (const auto& [a, v] : m.metadata) {
                icerik.emplace(a, v);
            }
            d.ust_veri.emplace_back(m.name, std::move(icerik));
        };
        tr.onAttachment = [&](const mcap::Attachment& a, mcap::ByteOffset) {
            KitaplikEk e;
            e.ad = a.name;
            e.ortam_turu = a.mediaType;
            const auto* p = reinterpret_cast<const std::uint8_t*>(a.data);
            e.veri.assign(p, p + a.dataSize);
            d.ekler.push_back(std::move(e));
        };
        tr.onChunk = [&](const mcap::Chunk& c, mcap::ByteOffset) {
            KitaplikParca p;
            p.acik_boy = c.uncompressedSize;
            p.crc = c.uncompressedCrc;
            p.sikistirma = c.compression;
            d.parcalar.push_back(std::move(p));
        };
        bool veri_sonu = false;
        tr.onDataEnd = [&](const mcap::DataEnd&, mcap::ByteOffset) { veri_sonu = true; };
        while (!veri_sonu && tr.next()) {
        }
        if (!tr.status().ok()) {
            d.hata = "kayıt taraması: " + tr.status().message;
            return d;
        }
    }
    okuyucu.close();
    d.acildi = true;
    return d;
}

}  // namespace ida::kayit::test
