// GDExtension giriş noktası (viz/proje/ida_viz.gdextension: entry_symbol = ida_viz_baslat).
#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "a2_istemci.hpp"

namespace {

void baslat(godot::ModuleInitializationLevel seviye) {
  if (seviye != godot::MODULE_INITIALIZATION_LEVEL_SCENE) return;
  GDREGISTER_CLASS(ida::viz::IdaA2Istemci);
}

void bitir(godot::ModuleInitializationLevel) {}

}  // namespace

extern "C" {
GDExtensionBool GDE_EXPORT ida_viz_baslat(GDExtensionInterfaceGetProcAddress adres_al, GDExtensionClassLibraryPtr kitaplik,
                                          GDExtensionInitialization* baslatma) {
  godot::GDExtensionBinding::InitObject n(adres_al, kitaplik, baslatma);
  n.register_initializer(baslat);
  n.register_terminator(bitir);
  n.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
  return n.init();
}
}
