// A3 HTTP kontrol uçlarının istemcisi (A3 §3.6, §4.1, §4.6).
//
// Her `/api/` isteği `Authorization: Bearer <jeton>` taşır (A3-K06). Durum
// değiştiren istekler ayrıca `Ida-Istemci` başlığı ve gövdede `istek_kimligi`
// taşır (A3-K50). Sanal acil durdurmanın yalnız basılması için sabit bir yol vardır;
// çalıştırma kontrolü ve senaryo yükleme de bu kimlik altyapısını kullanır.

import { HTTP_TABAN_YOLU } from './katalog';
import type {
  OnDenetimRaporu,
  YanitDurum,
  YanitIstemci,
  YanitKomut,
  YanitKuyrukta,
  YanitSenaryoDogrula,
  YanitSenaryoDosyasi,
  YanitSenaryoListesi,
  YanitSenaryoYukle,
  YanitOlaylar,
  YanitRet,
  YanitSurum,
} from './iletiler';

/** Sunucunun `yanit_ret` döndürdüğü ya da ağ hatası olan istek. */
export class A3Hatasi extends Error {
  constructor(
    /** A3 §3.9 kodu; ağ hatasında `ag_hatasi`, çözülemeyen yanıtta `yanit_cozulemedi` (istemci içi). */
    readonly kod: string,
    ileti: string,
    readonly http: number | null,
    readonly ayrinti: Record<string, unknown> | null = null,
    readonly komut_no: number | null = null,
  ) {
    super(ileti);
    this.name = 'A3Hatasi';
  }
}

type FetchIslevi = typeof fetch;

/** Çalıştırma kontrolü komutları (A3-K123). */
export type CalistirmaKomutu = 'baslat' | 'duraklat' | 'surdur' | 'adimla' | 'hiz' | 'durdur' | 'sifirla';

function istekKimligi(): string {
  const b = new Uint8Array(12);
  crypto.getRandomValues(b);
  return Array.from(b, (x) => x.toString(16).padStart(2, '0')).join('');
}

export class A3Http {
  private istemciId: number | null = null;

  constructor(
    /** Sunucu kökü, ör. `http://127.0.0.1:47100`. */
    readonly taban: string,
    private readonly jeton: string,
    private readonly fetchIslevi: FetchIslevi = (...a) => fetch(...a),
  ) {}

  private url(yol: string): string {
    return this.taban.replace(/\/$/, '') + HTTP_TABAN_YOLU + yol;
  }

  async istek<T>(yontem: string, yol: string, govde?: unknown, ekBaslik?: Record<string, string>, sinyal?: AbortSignal): Promise<T> {
    const baslik: Record<string, string> = { Authorization: `Bearer ${this.jeton}`, ...ekBaslik };
    const ayar: RequestInit = { method: yontem, headers: baslik, cache: 'no-store' };
    if (sinyal !== undefined) ayar.signal = sinyal;
    if (govde !== undefined) {
      baslik['Content-Type'] = 'application/json';
      ayar.body = JSON.stringify(govde);
    }
    let yanit: Response;
    try {
      yanit = await this.fetchIslevi(this.url(yol), ayar);
    } catch (h) {
      throw new A3Hatasi('ag_hatasi', h instanceof Error ? h.message : String(h), null);
    }
    const metin = await yanit.text();
    let json: unknown = null;
    try {
      json = metin.length > 0 ? JSON.parse(metin) : null;
    } catch {
      if (!yanit.ok) throw new A3Hatasi('yanit_cozulemedi', `HTTP ${yanit.status}`, yanit.status);
      throw new A3Hatasi('yanit_cozulemedi', 'yanıt JSON değil', yanit.status);
    }
    if (!yanit.ok) {
      const r = json as Partial<YanitRet> | null;
      if (r !== null && r.sonuc === 'ret' && r.hata !== undefined) {
        // A3-K18: bilinmeyen hata kodu `ileti` alanıyla gösterilir.
        throw new A3Hatasi(r.hata.kod, r.hata.ileti, yanit.status, r.hata.ayrinti ?? null, r.komut_no ?? null);
      }
      throw new A3Hatasi('yanit_cozulemedi', `HTTP ${yanit.status}`, yanit.status);
    }
    return json as T;
  }

  /** Yanıt JSON değilse (ör. senaryo baytları) ham metni döndürür. */
  async metinAl(yol: string): Promise<string> {
    let yanit: Response;
    try {
      yanit = await this.fetchIslevi(this.url(yol), {
        headers: { Authorization: `Bearer ${this.jeton}` },
        cache: 'no-store',
      });
    } catch (h) {
      throw new A3Hatasi('ag_hatasi', h instanceof Error ? h.message : String(h), null);
    }
    const metin = await yanit.text();
    if (!yanit.ok) {
      try {
        const r = JSON.parse(metin) as YanitRet;
        throw new A3Hatasi(r.hata.kod, r.hata.ileti, yanit.status, r.hata.ayrinti ?? null);
      } catch (h) {
        if (h instanceof A3Hatasi) throw h;
        throw new A3Hatasi('yanit_cozulemedi', `HTTP ${yanit.status}`, yanit.status);
      }
    }
    return metin;
  }

