#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <gdiplus.h>
#include "pstouch/image.hpp"
#include "pstouch/document.hpp"
#include "pstouch/psd.hpp"
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <cmath>
#include <filesystem>
#include <system_error>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")
using namespace Gdiplus;
namespace {
constexpr COLORREF BG=RGB(43,43,43), PANEL=RGB(52,52,52), PANEL2=RGB(37,37,37), ACCENT=RGB(73,139,207), TEXT=RGB(226,226,226), MUTED=RGB(165,165,165);
HWND g_hwnd{}; ULONG_PTR g_gdiplus{}; std::unique_ptr<Bitmap> g_image; std::unique_ptr<pstouch::Document> g_document; size_t g_selected_layer=0; std::vector<std::wstring> g_loaded_font_paths; PrivateFontCollection g_private_fonts; std::wstring g_path=L"Nenhuma imagem aberta"; float g_zoom=1.0f; bool g_showLayers=true, g_showTools=true; std::vector<std::unique_ptr<Bitmap>> g_undo, g_redo; bool g_text_mode=false; bool g_text_capturing=false; int g_active_tool=3; std::wstring g_text_input; int g_text_image_x=0,g_text_image_y=0,g_text_screen_x=0,g_text_screen_y=0; LOGFONTW g_text_logfont=[](){ LOGFONTW lf{}; lf.lfHeight=-32; lf.lfWeight=FW_NORMAL; wcscpy_s(lf.lfFaceName,LF_FACESIZE,L"Arial"); return lf; }(); COLORREF g_text_color=RGB(255,255,255); std::unique_ptr<Bitmap> g_mockup_design; float g_mockup_scale=0.55f; int g_mockup_dx=0,g_mockup_dy=0;
std::filesystem::path executable_directory() {
 wchar_t buffer[32768]{};
 constexpr DWORD buffer_count=(DWORD)(sizeof(buffer)/sizeof(buffer[0]));
 DWORD length=GetModuleFileNameW(nullptr,buffer,buffer_count);
 if(length==0 || length>=buffer_count) return std::filesystem::current_path();
 return std::filesystem::path(buffer).parent_path();
}
void load_custom_fonts() {
 std::error_code ec;
 const auto directory=executable_directory()/L"fonts";
 std::filesystem::create_directories(directory,ec);
 if(ec || !std::filesystem::exists(directory,ec)) return;
 for(std::filesystem::directory_iterator it(directory,ec),end; !ec && it!=end; it.increment(ec)) {
  if(!it->is_regular_file(ec) || ec) { ec.clear(); continue; }
  const auto extension=it->path().extension().wstring();
  if(_wcsicmp(extension.c_str(),L".ttf")!=0 && _wcsicmp(extension.c_str(),L".otf")!=0 && _wcsicmp(extension.c_str(),L".ttc")!=0) continue;
  const auto path=it->path().wstring();
  Status private_status=g_private_fonts.AddFontFile(path.c_str());
  if(AddFontResourceExW(path.c_str(),FR_PRIVATE,nullptr)!=0 || private_status==Ok) g_loaded_font_paths.push_back(path);
 }
}
void unload_custom_fonts() {
 for(const auto& path:g_loaded_font_paths) RemoveFontResourceExW(path.c_str(),FR_PRIVATE,nullptr);
 g_loaded_font_paths.clear();
}
HFONT font(int px=14, bool bold=false) { return CreateFontW(-px,0,0,0,bold?FW_SEMIBOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Arial"); }
void fill(HDC dc, RECT r, COLORREF c) { HBRUSH b=CreateSolidBrush(c); FillRect(dc,&r,b); DeleteObject(b); }
void label(HDC dc,int x,int y,const std::wstring& s,COLORREF c=TEXT,int px=14,bool bold=false) { HFONT f=font(px,bold), old=(HFONT)SelectObject(dc,f); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,c); TextOutW(dc,x,y,s.c_str(),(int)s.size()); SelectObject(dc,old); DeleteObject(f); }
void line(HDC dc,int x1,int y1,int x2,int y2,COLORREF c) { HPEN p=CreatePen(PS_SOLID,1,c),old=(HPEN)SelectObject(dc,p); MoveToEx(dc,x1,y1,nullptr); LineTo(dc,x2,y2); SelectObject(dc,old); DeleteObject(p); }
void button(HDC dc,RECT r,const wchar_t* s,bool active=false) { fill(dc,r,active?ACCENT:PANEL); FrameRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH)); label(dc,r.left+10,r.top+7,s,TEXT,13); }
std::unique_ptr<Bitmap> copy_bitmap(Bitmap& src) {
 auto out=std::make_unique<Bitmap>(src.GetWidth(),src.GetHeight(),PixelFormat32bppARGB);
 Graphics g(out.get()); g.SetCompositingMode(CompositingModeSourceCopy); g.DrawImage(&src,0,0,src.GetWidth(),src.GetHeight());
 if(out->GetLastStatus()!=Ok) return {}; return out;
}
std::unique_ptr<pstouch::Image> to_core_image(Bitmap& bitmap) {
 const UINT width=bitmap.GetWidth(), height=bitmap.GetHeight();
 if(width==0 || height==0) return {};
 auto image=std::make_unique<pstouch::Image>(width,height);
 Rect rect(0,0,(INT)width,(INT)height); BitmapData data{};
 if(bitmap.LockBits(&rect,ImageLockModeRead,PixelFormat32bppARGB,&data)!=Ok) return {};
 bool ok=true;
 for(INT y=0;y<data.Height;y++) {
  const auto* row=reinterpret_cast<const BYTE*>(data.Scan0)+static_cast<ptrdiff_t>(y)*data.Stride;
  for(INT x=0;x<data.Width;x++) {
   const BYTE* p=row+static_cast<ptrdiff_t>(x)*4;
   image->at((uint32_t)x,(uint32_t)y)=pstouch::Pixel{p[2],p[1],p[0],p[3]};
  }
 }
 if(bitmap.UnlockBits(&data)!=Ok) ok=false;
 return ok?std::move(image):std::unique_ptr<pstouch::Image>{};
}
std::unique_ptr<Bitmap> from_core_image(const pstouch::Image& image) {
 auto bitmap=std::make_unique<Bitmap>(image.width(),image.height(),PixelFormat32bppARGB);
 if(bitmap->GetLastStatus()!=Ok) return {};
 Rect rect(0,0,(INT)image.width(),(INT)image.height()); BitmapData data{};
 if(bitmap->LockBits(&rect,ImageLockModeWrite,PixelFormat32bppARGB,&data)!=Ok) return {};
 for(INT y=0;y<data.Height;y++) {
  auto* row=reinterpret_cast<BYTE*>(data.Scan0)+static_cast<ptrdiff_t>(y)*data.Stride;
  for(INT x=0;x<data.Width;x++) {
   const auto& pixel=image.at((uint32_t)x,(uint32_t)y);
   BYTE* p=row+static_cast<ptrdiff_t>(x)*4;
   p[0]=pixel.b; p[1]=pixel.g; p[2]=pixel.r; p[3]=pixel.a;
  }
 }
 if(bitmap->UnlockBits(&data)!=Ok) return {};
 return bitmap;
}
int encoder_clsid(const WCHAR* mime, CLSID* clsid) {
 UINT count=0,size=0; GetImageEncodersSize(&count,&size); if(!size) return -1;
 auto mem=std::make_unique<BYTE[]>(size); auto info=reinterpret_cast<ImageCodecInfo*>(mem.get());
 if(GetImageEncoders(count,size,info)!=Ok) return -1;
 for(UINT i=0;i<count;i++) if(wcscmp(info[i].MimeType,mime)==0){*clsid=info[i].Clsid;return (int)i;} return -1;
}
std::string wide_to_utf8(const std::wstring& value);
void save_image() {
 if(!g_image){MessageBoxW(g_hwnd,L"Abra uma imagem antes de salvar.",L"PS Touch PC",MB_OK|MB_ICONINFORMATION);return;}
 wchar_t path[MAX_PATH]=L"imagem.png"; OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=g_hwnd;
 ofn.lpstrFilter=L"PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg\0Bitmap (*.bmp)\0*.bmp\0TIFF (*.tif)\0*.tif\0Photoshop document (*.psd)\0*.psd\0";ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;ofn.lpstrDefExt=L"png";
 if(!GetSaveFileNameW(&ofn))return; const wchar_t* mime=L"image/png"; const wchar_t* ext=wcsrchr(path,L'.');
 if(ext && (_wcsicmp(ext,L".jpg")==0||_wcsicmp(ext,L".jpeg")==0))mime=L"image/jpeg";else if(ext&&_wcsicmp(ext,L".bmp")==0)mime=L"image/bmp";else if(ext&&(_wcsicmp(ext,L".tif")==0||_wcsicmp(ext,L".tiff")==0))mime=L"image/tiff";
 if(ext&&_wcsicmp(ext,L".psd")==0){try{const auto utf8=wide_to_utf8(path);if(g_document)pstouch::save_psd_layers(*g_document,utf8);else{auto core=to_core_image(*g_image);if(!core)throw std::runtime_error("image conversion failed");pstouch::save_psd_flattened(*core,utf8);}g_path=path;InvalidateRect(g_hwnd,nullptr,FALSE);return;}catch(const std::exception&){MessageBoxW(g_hwnd,L"Falha ao exportar o documento PSD. Verifique limites e modos de mesclagem das camadas.",L"PS Touch PC",MB_OK|MB_ICONERROR);return;}}
 CLSID clsid{}; if(encoder_clsid(mime,&clsid)<0||g_image->Save(path,&clsid,nullptr)!=Ok){MessageBoxW(g_hwnd,L"Falha ao salvar a imagem neste formato.",L"PS Touch PC",MB_OK|MB_ICONERROR);return;} g_path=path;InvalidateRect(g_hwnd,nullptr,FALSE);
}
void render_document() {
 if(!g_document) return;
 auto composite=g_document->composite();
 auto bitmap=from_core_image(composite);
 if(bitmap) g_image=std::move(bitmap);
 if(g_document->layers().empty()) g_selected_layer=0;
 else if(g_selected_layer>=g_document->layers().size()) g_selected_layer=g_document->layers().size()-1;
 InvalidateRect(g_hwnd,nullptr,FALSE);
}
void push_undo(const char* label="Edit image") {
 if(g_document) g_document->checkpoint(label);
 if(g_image){auto c=copy_bitmap(*g_image);if(c){g_undo.push_back(std::move(c));if(g_undo.size()>20)g_undo.erase(g_undo.begin());}}
 g_redo.clear();
}
void undo_image(){
 if(g_document && g_document->undo()){render_document();return;}
 if(g_undo.empty()||!g_image)return;
 auto c=copy_bitmap(*g_image);if(c)g_redo.push_back(std::move(c));
 g_image=std::move(g_undo.back());g_undo.pop_back();InvalidateRect(g_hwnd,nullptr,FALSE);
}
void redo_image(){
 if(g_document && g_document->redo()){render_document();return;}
 if(g_redo.empty()||!g_image)return;
 auto c=copy_bitmap(*g_image);if(c)g_undo.push_back(std::move(c));
 g_image=std::move(g_redo.back());g_redo.pop_back();InvalidateRect(g_hwnd,nullptr,FALSE);
}
bool initialize_document_from_bitmap(Bitmap& bitmap,const std::string& name) {
 auto image=to_core_image(bitmap); if(!image) return false;
 auto doc=std::make_unique<pstouch::Document>(image->width(),image->height(),name);
 doc->add_layer(pstouch::Layer("Background",std::move(*image)));
 doc->checkpoint("Open image");
 g_document=std::move(doc); g_selected_layer=0; return true;
}
bool add_transparent_layer(const std::string& name) {
 if(!g_document) return false;
 try {
  g_selected_layer=g_document->add_layer(pstouch::Layer(name,pstouch::Image(g_document->width(),g_document->height(),{0,0,0,0})));
  g_document->checkpoint("Add layer");
  render_document(); return true;
 } catch(...) { return false; }
}
void toggle_selected_visibility(){
 if(!g_document||g_document->layers().empty())return;
 const auto& layer=g_document->layers()[g_selected_layer];
 g_document->set_layer_visibility(g_selected_layer,!layer.visible); g_document->checkpoint("Toggle layer visibility"); render_document();
}
void duplicate_selected_layer(){
 if(!g_document||g_document->layers().empty())return;
 try{g_selected_layer=g_document->duplicate_layer(g_selected_layer);g_document->checkpoint("Duplicate layer");render_document();}catch(...){MessageBoxW(g_hwnd,L"Não foi possível duplicar a camada.",L"PS Touch PC",MB_OK|MB_ICONWARNING);}
}
void remove_selected_layer(){
 if(!g_document||g_document->layers().size()<2){MessageBoxW(g_hwnd,L"O documento precisa manter pelo menos uma camada.",L"PS Touch PC",MB_OK|MB_ICONINFORMATION);return;}
 g_document->remove_layer(g_selected_layer);if(g_selected_layer>=g_document->layers().size())g_selected_layer=g_document->layers().size()-1;g_document->checkpoint("Remove layer");render_document();
}
void rotate_image(bool clockwise){
 if(!g_document||g_document->layers().empty())return;
 try {
  g_document->rotate_canvas(clockwise);
  g_document->checkpoint(clockwise?"Rotate canvas clockwise":"Rotate canvas counterclockwise");
  render_document();
 } catch(const std::exception&) {
  MessageBoxW(g_hwnd,L"Não foi possível girar o documento sem perder conteúdo.",L"PS Touch PC",MB_OK|MB_ICONWARNING);
 }
}
void flip_image(bool horizontal){
 if(!g_document||g_document->layers().empty())return;
 auto& image=g_document->mutable_layers()[g_selected_layer].image;
 if(horizontal)pstouch::flip_horizontal(image);else pstouch::flip_vertical(image);
 g_document->checkpoint("Flip layer"); render_document();
}
void apply_tone(bool use_sepia){
 if(!g_document||g_document->layers().empty())return;
 auto& image=g_document->mutable_layers()[g_selected_layer].image;
 if(use_sepia)pstouch::sepia(image);else pstouch::grayscale(image);
 g_document->checkpoint(use_sepia?"Sepia layer":"Grayscale layer"); render_document();
}
std::string wide_to_utf8(const std::wstring& value) {
 if(value.empty()) return {};
 const int bytes=WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),nullptr,0,nullptr,nullptr);
 if(bytes<=0) return {};
 std::string out((size_t)bytes,'\0');
 if(WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),out.data(),bytes,nullptr,nullptr)<=0) return {};
 return out;
}
std::wstring utf8_to_wide(const std::string& value) {
 if(value.empty()) return {};
 const int chars=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),(int)value.size(),nullptr,0);
 if(chars<=0) return {};
 std::wstring out((size_t)chars,L'\0');
 if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),(int)value.size(),out.data(),chars)<=0) return {};
 return out;
}
void open_project() {
 wchar_t path[32768]{};
 OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=g_hwnd;
 ofn.lpstrFilter=L"PS Touch PC project (*.ptdoc)\0*.ptdoc\0All files\0*.*\0";
 ofn.lpstrFile=path; ofn.nMaxFile=(DWORD)(sizeof(path)/sizeof(path[0]));
 ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
 if(!GetOpenFileNameW(&ofn)) return;
 try {
  auto loaded=std::make_unique<pstouch::Document>(pstouch::Document::load(wide_to_utf8(path))); loaded->checkpoint("Open project");
  auto composite=from_core_image(loaded->composite());
  if(!composite) throw std::runtime_error("could not render project composite");
  g_document=std::move(loaded); g_image=std::move(composite); g_selected_layer=0;
  g_path=path; g_zoom=1.0f; g_undo.clear(); g_redo.clear(); g_mockup_design.reset();
  InvalidateRect(g_hwnd,nullptr,TRUE);
 } catch(const std::exception&) {
  MessageBoxW(g_hwnd,L"Não foi possível abrir o projeto .ptdoc. O arquivo pode estar corrompido ou ser incompatível.",L"PS Touch PC",MB_OK|MB_ICONERROR);
 }
}
void save_project() {
 if(!g_document) { MessageBoxW(g_hwnd,L"Abra ou crie um documento antes de salvar o projeto.",L"PS Touch PC",MB_OK|MB_ICONINFORMATION); return; }
 wchar_t path[32768]{};
 if(!g_path.empty() && g_path!=L"Nenhuma imagem aberta") {
  const auto ext=g_path.find_last_of(L'.');
  if(ext!=std::wstring::npos && _wcsicmp(g_path.c_str()+ext,L".ptdoc")==0) wcsncpy_s(path,g_path.c_str(),_TRUNCATE);
 }
 if(!path[0]) wcscpy_s(path,L"projeto.ptdoc");
 OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=g_hwnd;
 ofn.lpstrFilter=L"PS Touch PC project (*.ptdoc)\0*.ptdoc\0All files\0*.*\0";
 ofn.lpstrFile=path; ofn.nMaxFile=(DWORD)(sizeof(path)/sizeof(path[0]));
 ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST; ofn.lpstrDefExt=L"ptdoc";
 if(!GetSaveFileNameW(&ofn)) return;
 try {
  g_document->save(wide_to_utf8(path)); g_path=path;
  MessageBoxW(g_hwnd,L"Projeto salvo com camadas e metadados.",L"PS Touch PC",MB_OK|MB_ICONINFORMATION);
 } catch(const std::exception&) {
  MessageBoxW(g_hwnd,L"Falha ao salvar o projeto .ptdoc.",L"PS Touch PC",MB_OK|MB_ICONERROR);
 }
}
void open_image() {
 wchar_t path[MAX_PATH]{}; OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=g_hwnd;
 ofn.lpstrFilter=L"Images and PSD\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.psd\0All files\0*.*\0";
 ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH; ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
 if(!GetOpenFileNameW(&ofn))return;
 const wchar_t* ext=wcsrchr(path,L'.');
 if(ext&&_wcsicmp(ext,L".psd")==0){
  try {
   auto loaded=std::make_unique<pstouch::Document>(pstouch::load_psd_layers(wide_to_utf8(path)));
   auto composite=from_core_image(loaded->composite());if(!composite)throw std::runtime_error("PSD composite conversion failed");
   loaded->checkpoint("Open PSD");g_document=std::move(loaded);g_image=std::move(composite);g_selected_layer=0;g_path=path;g_zoom=1.0f;g_undo.clear();g_redo.clear();g_mockup_design.reset();
  } catch(const std::exception&) {
   try {
    auto flat=pstouch::load_psd_flattened(wide_to_utf8(path));auto bitmap=from_core_image(flat);
    if(!bitmap||!initialize_document_from_bitmap(*bitmap,"PSD (flattened)"))throw std::runtime_error("PSD conversion failed");
    g_image=std::move(bitmap);g_path=path;g_zoom=1.0f;g_undo.clear();g_redo.clear();
   } catch(const std::exception&) {
    MessageBoxW(g_hwnd,L"Não foi possível abrir este PSD. A importação em camadas exige canais raw; a alternativa achatada aceita PSD RGB de 8 bits com dados raw ou RLE.",L"PS Touch PC",MB_OK|MB_ICONERROR);
   }
  }
  InvalidateRect(g_hwnd,nullptr,TRUE);return;
 }
 auto candidate=std::make_unique<Bitmap>(path);
 if(candidate->GetLastStatus()==Ok){
  if(initialize_document_from_bitmap(*candidate,"Image")){g_image=std::move(candidate);g_path=path;g_zoom=1.0f;g_undo.clear();g_redo.clear();}
  else MessageBoxW(g_hwnd,L"Falha ao converter a imagem para o documento editável.",L"PS Touch PC",MB_OK|MB_ICONERROR);
 } else MessageBoxW(g_hwnd,L"Não foi possível abrir esta imagem. Use PNG, JPEG, BMP, TIFF ou PSD.",L"PS Touch PC",MB_ICONWARNING);
 InvalidateRect(g_hwnd,nullptr,TRUE);
}
void start_mockup() {
 if(!g_image){MessageBoxW(g_hwnd,L"Abra primeiro uma foto do produto ou uma imagem-base para o mockup.",L"PS Touch PC",MB_OK|MB_ICONINFORMATION);return;}
 wchar_t path[MAX_PATH]{}; OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=g_hwnd;ofn.lpstrFilter=L"Arte/design (PNG recomendado)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0Todos os arquivos\0*.*\0";ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
 if(!GetOpenFileNameW(&ofn))return;auto candidate=std::make_unique<Bitmap>(path);if(candidate->GetLastStatus()!=Ok){MessageBoxW(g_hwnd,L"Não foi possível abrir a arte escolhida.",L"PS Touch PC",MB_OK|MB_ICONWARNING);return;}
 g_mockup_design=std::move(candidate);g_mockup_scale=0.55f;g_mockup_dx=0;g_mockup_dy=0;InvalidateRect(g_hwnd,nullptr,TRUE);
}
void cancel_mockup(){g_mockup_design.reset();g_mockup_dx=g_mockup_dy=0;InvalidateRect(g_hwnd,nullptr,FALSE);}
void mockup_dimensions(int& w,int& h){if(!g_image||!g_mockup_design){w=h=0;return;}double factor=std::min((double)g_image->GetWidth()*0.62/g_mockup_design->GetWidth(),(double)g_image->GetHeight()*0.62/g_mockup_design->GetHeight())*g_mockup_scale/0.55;w=std::max(1,(int)(g_mockup_design->GetWidth()*factor));h=std::max(1,(int)(g_mockup_design->GetHeight()*factor));}
void commit_mockup(){
 if(!g_document||!g_mockup_design)return;
 int ow=0,oh=0;mockup_dimensions(ow,oh);
 const int x=((int)g_document->width()-ow)/2+g_mockup_dx,y=((int)g_document->height()-oh)/2+g_mockup_dy;
 auto design=std::make_unique<Bitmap>(g_document->width(),g_document->height(),PixelFormat32bppARGB);
 if(!design||design->GetLastStatus()!=Ok)return;
 Graphics gr(design.get());gr.Clear(Color(0,0,0,0));gr.SetCompositingMode(CompositingModeSourceOver);gr.SetInterpolationMode(InterpolationModeHighQualityBicubic);
 if(gr.DrawImage(g_mockup_design.get(),Rect(x,y,ow,oh))!=Ok){MessageBoxW(g_hwnd,L"Falha ao compor o mockup.",L"PS Touch PC",MB_OK|MB_ICONERROR);return;}
 auto pixels=to_core_image(*design);if(!pixels)return;
 try{g_selected_layer=g_document->add_layer(pstouch::Layer("Mockup artwork",std::move(*pixels)));g_document->checkpoint("Place mockup artwork");g_mockup_design.reset();render_document();}
 catch(...){MessageBoxW(g_hwnd,L"Não foi possível criar a camada do mockup.",L"PS Touch PC",MB_OK|MB_ICONERROR);}
}

