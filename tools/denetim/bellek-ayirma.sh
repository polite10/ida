#!/bin/bash
# Derlenmiş nesne ve arşivlerde yasak bellek ayırma başvurularını denetler.
#
# Kullanım:
#   tools/denetim/bellek-ayirma.sh <nesne-ya-da-arşiv>...
#
#   <nesne-ya-da-arşiv>  Denetlenecek .o ya da .a dosyası (ELF veya Mach-O).
#                       Bütün girdiler taranır; arşiv bulgusunda üye adı yazılır.
#
# Ortam değişkenleri:
#   NM  Tek komut adı ya da yolu; argümanlı değer desteklenmez.
#       Boşsa verilmemiş sayılır. ELF için varsa arm-none-eabi-nm,
#       diğer durumlarda nm seçilir. GNU/SysV arşivi ELF, BSD arşivi
#       Mach-O sayılır; biçim arşiv başlığından okunur, ar kullanılmaz.
#
# malloc/calloc/realloc/free, hizalı ayırma, strdup/strndup, sbrk,
# newlib yeniden girilebilir karşılıkları ve C++ new/delete ailesinin
# yalnız tanımsız başvuruları yasaktır. Mach-O ek alt çizgisi soyulur.
# nm hatası, tanınmayan çıktı ya da tanımsız tür harfi temiz sayılmaz.
# Çıkış: 0 temiz, 1 bulgu, 64 argüman yok, 66 dosya yok/okunamıyor,
#        70 nm/biçim hatası. Birleşen hatalarda öncelik: 70 > 66 > 1.

set -u
set -o pipefail
export LC_ALL=C

if [ "$#" -eq 0 ]; then
    printf 'Kullanım: tools/denetim/bellek-ayirma.sh <nesne-ya-da-arşiv>...\n' >&2
    exit 64
fi

sihir_oku() {
    local ham
    ham=$(od -An -tx1 -N8 "$1") || return 1
    printf '%s' "${ham//[[:space:]]/}"
}

macho_mu() {
    case "$1" in
        feedface*|cefaedfe*|feedfacf*|cffaedfe*|cafebabe*|bebafeca*|cafebabf*|bfbafeca*)
            return 0 ;;
        *) return 1 ;;
    esac
}

yasak_mi() {
    case "$1" in
        malloc|calloc|realloc|free|aligned_alloc|posix_memalign|memalign|valloc|\
        strdup|strndup|sbrk|_sbrk|_malloc_r|_calloc_r|_realloc_r|_free_r|_sbrk_r|\
        _Znw*|_Zna*|_Zdl*|_Zda*) return 0 ;;
        *) return 1 ;;
    esac
}

sonuc=0
# POSIX nm biçimi: dosya[üye]: sembol tür [değer [boyut]].
# GNU nm, tanımsız weak başvuruları w/v ile de gösterebilir.
satir_deseni='^(.*):[[:space:]]+([^[:space:]]+)[[:space:]]+([^[:space:]]+)([[:space:]].*)?$'

for girdi in "$@"; do
    dosya=$girdi
    case "$dosya" in -*) dosya=./$dosya ;; esac
    if [ ! -f "$dosya" ] || [ ! -r "$dosya" ]; then
        printf '%s: dosya yok ya da okunamıyor\n' "$girdi" >&2
        [ "$sonuc" -lt 66 ] && sonuc=66
        continue
    fi
    if ! sihir=$(sihir_oku "$dosya"); then
        printf '%s: dosya okunamadı\n' "$girdi" >&2
        [ "$sonuc" -lt 66 ] && sonuc=66
        continue
    fi
    arsiv=0
    macho=0
    elf=0
    case "$sihir" in
        213c617263683e0a)
            arsiv=1
            # İlk üye adının 16 baytlık alanı; sembol indeksinin içeriği okunmaz.
            if ! ilk_uye=$(dd if="$dosya" bs=1 skip=8 count=16 2>/dev/null); then
                printf '%s: arşiv başlığı okunamadı\n' "$girdi" >&2
                sonuc=70
                continue
            fi
            ilk_uye=${ilk_uye%% *}
            case "$ilk_uye" in
                __.SYMDEF*|'#1/'*) macho=1 ;;
                /|//|*/) elf=1 ;;
                *)
                    printf '%s: arşiv biçimi tanınmadı\n' "$girdi" >&2
                    sonuc=70
                    continue ;;
            esac ;;
        7f454c46*) elf=1 ;;
        *) macho_mu "$sihir" && macho=1 ;;
    esac
    if [ -n "${NM-}" ]; then
        nm_araci=$NM
    elif [ "$elf" -eq 1 ] && command -v arm-none-eabi-nm >/dev/null 2>&1; then
        nm_araci=arm-none-eabi-nm
    else
        nm_araci=nm
    fi
    if ! command -v "$nm_araci" >/dev/null 2>&1; then
        printf '%s: nm bulunamadı: %s\n' "$girdi" "$nm_araci" >&2
        sonuc=70
        continue
    fi
    if ! liste=$("$nm_araci" -A -P -u "$dosya"); then
        printf '%s: nm çalıştırılamadı ya da başarısız oldu: %s\n' "$girdi" "$nm_araci" >&2
        sonuc=70
        continue
    fi

    while IFS= read -r satir; do
        [ -n "$satir" ] || continue
        if ! [[ "$satir" =~ $satir_deseni ]]; then
            printf '%s: nm çıktı biçimi tanınmadı: %s\n' "$girdi" "$satir" >&2
            sonuc=70
            continue
        fi
        konum=${BASH_REMATCH[1]}
        sembol=${BASH_REMATCH[2]}
        tur=${BASH_REMATCH[3]}
        case "$tur" in
            U|w|v) ;;
            *)
                printf '%s: nm tanımsız tür harfi tanınmadı: %s (%s)\n' "$konum" "$tur" "$sembol" >&2
                sonuc=70
                continue ;;
        esac
        if [ "$arsiv" -eq 1 ]; then
            # Hem BSD/LLVM hem GNU nm -A -P üye adını böyle yazar.
            case "$konum" in
                "$dosya["*"]") ;;
                *)
                    printf '%s: nm arşiv üyesi biçimi tanınmadı\n' "$konum" >&2
                    sonuc=70
                    continue ;;
            esac
        fi
        ad=$sembol
        [ "$macho" -eq 0 ] || ad=${ad#_}
        if yasak_mi "$ad"; then
            printf '%s: yasak tanımsız başvuru: %s\n' "$konum" "$sembol"
            [ "$sonuc" -eq 0 ] && sonuc=1
        fi
    done <<< "$liste"
done

exit "$sonuc"
