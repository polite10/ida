extends RefCounted
## A7 varlık kümesi yükleyicisi (A2-K13, A7-K53): manifesti ve her .glb'yi sahne kurulmadan
## önce doğrular, şablonları kurar, gövde örneği üretir, etkin malzeme değerlerini uygular.
##
## Sıra (sol #7–#9): manifest okunur → özet hosgeldin'dekiyle karşılaştırılır (uyuşmazsa
## hiçbir şey yüklenmez) → K04 → A7 şeması (assets/manifest.schema.json) → küme kimliği ve
## tekillik → her varlık için K06 (her oturumda; önbellek isabetinde de) → önbellek
## (anahtar: manifest özeti + küme dizini) → her .glb için K10–K13, K19–K21, K23, K28
## (Godot içe aktarıcısına vermeden, dosyanın kendi JSON ve BIN parçasından) → içe aktarma
## ve K22. Başarısızlıkta hazir.durum kodu döner (A2 §5 H04; A7 §5): 1 okunamadı,
## 2 geçersiz (SEMA, MANIFEST_BICIM, EKSEN, özet uyuşmazlığı), 3 varlık eksik, 4 özet ya da
## boyut, 5 varlık kuralı (BIRIM, GLTF, UZANTI, DUGUM, ERISILEMEZ, MALZEME).

const GltfIndeks := preload("res://betikler/gltf_indeks.gd")
const GLTF_GODOT := Basis(Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, -1))  # A7-K14: C·M
const IZINLI_UZANTILAR := ["KHR_lights_punctual", "KHR_materials_emissive_strength", "KHR_texture_transform"]
const GOSTERGE_KATMANI := 1 << 10
const AGLI_ROLLER := ["gorsel", "yuzerlik", "carpisma", "akustik", "isabet_bolgeleri"]
const AGSIZ_ROLLER := ["referans", "baglanti_noktalari"]

var manifest_ozeti := ""
var onbellek_anahtari := ""
var varliklar: Array = []           # indeks → {sablon, kosul1, kosul2, t_ref, malzemeler}
var malzeme_kimlikleri: Array = []  # manifest malzemeler[] sırası
var son_onbellekten := false
var son_neden := ""
# manifest malzeme indeksi → [[StandardMaterial3D, renk, metal, pürüz], ...]
var _malzeme_hedefleri := {}

static var _kayitli := false


func _init() -> void:
	if not _kayitli:
		GLTFDocument.register_gltf_document_extension(GltfIndeks.new(), true)
		_kayitli = true


static func sha256(b: PackedByteArray) -> PackedByteArray:
	var h := HashingContext.new()
	h.start(HashingContext.HASH_SHA256)
	h.update(b)
	return h.finish()


func temizle() -> void:
	for v in varliklar:
		if v.has("sablon") and is_instance_valid(v["sablon"]):
			v["sablon"].free()
	varliklar.clear()
	_malzeme_hedefleri.clear()
	manifest_ozeti = ""
	onbellek_anahtari = ""


func _hata(kod: int, neden: String) -> int:
	son_neden = neden
	push_error("ida-viz: A7: %s" % neden)
	return kod


