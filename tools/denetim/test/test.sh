#!/bin/bash
# Kullanım: tools/denetim/test/test.sh
# Yerel C17/C++ ve varsa ARM ELF örneklerini gerçekten derleyip denetler.
# stdout/stderr ayrı dosyalarda tutulur; yalnız tam bulgu satırları kabul edilir.

set -u
export LC_ALL=C

kok=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd) || exit 1
cd "$kok" || exit 1
denetim=tools/denetim/bellek-ayirma.sh
kaynak=tools/denetim/test
derleme=$kaynak/derleme
basarisiz=0
mkdir -p "$derleme" || exit 1
son_stdout=$derleme/son.stdout
son_stderr=$derleme/son.stderr
# Testin varsayılan nm seçimini dış ortamın NM değeri değiştiremez.
unset NM

bildir() {
    if [ "$2" -eq 0 ]; then
        printf 'GEÇTİ: %s\n' "$1"
    else
        printf 'KALDI: %s\n' "$1"
        basarisiz=1
        printf 'stdout:\n'
        cat "$son_stdout"
        printf 'stderr:\n'
        cat "$son_stderr"
    fi
}

dogrula() {
    local etiket=$1 beklenen=$2 gercek
    shift 2
    "$@" >"$son_stdout" 2>"$son_stderr"
    gercek=$?
    if [ "$gercek" -eq "$beklenen" ]; then
        bildir "$etiket" 0
    else
        bildir "$etiket (beklenen çıkış $beklenen, gerçek $gercek)" 1
    fi
}

bos_dogrula() {
    local sonuc=0
    [ ! -s "$son_stdout" ] && [ ! -s "$son_stderr" ] || sonuc=1
    bildir "$1" "$sonuc"
}

tam_bulgular_dogrula() {
    local etiket=$1 satir sonuc=0 adet
    shift
    adet=$(wc -l <"$son_stdout")
    [ "$adet" -eq "$#" ] && [ ! -s "$son_stderr" ] || sonuc=1
    for satir in "$@"; do
        grep -Fqx -- "$satir" "$son_stdout" || sonuc=1
    done
    bildir "$etiket" "$sonuc"
}

cpp_bulgular_dogrula() {
    local etiket=$1 konum=$2 onek=$3 aile sonuc=0 adet satir sembol desen bulundu
    adet=$(wc -l <"$son_stdout")
    [ "$adet" -eq 4 ] && [ ! -s "$son_stderr" ] || sonuc=1
    # Konum düz metin olarak eşleşir; regex yalnız mangled sembolü sınar.
    for aile in 'nw[jm]' 'na[jm]' 'dlPv' 'daPv'; do
        desen="^${onek}_Z${aile}$"
        bulundu=1
        while IFS= read -r satir; do
            case "$satir" in
                "$konum: yasak tanımsız başvuru: "*)
                    sembol=${satir#"$konum: yasak tanımsız başvuru: "}
                    [[ "$sembol" =~ $desen ]] && bulundu=0 ;;
            esac
        done <"$son_stdout"
        [ "$bulundu" -eq 0 ] || sonuc=1
    done
    bildir "$etiket" "$sonuc"
}

