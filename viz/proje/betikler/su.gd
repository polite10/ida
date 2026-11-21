extends Node3D
## Su (07 §2.2 `Su`): üst yüzey, alt yüzey ve kademe.
##
## Tam: kameranın çevresinde yoğun, dışa doğru seyrekleşen ızgara (yakın halkada 0,12 m;
## 07 §2.7: Δx ≤ λ_min/16, en kısa dalga 1,6 m), dalga geometrisi gölgelendiricide.
## Sade: tek düz düzlem (A2 §6), ekran/derinlik dokusu okumayan hafif gölgelendirici.
## Ağ, ince aralığın katlarıyla kamerayı izler; böylece yakın köşeler dünyada aynı noktalara
## düşer (yüzey "yüzmez").

const INCE := 0.12          # yakın ağ aralığı (m)
const INCE_YARICAP := 80    # ince halkanın adım sayısı (≈ 9,6 m)
const ADIM_SAYISI := 180    # merkezden kenara adım
const BUYUME := 1.075       # ince halkanın dışında adım başına büyüme

var ust: MeshInstance3D
var alt: MeshInstance3D
var ust_malzeme: ShaderMaterial
var alt_malzeme: ShaderMaterial
var kademe := 0
var _baslangic_ms := 0


func kur(a2: IdaA2Istemci, yeni_kademe: int) -> void:
	if kademe == yeni_kademe and ust != null:
		return
	kademe = yeni_kademe
	for c in get_children():
		c.queue_free()
	ust = MeshInstance3D.new()
	ust.name = "Üst"
	alt = MeshInstance3D.new()
	alt.name = "Alt"
	ust.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	alt.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var ag: Mesh
	if kademe == 2:
		ag = _izgara()
		ust_malzeme = ShaderMaterial.new()
		ust_malzeme.shader = load("res://golgelendiriciler/su_ust_tam.gdshader")
		ust_malzeme.set_shader_parameter("ayrinti_1", _ayrinti_dokusu(7, 0.9))
		ust_malzeme.set_shader_parameter("ayrinti_2", _ayrinti_dokusu(13, 0.6))
	else:
		var p := PlaneMesh.new()
		p.size = Vector2(8000.0, 8000.0)
		ag = p
		ust_malzeme = ShaderMaterial.new()
		ust_malzeme.shader = load("res://golgelendiriciler/su_ust_sade.gdshader")
	alt_malzeme = ShaderMaterial.new()
	alt_malzeme.shader = load("res://golgelendiriciler/su_alt.gdshader")
	ust.mesh = ag
	alt.mesh = ag
	ust.material_override = ust_malzeme
	alt.material_override = alt_malzeme
	# Dalga yer değiştirmesi ağın sınır kutusunu aşar; kırpılmasın.
	ust.custom_aabb = AABB(Vector3(-5000, -20, -5000), Vector3(10000, 40, 10000))
	alt.custom_aabb = ust.custom_aabb
	alt.visible = false
	add_child(ust)
	add_child(alt)
	a2.su_malzemesi_ekle(alt_malzeme)
	if kademe == 2:
		a2.su_malzemesi_ekle(ust_malzeme)
	_baslangic_ms = Time.get_ticks_msec()


func optik(o: Dictionary, isik_olcegi: float, gok_ufuk: Color, gok_tepe: Color, gunes_g: Vector3) -> void:
	if ust_malzeme == null:
		return
	var bd := _v3(o.get("beta_d", [0.4, 0.08, 0.07]))
	var bb := _v3(o.get("beta_b", [0.3, 0.08, 0.06]))
	var bs := _v3(o.get("b_sonsuz", [0.01, 0.1, 0.16]))
	if kademe == 2:
		ust_malzeme.set_shader_parameter("beta_d", bd)
		ust_malzeme.set_shader_parameter("beta_b", bb)
		ust_malzeme.set_shader_parameter("b_sonsuz", bs)
		ust_malzeme.set_shader_parameter("isik_olcegi", isik_olcegi)
	else:
		ust_malzeme.set_shader_parameter("su_rengi", bs * isik_olcegi)
		ust_malzeme.set_shader_parameter("sonum", (bd.x + bd.y + bd.z) / 3.0)
	var gu := gok_ufuk.srgb_to_linear()
	var gt := gok_tepe.srgb_to_linear()
	alt_malzeme.set_shader_parameter("gok_ufuk", Vector3(gu.r, gu.g, gu.b))
	alt_malzeme.set_shader_parameter("gok_tepe", Vector3(gt.r, gt.g, gt.b))
	alt_malzeme.set_shader_parameter("gunes_yonu", gunes_g)
	alt_malzeme.set_shader_parameter("yansima_rengi", bs * isik_olcegi * 0.5)
	alt_malzeme.set_shader_parameter("parlaklik", isik_olcegi)