## yol: manifest dosyası. beklenen_ozet_hex: hosgeldin.manifest_sha256 (boşsa karşılaştırılmaz).
## Döner: [hazir.durum, manifest_ozeti (PackedByteArray)].
func yukle(yol: String, beklenen_ozet_hex: String = "", sema_yolu: String = "") -> Array:
	son_onbellekten = false
	son_neden = ""
	var bos := PackedByteArray()
	if not FileAccess.file_exists(yol):
		return [_hata(1, "manifest okunamadı: " + yol), bos]
	var baytlar := FileAccess.get_file_as_bytes(yol)
	if baytlar.is_empty():
		return [_hata(1, "manifest boş ya da okunamadı: " + yol), bos]
	var ozet := sha256(baytlar)
	var ozet_hex := ozet.hex_encode()
	# sol #8 (A2-K13/K14, A7-K53): özetler eşleşmeden hiçbir varlık yüklenmez, sahne kurulmaz.
	if beklenen_ozet_hex != "" and ozet_hex != beklenen_ozet_hex.to_lower():
		return [_hata(2, "manifest özeti hosgeldin'dekiyle aynı değil (%s ≠ %s)" % [ozet_hex, beklenen_ozet_hex]), ozet]
	# A7-K04: BOM ve CR yok
	if (baytlar.size() >= 3 and baytlar[0] == 0xEF and baytlar[1] == 0xBB and baytlar[2] == 0xBF) or baytlar.find(0x0D) >= 0:
		return [_hata(2, "MANIFEST_BICIM: BOM ya da CR"), ozet]
	var metin := baytlar.get_string_from_utf8()
	if metin.to_utf8_buffer() != baytlar:
		return [_hata(2, "MANIFEST_BICIM: geçersiz UTF-8"), ozet]
	var js := JSON.new()
	if js.parse(metin) != OK or typeof(js.data) != TYPE_DICTIONARY:
		return [_hata(2, "SEMA: JSON değil"), ozet]
	var m: Dictionary = js.data
	var dizin := yol.get_base_dir()
	# A7 şeması (sözleşmenin parçası): şemanın tamamı uygulanır.
	if sema_yolu == "":
		sema_yolu = dizin.get_base_dir().path_join("manifest.schema.json")
	var sj := JSON.new()
	if not FileAccess.file_exists(sema_yolu) or sj.parse(FileAccess.get_file_as_string(sema_yolu)) != OK:
		return [_hata(2, "A7 şeması okunamadı: " + sema_yolu), ozet]
	var sh := _sema(m, sj.data, sj.data, "")
	if sh != "":
		# §3.2: birimler alanı BIRIM; görselleştiricide varlik_kurali (A7 §5)
		return [_hata(5 if sh.begins_with("/birimler") else 2, "SEMA: " + sh), ozet]
	if str(m["kume_kimligi"]) != dizin.get_file():
		return [_hata(2, "SEMA: küme kimliği dizin adıyla aynı değil"), ozet]
	var kimlikler := {}
	malzeme_kimlikleri.clear()
	for mz in m["malzemeler"]:
		if kimlikler.has(mz["kimlik"]):
			return [_hata(2, "SEMA: malzeme kimliği tekil değil"), ozet]
		kimlikler[mz["kimlik"]] = true
		malzeme_kimlikleri.append(str(mz["kimlik"]))
	var vk := {}
	var vd := {}
	for g in m["varliklar"]:
		if vk.has(g["kimlik"]) or vd.has(g["dosya"]):
			return [_hata(2, "SEMA: varlık kimliği ya da dosya adı tekil değil"), ozet]
		vk[g["kimlik"]] = true
		vd[g["dosya"]] = true
	# A7-K06: her oturumda, önbellek isabetinde de (sol #9).
	var glb_baytlari := []
	for g in m["varliklar"]:
		var gy := dizin.path_join(str(g["dosya"]))
		if not FileAccess.file_exists(gy):
			return [_hata(3, "EKSIK_VARLIK: " + gy), ozet]
		var gb := FileAccess.get_file_as_bytes(gy)
		if gb.size() != int(g["bayt"]) or sha256(gb).hex_encode() != str(g["sha256"]):
			return [_hata(4, "OZET_UYUSMAZ: " + gy), ozet]
		glb_baytlari.append(gb)
	var anahtar := ozet_hex + "|" + ProjectSettings.globalize_path(dizin).simplify_path()
	if anahtar == onbellek_anahtari and not varliklar.is_empty():
		son_onbellekten = true
		return [0, ozet]
	temizle()
	for i in m["varliklar"].size():
		var r = _varlik_yukle(dizin, m["varliklar"][i], glb_baytlari[i], m)
		if typeof(r) == TYPE_INT:
			temizle()
			return [r, ozet]
		varliklar.append(r)
	manifest_ozeti = ozet_hex
	onbellek_anahtari = anahtar
	return [0, ozet]