takim_denetle() {
    local etiket=$1 derleyici=$2 cpp_derleyici=$3 arsivci=$4 cikti=$5 onek=$6
    local nesne=$cikti/gecmeyen.o arsiv=$cikti/ornek.a bulgu='yasak tanımsız başvuru'
    if ! mkdir -p "$cikti" ||
       ! "$derleyici" -std=c17 -Wall -Wextra -Werror -O0 -fno-builtin -c "$kaynak/gecen.c" -o "$cikti/gecen.o" ||
       ! "$derleyici" -std=c17 -Wall -Wextra -Werror -O0 -fno-builtin -c "$kaynak/gecmeyen.c" -o "$nesne" ||
       ! "$derleyici" -std=c17 -Wall -Wextra -Werror -O0 -fno-builtin -c "$kaynak/zayif.c" -o "$cikti/zayif.o" ||
       ! "$cpp_derleyici" -std=c++11 -Wall -Wextra -Werror -O0 -c "$kaynak/gecmeyen.cpp" -o "$cikti/cpp.o" ||
       ! rm -f "$arsiv" ||
       ! "$arsivci" rcs "$arsiv" "$cikti/gecen.o" "$nesne"; then
        printf 'KALDI: %s C17/C++ derleme ve arşiv üretimi\n' "$etiket"
        basarisiz=1
        return
    fi
    printf 'GEÇTİ: %s C17/C++ derleme ve arşiv üretimi\n' "$etiket"
    dogrula "$etiket gecen.o -> 0" 0 "$denetim" "$cikti/gecen.o"
    bos_dogrula "$etiket temiz nesnede stdout/stderr boş"
    dogrula "$etiket gecmeyen.o -> 1" 1 "$denetim" "$nesne"
    tam_bulgular_dogrula "$etiket malloc/free tam bulgu satırları, stderr boş" \
        "$nesne: $bulgu: ${onek}malloc" "$nesne: $bulgu: ${onek}free"
    dogrula "$etiket arşiv -> 1" 1 "$denetim" "$arsiv"
    tam_bulgular_dogrula "$etiket arşivde tam bulgular, temiz üyede bulgu yok, stderr boş" \
        "$arsiv[gecmeyen.o]: $bulgu: ${onek}malloc" "$arsiv[gecmeyen.o]: $bulgu: ${onek}free"
    dogrula "$etiket birden çok girdi -> 1" 1 "$denetim" "$nesne" "$arsiv"
    tam_bulgular_dogrula "$etiket bütün girdilerde tam bulgular, stderr boş" \
        "$nesne: $bulgu: ${onek}malloc" "$nesne: $bulgu: ${onek}free" \
        "$arsiv[gecmeyen.o]: $bulgu: ${onek}malloc" "$arsiv[gecmeyen.o]: $bulgu: ${onek}free"
    dogrula "$etiket C++ nesnesi -> 1" 1 "$denetim" "$cikti/cpp.o"
    cpp_bulgular_dogrula "$etiket new/delete/new[]/delete[] tam bulguları, stderr boş" "$cikti/cpp.o" "$onek"
    dogrula "$etiket zayıf malloc -> 1" 1 "$denetim" "$cikti/zayif.o"
    tam_bulgular_dogrula "$etiket zayıf malloc tam bulgusu, stderr boş" "$cikti/zayif.o: $bulgu: ${onek}malloc"
    dogrula "$etiket boş NM varsayılan seçim -> 1" 1 env NM= "$denetim" "$arsiv"
    tam_bulgular_dogrula "$etiket boş NM ile arşivde tam bulgular, stderr boş" \
        "$arsiv[gecmeyen.o]: $bulgu: ${onek}malloc" "$arsiv[gecmeyen.o]: $bulgu: ${onek}free"
    dogrula "$etiket olmayan dosya -> 66" 66 "$denetim" "$cikti/olmayan.o"
    dogrula "$etiket eksik dosya + bulgulu dosya -> 66" 66 "$denetim" "$cikti/olmayan.o" "$nesne"
    if grep -Fqx -- "$nesne: $bulgu: ${onek}malloc" "$son_stdout" &&
       grep -Fqx -- "$nesne: $bulgu: ${onek}free" "$son_stdout" &&
       [ "$(wc -l <"$son_stdout")" -eq 2 ] &&
       grep -Fqx -- "$cikti/olmayan.o: dosya yok ya da okunamıyor" "$son_stderr"; then
        bildir "$etiket eksik dosyadan sonra tam bulgular tarandı" 0
    else
        bildir "$etiket eksik dosyadan sonra tam bulgular tarandı" 1
    fi
    cp "$cikti/gecen.o" "$cikti/izinsiz.o" || { basarisiz=1; return; }
    chmod 000 "$cikti/izinsiz.o" || { basarisiz=1; return; }
    if [ ! -r "$cikti/izinsiz.o" ]; then
        dogrula "$etiket okunamayan dosya -> 66" 66 "$denetim" "$cikti/izinsiz.o"
    else
        printf 'ATLANDI: %s okunamayan dosya (oturum izinleri okumayı engellemiyor)\n' "$etiket"
    fi
    chmod 600 "$cikti/izinsiz.o" || basarisiz=1
    dogrula "$etiket argümansız -> 64" 64 "$denetim"
    dogrula "$etiket bulunamayan nm -> 70" 70 env NM="$cikti/olmayan-nm" "$denetim" "$cikti/gecen.o"
    dogrula "$etiket başarısız nm -> 70" 70 env NM=false "$denetim" "$cikti/gecen.o"
    dogrula "$etiket argümanlı NM desteklenmez -> 70" 70 env NM='nm -u' "$denetim" "$cikti/gecen.o"
    dogrula "$etiket araç hatası + eksik dosya -> 70" 70 env NM=false "$denetim" "$cikti/gecen.o" "$cikti/olmayan.o"
}