  surum(): Promise<YanitSurum> {
    return this.istek('GET', '/surum');
  }
  durum(): Promise<YanitDurum> {
    return this.istek('GET', '/durum');
  }
  /** A3-K45: `sonra` sırasından sonraki en çok `enCok` (≤ 1000) olay. */
  olaylar(sonra: number, enCok = 1000): Promise<YanitOlaylar> {
    return this.istek('GET', `/olaylar?sonra=${sonra}&en_cok=${Math.min(1000, Math.max(1, enCok))}`);
  }
  onDenetim(): Promise<OnDenetimRaporu> {
    return this.istek('GET', '/on_denetim');
  }
  /** Yüklü çözümlenmiş senaryo (A3-K80): A5 kanonik JSON. */
  async cozumlenmisSenaryo(): Promise<Record<string, unknown>> {
    return JSON.parse(await this.metinAl('/senaryo/cozumlenmis')) as Record<string, unknown>;
  }
  /** Çözümlenmiş senaryo ve `Ida-Davranis-Ozeti` başlığı (A3-K80). */
  async cozumlenmisSenaryoOzetli(): Promise<{ senaryo: Record<string, unknown>; ozet: string | null }> {
    let yanit: Response;
    try {
      yanit = await this.fetchIslevi(this.url('/senaryo/cozumlenmis'), { headers: { Authorization: `Bearer ${this.jeton}` }, cache: 'no-store' });
    } catch (h) {
      throw new A3Hatasi('ag_hatasi', h instanceof Error ? h.message : String(h), null);
    }
    const metin = await yanit.text();
    if (!yanit.ok) {
      try {
        const r = JSON.parse(metin) as YanitRet;
        throw new A3Hatasi(r.hata.kod, r.hata.ileti, yanit.status, r.hata.ayrinti ?? null);
      } catch (h) {
        if (h instanceof A3Hatasi) throw h;
        throw new A3Hatasi('yanit_cozulemedi', `HTTP ${yanit.status}`, yanit.status);
      }
    }
    try {
      return { senaryo: JSON.parse(metin) as Record<string, unknown>, ozet: yanit.headers.get('Ida-Davranis-Ozeti') };
    } catch {
      throw new A3Hatasi('yanit_cozulemedi', 'çözümlenmiş senaryo JSON değil', yanit.status);
    }
  }

  /** `GET /komutlar/{komut_no}` (A3-K60). */
  komutDurumu(komutNo: number): Promise<YanitKomut> {
    return this.istek('GET', `/komutlar/${komutNo}`);
  }
  /** `GET /on_denetim/{rapor_kimligi}` (A3-K62). */
  onDenetimRaporu(kimlik: string): Promise<OnDenetimRaporu> {
    return this.istek('GET', `/on_denetim/${encodeURIComponent(kimlik)}`);
  }

  senaryolar(): Promise<YanitSenaryoListesi> {
    return this.istek('GET', '/senaryolar');
  }
  senaryoDosyasi(ad: string): Promise<YanitSenaryoDosyasi> {
    return this.istek('GET', `/senaryolar/${encodeURIComponent(ad)}`);
  }
  /** Doğrulama dosyayı ve durumu değiştirmez, komut numarası almaz (A3-K76). */
  senaryoDogrula(girdi: { ad: string } | { icerik: string }): Promise<YanitSenaryoDogrula> {
    return this.istek('POST', '/senaryo/dogrula', girdi);
  }
  /**
   * Senaryo yükleme (A3-K77): senkron uç, 200 ile sonuçlanır; çalıştırma kontrolü
   * revizyonlarını taşır (A3-K123, K124).
   */
  senaryoYukle(
    girdi: { ad: string } | { icerik: string },
    beklenen: { run_id: string | null; durum_rev: number },
  ): Promise<YanitSenaryoYukle> {
    return this.komut<YanitSenaryoYukle>('/senaryo/yukle', {
      beklenen_run_id: beklenen.run_id,
      beklenen_durum_rev: beklenen.durum_rev,
      ...girdi,
    });
  }

  /** `POST /istemciler` (A3 §2): durum değiştiren isteklerin kimliği. */
  async istemciKaydet(ad = 'panel', sinyal?: AbortSignal): Promise<number> {
    const y = await this.istek<YanitIstemci>('POST', '/istemciler', { ad: ad.slice(0, 64) }, undefined, sinyal);
    this.istemciId = y.istemci_id;
    return y.istemci_id;
  }

  istemciUnut(): void {
    this.istemciId = null;
  }

  /**
   * Durum değiştiren istek (A3-K50…K56). Kimlik yoksa önce istemci kaydı alınır.
   * `istek_kimligi` verilmezse rastgele üretilir; yeniden denemede aynı kimlik
   * verilerek yineleme güvencesi (A3-K53) kullanılabilir.
   */
  async komut<T = YanitKuyrukta>(yol: string, govde: Record<string, unknown>, sinyal?: AbortSignal): Promise<T> {
    if (this.istemciId === null) await this.istemciKaydet('panel', sinyal);
    const g = { istek_kimligi: istekKimligi(), ...govde };
    return this.istek<T>('POST', yol, g, { 'Ida-Istemci': String(this.istemciId) }, sinyal);
  }

  /** A3-K90/K92: tek yönlü sanal acil durdurma; onay ya da revizyon istemez. */
  acilDurdur(runId: string | null, sinyal?: AbortSignal): Promise<YanitKuyrukta> {
    return this.komut('/emniyet/degistir', {
      beklenen_run_id: runId,
      girdi: 'acil_durdurma',
      hedef_durum: 'basili',
    }, sinyal);
  }

  /**
   * Çalıştırma kontrolü (A3-K123): `beklenen_run_id` ve `beklenen_durum_rev` zorunludur.
   * Panellerde tek yol `KomutYoneticisi`'dir (komut.ts); bu yöntem onun taşımasıdır.
   */
  calistirma(
    komut: CalistirmaKomutu,
    beklenen: { run_id: string | null; durum_rev: number },
    ek: Record<string, unknown> = {},
  ): Promise<YanitKuyrukta> {
    return this.komut(`/calistirma/${komut}`, {
      beklenen_run_id: beklenen.run_id,
      beklenen_durum_rev: beklenen.durum_rev,
      ...ek,
    });
  }
}