# ------------------------------------------------------------------------------------
# A7 şeması: manifest.schema.json'daki anahtar sözcüklerin alt kümesi (bu şemanın kullandığı
# hepsi): $ref, allOf, anyOf, not, if/then/else, const, enum, type, required, properties,
# additionalProperties, items, min/maxItems, min/maxLength, pattern, minimum, maximum,
# exclusiveMinimum. Döner: boş (geçerli) ya da ilk hatanın yolu ve nedeni.
# ------------------------------------------------------------------------------------

func _sema(d, s, kok: Dictionary, yol: String) -> String:
	if typeof(s) == TYPE_BOOL:
		return "" if s else yol + ": şema false"
	if s.has("$ref"):
		var ref: String = s["$ref"]
		var hedef = kok
		for parca in ref.trim_prefix("#/").split("/"):
			hedef = hedef[parca]
		var e := _sema(d, hedef, kok, yol)
		if e != "":
			return e
	for alt in s.get("allOf", []):
		var e := _sema(d, alt, kok, yol)
		if e != "":
			return e
	if s.has("anyOf"):
		var biri := false
		for alt in s["anyOf"]:
			if _sema(d, alt, kok, yol) == "":
				biri = true
				break
		if not biri:
			return yol + ": anyOf"
	if s.has("not") and _sema(d, s["not"], kok, yol) == "":
		return yol + ": not"
	if s.has("if"):
		if _sema(d, s["if"], kok, yol) == "":
			if s.has("then"):
				var e := _sema(d, s["then"], kok, yol)
				if e != "":
					return e
		elif s.has("else"):
			var e := _sema(d, s["else"], kok, yol)
			if e != "":
				return e
	if s.has("const") and not _json_esit(d, s["const"]):
		return yol + ": sabit değer değil"
	if s.has("enum"):
		var var_mi := false
		for x in s["enum"]:
			if _json_esit(d, x):
				var_mi = true
		if not var_mi:
			return yol + ": izinli değerlerden değil"
	if s.has("type"):
		var turler = s["type"] if typeof(s["type"]) == TYPE_ARRAY else [s["type"]]
		var uyar := false
		for t in turler:
			if _tur_uyar(d, str(t)):
				uyar = true
		if not uyar:
			return yol + ": tür %s değil" % str(turler)
	if typeof(d) == TYPE_DICTIONARY:
		for r in s.get("required", []):
			if not d.has(r):
				return yol + "/" + str(r) + ": zorunlu"
		var ozellikler: Dictionary = s.get("properties", {})
		for k in d.keys():
			if ozellikler.has(k):
				var e := _sema(d[k], ozellikler[k], kok, yol + "/" + str(k))
				if e != "":
					return e
			elif s.has("additionalProperties"):
				var ek = s["additionalProperties"]
				var e := _sema(d[k], ek, kok, yol + "/" + str(k))
				if e != "":
					return yol + "/" + str(k) + ": izinsiz alan" if typeof(ek) == TYPE_BOOL else e
	if typeof(d) == TYPE_ARRAY:
		if s.has("minItems") and d.size() < int(s["minItems"]):
			return yol + ": çok az öğe"
		if s.has("maxItems") and d.size() > int(s["maxItems"]):
			return yol + ": çok fazla öğe"
		if s.has("items"):
			for i in d.size():
				var e := _sema(d[i], s["items"], kok, yol + "/" + str(i))
				if e != "":
					return e
	if typeof(d) == TYPE_STRING:
		if s.has("minLength") and d.length() < int(s["minLength"]):
			return yol + ": çok kısa"
		if s.has("maxLength") and d.length() > int(s["maxLength"]):
			return yol + ": çok uzun"
		if s.has("pattern"):
			var re := RegEx.new()
			re.compile(str(s["pattern"]))
			if re.search(d) == null:
				return yol + ": desene uymuyor"
	if typeof(d) == TYPE_INT or typeof(d) == TYPE_FLOAT:
		var x := float(d)
		if s.has("minimum") and x < float(s["minimum"]):
			return yol + ": alt sınırın altında"
		if s.has("maximum") and x > float(s["maximum"]):
			return yol + ": üst sınırın üstünde"
		if s.has("exclusiveMinimum") and x <= float(s["exclusiveMinimum"]):
			return yol + ": alt sınıra eşit ya da altında"
	return ""


