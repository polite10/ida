extends Node3D
## Sahne içi göstergeler (07 §2.2 `Göstergeler`): gövde eksenleri ve iz çizgisi.
## Hepsi ayrı görünürlük katmanında (KATMAN); ileride kamera sensörünün cull_mask'i bu
## katmanı içermeyecek (A2-K37). Yalnız operatör görünümündedir.

const KATMAN := 1 << 10   # görünürlük katmanı 11
const IZ_ARALIGI := 0.1   # s (duvar)
const IZ_AZAMI := 1500

var eksenler_acik := false
var iz_acik := true
var _eksenler: Array[MeshInstance3D] = []
var _iz_noktalari := PackedVector3Array()
var _iz_mesh: ImmediateMesh
var _iz: MeshInstance3D
var _iz_sayac := 0.0


func _ready() -> void:
	_iz_mesh = ImmediateMesh.new()
	_iz = MeshInstance3D.new()
	_iz.name = "İz"
	_iz.mesh = _iz_mesh
	_iz.layers = KATMAN
	_iz.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.vertex_color_use_as_albedo = true
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.no_depth_test = false
	m.render_priority = 120  # su altı son işleminden (100) sonra: gösterge soğurulmaz
	_iz.material_override = m
	_iz.custom_aabb = AABB(Vector3(-5000, -500, -5000), Vector3(10000, 1000, 10000))
	add_child(_iz)


## Bir gövde düğümüne eksen göstergesi ekler (Godot yerelinde gövde eksenleri: x_b → −Z,
## y_b → +X, z_b → −Y; A2 §2 C matrisi).
func eksen_ekle(govde: Node3D, uzunluk: float = 0.35) -> void:
	var im := ImmediateMesh.new()
	im.surface_begin(Mesh.PRIMITIVE_LINES)
	var eks := [[Vector3(0, 0, -1), Color(1.0, 0.25, 0.2)], [Vector3(1, 0, 0), Color(0.3, 1.0, 0.3)],
		[Vector3(0, -1, 0), Color(0.35, 0.55, 1.0)]]
	for e in eks:
		im.surface_set_color(e[1])
		im.surface_add_vertex(Vector3.ZERO)
		im.surface_set_color(e[1])
		im.surface_add_vertex(e[0] * uzunluk)
	im.surface_end()
	var mi := MeshInstance3D.new()
	mi.name = "Eksenler"
	mi.mesh = im
	mi.layers = KATMAN
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.vertex_color_use_as_albedo = true
	m.no_depth_test = true
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.render_priority = 121
	mi.material_override = m
	mi.visible = eksenler_acik
	govde.add_child(mi)
	_eksenler.append(mi)


func eksenleri_ac(acik: bool) -> void:
	eksenler_acik = acik
	_eksenler = _eksenler.filter(func(e): return is_instance_valid(e))
	for e in _eksenler:
		e.visible = acik


func izi_ac(acik: bool) -> void:
	iz_acik = acik
	_iz.visible = acik


func izi_temizle() -> void:
	_iz_noktalari.clear()
	_iz_mesh.clear_surfaces()


func iz_ekle(konum: Vector3, delta: float) -> void:
	_iz_sayac += delta
	if _iz_sayac < IZ_ARALIGI:
		return
	_iz_sayac = 0.0
	if _iz_noktalari.size() > 0 and _iz_noktalari[_iz_noktalari.size() - 1].distance_to(konum) < 0.01:
		return
	_iz_noktalari.append(konum)
	if _iz_noktalari.size() > IZ_AZAMI:
		_iz_noktalari = _iz_noktalari.slice(_iz_noktalari.size() - IZ_AZAMI)
	_iz_mesh.clear_surfaces()
	if _iz_noktalari.size() < 2:
		return
	_iz_mesh.surface_begin(Mesh.PRIMITIVE_LINE_STRIP)
	var n := _iz_noktalari.size()
	for i in n:
		var a := float(i) / float(n - 1)
		_iz_mesh.surface_set_color(Color(0.2, 0.95, 1.0, 0.15 + 0.75 * a))
		_iz_mesh.surface_add_vertex(_iz_noktalari[i])
	_iz_mesh.surface_end()
