#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <gdiplus.h>
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
HWND g_hwnd{}; ULONG_PTR g_gdiplus{}; std::unique_ptr<Bitmap> g_image; std::vector<std::wstring> g_loaded_font_paths; std::wstring g_path=L"Nenhuma imagem aberta"; float g_zoom=1.0f; bool g_showLayers=true, g_showTools=true; std::vector<std::unique_ptr<Bitmap>> g_undo, g_redo; bool g_text_mode=false; bool g_text_capturing=false; std::wstring g_text_input; int g_text_image_x=0,g_text_image_y=0,g_text_screen_x=0,g_text_screen_y=0; std::unique_ptr<Bitmap> g_mockup_design; float g_mockup_scale=0.55f; int g_mockup_dx=0,g_mockup_dy=0;
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
  if(AddFontResourceExW(path.c_str(),FR_PRIVATE,nullptr)!=0) g_loaded_font_paths.push_back(path);
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
int encoder_clsid(const WCHAR* mime, CLSID* clsid) {
 UINT count=0,size=0; GetImageEncodersSize(&count,&size); if(!size) return -1;
 auto mem=std::make_unique<BYTE[]>(size); auto info=reinterpret_cast<ImageCodecInfo*>(mem.get());
 if(GetImageEncoders(count,size,info)!=Ok) return -1;
 for(UINT i=0;i<count;i++) if(wcscmp(info[i].MimeType,mime)==0){*clsid=info[i].Clsid;return (int)i;} return -1;
}
void save_image() {
 if(!g_image){MessageBoxW(g_hwnd,L"Abra uma imagem antes de salvar.",L"PS Touch PC",MB_OK|MB_ICONINFORMATION);return;}
 wchar_t path[MAX_PATH]=L"imagem.png"; OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=g_hwnd;
 ofn.lpstrFilter=L"PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg\0Bitmap (*.bmp)\0*.bmp\0TIFF (*.tif)\0*.tif\0";ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;ofn.lpstrDefExt=L"png";
 if(!GetSaveFileNameW(&ofn))return; const wchar_t* mime=L"image/png"; const wchar_t* ext=wcsrchr(path,L'.');
 if(ext && (_wcsicmp(ext,L".jpg")==0||_wcsicmp(ext,L".jpeg")==0))mime=L"image/jpeg";else if(ext&&_wcsicmp(ext,L".bmp")==0)mime=L"image/bmp";else if(ext&&(_wcsicmp(ext,L".tif")==0||_wcsicmp(ext,L".tiff")==0))mime=L"image/tiff";
 CLSID clsid{}; if(encoder_clsid(mime,&clsid)<0||g_image->Save(path,&clsid,nullptr)!=Ok){MessageBoxW(g_hwnd,L"Falha ao salvar a imagem neste formato.",L"PS Touch PC",MB_OK|MB_ICONERROR);return;} g_path=path;InvalidateRect(g_hwnd,nullptr,FALSE);
}
void push_undo(){if(!g_image)return;auto c=copy_bitmap(*g_image);if(c){g_undo.push_back(std::move(c));if(g_undo.size()>20)g_undo.erase(g_undo.begin());}g_redo.clear();}
void undo_image(){if(g_undo.empty()||!g_image)return;auto c=copy_bitmap(*g_image);if(c)g_redo.push_back(std::move(c));g_image=std::move(g_undo.back());g_undo.pop_back();InvalidateRect(g_hwnd,nullptr,FALSE);}
void redo_image(){if(g_redo.empty()||!g_image)return;auto c=copy_bitmap(*g_image);if(c)g_undo.push_back(std::move(c));g_image=std::move(g_redo.back());g_redo.pop_back();InvalidateRect(g_hwnd,nullptr,FALSE);}
void rotate_image(bool clockwise){if(!g_image)return;auto out=copy_bitmap(*g_image);if(!out)return;push_undo();if(out->RotateFlip(clockwise?Rotate90FlipNone:Rotate270FlipNone)!=Ok){if(!g_undo.empty())g_undo.pop_back();return;}g_image=std::move(out);InvalidateRect(g_hwnd,nullptr,FALSE);}
void flip_image(bool horizontal){if(!g_image)return;auto out=copy_bitmap(*g_image);if(!out)return;push_undo();if(out->RotateFlip(horizontal?RotateNoneFlipX:RotateNoneFlipY)!=Ok){if(!g_undo.empty())g_undo.pop_back();return;}g_image=std::move(out);InvalidateRect(g_hwnd,nullptr,FALSE);}
void apply_tone(bool sepia){if(!g_image)return;auto out=copy_bitmap(*g_image);if(!out)return;push_undo();Rect r(0,0,(INT)out->GetWidth(),(INT)out->GetHeight());BitmapData data{};if(out->LockBits(&r,ImageLockModeRead|ImageLockModeWrite,PixelFormat32bppARGB,&data)!=Ok){if(!g_undo.empty())g_undo.pop_back();return;}
 for(INT y=0;y<data.Height;y++){auto row=reinterpret_cast<BYTE*>(data.Scan0)+static_cast<ptrdiff_t>(y)*data.Stride;for(INT x=0;x<data.Width;x++){BYTE* p=row+x*4;double b=p[0],g=p[1],rr=p[2];double nr,ng,nb;if(sepia){nr=0.393*rr+0.769*g+0.189*b;ng=0.349*rr+0.686*g+0.168*b;nb=0.272*rr+0.534*g+0.131*b;}else{double gray=0.299*rr+0.587*g+0.114*b;nr=ng=nb=gray;}p[2]=(BYTE)std::clamp(nr,0.0,255.0);p[1]=(BYTE)std::clamp(ng,0.0,255.0);p[0]=(BYTE)std::clamp(nb,0.0,255.0);}}
 out->UnlockBits(&data);g_image=std::move(out);InvalidateRect(g_hwnd,nullptr,FALSE);}