func _tur_uyar(d, t: String) -> bool:
	match t:
		"object": return typeof(d) == TYPE_DICTIONARY
		"array": return typeof(d) == TYPE_ARRAY
		"string": return typeof(d) == TYPE_STRING
		"boolean": return typeof(d) == TYPE_BOOL
		"null": return typeof(d) == TYPE_NIL
		"number": return typeof(d) == TYPE_INT or typeof(d) == TYPE_FLOAT
		"integer": return (typeof(d) == TYPE_INT or typeof(d) == TYPE_FLOAT) and float(d) == floorf(float(d))
	return false


func _json_esit(a, b) -> bool:
	var sayi_a := typeof(a) == TYPE_INT or typeof(a) == TYPE_FLOAT
	var sayi_b := typeof(b) == TYPE_INT or typeof(b) == TYPE_FLOAT
	if sayi_a or sayi_b:
		return sayi_a and sayi_b and float(a) == float(b)
	if typeof(a) != typeof(b):
		return false
	if typeof(a) == TYPE_ARRAY:
		if a.size() != b.size():
			return false
		for i in a.size():
			if not _json_esit(a[i], b[i]):
				return false
		return true
	if typeof(a) == TYPE_DICTIONARY:
		if a.size() != b.size():
			return false
		for k in a.keys():
			if not b.has(k) or not _json_esit(a[k], b[k]):
				return false
		return true
	return a == b


# ------------------------------------------------------------------------------------
# GLB doğrulaması (A7-K10–K13, K19–K21, K23, K28): Godot içe aktarıcısına vermeden önce
# ------------------------------------------------------------------------------------

func _glb_parcala(b: PackedByteArray) -> Dictionary:
	# A7-K10: başlık "glTF", sürüm 2, toplam uzunluk dosya boyu; ilk parça JSON, varsa ikinci BIN.
	if b.size() < 20 or b.decode_u32(0) != 0x46546C67 or b.decode_u32(4) != 2 or b.decode_u32(8) != b.size():
		return {}
	var n := b.decode_u32(12)
	if b.decode_u32(16) != 0x4E4F534A or 20 + n > b.size():
		return {}
	var js := JSON.new()
	if js.parse(b.slice(20, 20 + n).get_string_from_utf8()) != OK or typeof(js.data) != TYPE_DICTIONARY:
		return {}
	var bin := PackedByteArray()
	var son := 20 + n
	if son < b.size():
		if son + 8 > b.size() or b.decode_u32(son + 4) != 0x004E4942:
			return {}
		var bn := b.decode_u32(son)
		if son + 8 + bn != b.size():
			return {}
		bin = b.slice(son + 8, son + 8 + bn)
	return {"json": js.data, "bin": bin}


func _dugum_matrisi(d: Dictionary) -> Transform3D:
	if d.has("matrix"):
		var a: Array = d["matrix"]
		return Transform3D(Vector3(a[0], a[1], a[2]), Vector3(a[4], a[5], a[6]), Vector3(a[8], a[9], a[10]),
			Vector3(a[12], a[13], a[14]))
	var t := Transform3D.IDENTITY
	if d.has("rotation"):
		var r: Array = d["rotation"]
		t.basis = Basis(Quaternion(r[0], r[1], r[2], r[3]).normalized())
	if d.has("scale"):
		var s: Array = d["scale"]
		t.basis = t.basis * Basis.from_scale(Vector3(s[0], s[1], s[2]))
	if d.has("translation"):
		var o: Array = d["translation"]
		t.origin = Vector3(o[0], o[1], o[2])
	return t


