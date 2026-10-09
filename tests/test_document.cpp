#include "pstouch/document.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace pstouch;
int main(){
 Document d(2,2,"Test Document"); Image bottom(2,2,{0,0,255,255}); d.add_layer(Layer("Background",bottom));
 Image top(1,1,{255,0,0,128}); Layer l("Paint",top); l.x=0;l.y=0;d.add_layer(l);
 Image c=d.composite();assert(c.at(0,0).r==128&&c.at(0,0).b==127&&c.at(0,0).a==255);assert(c.at(1,1).b==255);
 d.mutable_layers()[1].visible=false;c=d.composite();assert(c.at(0,0).r==0&&c.at(0,0).b==255);
 d.mutable_layers()[1].visible=true;d.mutable_layers()[1].opacity=128;c=d.composite();assert(c.at(0,0).r==64&&c.at(0,0).b==191);
 d.mutable_layers()[1].opacity=255;d.checkpoint("before rename");d.set_name("Renamed");d.checkpoint("rename");assert(d.undo_label()=="rename");assert(d.undo()&&d.name()=="Test Document");assert(d.redo()&&d.name()=="Renamed");
 // History snapshots are post-edit states: undo restores the prior pixels and redo restores the edit.
 d.mutable_layers()[1].image.at(0,0)={0,255,0,255};d.checkpoint("paint pixel");
 assert(d.can_undo()&&d.undo());assert(d.layers()[1].image.at(0,0).r==255&&d.layers()[1].image.at(0,0).g==0);
 assert(d.can_redo()&&d.redo());assert(d.layers()[1].image.at(0,0).g==255);
 // A new edit after undo must discard the old redo branch.
 assert(d.undo());d.set_name("Branched edit");d.checkpoint("branch");assert(!d.can_redo());
 // Reordering and visibility changes are also checkpointed as complete document states.
 Document stack_layers(1,1,"Layer order");
 stack_layers.add_layer(Layer("Blue",Image(1,1,{0,0,255,255})));
 stack_layers.add_layer(Layer("Red",Image(1,1,{255,0,0,255})));
 stack_layers.checkpoint("initial layer order");
 assert(stack_layers.composite().at(0,0).r==255);
 stack_layers.move_layer(1,0);stack_layers.checkpoint("move red below blue");
 assert(stack_layers.composite().at(0,0).b==255);
 assert(stack_layers.undo()&&stack_layers.composite().at(0,0).r==255);
 assert(stack_layers.redo()&&stack_layers.composite().at(0,0).b==255);
 stack_layers.set_layer_visibility(0,false);stack_layers.checkpoint("hide top layer");
 assert(stack_layers.composite().at(0,0).r==255);
 assert(stack_layers.undo()&&stack_layers.composite().at(0,0).b==255);
 d.save("pstouch-test.ptdoc");Document r=Document::load("pstouch-test.ptdoc");assert(r.name()=="Renamed"&&r.width()==2&&r.layers().size()==2);assert(r.layers()[0].name=="Background"&&r.layers()[1].name=="Paint");assert(r.composite().at(0,0).r==128);d.set_name("Saved Again");d.save("pstouch-test.ptdoc");Document overwritten=Document::load("pstouch-test.ptdoc");assert(overwritten.name()=="Saved Again");std::remove("pstouch-test.ptdoc");
 const std::string unicode_path=u8"pstouch-projeto-a\u00e7\u00e3o-\u65e5\u672c.ptdoc";
 d.save(unicode_path);Document unicode_roundtrip=Document::load(unicode_path);
 assert(unicode_roundtrip.name()=="Saved Again" && unicode_roundtrip.layers().size()==2);
 std::error_code remove_error;std::filesystem::remove(std::filesystem::u8path(unicode_path),remove_error);assert(!remove_error);
 bool threw=false;try{Document::load("not-a-project.ptdoc");}catch(const std::runtime_error&){threw=true;}assert(threw);
 Document stack(1,1);for(int n=0;n<25;++n){stack.set_name(std::to_string(n));stack.checkpoint("step");}int undos=0;while(stack.undo())++undos;assert(undos==20);
 std::cout<<"PASS: layer compositing, visibility, opacity, post-edit pixel undo/redo, redo-branch invalidation, layer-order/visibility undo/redo, document round-trip and overwrite, history/redo cap, invalid project rejection\n";
}
