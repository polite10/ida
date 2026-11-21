extends GLTFDocumentExtension
## A7-K22: düğümler manifestteki glTF indeksiyle, GLTFState.get_scene_node(indeks) ile bulunur.
##
## Godot 4.7.2'de çalışma zamanı generate_scene(), sahne kurulduktan sonra yerleşik
## "ImporterMeshInstance3D → MeshInstance3D" dönüştürme uzantısını (import_post) çalıştırır;
## ağ taşıyan düğümlerin eski nesneleri silinir ve get_scene_node() bunlar için artık geçerli
## bir düğüm döndürmez (bu iş sırasında gözlendi; modules/gltf/gltf_document.cpp,
## GLTFDocument::generate_scene). Bu uzantı en önce kaydedilir ve dönüşümden önce, her glTF
## indeksi için get_scene_node() sonucunun kökten yolunu GLTFState'e yazar. Dönüştürme düğümü
## aynı ad ve yerle değiştirdiği için yol geçerli kalır. Ada göre arama yapılmaz.

const ANAHTAR := &"ida_dugum_yollari"


func _import_preflight(_state: GLTFState, _extensions: PackedStringArray) -> Error:
	return OK


func _import_post(state: GLTFState, root: Node) -> Error:
	var yollar := {}
	var n := state.get_nodes().size()
	for i in n:
		var d := state.get_scene_node(i)
		if d != null and is_instance_valid(d):
			yollar[i] = root.get_path_to(d)
	state.set_additional_data(ANAHTAR, yollar)
	return OK