func _varlik_dogrula(glb: Dictionary, g: Dictionary, m: Dictionary) -> String:
	var j: Dictionary = glb["json"]
	var bin: PackedByteArray = glb["bin"]
	# A7-K10
	if str(j.get("asset", {}).get("version", "")) != "2.0":
		return "GLTF: asset.version 2.0 değil"
	var buffers: Array = j.get("buffers", [])
	if buffers.size() > 1:
		return "GLTF: birden çok tampon (dış dosya gerekir)"
	for bf in buffers:
		if bf.has("uri"):
			return "GLTF: dışa başvuran URI (tampon)"
		if int(bf.get("byteLength", 0)) > bin.size():
			return "GLTF: tampon BIN parçasından büyük"
	for im in j.get("images", []):
		if im.has("uri") or not im.has("bufferView"):
			return "GLTF: dışa başvuran URI (görüntü)"
	# A7-K11
	if j.has("extensionsRequired") and not (j["extensionsRequired"] as Array).is_empty():
		return "UZANTI: extensionsRequired dolu"
	for u in j.get("extensionsUsed", []):
		if not IZINLI_UZANTILAR.has(u):
			return "UZANTI: " + str(u)
	# A7-K12
	for alan in ["skins", "animations", "cameras"]:
		if j.has(alan) and not (j[alan] as Array).is_empty():
			return "GLTF: %s bulunmamalı" % alan
	var erisimciler: Array = j.get("accessors", [])
	for e in erisimciler:
		if e.has("sparse"):
			return "GLTF: seyrek erişimci"
	for ag in j.get("meshes", []):
		for p in ag.get("primitives", []):
			if int(p.get("mode", 4)) != 4:
				return "GLTF: üçgen listesi olmayan temel öğe"
			if p.has("targets"):
				return "GLTF: biçim hedefi"
			var pa = p.get("attributes", {}).get("POSITION", -1)
			if int(pa) < 0 or int(pa) >= erisimciler.size():
				return "GLTF: POSITION yok"
			var pe: Dictionary = erisimciler[int(pa)]
			if int(pe.get("componentType", 0)) != 5126 or str(pe.get("type", "")) != "VEC3":
				return "GLTF: POSITION float32 VEC3 değil"
	# A7-K13
	var sahneler: Array = j.get("scenes", [])
	if sahneler.size() != 1 or not j.has("scene") or int(j["scene"]) != 0:
		return "GLTF: tek sahne ve scene = 0 değil"
	var dugumler: Array = j.get("nodes", [])
	for i in dugumler.size():
		var d: Dictionary = dugumler[i]
		if d.has("skin") or d.has("camera"):
			return "GLTF: düğümde deri ya da kamera"
		if d.has("scale"):
			for s in d["scale"]:
				if absf(float(s) - 1.0) > 1e-6:
					return "BIRIM: düğüm %d ölçekli" % i
		if d.has("matrix"):
			var t := _dugum_matrisi(d)
			var bz := t.basis
			if not bz.is_conformal() or absf(bz.determinant() - 1.0) > 1e-6 \
					or absf(bz.x.length() - 1.0) > 1e-6 or absf(bz.y.length() - 1.0) > 1e-6 or absf(bz.z.length() - 1.0) > 1e-6:
				return "BIRIM: düğüm %d matrisi birim ölçekli dönüş değil" % i
	# A7-K19: ağaç, tek ebeveyn, döngüsüz, erişilebilir
	var ebeveyn := {}
	for i in dugumler.size():
		for c in dugumler[i].get("children", []):
			if ebeveyn.has(int(c)) or int(c) < 0 or int(c) >= dugumler.size():
				return "ERISILEMEZ: düğüm %d birden çok ebeveynli ya da geçersiz" % int(c)
			ebeveyn[int(c)] = i
	var kokler: Array = sahneler[0].get("nodes", [])
	var erisilen := {}
	var yigin: Array = []
	for r in kokler:
		if ebeveyn.has(int(r)):
			return "ERISILEMEZ: kök düğümün ebeveyni var"
		yigin.append(int(r))
	while not yigin.is_empty():
		var n: int = yigin.pop_back()
		if erisilen.has(n):
			return "ERISILEMEZ: düğüm hiyerarşisinde döngü"
		erisilen[n] = true
		for c in dugumler[n].get("children", []):
			yigin.append(int(c))
	# A7-K20 ve K21: rol tablosu
	var rol := {}
	var ref := int(g["referans"]["dugum"])
	var roller := [[ref, "REF", "referans"]]
	for anahtar in AGLI_ROLLER + ["baglanti_noktalari"]:
		for e in g.get(anahtar, []):
			roller.append([int(e["dugum"]), str(e["ad"]), anahtar])
	for r in roller:
		if r[0] < 0 or r[0] >= dugumler.size():
			return "DUGUM: düğüm %d yok" % r[0]
		if rol.has(r[0]):
			return "DUGUM: düğüm %d birden çok rolde" % r[0]
		rol[r[0]] = r[2]
		if str(dugumler[r[0]].get("name", "")) != r[1]:
			return "DUGUM: düğüm %d adı '%s' değil" % [r[0], r[1]]
		var agli: bool = dugumler[r[0]].has("mesh")
		if r[2] in AGLI_ROLLER and not agli:
			return "DUGUM: %s rolündeki düğüm %d ağ taşımıyor" % [r[2], r[0]]
		if r[2] in AGSIZ_ROLLER and agli:
			return "DUGUM: %s rolündeki düğüm %d ağ taşıyor" % [r[2], r[0]]
		if not erisilen.has(r[0]):
			return "ERISILEMEZ: rol düğümü %d sahneden erişilemiyor" % r[0]
	var ref_sayisi := 0
	for i in dugumler.size():
		if str(dugumler[i].get("name", "")) == "REF":
			ref_sayisi += 1
		if dugumler[i].has("mesh"):
			if not rol.has(i):
				return "DUGUM: ağ taşıyan düğüm %d hiçbir rolde değil" % i
			if not erisilen.has(i):
				return "ERISILEMEZ: ağ taşıyan düğüm %d erişilemiyor" % i
	var rd: Dictionary = dugumler[ref]
	if ref_sayisi != 1 or not kokler.has(float(ref)) and not kokler.has(ref):
		return "DUGUM: tek REF ve sahne kökü değil"
	if not _dugum_matrisi(rd).basis.is_equal_approx(Basis.IDENTITY):
		return "DUGUM: REF dönüşü birim değil"
	# A7-K23: görsel ve çarpışma köşelerinin gövde çerçevesindeki sınır kutusu, 1 mm
	var dunya := {}
	for i in dugumler.size():
		var t := _dugum_matrisi(dugumler[i])
		var p: int = ebeveyn.get(i, -1)
		while p >= 0:
			t = _dugum_matrisi(dugumler[p]) * t
			p = ebeveyn.get(p, -1)
		dunya[i] = t
	var t_ref: Vector3 = dunya[ref].origin
	var mn := Vector3(INF, INF, INF)
	var mx := Vector3(-INF, -INF, -INF)
	for anahtar in ["gorsel", "carpisma"]:
		for e in g.get(anahtar, []):
			var i := int(e["dugum"])
			var ag: Dictionary = j["meshes"][int(dugumler[i]["mesh"])]
			for p in ag["primitives"]:
				var ee: Dictionary = erisimciler[int(p["attributes"]["POSITION"])]
				var bv: Dictionary = j["bufferViews"][int(ee["bufferView"])]
				var adim := int(bv.get("byteStride", 12))
				var bas := int(bv.get("byteOffset", 0)) + int(ee.get("byteOffset", 0))
				var sayi := int(ee["count"])
				if bas + (sayi - 1) * adim + 12 > bin.size():
					return "GLTF: erişimci BIN dışında"
				for k in sayi:
					var o := bas + k * adim
					var pg: Vector3 = dunya[i] * Vector3(bin.decode_float(o), bin.decode_float(o + 4), bin.decode_float(o + 8))
					var q := pg - t_ref
					var pb := Vector3(q.z, -q.x, -q.y)  # M · (p − t_REF)
					mn = mn.min(pb)
					mx = mx.max(pb)
	var sk: Dictionary = g["sinir_kutusu_govde_m"]
	for a in 3:
		if absf(mn[a] - float(sk["en_kucuk"][a])) > 1e-3 or absf(mx[a] - float(sk["en_buyuk"][a])) > 1e-3:
			return "BIRIM: sınır kutusu manifestten 1 mm'den fazla farklı (%s … %s)" % [mn, mx]
	# A7-K28: görsel, akustik ve çarpışma ağlarının her temel öğesinin malzemesi atanmış
	var atama := {}
	for at in g["malzeme_atamasi"]:
		var gi := int(at["gltf_malzeme"])
		atama[gi] = atama.get(gi, 0) + 1
		if not malzeme_kimlikleri.has(str(at["malzeme"])):
			return "MALZEME: bilinmeyen malzeme kimliği " + str(at["malzeme"])
	for anahtar in ["gorsel", "akustik", "carpisma"]:
		for e in g.get(anahtar, []):
			var ag: Dictionary = j["meshes"][int(dugumler[int(e["dugum"])]["mesh"])]
			for p in ag["primitives"]:
				if not p.has("material"):
					return "MALZEME: malzemesiz temel öğe (düğüm %d)" % int(e["dugum"])
				if atama.get(int(p["material"]), 0) != 1:
					return "MALZEME: glTF malzemesi %d atamada tam bir kez yok" % int(p["material"])
	return ""


