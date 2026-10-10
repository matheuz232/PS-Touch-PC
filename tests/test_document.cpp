#include "pstouch/document.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>
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
 // A failed in-place operation must restore the current checkpoint without
 // consuming an undo step or reverting the preceding successful edit.
 Document recovery(1,1,"Recovery");
 recovery.add_layer(Layer("Pixels",Image(1,1,{255,0,0,255})));
 recovery.checkpoint("initial");
 recovery.mutable_layers()[0].image.at(0,0)={0,255,0,255};
 recovery.checkpoint("successful edit");
 recovery.mutable_layers()[0].image.at(0,0)={0,0,255,255}; // simulated partial failure
 assert(recovery.restore_current_checkpoint());
 auto restored=recovery.layers()[0].image.at(0,0);
 assert(restored.r==0&&restored.g==255&&restored.b==0);
 assert(recovery.can_undo()&&recovery.undo());
 restored=recovery.layers()[0].image.at(0,0);
 assert(restored.r==255&&restored.g==0&&restored.b==0);
 // Each supported blend mode is exercised with opaque source/destination pixels.
 const BlendMode modes[]={BlendMode::Darken,BlendMode::Multiply,BlendMode::Lighten,BlendMode::Screen,BlendMode::Add,BlendMode::Overlay,BlendMode::Difference,BlendMode::Subtract};
 for(const auto mode:modes){
  Document blended(1,1,"Blend test");
  blended.add_layer(Layer("Destination",Image(1,1,{100,150,200,255})));
  Layer source("Source",Image(1,1,{200,100,50,255}));source.blend=mode;blended.add_layer(source);
  const auto pixel=blended.composite().at(0,0);assert(pixel.a==255);
  if(mode==BlendMode::Darken)assert(pixel.r==100&&pixel.g==100&&pixel.b==50);
  if(mode==BlendMode::Lighten)assert(pixel.r==200&&pixel.g==150&&pixel.b==200);
  if(mode==BlendMode::Multiply)assert(pixel.r==78&&pixel.g==59&&pixel.b==39);
  if(mode==BlendMode::Screen)assert(pixel.r==222&&pixel.g==191&&pixel.b==211);
  if(mode==BlendMode::Add)assert(pixel.r==255&&pixel.g==250&&pixel.b==250);
  if(mode==BlendMode::Overlay)assert(pixel.r==157&&pixel.g==127&&pixel.b==167);
  if(mode==BlendMode::Difference)assert(pixel.r==100&&pixel.g==50&&pixel.b==150);
  if(mode==BlendMode::Subtract)assert(pixel.r==0&&pixel.g==50&&pixel.b==150);
 }
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
 stack_layers.set_layer_visibility(1,false);stack_layers.checkpoint("hide top layer");
 assert(stack_layers.composite().at(0,0).r==255);
 assert(stack_layers.undo()&&stack_layers.composite().at(0,0).b==255);
 // PTDOC v3 must preserve all text metadata, and document history must restore it.
 Document text_doc(4,3,"Text metadata");
 text_doc.add_layer(Layer("Background",Image(4,3,{0,0,0,0})));
 Layer text_layer("Editable text",Image(4,3,{0,0,0,0}));
 TextMetadata text_metadata;
 text_metadata.text=std::string("Ol\xC3\xA1 \xF0\x9F\x8C\x84");
 text_metadata.font_family="Arial";
 text_metadata.pixel_size=37;
 text_metadata.color_rgb=0x12ABEF;
 text_metadata.origin_x=2;
 text_metadata.origin_y=1;
 text_metadata.bold=true;
 text_metadata.italic=true;
 text_metadata.underline=true;
 text_metadata.strikeout=true;
 text_layer.text=text_metadata;
 text_doc.add_layer(std::move(text_layer));
 text_doc.checkpoint("before text change");
 text_doc.mutable_layers()[1].text->text="changed";
 text_doc.checkpoint("change text metadata");
 assert(text_doc.undo());
 assert(text_doc.layers()[1].text && text_doc.layers()[1].text->text==u8"Olá 🌄");
 assert(text_doc.redo());
 assert(text_doc.layers()[1].text && text_doc.layers()[1].text->text=="changed");
 text_doc.undo();
 text_doc.save("pstouch-text-metadata.ptdoc");
 Document text_roundtrip=Document::load("pstouch-text-metadata.ptdoc");
 assert(text_roundtrip.layers().size()==2 && text_roundtrip.layers()[1].text);
 const auto& saved_text=*text_roundtrip.layers()[1].text;
 assert(saved_text.text==u8"Olá 🌄" && saved_text.font_family=="Arial");
 assert(saved_text.pixel_size==37 && saved_text.color_rgb==0x12ABEF);
 assert(saved_text.origin_x==2 && saved_text.origin_y==1);
 assert(saved_text.bold && saved_text.italic && saved_text.underline && saved_text.strikeout);
 std::remove("pstouch-text-metadata.ptdoc");
 d.save("pstouch-test.ptdoc");Document r=Document::load("pstouch-test.ptdoc");assert(r.name()=="Renamed"&&r.width()==2&&r.layers().size()==2);assert(r.layers()[0].name=="Background"&&r.layers()[1].name=="Paint");assert(r.composite().at(0,0).r==128);d.set_name("Saved Again");d.save("pstouch-test.ptdoc");Document overwritten=Document::load("pstouch-test.ptdoc");assert(overwritten.name()=="Saved Again");std::remove("pstouch-test.ptdoc");
 const std::string unicode_path=u8"pstouch-projeto-a\u00e7\u00e3o-\u65e5\u672c.ptdoc";
 d.save(unicode_path);Document unicode_roundtrip=Document::load(unicode_path);
 assert(unicode_roundtrip.name()=="Saved Again" && unicode_roundtrip.layers().size()==2);
 std::error_code remove_error;std::filesystem::remove(std::filesystem::u8path(unicode_path),remove_error);assert(!remove_error);
 bool threw=false;try{Document::load("not-a-project.ptdoc");}catch(const std::runtime_error&){threw=true;}assert(threw);
 Document stack(1,1);for(int n=0;n<25;++n){stack.set_name(std::to_string(n));stack.checkpoint("step");}int undos=0;while(stack.undo())++undos;assert(undos==20);
 std::cout<<"PASS: layer compositing, visibility, opacity, post-edit pixel undo/redo, redo-branch invalidation, layer-order/visibility undo/redo, blend modes, document round-trip and overwrite, history/redo cap, invalid project rejection\n";
}
