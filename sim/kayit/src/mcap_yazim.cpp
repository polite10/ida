// mcap_yazim.cpp — `foxglove/mcap` C++ yazıcısının sarmalayıcısı ve kitaplığın yazıcı uygulaması.
//
// Kitaplık başlık-yalnızdır: uygulaması `MCAP_IMPLEMENTATION` tanımlı tek bir çeviri biriminde
// derlenir; o birim budur (yalnız yazıcı: `writer.hpp` → `types.inl`, `writer.inl`). Okuyucu
// uygulaması (`reader.inl`) ürüne girmez; A6 okuyucusu kendi ayrıştırıcısını kullanır (A6-K59, K60).
// lz4 yoktur: `MCAP_COMPRESSION_NO_LZ4` vcpkg hedefinden gelir (W5-K35).
#define MCAP_IMPLEMENTATION
#include <mcap/writer.hpp>

#include "mcap_yazim.hpp"

#include <zstd.h>

#include <algorithm>
#include <limits>

namespace ida::kayit::ic {

namespace {

class Kopru final : public mcap::IWritable {
public:
    explicit Kopru(McapCikti& c) : c_(&c) {}
    void end() override { c_->bosalt(); }
    void flush() override { c_->bosalt(); }
    uint64_t size() const override { return c_->boyut(); }

