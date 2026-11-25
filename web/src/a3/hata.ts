// A3 hata kodlarının (§3.9) okunur Türkçe iletileri. İstemci mantığı yalnız `kod`'a
// bakar; bu metin kullanıcıya gösterilir. Bilinmeyen kod sunucunun kendi iletisiyle
// gösterilir (A3-K18).

import type { A3Hatasi } from './http';

const ILETILER: Readonly<Record<string, string>> = {
  erisim_reddedildi: 'Erişim reddedildi: jeton geçersiz ya da adres izinli değil.',
  bulunamadi: 'Sunucu bu ucu sunmuyor ya da istenen kaynak yok.',
  yontem_desteklenmiyor: 'Sunucu bu ucu bu yöntemle sunmuyor.',
  gecersiz_istek: 'İstek sunucu tarafından anlaşılamadı (biçim ya da zorunlu alan hatası).',
  gecersiz_deger: 'İstekteki bir değer geçerli aralığın dışında.',
  senaryo_gecersiz: 'Senaryo doğrulamadan geçmedi; bulgular raporda.',
  govde_cok_buyuk: 'İstek gövdesi sunucunun sınırından büyük.',
  icerik_turu: 'İstek içerik türü kabul edilmedi.',
  gecersiz_durum: 'Komut çalıştırmanın şu anki durumunda geçerli değil.',
  kipte_gecersiz: 'Komut bu zaman kipinde geçerli değil (HIL kiplerinde adımlama ve hız değişimi yok).',
  beklenen_durum_uyusmuyor: 'Ekrandaki çalıştırma artık güncel değil (çalıştırma kimliği değişmiş); ekran yenilenince yeniden deneyin.',
  onay_gerekli: 'Komut onay istiyor (HIL kipinde duraklatma çalıştırmayı geçersiz kılar).',
  onay_jetonu_gecersiz: 'Onay jetonu geçersiz ya da süresi geçmiş.',
  kimlik_cakismasi: 'Aynı istek kimliği başka bir istekle çakıştı.',
  emniyet_rev_uyusmuyor: 'Emniyet revizyonu değişmiş; ekran yenilenince yeniden deneyin.',
  durum_rev_uyusmuyor: 'Durum başka bir komutla değişmiş (ekrandaki revizyon eski); ekran yenilenince yeniden deneyin.',
  emniyet_rev_doydu: 'Emniyet revizyonu doydu; gevşetme kabul edilmez.',
  kisitlama_hakim: 'Aynı adımda kısıtlayan bir değişiklik var; o öncelikli.',
  tekrar_kosulu_saglanmiyor: 'Tekrar koşulu sağlanmıyor (sürüm, platform ya da özet).',
  tekrar_kipinde_yasak: 'Tekrar kipinde girdiyi değiştiren müdahale yapılamaz.',
  izleme_etkin: 'İzleme sürerken canlı çalıştırma komutu verilemez.',
  canli_calistirma_yok: 'Komut canlı bir çalıştırma gerektiriyor.',
  komut_listede_yok: 'Komut yer istasyonu listesinde yok.',
  kayit_yok: 'İstenen kayıt dosyası sunucuda yok.',
  kayit_okunamadi: 'Kayıt bozuk ya da sürümü uyumsuz.',
  gecmis_yok: 'İstenen olay geçmişi sunucuda artık yok.',
  mesgul: 'Sunucu meşgul (kapanıyor ya da komut kuyruğu dolu).',
  sunucu_hatasi: 'Sunucuda beklenmeyen iç hata.',
  on_denetim_kaldi: 'Ön denetim geçmedi; çalıştırma başlamadı. Maddeler ön denetim raporunda.',
  dosya_var: 'Aynı adlı senaryo dosyası zaten var.',
  ag_hatasi: 'Sunucuya ulaşılamadı; komutun uygulanıp uygulanmadığı bilinmiyor.',
  yanit_cozulemedi: 'Sunucunun yanıtı anlaşılamadı.',
  zaman_asimi: 'Sunucudan süre sınırı içinde sonuç gelmedi; komutun sonucu bilinmiyor.',
  baglanti_koptu: 'Bağlantı koptu; komutun sonucu bilinmiyor.',
};

/** Koda özgü kısa ek bilgi (yalnız bilinen alanlar; ayrıntı şeması koda aittir). */
function ek(kod: string, ayrinti: Record<string, unknown> | null | undefined): string {
  if (ayrinti === null || ayrinti === undefined) return '';
  const g = (k: string): string | null => {
    const v = ayrinti[k];
    return typeof v === 'string' || typeof v === 'number' ? String(v) : null;
  };
  if (kod === 'beklenen_durum_uyusmuyor' || kod === 'durum_rev_uyusmuyor') {
    const p: string[] = [];
    if (g('guncel_durum') !== null) p.push(`güncel durum: ${g('guncel_durum')}`);
    if (g('guncel_run_id') !== null) p.push(`güncel çalıştırma: ${g('guncel_run_id')}`);
    if (g('guncel_durum_rev') !== null) p.push(`güncel revizyon: ${g('guncel_durum_rev')}`);
    return p.length > 0 ? ` (${p.join('; ')})` : '';
  }
  return '';
}

/** Kod için okunur iletiyi verir; bilinmeyen kodda sunucunun kendi iletisini. */
export function hataIletisi(kod: string, sunucuIletisi?: string | null, ayrinti?: Record<string, unknown> | null): string {
  const bilinen = ILETILER[kod];
  if (bilinen !== undefined) return bilinen + ek(kod, ayrinti);
  return sunucuIletisi !== undefined && sunucuIletisi !== null && sunucuIletisi !== '' ? `${sunucuIletisi} (${kod})` : `Bilinmeyen hata kodu: ${kod}`;
}

export function hatadanIleti(h: A3Hatasi): string {
  return hataIletisi(h.kod, h.message, h.ayrinti);
}

/** Sunucunun "bu uç yok" anlamına gelen yanıtları: 404 `bulunamadi` ve 405 `yontem_desteklenmiyor` (W5-K45). */
export function ucYok(h: unknown): boolean {
  const k = (h as { kod?: unknown } | null)?.kod;
  return k === 'bulunamadi' || k === 'yontem_desteklenmiyor';
}