func _varlik_yukle(dizin: String, g: Dictionary, baytlar: PackedByteArray, m: Dictionary):
	var glb := _glb_parcala(baytlar)
	if glb.is_empty():
		return _hata(5, "GLTF: GLB 2 düzeni bozuk (%s)" % g["dosya"])
	var neden := _varlik_dogrula(glb, g, m)
	if neden != "":
		return _hata(5, "%s (%s)" % [neden, g["dosya"]])
	# Doğrulamadan geçen dosya içe aktarılır (dış başvuru yok; base_path kullanılmaz).
	var belge := GLTFDocument.new()
	var durum := GLTFState.new()
	if belge.append_from_buffer(baytlar, "", durum) != OK:
		return _hata(5, "GLTF: Godot içe aktaramadı (%s)" % g["dosya"])
	var kok: Node = belge.generate_scene(durum)
	if kok == null or not (kok is Node3D):
		return _hata(5, "GLTF: sahne üretilemedi (%s)" % g["dosya"])
	var yollar: Dictionary = durum.get_additional_data(GltfIndeks.ANAHTAR)
	var j: Dictionary = glb["json"]
	var ref := int(g["referans"]["dugum"])
	var rd: Dictionary = j["nodes"][ref]
	var sonuc := {"sablon": kok, "kosul1": [], "kosul2": [], "t_ref": _dugum_matrisi(rd).origin}
	var tum := [[ref, "REF", "referans", ""]]
	for e in g.get("gorsel", []):
		tum.append([int(e["dugum"]), str(e["ad"]), "gorsel", str(e["kosul"])])
	for anahtar in ["yuzerlik", "carpisma", "akustik", "isabet_bolgeleri", "baglanti_noktalari"]:
		for e in g.get(anahtar, []):
			tum.append([int(e["dugum"]), str(e["ad"]), anahtar, ""])
	for r in tum:  # A7-K22: indeksle (gltf_indeks.gd), ada göre aranmaz
		var n: Node = kok.get_node_or_null(yollar[r[0]]) if yollar.has(r[0]) else null
		if n == null:
			kok.free()
			return _hata(5, "ERISILEMEZ: düğüm %d (%s) için sahne düğümü yok" % [r[0], r[1]])
		if r[2] == "gorsel":
			if r[3] == "tup_1_dolu":
				sonuc["kosul1"].append(yollar[r[0]])
			elif r[3] == "tup_2_dolu":
				sonuc["kosul2"].append(yollar[r[0]])
		elif r[2] in ["yuzerlik", "carpisma", "akustik", "isabet_bolgeleri"]:
			# A7-K54: yalnız hata ayıklama katmanında; varsayılan gizli.
			if n is VisualInstance3D:
				(n as VisualInstance3D).layers = GOSTERGE_KATMANI
			(n as Node3D).visible = false
	# A7-K30 için: manifest malzeme indeksi → bu varlığın glTF malzemeleri
	var gm: Array = durum.get_materials()
	for at in g["malzeme_atamasi"]:
		var gi := int(at["gltf_malzeme"])
		var mi := malzeme_kimlikleri.find(str(at["malzeme"]))
		if gi < gm.size() and gm[gi] is StandardMaterial3D:
			var sm := gm[gi] as StandardMaterial3D
			if not _malzeme_hedefleri.has(mi):
				_malzeme_hedefleri[mi] = []
			_malzeme_hedefleri[mi].append([sm, sm.albedo_color, sm.metallic, sm.roughness])
	_mipmap(kok)
	return sonuc


