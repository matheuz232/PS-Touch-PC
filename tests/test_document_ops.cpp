#include "pstouch/document.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>
int main(){pstouch::Document d(3,3);d.add_layer(pstouch::Layer("base",pstouch::Image(1,1,{1,2,3,255})));auto copy=d.duplicate_layer(0);assert(copy==1&&d.layers().size()==2&&d.layers()[1].name=="base copy");d.rename_layer(1,"edited");d.set_layer_visibility(1,false);d.set_layer_opacity(1,128);assert(d.layers()[1].name=="edited"&&!d.layers()[1].visible&&d.layers()[1].opacity==128);d.move_layer(1,0);assert(d.layers()[0].name=="edited");bool bad=false;try{d.rename_layer(8,"x");}catch(const std::out_of_range&){bad=true;}assert(bad);
 // Composite must respect layer order, offsets and visibility.
 pstouch::Document composite(3,2,"composite");
 composite.add_layer(pstouch::Layer("base",pstouch::Image(3,2,{10,20,30,255})));
 pstouch::Image overlay(1,1,{200,100,50,255});
 pstouch::Layer top("overlay",std::move(overlay)); top.x=1; top.y=0;
 composite.add_layer(std::move(top));
 auto merged=composite.composite();
 assert(merged.at(0,0).r==10 && merged.at(0,0).g==20);
 assert(merged.at(1,0).r==200 && merged.at(1,0).g==100 && merged.at(1,0).b==50);
 composite.set_layer_visibility(1,false);
 merged=composite.composite();
 assert(merged.at(1,0).r==10 && merged.at(1,0).g==20 && merged.at(1,0).b==30);
 // Checkpoints restore document state and support redo.
 composite.checkpoint("before layer edit");
 composite.set_layer_visibility(1,true);
 composite.checkpoint("show overlay");
 assert(composite.can_undo() && composite.undo());
 assert(!composite.layers()[1].visible);
 assert(composite.can_redo() && composite.redo());
 assert(composite.layers()[1].visible);

 // Canvas rotation swaps dimensions and transforms every layer offset without clipping.
 pstouch::Document rotated(3,2,"rotation");
 pstouch::Image base_pixels(3,2,{0,0,0,255});
 base_pixels.at(1,0)={10,20,30,255};
 rotated.add_layer(pstouch::Layer("base",std::move(base_pixels)));
 pstouch::Layer marker_layer("marker",pstouch::Image(1,1,{200,100,50,255}));
 marker_layer.x=1; marker_layer.y=0; rotated.add_layer(std::move(marker_layer));
 rotated.checkpoint("before rotation");
 rotated.rotate_canvas(true);
 assert(rotated.width()==2 && rotated.height()==3);
 assert(rotated.layers()[1].x==1 && rotated.layers()[1].y==1);
 auto rotated_composite=rotated.composite();
 assert(rotated_composite.at(1,1).r==200 && rotated_composite.at(1,1).g==100);
 rotated.checkpoint("rotated clockwise");
 assert(rotated.undo() && rotated.width()==3 && rotated.height()==2);
 assert(rotated.redo() && rotated.width()==2 && rotated.height()==3);
 rotated.rotate_canvas(false);
 assert(rotated.width()==3 && rotated.height()==2);
 assert(rotated.layers()[1].x==1 && rotated.layers()[1].y==0);
 // Match the editor transaction order: mutate first, then record the resulting state.
 pstouch::Document transactions(2,2,"transactions");
 transactions.add_layer(pstouch::Layer("base",pstouch::Image(2,2,{1,2,3,255})));
 transactions.checkpoint("Open image");
 transactions.set_layer_visibility(0,false);
 transactions.checkpoint("Hide base");
 assert(!transactions.layers()[0].visible);
 assert(transactions.undo() && transactions.layers()[0].visible);
 assert(transactions.redo() && !transactions.layers()[0].visible);
 transactions.set_layer_visibility(0,true);
 transactions.checkpoint("Show base");
 transactions.add_layer(pstouch::Layer("paint",pstouch::Image(1,1,{9,8,7,255})));
 transactions.checkpoint("Add paint");
 assert(transactions.undo() && transactions.layers().size()==1);
 assert(transactions.redo() && transactions.layers().size()==2);
 // Text-layer properties survive the native project round trip.
 pstouch::Document text_doc(8,8,"editable text");
 pstouch::Layer text_layer("Greeting",pstouch::Image(2,2,{0,0,0,0}));
 pstouch::TextMetadata text;
 text.text="Olá, mundo!";
 text.font_family="Example Sans";
 text.pixel_size=48;
 text.color_rgb=0x12ABEF;
 text.origin_x=123;
 text.origin_y=234;
 text.bold=true;
 text.italic=true;
 text.underline=true;
 text.strikeout=false;
 text_layer.text=text;
 text_doc.add_layer(std::move(text_layer));
 const auto text_path=std::filesystem::temp_directory_path()/"pstouch-text-metadata-roundtrip.ptdoc";
 text_doc.save(text_path.string());
 auto text_loaded=pstouch::Document::load(text_path.string());
 std::filesystem::remove(text_path);
 assert(text_loaded.layers().size()==1 && text_loaded.layers()[0].text.has_value());
 const auto& restored=*text_loaded.layers()[0].text;
 assert(restored.text=="Olá, mundo!" && restored.font_family=="Example Sans");
 assert(restored.pixel_size==48 && restored.color_rgb==0x12ABEF);
 assert(restored.origin_x==123 && restored.origin_y==234);
 assert(restored.bold && restored.italic && restored.underline && !restored.strikeout);
 std::cout<<"PASS: layer operations, compositing, history, and editable text metadata round-trip\n";}