    // Kuyruk yazımı: CRC verilen ara durumdan sürdürülür. Kitaplığın kendi CRC alanı (`crc_`)
    // dışarıdan kurulamadığı için bu yolda hesap burada yapılır (kitaplığın `crc32Update` işleviyle;
    // `crc32.hpp` `writer.inl` üzerinden içerilir).
    void crc_surdur(std::uint32_t ara) noexcept
    {
        ara_ = ara;
        kendi_crc_ = true;
    }
    std::uint32_t crc_sonucu() const noexcept { return mcap::internal::crc32Final(ara_); }

protected:
    void handleWrite(const std::byte* data, uint64_t size) override
    {
        if (kendi_crc_) {
            ara_ = mcap::internal::crc32Update(ara_, data, static_cast<std::size_t>(size));
        }
        c_->yaz(reinterpret_cast<const std::uint8_t*>(data), size);
    }

private:
    McapCikti* c_;
    std::uint32_t ara_ = mcap::internal::CRC32_INIT;
    bool kendi_crc_ = false;
};

mcap::KeyValueMap esle(const AnahtarDeger& a)
{
    mcap::KeyValueMap m;
    m.reserve(a.size());
    for (const auto& [k, v] : a) {
        m.emplace(k, v);
    }
    return m;
}

}  // namespace

struct McapYazim::Ic {
    std::unique_ptr<Kopru> kopru;
    mcap::McapWriter yazici;
    bool acik = false;
};

McapYazim::McapYazim() : ic_(std::make_unique<Ic>()) {}

McapYazim::~McapYazim()
{
    // Açık kaldıysa kitaplığın yıkıcısı dosyayı kapatmaya çalışmasın: çıktı artık geçerli olmayabilir.
    if (ic_->acik) {
        ic_->yazici.terminate();
    }
}

void McapYazim::ac(McapCikti& cikti, std::string_view profil, std::uint64_t parca_hedefi_bayt)
{
    ic_->kopru = std::make_unique<Kopru>(cikti);
    mcap::McapWriterOptions secenek(profil);
    secenek.chunkSize = parca_hedefi_bayt;
    secenek.compression = mcap::Compression::Zstd;
    // A6-K51: parça ve ek CRC'si kapatılmaz (varsayılan: açık). Öteki seçenekler kitaplık
    // varsayılanıdır (A6-K03): ileti indeksleri, özet, `forceCompression = false`.
    secenek.noChunkCRC = false;
    secenek.noAttachmentCRC = false;
    // W5-K39: veri bölümü CRC'si her zaman yazılır. Kitaplık CRC'yi `open` içinde, baştaki sihirli
    // baytlardan başlatır ve `close` içinde Data End'e yazar (`writer.inl`).
    secenek.enableDataCRC = true;
    ic_->yazici.open(*ic_->kopru, secenek);
    ic_->acik = true;
}

std::uint16_t McapYazim::sema_ekle(std::string_view ad, std::string_view kodlama, std::string_view veri)
{
    mcap::Schema s(ad, kodlama, veri);
    ic_->yazici.addSchema(s);
    return s.id;
}

std::uint16_t McapYazim::kanal_ekle(std::string_view konu, std::string_view ileti_kodlamasi, std::uint16_t sema_id,
                                    const AnahtarDeger& metadata)
{
    mcap::Channel c(konu, ileti_kodlamasi, sema_id, esle(metadata));
    ic_->yazici.addChannel(c);
    return c.id;
}

bool McapYazim::ileti_yaz(std::uint16_t kanal_id, std::uint32_t sira, std::uint64_t log_time,
                          std::uint64_t publish_time, const std::uint8_t* veri, std::size_t n)
{
    mcap::Message m;
    m.channelId = kanal_id;
    m.sequence = sira;
    m.logTime = log_time;
    m.publishTime = publish_time;
    m.dataSize = n;
    m.data = reinterpret_cast<const std::byte*>(veri);
    return ic_->yazici.write(m).ok();
}

bool McapYazim::ust_veri_yaz(std::string_view ad, const AnahtarDeger& icerik)
{
    mcap::Metadata m;
    m.name = std::string(ad);
    m.metadata = esle(icerik);
    return ic_->yazici.write(m).ok();
}

bool McapYazim::ek_yaz(std::string_view ad, std::string_view ortam_turu, std::uint64_t log_time,
                       std::uint64_t create_time, const std::uint8_t* veri, std::uint64_t n)
{
    mcap::Attachment a;
    a.logTime = log_time;
    a.createTime = create_time;
    a.name = std::string(ad);
    a.mediaType = std::string(ortam_turu);
    a.dataSize = n;
    a.data = reinterpret_cast<const std::byte*>(veri);
    a.crc = 0;  // kitaplık hesaplar (noAttachmentCRC = false)
    return ic_->yazici.write(a).ok();
}

std::uint32_t McapYazim::veri_crc_ara() const
{
    // `IWritable::crc()` bitirilmiş değeri (ara durum XOR 0xFFFFFFFF) verir; `crc32Final` kendi
    // tersidir, ara durum onunla geri alınır.
    return mcap::internal::crc32Final(ic_->kopru->crc());
}

void McapYazim::parca_kapat()
{
    ic_->yazici.closeLastChunk();
}

void McapYazim::kapat()
{
    ic_->yazici.close();
    ic_->acik = false;
}

void McapYazim::birak()
{
    ic_->yazici.terminate();
    ic_->acik = false;
}

void McapYazim::kuyruk_yaz(McapCikti& cikti, std::uint32_t veri_crc_ara, const KuyrukKanali& kanal,
                           std::span<const KuyrukIletisi> iletiler, std::string_view ust_veri_adi,
                           const AnahtarDeger& ust_veri)
{
    Kopru k(cikti);
    k.crc_surdur(veri_crc_ara);
    if (!iletiler.empty()) {
        mcap::BufferWriter tampon;
        tampon.crcEnabled = true;
        mcap::Schema s(kanal.sema_adi, kanal.sema_kodlamasi, kanal.sema_verisi);
        s.id = kanal.sema_id;
        mcap::Channel c(kanal.konu, kanal.ileti_kodlamasi, kanal.sema_id, esle(kanal.metadata));
        c.id = kanal.kanal_id;
        mcap::McapWriter::write(tampon, s);
        mcap::McapWriter::write(tampon, c);
        mcap::Timestamp ilk = std::numeric_limits<mcap::Timestamp>::max();
        mcap::Timestamp son = 0;
        for (const auto& i : iletiler) {
            mcap::Message m;
            m.channelId = kanal.kanal_id;
            m.sequence = i.sira;
            m.logTime = i.log_time;
            m.publishTime = i.publish_time;
            m.dataSize = i.veri.size();
            m.data = reinterpret_cast<const std::byte*>(i.veri.data());
            mcap::McapWriter::write(tampon, m);
            ilk = std::min(ilk, m.logTime);
            son = std::max(son, m.logTime);
        }
        mcap::Chunk p;
        p.messageStartTime = ilk;
        p.messageEndTime = son;
        p.uncompressedSize = tampon.size();
        p.uncompressedCrc = tampon.crc();
        p.compression = std::string();
        p.compressedSize = tampon.size();
        p.records = tampon.data();
        mcap::McapWriter::write(k, p);
    }
    mcap::Metadata m;
    m.name = std::string(ust_veri_adi);
    m.metadata = esle(ust_veri);
    mcap::McapWriter::write(k, m);
    // Data End: buraya kadarki bütün baytların CRC'si (kayıt kendisi kapsam dışıdır).
    mcap::McapWriter::write(k, mcap::DataEnd{k.crc_sonucu()});
    mcap::McapWriter::write(k, mcap::Footer{0, 0}, false);
    mcap::McapWriter::writeMagic(k);
    cikti.bosalt();
}

std::string McapYazim::mcap_surumu()
{
    return MCAP_LIBRARY_VERSION;
}

std::string McapYazim::zstd_surumu()
{
    return ZSTD_versionString();
}

}  // namespace ida::kayit::ic