func _mipmap(n: Node) -> void:
	# glTF'ten çalışma zamanında yüklenen dokuların mipmap'i yok: üret (eğik bakışta titreme olmasın).
	if n is MeshInstance3D and (n as MeshInstance3D).mesh:
		var ag: Mesh = (n as MeshInstance3D).mesh
		for i in ag.get_surface_count():
			var mat := ag.surface_get_material(i) as StandardMaterial3D
			if mat == null or mat.albedo_texture == null:
				continue
			var img := mat.albedo_texture.get_image()
			if img and not img.has_mipmaps():
				img.generate_mipmaps()
				mat.albedo_texture = ImageTexture.create_from_image(img)
			mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	for c in n.get_children():
		_mipmap(c)


## Gövde örneği: sarmalayıcı düğüm A7-K20/K14 dönüşümünü (C·M·(p − t_REF)) taşır; içe
## aktarılan kökün kendi dönüşümü korunur (sol #6).
func ornek(varlik: int) -> Dictionary:
	var v: Dictionary = varliklar[varlik]
	var sarmalayici := Node3D.new()
	sarmalayici.name = "glTF"
	sarmalayici.transform = Transform3D(GLTF_GODOT, -(GLTF_GODOT * (v["t_ref"] as Vector3)))
	var model: Node3D = (v["sablon"] as Node3D).duplicate()
	sarmalayici.add_child(model)
	var k1 := []
	var k2 := []
	for p in v["kosul1"]:
		k1.append(model.get_node(p))
	for p in v["kosul2"]:
		k2.append(model.get_node(p))
	return {"model": sarmalayici, "kosullu": {1: k1, 2: k2}}