void open_image() { wchar_t path[MAX_PATH]{}; OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=g_hwnd; ofn.lpstrFilter=L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0All files\0*.*\0"; ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH; ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST; if(GetOpenFileNameW(&ofn)){ auto candidate=std::make_unique<Bitmap>(path); if(candidate->GetLastStatus()==Ok){g_image=std::move(candidate);g_path=path;g_zoom=1.0f;g_undo.clear();g_redo.clear();} else MessageBoxW(g_hwnd,L"Não foi possível abrir esta imagem. Use PNG, JPEG, BMP ou TIFF nesta versão.",L"PS Touch PC",MB_ICONWARNING); InvalidateRect(g_hwnd,nullptr,TRUE); } }
void start_mockup() {
 if(!g_image){MessageBoxW(g_hwnd,L"Abra primeiro uma foto do produto ou uma imagem-base para o mockup.",L"PS Touch PC",MB_OK|MB_ICONINFORMATION);return;}
 wchar_t path[MAX_PATH]{}; OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=g_hwnd;ofn.lpstrFilter=L"Arte/design (PNG recomendado)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0Todos os arquivos\0*.*\0";ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
 if(!GetOpenFileNameW(&ofn))return;auto candidate=std::make_unique<Bitmap>(path);if(candidate->GetLastStatus()!=Ok){MessageBoxW(g_hwnd,L"Não foi possível abrir a arte escolhida.",L"PS Touch PC",MB_OK|MB_ICONWARNING);return;}
 g_mockup_design=std::move(candidate);g_mockup_scale=0.55f;g_mockup_dx=0;g_mockup_dy=0;InvalidateRect(g_hwnd,nullptr,TRUE);
}
void cancel_mockup(){g_mockup_design.reset();g_mockup_dx=g_mockup_dy=0;InvalidateRect(g_hwnd,nullptr,FALSE);}
void mockup_dimensions(int& w,int& h){if(!g_image||!g_mockup_design){w=h=0;return;}double factor=std::min((double)g_image->GetWidth()*0.62/g_mockup_design->GetWidth(),(double)g_image->GetHeight()*0.62/g_mockup_design->GetHeight())*g_mockup_scale/0.55;w=std::max(1,(int)(g_mockup_design->GetWidth()*factor));h=std::max(1,(int)(g_mockup_design->GetHeight()*factor));}
void commit_mockup(){if(!g_image||!g_mockup_design)return;auto out=copy_bitmap(*g_image);if(!out)return;int ow=0,oh=0;mockup_dimensions(ow,oh);int x=((int)out->GetWidth()-ow)/2+g_mockup_dx,y=((int)out->GetHeight()-oh)/2+g_mockup_dy;push_undo();Graphics gr(out.get());gr.SetCompositingMode(CompositingModeSourceOver);gr.SetInterpolationMode(InterpolationModeHighQualityBicubic);if(gr.DrawImage(g_mockup_design.get(),Rect(x,y,ow,oh))!=Ok){if(!g_undo.empty())g_undo.pop_back();MessageBoxW(g_hwnd,L"Falha ao compor o mockup.",L"PS Touch PC",MB_OK|MB_ICONERROR);return;}g_image=std::move(out);g_mockup_design.reset();InvalidateRect(g_hwnd,nullptr,TRUE);}

void commit_text() {
 if(!g_image || g_text_input.empty()) { g_text_capturing=false; g_text_input.clear(); InvalidateRect(g_hwnd,nullptr,FALSE); return; }
 push_undo();
 Graphics gr(g_image.get());
 gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
 FontFamily family(L"Arial");
 Font text_font(&family,32.0f,FontStyleRegular,UnitPixel);
 SolidBrush brush(Color(255,255,255,255));
 PointF origin((REAL)g_text_image_x,(REAL)g_text_image_y);
 if(gr.DrawString(g_text_input.c_str(),(INT)g_text_input.size(),&text_font,origin,&brush)!=Ok && !g_undo.empty()) g_undo.pop_back();
 g_text_capturing=false; g_text_mode=false; g_text_input.clear(); InvalidateRect(g_hwnd,nullptr,FALSE);
}
void draw_ui(HDC dc, RECT c) {
 int w=c.right,h=c.bottom; fill(dc,c,BG);
 // Top menu and command bar remain fixed-height; content below is fully responsive.
 fill(dc,{0,0,w,34},PANEL2); label(dc,14,8,L"PS Touch",RGB(245,245,245),16,true); label(dc,110,10,L"Arquivo   Editar   Imagem   Camada   Selecionar   Filtro   Exibir   |   F3 Ferramentas   F4 Camadas",TEXT,13);
 fill(dc,{0,34,w,76},PANEL); button(dc,{12,43,86,67},L"Abrir",true); button(dc,{94,43,168,67},L"Salvar"); button(dc,{176,43,252,67},L"Desfazer"); button(dc,{260,43,338,67},L"Refazer"); button(dc,{346,43,430,67},L"Cinza"); button(dc,{438,43,522,67},L"Sépia"); button(dc,{530,43,614,67},L"Mockup",g_mockup_design!=nullptr); if(g_mockup_design){button(dc,{622,43,704,67},L"Aplicar",true);button(dc,{712,43,794,67},L"Cancelar");label(dc,804,49,L"Setas mover · +/- tamanho · Enter aplicar",MUTED,11);}else{button(dc,{622,43,696,67},L"Girar ↶");button(dc,{702,43,776,67},L"Girar ↷");button(dc,{782,43,856,67},L"Esp. H");button(dc,{862,43,936,67},L"Esp. V");}label(dc,std::max(960,w-260),49,L"Layout adaptável",MUTED,12);
 const int top=76,bottom=26; fill(dc,{0,h-bottom,w,h},PANEL2); label(dc,12,h-bottom+6,g_text_capturing?L"Digite o texto · Enter confirma · Esc cancela":(g_text_mode?L"Ferramenta Texto ativa · clique na imagem":g_path),MUTED,11); label(dc,std::max(250,w-250),h-bottom+6,L"Fontes detectadas: "+std::to_wstring(g_loaded_font_paths.size()),MUTED,11);
 int usableH=std::max(0,h-top-bottom); bool compact=w<860; bool tiny=w<570; int left=g_showTools?(tiny?0:(compact?44:190)):0; int right=g_showLayers?(tiny?0:(compact?0:230)):0; if(w-left-right<160){right=0;left= g_showTools?36:0;}
 if(left>0){fill(dc,{0,top,left,h-bottom},PANEL2); if(left>50){label(dc,14,top+14,L"Ferramentas",TEXT,13,true); const wchar_t* tools[]={L"Mover",L"Seleção",L"Laço",L"Pincel",L"Borracha",L"Preenchimento",L"Texto",L"Cortar",L"Conta-gotas",L"Mão"}; for(int i=0;i<10;i++){int yy=top+44+i*35; RECT r{10,yy,left-10,yy+28}; button(dc,r,tools[i],i==3);} } else {for(int i=0;i<8;i++){int yy=top+12+i*42; RECT r{7,yy,left-7,yy+30}; fill(dc,r,i==3?ACCENT:PANEL); label(dc,14,yy+7,std::to_wstring(i+1),TEXT,13,true);}} }
 if(right>0){int rx=w-right; fill(dc,{rx,top,w,h-bottom},PANEL2); label(dc,rx+14,top+14,L"Camadas",TEXT,14,true); line(dc,rx,top+40,w,top+40,RGB(72,72,72)); button(dc,{rx+10,top+49,w-10,top+79},L"+  Nova camada"); label(dc,rx+14,top+96,L"Opacidade",MUTED,12); fill(dc,{rx+14,top+120,w-14,top+124},RGB(90,90,90)); fill(dc,{rx+14,top+120,rx+14+(right-28)*3/4,top+124},ACCENT); label(dc,rx+14,top+144,L"Normal",TEXT,12); line(dc,rx,top+170,w,top+170,RGB(72,72,72)); label(dc,rx+14,top+186,L"Camada 1",TEXT,13); label(dc,rx+14,top+212,L"Fundo",MUTED,12); label(dc,rx+14,top+254,L"Propriedades",TEXT,13,true); label(dc,rx+14,top+281,L"Posição",MUTED,12); label(dc,rx+14,top+304,L"Tamanho",MUTED,12); }
 int cx=left, cw=std::max(0,w-left-right); fill(dc,{cx,top,cx+cw,h-bottom},RGB(64,64,64)); // subtle checkerboard behind canvas
 for(int y=top;y<h-bottom;y+=24)for(int x=cx;x<cx+cw;x+=24)if((((x-cx)/24)+((y-top)/24))%2==0)fill(dc,{x,y,std::min(x+24,cx+cw),std::min(y+24,h-bottom)},RGB(69,69,69));
 if(g_image && cw>20 && usableH>20){ double maxW=std::max(1,cw-48), maxH=std::max(1,usableH-48); double scale=std::min(maxW/g_image->GetWidth(),maxH/g_image->GetHeight())*g_zoom; scale=std::max(0.01,std::min(scale,8.0)); int iw=(int)(g_image->GetWidth()*scale), ih=(int)(g_image->GetHeight()*scale); int x=cx+(cw-iw)/2,y=top+(usableH-ih)/2; Graphics gr(dc); gr.SetInterpolationMode(InterpolationModeHighQualityBicubic); gr.DrawImage(g_image.get(),Rect(x,y,iw,ih)); if(g_mockup_design){int ow=0,oh=0;mockup_dimensions(ow,oh);int ox=x+(int)((double)(((int)g_image->GetWidth()-ow)/2+g_mockup_dx)*scale);int oy=y+(int)((double)(((int)g_image->GetHeight()-oh)/2+g_mockup_dy)*scale);int sw=std::max(1,(int)(ow*scale)),sh=std::max(1,(int)(oh*scale));gr.DrawImage(g_mockup_design.get(),Rect(ox,oy,sw,sh));Pen outline(Color(255,100,180,255),1.0f);outline.SetDashStyle(DashStyleDash);gr.DrawRectangle(&outline,Rect(ox,oy,sw,sh));} label(dc,cx+12,h-bottom-26,std::to_wstring(g_image->GetWidth())+L" × "+std::to_wstring(g_image->GetHeight())+L" px",TEXT,11); if(g_text_capturing && !g_text_input.empty()) label(dc,g_text_screen_x,g_text_screen_y,g_text_input,RGB(255,255,255),24,true); }
 else { label(dc,cx+std::max(12,cw/2-110),top+std::max(20,usableH/2-12),L"Abra uma imagem para começar",RGB(210,210,210),17,true); label(dc,cx+std::max(12,cw/2-138),top+std::max(48,usableH/2+20),L"PNG, JPEG, BMP, TIFF (preview nesta versão)",MUTED,12); }
}
LRESULT CALLBACK wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_SIZE:InvalidateRect(hwnd,nullptr,FALSE);return 0;case WM_KEYDOWN:if(g_text_capturing){if(wp==VK_RETURN){commit_text();return 0;}if(wp==VK_ESCAPE){g_text_capturing=false;g_text_input.clear();InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(wp==VK_BACK){if(!g_text_input.empty())g_text_input.pop_back();InvalidateRect(hwnd,nullptr,FALSE);return 0;}return 0;}if(g_mockup_design){if(wp==VK_ESCAPE){cancel_mockup();return 0;}if(wp==VK_RETURN){commit_mockup();return 0;}if(wp==VK_LEFT)g_mockup_dx-=5;if(wp==VK_RIGHT)g_mockup_dx+=5;if(wp==VK_UP)g_mockup_dy-=5;if(wp==VK_DOWN)g_mockup_dy+=5;if(wp==VK_ADD||wp==VK_OEM_PLUS)g_mockup_scale=std::min(1.5f,g_mockup_scale+0.03f);if(wp==VK_SUBTRACT||wp==VK_OEM_MINUS)g_mockup_scale=std::max(0.05f,g_mockup_scale-0.03f);InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(wp==VK_F4){g_showLayers=!g_showLayers;InvalidateRect(hwnd,nullptr,TRUE);return 0;}if(wp==VK_F3){g_showTools=!g_showTools;InvalidateRect(hwnd,nullptr,TRUE);return 0;}if(wp=='O' && (GetKeyState(VK_CONTROL)&0x8000)){open_image();return 0;}if(wp==VK_ESCAPE){g_zoom=1.0f;InvalidateRect(hwnd,nullptr,FALSE);return 0;}if(wp=='Z'&&(GetKeyState(VK_CONTROL)&0x8000)){undo_image();return 0;}if(wp=='Y'&&(GetKeyState(VK_CONTROL)&0x8000)){redo_image();return 0;}if(wp=='S'&&(GetKeyState(VK_CONTROL)&0x8000)){save_image();return 0;}return 0;case WM_CHAR:if(g_text_capturing){if(wp>=32 && wp!=127 && g_text_input.size()<512)g_text_input.push_back((wchar_t)wp);InvalidateRect(hwnd,nullptr,FALSE);return 0;}break;case WM_LBUTTONUP:{int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);if(y>=120 && y<426 && x<190 && x>=10){int tool=(y-120)/35;if(tool==6){g_text_mode=!g_text_mode;g_text_capturing=false;g_text_input.clear();InvalidateRect(hwnd,nullptr,FALSE);}return 0;}if(g_text_mode && g_image && y>=76){RECT client{};GetClientRect(hwnd,&client);int w=client.right,h=client.bottom;int bottom=26,top=76;bool compact=w<860,tiny=w<570;int left=g_showTools?(tiny?0:(compact?44:190)):0;int right=g_showLayers?(tiny?0:(compact?0:230)):0;if(w-left-right<160){right=0;left=g_showTools?36:0;}int cw=std::max(0,w-left-right),usableH=std::max(0,h-top-bottom);if(x>=left&&x<left+cw&&cw>20&&usableH>20){double scale=std::min((double)std::max(1,cw-48)/g_image->GetWidth(),(double)std::max(1,usableH-48)/g_image->GetHeight())*g_zoom;scale=std::max(0.01,std::min(scale,8.0));int iw=(int)(g_image->GetWidth()*scale),ih=(int)(g_image->GetHeight()*scale);int ix=left+(cw-iw)/2,iy=top+(usableH-ih)/2;if(x>=ix&&x<=ix+iw&&y>=iy&&y<=iy+ih){g_text_image_x=std::clamp((int)((x-ix)/scale),0,(int)g_image->GetWidth()-1);g_text_image_y=std::clamp((int)((y-iy)/scale),0,(int)g_image->GetHeight()-1);g_text_screen_x=x;g_text_screen_y=y;g_text_input.clear();g_text_capturing=true;SetFocus(hwnd);InvalidateRect(hwnd,nullptr,FALSE);return 0;}}}if(y>=43&&y<=67&&x>=12&&x<=86)open_image();else if(y>=43&&y<=67&&x>=94&&x<=168)save_image();else if(y>=43&&y<=67&&x>=176&&x<=252)undo_image();else if(y>=43&&y<=67&&x>=260&&x<=338)redo_image();else if(y>=43&&y<=67&&x>=346&&x<=430)apply_tone(false);else if(y>=43&&y<=67&&x>=438&&x<=522)apply_tone(true);else if(y>=43&&y<=67&&x>=530&&x<=614)start_mockup();else if(g_mockup_design&&y>=43&&y<=67&&x>=622&&x<=704)commit_mockup();else if(g_mockup_design&&y>=43&&y<=67&&x>=712&&x<=794)cancel_mockup();else if(!g_mockup_design&&y>=43&&y<=67&&x>=622&&x<=696)rotate_image(false);else if(!g_mockup_design&&y>=43&&y<=67&&x>=702&&x<=776)rotate_image(true);else if(!g_mockup_design&&y>=43&&y<=67&&x>=782&&x<=856)flip_image(true);else if(!g_mockup_design&&y>=43&&y<=67&&x>=862&&x<=936)flip_image(false);return 0;}case WM_MOUSEWHEEL:{short d=GET_WHEEL_DELTA_WPARAM(wp);if(g_mockup_design)g_mockup_scale=std::clamp(g_mockup_scale+(d>0?0.03f:-0.03f),0.05f,1.5f);else g_zoom=std::clamp(g_zoom+(d>0?0.1f:-0.1f),0.1f,4.0f);InvalidateRect(hwnd,nullptr,FALSE);return 0;}case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);draw_ui(dc,r);EndPaint(hwnd,&ps);return 0;}case WM_DESTROY:unload_custom_fonts();PostQuitMessage(0);return 0;}return DefWindowProcW(hwnd,msg,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE, PWSTR,int show){GdiplusStartupInput gsi;if(GdiplusStartup(&g_gdiplus,&gsi,nullptr)!=Ok)return 1; load_custom_fonts(); SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); WNDCLASSW wc{};wc.lpfnWndProc=wndproc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);wc.lpszClassName=L"PSTouchPortableWindow";wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);if(!RegisterClassW(&wc)){GdiplusShutdown(g_gdiplus);return 2;}RECT r{0,0,1280,800};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);g_hwnd=CreateWindowW(wc.lpszClassName,L"PS Touch PC — Portable",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!g_hwnd){GdiplusShutdown(g_gdiplus);return 3;}ShowWindow(g_hwnd,show);UpdateWindow(g_hwnd);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}GdiplusShutdown(g_gdiplus);return (int)msg.wParam;}