bool choose_text_style() {
 CHOOSEFONTW cf{}; cf.lStructSize=sizeof(cf); cf.hwndOwner=g_hwnd; cf.lpLogFont=&g_text_logfont;
 cf.rgbColors=g_text_color; cf.Flags=CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_EFFECTS;
 if(!ChooseFontW(&cf)) return false;
 g_text_color=cf.rgbColors;
 return true;
}
void commit_text() {
 if(!g_image || g_text_input.empty()) { g_text_capturing=false; g_text_input.clear(); InvalidateRect(g_hwnd,nullptr,FALSE); return; }
 auto family=std::make_unique<FontFamily>(g_text_logfont.lfFaceName,&g_private_fonts);
 if(family->GetLastStatus()!=Ok) family=std::make_unique<FontFamily>(g_text_logfont.lfFaceName);
 if(family->GetLastStatus()!=Ok) { MessageBoxW(g_hwnd,L"A família selecionada não pôde ser carregada. Escolha outra fonte.",L"PS Touch PC",MB_OK|MB_ICONWARNING); g_text_capturing=false; g_text_input.clear(); InvalidateRect(g_hwnd,nullptr,FALSE); return; }
 INT style=FontStyleRegular;
 if(g_text_logfont.lfWeight>=FW_BOLD) style|=FontStyleBold;
 if(g_text_logfont.lfItalic) style|=FontStyleItalic;
 if(g_text_logfont.lfUnderline) style|=FontStyleUnderline;
 if(g_text_logfont.lfStrikeOut) style|=FontStyleStrikeout;
 const REAL font_size=(REAL)std::clamp(std::abs(g_text_logfont.lfHeight),1L,512L);
 Font text_font(family.get(),font_size,style,UnitPixel);
 if(text_font.GetLastStatus()!=Ok) { MessageBoxW(g_hwnd,L"Não foi possível criar a fonte selecionada.",L"PS Touch PC",MB_OK|MB_ICONWARNING); g_text_capturing=false; g_text_input.clear(); InvalidateRect(g_hwnd,nullptr,FALSE); return; }
 auto text_layer=std::make_unique<Bitmap>(g_document?g_document->width():g_image->GetWidth(),g_document?g_document->height():g_image->GetHeight(),PixelFormat32bppARGB);
 if(!text_layer||text_layer->GetLastStatus()!=Ok){g_text_capturing=false;g_text_input.clear();return;}
 Graphics gr(text_layer.get());gr.Clear(Color(0,0,0,0));gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
 SolidBrush brush(Color(255,GetRValue(g_text_color),GetGValue(g_text_color),GetBValue(g_text_color)));
 PointF origin((REAL)g_text_image_x,(REAL)g_text_image_y);
 if(gr.DrawString(g_text_input.c_str(),(INT)g_text_input.size(),&text_font,origin,&brush)!=Ok){g_text_capturing=false;g_text_input.clear();return;}
 auto pixels=to_core_image(*text_layer);
 if(pixels&&g_document){try{g_selected_layer=g_document->add_layer(pstouch::Layer("Text",std::move(*pixels)));g_document->checkpoint("Add text layer");render_document();}catch(...){MessageBoxW(g_hwnd,L"Não foi possível criar a camada de texto.",L"PS Touch PC",MB_OK|MB_ICONERROR);}}
 g_text_capturing=false; g_text_mode=false; g_text_input.clear(); InvalidateRect(g_hwnd,nullptr,FALSE);
}
void draw_ui(HDC dc, RECT c) {
 int w=c.right,h=c.bottom; fill(dc,c,BG);
 // Top menu and command bar remain fixed-height; content below is fully responsive.
 fill(dc,{0,0,w,34},PANEL2); label(dc,14,8,L"PS Touch",RGB(245,245,245),16,true); label(dc,110,10,L"Arquivo   Editar   Imagem   Camada   Selecionar   Filtro   Exibir   |   F3 Ferramentas   F4 Camadas",TEXT,13);
 fill(dc,{0,34,w,76},PANEL); button(dc,{12,43,86,67},L"Abrir",true); button(dc,{94,43,168,67},L"Salvar"); button(dc,{176,43,252,67},L"Desfazer"); button(dc,{260,43,338,67},L"Refazer"); button(dc,{346,43,430,67},L"Cinza"); button(dc,{438,43,522,67},L"Sépia"); button(dc,{530,43,614,67},L"Mockup",g_mockup_design!=nullptr); if(g_mockup_design){button(dc,{622,43,704,67},L"Aplicar",true);button(dc,{712,43,794,67},L"Cancelar");label(dc,804,49,L"Setas mover · +/- tamanho · Enter aplicar",MUTED,11);}else{button(dc,{622,43,696,67},L"Girar ↶");button(dc,{702,43,776,67},L"Girar ↷");button(dc,{782,43,856,67},L"Esp. H");button(dc,{862,43,936,67},L"Esp. V");}button(dc,{944,43,1020,67},L"Abrir proj.");button(dc,{1026,43,1104,67},L"Salvar proj.");label(dc,std::max(1110,w-150),49,L"Adaptável",MUTED,12);
 const int top=76,bottom=26; fill(dc,{0,h-bottom,w,h},PANEL2); label(dc,12,h-bottom+6,g_text_capturing?L"Enter: nova linha · Ctrl+Enter: confirmar · Esc: cancelar":(g_text_mode?L"Ferramenta Texto ativa · clique na imagem":g_path),MUTED,11); label(dc,std::max(250,w-250),h-bottom+6,L"Fontes detectadas: "+std::to_wstring(g_loaded_font_paths.size()),MUTED,11);
 int usableH=std::max(0,h-top-bottom); bool compact=w<860; bool tiny=w<570; int left=g_showTools?(tiny?0:(compact?44:190)):0; int right=g_showLayers?(tiny?0:(compact?0:230)):0; if(w-left-right<160){right=0;left= g_showTools?36:0;}
 if(left>0){fill(dc,{0,top,left,h-bottom},PANEL2); if(left>50){label(dc,14,top+14,L"Ferramentas",TEXT,13,true); const wchar_t* tools[]={L"Mover",L"Seleção",L"Laço",L"Pincel",L"Borracha",L"Preenchimento",L"Texto",L"Cortar",L"Conta-gotas",L"Mão"}; for(int i=0;i<10;i++){int yy=top+44+i*35; RECT r{10,yy,left-10,yy+28}; button(dc,r,tools[i],i==g_active_tool);} } else {for(int i=0;i<8;i++){int yy=top+12+i*42; RECT r{7,yy,left-7,yy+30}; fill(dc,r,i==g_active_tool?ACCENT:PANEL); label(dc,14,yy+7,std::to_wstring(i+1),TEXT,13,true);}} }
 if(right>0){
  int rx=w-right; fill(dc,{rx,top,w,h-bottom},PANEL2); label(dc,rx+14,top+14,L"Camadas",TEXT,14,true); line(dc,rx,top+40,w,top+40,RGB(72,72,72));
  button(dc,{rx+10,top+49,rx+right/2-4,top+79},L"+ Camada");
  button(dc,{rx+right/2+2,top+49,w-10,top+79},L"Duplicar");
  button(dc,{rx+10,top+84,rx+right/2-4,top+112},L"Mostrar/ocultar");
  button(dc,{rx+right/2+2,top+84,w-10,top+112},L"Remover");
  line(dc,rx,top+122,w,top+122,RGB(72,72,72));
  if(g_document){
   const auto& layers=g_document->layers();
   int row_y=top+130;
   for(size_t i=layers.size();i>0;--i){
    size_t idx=i-1; int yy=row_y+(int)(layers.size()-i)*34;
    if(yy+30>h-bottom-8)break;
    RECT lr{rx+8,yy,w-8,yy+30};fill(dc,lr,idx==g_selected_layer?RGB(57,91,125):PANEL);
    label(dc,rx+14,yy+8,layers[idx].visible?L"◉":L"○",layers[idx].visible?ACCENT:MUTED,12,true);
    std::wstring lname=utf8_to_wide(layers[idx].name);if(lname.empty()&&!layers[idx].name.empty())lname=L"(nome inválido)";
    if(lname.size()>20)lname.resize(20);
    label(dc,rx+38,yy+8,lname,TEXT,12,idx==g_selected_layer);
   }
   label(dc,rx+14,h-bottom-78,L"Opacidade",MUTED,12);
   int bar_y=h-bottom-54;fill(dc,{rx+14,bar_y,w-14,bar_y+5},RGB(90,90,90));
   int fillw=(right-28)*(layers.empty()?0:layers[g_selected_layer].opacity)/255;
   fill(dc,{rx+14,bar_y,rx+14+fillw,bar_y+5},ACCENT);
   label(dc,rx+14,h-bottom-32,L"Selecionar camada · clique na barra para opacidade",MUTED,10);
  } else label(dc,rx+14,top+140,L"Abra uma imagem para criar camadas",MUTED,12);
 }
 int cx=left, cw=std::max(0,w-left-right); fill(dc,{cx,top,cx+cw,h-bottom},RGB(64,64,64)); // subtle checkerboard behind canvas
 for(int y=top;y<h-bottom;y+=24)for(int x=cx;x<cx+cw;x+=24)if((((x-cx)/24)+((y-top)/24))%2==0)fill(dc,{x,y,std::min(x+24,cx+cw),std::min(y+24,h-bottom)},RGB(69,69,69));
 if(g_image && cw>20 && usableH>20){ double maxW=std::max(1,cw-48), maxH=std::max(1,usableH-48); double scale=std::min(maxW/g_image->GetWidth(),maxH/g_image->GetHeight())*g_zoom; scale=std::max(0.01,std::min(scale,8.0)); int iw=(int)(g_image->GetWidth()*scale), ih=(int)(g_image->GetHeight()*scale); int x=cx+(cw-iw)/2,y=top+(usableH-ih)/2; Graphics gr(dc); gr.SetInterpolationMode(InterpolationModeHighQualityBicubic); gr.DrawImage(g_image.get(),Rect(x,y,iw,ih)); if(g_mockup_design){int ow=0,oh=0;mockup_dimensions(ow,oh);int ox=x+(int)((double)(((int)g_image->GetWidth()-ow)/2+g_mockup_dx)*scale);int oy=y+(int)((double)(((int)g_image->GetHeight()-oh)/2+g_mockup_dy)*scale);int sw=std::max(1,(int)(ow*scale)),sh=std::max(1,(int)(oh*scale));gr.DrawImage(g_mockup_design.get(),Rect(ox,oy,sw,sh));Pen outline(Color(255,100,180,255),1.0f);outline.SetDashStyle(DashStyleDash);gr.DrawRectangle(&outline,Rect(ox,oy,sw,sh));} label(dc,cx+12,h-bottom-26,std::to_wstring(g_image->GetWidth())+L" × "+std::to_wstring(g_image->GetHeight())+L" px",TEXT,11); if(g_text_capturing && !g_text_input.empty()){
  auto preview_family=std::make_unique<FontFamily>(g_text_logfont.lfFaceName,&g_private_fonts);
  if(preview_family->GetLastStatus()!=Ok) preview_family=std::make_unique<FontFamily>(g_text_logfont.lfFaceName);
  if(preview_family->GetLastStatus()==Ok){
   INT preview_style=FontStyleRegular;
   if(g_text_logfont.lfWeight>=FW_BOLD) preview_style|=FontStyleBold;
   if(g_text_logfont.lfItalic) preview_style|=FontStyleItalic;
   if(g_text_logfont.lfUnderline) preview_style|=FontStyleUnderline;
   if(g_text_logfont.lfStrikeOut) preview_style|=FontStyleStrikeout;
   REAL preview_size=std::max(1.0f,(REAL)std::clamp(std::abs(g_text_logfont.lfHeight),1L,512L)*(REAL)scale);
   Font preview_font(preview_family.get(),preview_size,preview_style,UnitPixel);
   SolidBrush preview_brush(Color(255,GetRValue(g_text_color),GetGValue(g_text_color),GetBValue(g_text_color)));
   PointF preview_origin((REAL)g_text_screen_x,(REAL)g_text_screen_y);
   gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
   gr.DrawString(g_text_input.c_str(),(INT)g_text_input.size(),&preview_font,preview_origin,&preview_brush);
  }
 } }
 else { label(dc,cx+std::max(12,cw/2-110),top+std::max(20,usableH/2-12),L"Abra uma imagem para começar",RGB(210,210,210),17,true); label(dc,cx+std::max(12,cw/2-138),top+std::max(48,usableH/2+20),L"PNG, JPEG, BMP, TIFF (preview nesta versão)",MUTED,12); }
}
LRESULT CALLBACK wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_SIZE:InvalidateRect(hwnd,nullptr,FALSE);return 0;case WM_KEYDOWN:if(g_text_capturing){if(wp==VK_RETURN){if(GetKeyState(VK_CONTROL)&0x8000)commit_text();else if(g_text_input.size()<2048)g_text_input.push_back(L"\n"[0]);InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(wp==VK_ESCAPE){g_text_capturing=false;g_text_input.clear();InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(wp==VK_BACK){if(!g_text_input.empty()){if(g_text_input.size()>=2&&g_text_input[g_text_input.size()-1]>=0xDC00&&g_text_input[g_text_input.size()-1]<=0xDFFF&&g_text_input[g_text_input.size()-2]>=0xD800&&g_text_input[g_text_input.size()-2]<=0xDBFF)g_text_input.resize(g_text_input.size()-2);else g_text_input.pop_back();}InvalidateRect(hwnd,nullptr,FALSE);return 0;}return 0;}if(g_mockup_design){if(wp==VK_ESCAPE){cancel_mockup();return 0;}if(wp==VK_RETURN){commit_mockup();return 0;}if(wp==VK_LEFT)g_mockup_dx-=5;if(wp==VK_RIGHT)g_mockup_dx+=5;if(wp==VK_UP)g_mockup_dy-=5;if(wp==VK_DOWN)g_mockup_dy+=5;if(wp==VK_ADD||wp==VK_OEM_PLUS)g_mockup_scale=std::min(1.5f,g_mockup_scale+0.03f);if(wp==VK_SUBTRACT||wp==VK_OEM_MINUS)g_mockup_scale=std::max(0.05f,g_mockup_scale-0.03f);InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(wp==VK_F4){g_showLayers=!g_showLayers;InvalidateRect(hwnd,nullptr,TRUE);return 0;}if(wp==VK_F3){g_showTools=!g_showTools;InvalidateRect(hwnd,nullptr,TRUE);return 0;}if(wp=='O' && (GetKeyState(VK_CONTROL)&0x8000)){if(GetKeyState(VK_SHIFT)&0x8000)open_project();else open_image();return 0;}if(wp==VK_ESCAPE){g_zoom=1.0f;InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(wp=='Z'&&(GetKeyState(VK_CONTROL)&0x8000)){undo_image();return 0;}if(wp=='Y'&&(GetKeyState(VK_CONTROL)&0x8000)){redo_image();return 0;}if(wp=='S'&&(GetKeyState(VK_CONTROL)&0x8000)){if(GetKeyState(VK_SHIFT)&0x8000)save_project();else save_image();return 0;}return 0;case WM_CHAR:if(g_text_capturing){if(wp>=32 && wp!=127 && g_text_input.size()<2048)g_text_input.push_back((wchar_t)wp);InvalidateRect(hwnd,nullptr,FALSE);return 0;}break;case WM_LBUTTONUP:{int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);{RECT client{};GetClientRect(hwnd,&client);int client_w=client.right;bool compact=client_w<860,tiny=client_w<570;int tool_panel_w=g_showTools?(tiny?0:(compact?44:190)):0;int right_panel_w=g_showLayers?(tiny?0:(compact?0:230)):0;if(client_w-tool_panel_w-right_panel_w<160){right_panel_w=0;tool_panel_w=g_showTools?36:0;}int tool=-1;if(tool_panel_w>50&&x>=10&&x<tool_panel_w-10){for(int i=0;i<10;i++){int yy=76+44+i*35;if(y>=yy&&y<yy+28){tool=i;break;}}}else if(tool_panel_w>0&&tool_panel_w<=50&&x>=7&&x<tool_panel_w-7){for(int i=0;i<8;i++){int yy=76+12+i*42;if(y>=yy&&y<yy+30){tool=i;break;}}}if(tool>=0){g_active_tool=tool;if(tool==6){if(!g_text_mode){if(choose_text_style())g_text_mode=true;}else g_text_mode=false;g_text_capturing=false;g_text_input.clear();}InvalidateRect(hwnd,nullptr,FALSE);return 0;}}if(g_text_mode && g_image && y>=76){RECT client{};GetClientRect(hwnd,&client);int w=client.right,h=client.bottom;int bottom=26,top=76;bool compact=w<860,tiny=w<570;int left=g_showTools?(tiny?0:(compact?44:190)):0;int right=g_showLayers?(tiny?0:(compact?0:230)):0;if(w-left-right<160){right=0;left=g_showTools?36:0;}int cw=std::max(0,w-left-right),usableH=std::max(0,h-top-bottom);if(x>=left&&x<left+cw&&cw>20&&usableH>20){double scale=std::min((double)std::max(1,cw-48)/g_image->GetWidth(),(double)std::max(1,usableH-48)/g_image->GetHeight())*g_zoom;scale=std::max(0.01,std::min(scale,8.0));int iw=(int)(g_image->GetWidth()*scale),ih=(int)(g_image->GetHeight()*scale);int ix=left+(cw-iw)/2,iy=top+(usableH-ih)/2;if(x>=ix&&x<=ix+iw&&y>=iy&&y<=iy+ih){g_text_image_x=std::clamp((int)((x-ix)/scale),0,(int)g_image->GetWidth()-1);g_text_image_y=std::clamp((int)((y-iy)/scale),0,(int)g_image->GetHeight()-1);g_text_screen_x=x;g_text_screen_y=y;g_text_input.clear();g_text_capturing=true;SetFocus(hwnd);InvalidateRect(hwnd,nullptr,FALSE);return 0;}}}RECT client{};GetClientRect(hwnd,&client);int client_w=client.right;if(g_showLayers&&x>=client_w-230&&client_w>=860&&y>=125){
 int rx=client_w-230;
 if(y>=125&&y<155&&x>=rx+8&&x<rx+230){if(x<rx+115)add_transparent_layer("Layer");else duplicate_selected_layer();return 0;}
 if(y>=160&&y<188&&x>=rx+8&&x<rx+230){if(x<rx+115)toggle_selected_visibility();else remove_selected_layer();return 0;}
 if(g_document&&g_document->layers().size()>0){
  int row_y=76+130;
  for(size_t i=g_document->layers().size();i>0;--i){size_t idx=i-1;int yy=row_y+(int)(g_document->layers().size()-i)*34;if(yy+30>=client.bottom-26-8)break;if(y>=yy&&y<yy+30){g_selected_layer=idx;InvalidateRect(hwnd,nullptr,FALSE);return 0;}}
  int bar_y=client.bottom-26-54;
  if(y>=bar_y-5&&y<=bar_y+10){int range=std::max(1,client_w-230-28);int value=std::clamp((x-(rx+14))*255/range,0,255);g_document->set_layer_opacity(g_selected_layer,(uint8_t)value);g_document->checkpoint("Change layer opacity");render_document();return 0;}
 }
}
if(y>=43&&y<=67&&x>=944&&x<=1020)open_project();else if(y>=43&&y<=67&&x>=1026&&x<=1104)save_project();else if(y>=43&&y<=67&&x>=12&&x<=86)open_image();else if(y>=43&&y<=67&&x>=94&&x<=168)save_image();else if(y>=43&&y<=67&&x>=176&&x<=252)undo_image();else if(y>=43&&y<=67&&x>=260&&x<=338)redo_image();else if(y>=43&&y<=67&&x>=346&&x<=430)apply_tone(false);else if(y>=43&&y<=67&&x>=438&&x<=522)apply_tone(true);else if(y>=43&&y<=67&&x>=530&&x<=614)start_mockup();else if(g_mockup_design&&y>=43&&y<=67&&x>=622&&x<=704)commit_mockup();else if(g_mockup_design&&y>=43&&y<=67&&x>=712&&x<=794)cancel_mockup();else if(!g_mockup_design&&y>=43&&y<=67&&x>=622&&x<=696)rotate_image(false);else if(!g_mockup_design&&y>=43&&y<=67&&x>=702&&x<=776)rotate_image(true);else if(!g_mockup_design&&y>=43&&y<=67&&x>=782&&x<=856)flip_image(true);else if(!g_mockup_design&&y>=43&&y<=67&&x>=862&&x<=936)flip_image(false);return 0;}case WM_MOUSEWHEEL:{short d=GET_WHEEL_DELTA_WPARAM(wp);if(g_mockup_design)g_mockup_scale=std::clamp(g_mockup_scale+(d>0?0.03f:-0.03f),0.05f,1.5f);else g_zoom=std::clamp(g_zoom+(d>0?0.1f:-0.1f),0.1f,4.0f);InvalidateRect(hwnd,nullptr,FALSE);return 0;}case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);draw_ui(dc,r);EndPaint(hwnd,&ps);return 0;}case WM_DESTROY:unload_custom_fonts();PostQuitMessage(0);return 0;}return DefWindowProcW(hwnd,msg,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE, PWSTR,int show){GdiplusStartupInput gsi;if(GdiplusStartup(&g_gdiplus,&gsi,nullptr)!=Ok)return 1; load_custom_fonts(); SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); WNDCLASSW wc{};wc.lpfnWndProc=wndproc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);wc.lpszClassName=L"PSTouchPortableWindow";wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);if(!RegisterClassW(&wc)){GdiplusShutdown(g_gdiplus);return 2;}RECT r{0,0,1280,800};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);g_hwnd=CreateWindowW(wc.lpszClassName,L"PS Touch PC — Portable",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!g_hwnd){GdiplusShutdown(g_gdiplus);return 3;}ShowWindow(g_hwnd,show);UpdateWindow(g_hwnd);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}GdiplusShutdown(g_gdiplus);return (int)msg.wParam;}