func kamerayi_izle(kamera: Vector3, su_altinda: bool) -> void:
	if ust == null:
		return
	var adim := INCE if kademe == 2 else 50.0
	var p := Vector3(snappedf(kamera.x, adim), 0.0, snappedf(kamera.z, adim))
	ust.position = p
	alt.position = p
	ust.visible = not su_altinda
	alt.visible = su_altinda
	if kademe == 2:
		ust_malzeme.set_shader_parameter("zaman", float(Time.get_ticks_msec() - _baslangic_ms) * 0.001)


static func _v3(a) -> Vector3:
	return Vector3(float(a[0]), float(a[1]), float(a[2]))


func _konum(i: int) -> float:
	var n := absi(i)
	var s: float
	if n <= INCE_YARICAP:
		s = INCE * n
	else:
		var k := n - INCE_YARICAP
		s = INCE * INCE_YARICAP + INCE * (pow(BUYUME, k) - 1.0) / (BUYUME - 1.0)
	return s if i >= 0 else -s


func _aralik(i: int) -> float:
	return absf(_konum(absi(i) + 1) - _konum(absi(i)))


func _izgara() -> ArrayMesh:
	var n := ADIM_SAYISI
	var genislik := 2 * n + 1
	var konum := PackedVector3Array()
	var uv2 := PackedVector2Array()
	var normal := PackedVector3Array()
	var indeks := PackedInt32Array()
	konum.resize(genislik * genislik)
	uv2.resize(genislik * genislik)
	normal.resize(genislik * genislik)
	var eksen := PackedFloat32Array()
	var aralik := PackedFloat32Array()
	eksen.resize(genislik)
	aralik.resize(genislik)
	for i in range(-n, n + 1):
		eksen[i + n] = _konum(i)
		aralik[i + n] = _aralik(i)
	for j in genislik:
		for i in genislik:
			var k := j * genislik + i
			konum[k] = Vector3(eksen[i], 0.0, eksen[j])
			uv2[k] = Vector2(maxf(aralik[i], aralik[j]), 0.0)
			normal[k] = Vector3.UP
	indeks.resize((genislik - 1) * (genislik - 1) * 6)
	var t := 0
	for j in genislik - 1:
		for i in genislik - 1:
			var a := j * genislik + i
			var b := a + 1
			var c := a + genislik
			var d := c + 1
			# Yukarıdan bakınca saat yönü (Godot ön yüz kabulü)
			indeks[t] = a
			indeks[t + 1] = b
			indeks[t + 2] = c
			indeks[t + 3] = b
			indeks[t + 4] = d
			indeks[t + 5] = c
			t += 6
	var diziler := []
	diziler.resize(Mesh.ARRAY_MAX)
	diziler[Mesh.ARRAY_VERTEX] = konum
	diziler[Mesh.ARRAY_NORMAL] = normal
	diziler[Mesh.ARRAY_TEX_UV2] = uv2
	diziler[Mesh.ARRAY_INDEX] = indeks
	var ag := ArrayMesh.new()
	ag.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, diziler)
	return ag


func _ayrinti_dokusu(tohum: int, frekans_olcegi: float) -> NoiseTexture2D:
	var g := FastNoiseLite.new()
	g.seed = tohum
	g.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	g.frequency = 0.018 * frekans_olcegi
	g.fractal_octaves = 4
	var d := NoiseTexture2D.new()
	d.width = 512
	d.height = 512
	d.seamless = true
	d.as_normal_map = true
	d.bump_strength = 6.0
	d.generate_mipmaps = true
	d.noise = g
	return d