## A7-K30: A2 malzeme_optik'in etkin değerleri (maskeli alanlar) glTF malzemelerine uygulanır;
## maskede olmayan alan glTF'in kendi değerine döner. Geçersiz indeks: hata metni (hiçbir şey
## değişmez). girdiler: [{malzeme_indeksi, maske, taban_rengi[4], metaliklik, puruzluluk}]
func malzeme_uygula(girdiler: Array) -> String:
	for gd in girdiler:
		var i := int(gd["malzeme_indeksi"])
		if i < 0 or i >= malzeme_kimlikleri.size():
			return "malzeme_optik: malzeme indeksi %d manifest dışında" % i
	for mi in _malzeme_hedefleri:
		for h in _malzeme_hedefleri[mi]:
			var sm: StandardMaterial3D = h[0]
			sm.albedo_color = h[1]
			sm.metallic = h[2]
			sm.roughness = h[3]
	for gd in girdiler:
		var maske := int(gd["maske"])
		for h in _malzeme_hedefleri.get(int(gd["malzeme_indeksi"]), []):
			var sm: StandardMaterial3D = h[0]
			if maske & 1:
				var c: Array = gd["taban_rengi"]
				sm.albedo_color = Color(c[0], c[1], c[2], c[3]).linear_to_srgb()  # glTF içe aktarımıyla aynı
			if maske & 2:
				sm.metallic = float(gd["metaliklik"])
			if maske & 4:
				sm.roughness = float(gd["puruzluluk"])
	return ""