# Yerel nesnenin Mach-O olup olmadığı derleyici hedef makrosundan belirlenir.
yerel_onek=
if cc -dM -E - </dev/null | grep -q '^#define __APPLE__ '; then
    yerel_onek=_
fi
takim_denetle yerel cc c++ ar "$derleme" "$yerel_onek"

# Denetimin kendi tanımadığı türü sessizce atlamadığını yapay nm ile sına.
sahte_nm=$derleme/sahte-nm
cat >"$sahte_nm" <<'NM'
#!/bin/bash
printf '%s: malloc X\n' "$4"
printf '%s: free U\n' "$4"
NM
chmod +x "$sahte_nm" || exit 1
dogrula 'bilinmeyen tanımsız tür + bulgu + eksik dosya -> 70' 70 \
    env NM="$sahte_nm" "$denetim" "$derleme/gecen.o" "$derleme/olmayan.o"
if grep -Fqx -- "$derleme/gecen.o: nm tanımsız tür harfi tanınmadı: X (malloc)" "$son_stderr" &&
   grep -Fqx -- "$derleme/gecen.o: yasak tanımsız başvuru: free" "$son_stdout"; then
    bildir 'bilinmeyen tür bildirildi, sonraki sembol tarandı' 0
else
    bildir 'bilinmeyen tür bildirildi, sonraki sembol tarandı' 1
fi

if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    takim_denetle ELF arm-none-eabi-gcc arm-none-eabi-g++ arm-none-eabi-ar "$derleme/elf" ''
    if arm-none-eabi-gcc -std=c17 -Wall -Wextra -Werror -O0 -fno-builtin \
        -c "$kaynak/newlib.c" -o "$derleme/elf/newlib.o"; then
        printf 'GEÇTİ: ELF newlib derleme\n'
        dogrula 'ELF newlib nesnesi -> 1' 1 "$denetim" "$derleme/elf/newlib.o"
        tam_bulgular_dogrula 'ELF _malloc_r/_sbrk tam bulguları, stderr boş' \
            "$derleme/elf/newlib.o: yasak tanımsız başvuru: _malloc_r" \
            "$derleme/elf/newlib.o: yasak tanımsız başvuru: _sbrk"
    else
        printf 'KALDI: ELF newlib derleme\n'
        basarisiz=1
    fi
    dogrula 'ELF arşiv açık NM ile -> 1' 1 env NM=arm-none-eabi-nm "$denetim" "$derleme/elf/ornek.a"
    tam_bulgular_dogrula 'ELF açık NM ile arşivde tam bulgular, stderr boş' \
        "$derleme/elf/ornek.a[gecmeyen.o]: yasak tanımsız başvuru: malloc" \
        "$derleme/elf/ornek.a[gecmeyen.o]: yasak tanımsız başvuru: free"
    # Varsayılan seçimi gerçek nm'yi çağırıp izi dosyaya yazan bir sarmalayıcıyla sına.
    gercek_nm=$(command -v arm-none-eabi-nm)
    mkdir -p "$derleme/nm-secimi" || exit 1
    cat >"$derleme/nm-secimi/arm-none-eabi-nm" <<'NM'
#!/bin/bash
printf 'arm-none-eabi-nm\n' >>"$DENETIM_NM_IZ"
exec "$DENETIM_GERCEK_NM" "$@"
NM
    chmod +x "$derleme/nm-secimi/arm-none-eabi-nm" || exit 1
    : >"$derleme/nm-secimi/iz"
    dogrula 'ELF arşiv varsayılan araç seçimi -> 1' 1 \
        env PATH="$kok/$derleme/nm-secimi:$PATH" DENETIM_GERCEK_NM="$gercek_nm" \
        DENETIM_NM_IZ="$derleme/nm-secimi/iz" "$denetim" "$derleme/elf/ornek.a"
    if [ "$(cat "$derleme/nm-secimi/iz")" = arm-none-eabi-nm ]; then
        bildir 'GNU/SysV arşivi için arm-none-eabi-nm seçildi' 0
    else
        bildir 'GNU/SysV arşivi için arm-none-eabi-nm seçildi' 1
    fi
    tam_bulgular_dogrula 'ELF araç seçimi sonrasında tam bulgular, stderr boş' \
        "$derleme/elf/ornek.a[gecmeyen.o]: yasak tanımsız başvuru: malloc" \
        "$derleme/elf/ornek.a[gecmeyen.o]: yasak tanımsız başvuru: free"
else
    printf 'ATLANDI: ELF denetimleri (arm-none-eabi-gcc PATH içinde yok)\n'
fi

exit "$basarisiz"
