
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <string>
#include <vector>
#include <gdiplus.h>
using namespace Gdiplus;

struct blMenuEntry{ int id; HMENU handle; };
struct blMenuItemEntry{ int id; HMENU parent; };
static int g_blMenuNextId=1000,g_blPendingMenuCommand=0;
static HWND g_blMenuWindow=0; static WNDPROC g_blMenuOldProc=0;
static ULONG_PTR g_blMenuGdiToken=0;
static bool g_blMenuGdiReady=false;
static std::vector<blMenuEntry> g_blMenus; static std::vector<blMenuItemEntry> g_blMenuItems; static std::vector<HBITMAP> g_blMenuBitmaps;
// La finestra e' quella di glfw, non "quella attiva": in OnCreate la finestra
// puo' non avere ancora il fuoco, e GetActiveWindow()/GetForegroundWindow()
// restituirebbero 0 o - peggio - la finestra di un altro programma, con il
// menu che finisce attaccato a quella. Il nativo Linux prende gia' la finestra
// glfw allo stesso modo.
static HWND blGetMenuWindow(){
    GLFWwindow* w = BBGlfwGame::GlfwGame() ? BBGlfwGame::GlfwGame()->GetGLFWwindow() : 0;
    if( w ){ HWND h = glfwGetWin32Window( w ); if( h ) return h; }
    HWND hWnd=GetActiveWindow(); if(!hWnd) hWnd=GetForegroundWindow(); return hWnd;
}
static HMENU blGetMenuHandle(int menuId){ for(auto &m:g_blMenus) if(m.id==menuId) return m.handle; return 0; }
static HMENU blGetMenuItemParent(int itemId){ for(auto &it:g_blMenuItems) if(it.id==itemId) return it.parent; return 0; }
static bool blIsKnownMenuItem(int itemId){ return blGetMenuItemParent(itemId)!=0; }
// Una barra dei menu di Windows NON sta nella cornice: si mangia una fetta di
// CLIENT AREA. SetMenu lascia la finestra grande uguale e abbassa di ~20 pixel
// il bordo alto del client, ma NON fa arrivare un WM_SIZE vero: mojo resta con
// le misure di prima e continua a disegnare con l'altezza vecchia ancorata in
// basso (in OpenGL l'origine e' in basso a sinistra), cosi' tutto il disegno
// scivola giu' di quanto e' alta la barra. Il mouse invece resta giusto, perche'
// glfw lo rilegge ogni volta dal client vero: si vede una cosa e si clicca venti
// pixel piu' in alto. Misurato il 2026-09-06 su blmix-example, dove la X di
// chiusura di una linguetta non si accendeva col mouse sopra.
//
// Il rimedio e' il minimo che basta: un ridimensionamento vero di un pixel e
// subito indietro. Windows manda il WM_SIZE, glfw lo gira a mojo, che rifa' i
// suoi conti sul client nuovo - e la finestra resta esattamente com'era. NON si
// tocca la misura della finestra per restituire i pixel persi: provato, e a
// client cosi' cambiato il layout di blScreens perde la linguetta del primo
// foglio. Qui si corregge lo sfasamento e basta.
static int blClientH(HWND hWnd){ RECT c; if(!GetClientRect(hWnd,&c)) return 0; return c.bottom-c.top; }
static void blPokeResize(HWND hWnd){
    RECT w; if(!GetWindowRect(hWnd,&w)) return;
    int ww=w.right-w.left, wh=w.bottom-w.top;
    if(ww<2||wh<2) return;
    SetWindowPos(hWnd,0,0,0,ww,wh-1,SWP_NOMOVE|SWP_NOZORDER);
    SetWindowPos(hWnd,0,0,0,ww,wh,SWP_NOMOVE|SWP_NOZORDER);
}
static void blRefreshMenuBar(HWND hWnd){ int h0=blClientH(hWnd); SetMenu(hWnd,GetMenu(hWnd)); DrawMenuBar(hWnd); SetWindowPos(hWnd,0,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED); if(blClientH(hWnd)!=h0) blPokeResize(hWnd); }
static LRESULT CALLBACK blMenuProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){ if(msg==WM_COMMAND){ int id=LOWORD(wParam); if(blIsKnownMenuItem(id)){ g_blPendingMenuCommand=id; return 0; } } return CallWindowProc(g_blMenuOldProc,hWnd,msg,wParam,lParam); }
static void blEnsureMenuHook(HWND hWnd){ if(!hWnd||g_blMenuWindow==hWnd) return; g_blMenuOldProc=(WNDPROC)SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LONG_PTR)blMenuProc); g_blMenuWindow=hWnd; }
static void blEnsureGdiPlus(){ if(g_blMenuGdiReady) return; GdiplusStartupInput in; if(GdiplusStartup(&g_blMenuGdiToken,&in,0)==Ok) g_blMenuGdiReady=true; }
static void blEnableMenuBitmaps(HMENU m){ MENUINFO mi={}; mi.cbSize=sizeof(mi); mi.fMask=MIM_STYLE; mi.dwStyle=MNS_CHECKORBMP; SetMenuInfo(m,&mi); }
static HBITMAP blLoadMenuBitmap(String path){ std::wstring p(path.Data(),path.Length()); if(p.empty()) return 0; blEnsureGdiPlus(); if(!g_blMenuGdiReady) return 0; Bitmap bmp(p.c_str()); if(bmp.GetLastStatus()!=Ok) return 0; HBITMAP hb=0; if(bmp.GetHBITMAP(Color(255,0,255,255),&hb)!=Ok) return 0; return hb; }
static int blAddMenuItemCore(int menuId,String title,HBITMAP hb){ std::wstring t(title.Data(),title.Length()); if(!menuId||t.empty()) return 0; HMENU m=blGetMenuHandle(menuId); if(!m) return 0; int itemId=g_blMenuNextId++; if(!AppendMenuW(m,MF_STRING,itemId,t.c_str())) return 0; g_blMenuItems.push_back({itemId,m}); if(hb){ MENUITEMINFOW i={}; i.cbSize=sizeof(i); i.fMask=MIIM_BITMAP; i.hbmpItem=hb; SetMenuItemInfoW(m,itemId,FALSE,&i); g_blMenuBitmaps.push_back(hb); } HWND hWnd=blGetMenuWindow(); if(hWnd) blRefreshMenuBar(hWnd); return itemId; }

int _AddMenu(String title){ std::wstring t(title.Data(),title.Length()); if(t.empty()) return 0; HWND hWnd=blGetMenuWindow(); if(!hWnd) return 0; blEnsureMenuHook(hWnd); int h0=blClientH(hWnd); HMENU bar=GetMenu(hWnd); if(!bar){ bar=CreateMenu(); if(!bar) return 0; SetMenu(hWnd,bar); } HMENU pop=CreatePopupMenu(); if(!pop) return 0; blEnableMenuBitmaps(pop); int id=g_blMenuNextId++; if(!AppendMenuW(bar,MF_POPUP,(UINT_PTR)pop,t.c_str())){ DestroyMenu(pop); return 0; } g_blMenus.push_back({id,pop}); blRefreshMenuBar(hWnd); if(blClientH(hWnd)!=h0) blPokeResize(hWnd); return id; }
int _AddSubMenu(int menuId,String title){ std::wstring t(title.Data(),title.Length()); if(!menuId||t.empty()) return 0; HMENU parent=blGetMenuHandle(menuId); if(!parent) return 0; HMENU pop=CreatePopupMenu(); if(!pop) return 0; blEnableMenuBitmaps(pop); int id=g_blMenuNextId++; if(!AppendMenuW(parent,MF_POPUP,(UINT_PTR)pop,t.c_str())){ DestroyMenu(pop); return 0; } g_blMenus.push_back({id,pop}); HWND hWnd=blGetMenuWindow(); if(hWnd) blRefreshMenuBar(hWnd); return id; }
int _AddMenuItem(int menuId,String title){ return blAddMenuItemCore(menuId,title,0); }
int _AddMenuItemWithIcon(int menuId,String title,String iconPath){ return blAddMenuItemCore(menuId,title,blLoadMenuBitmap(iconPath)); }
int _AddMenuSeparator(int menuId){ HMENU m=blGetMenuHandle(menuId); if(!m) return 0; if(!AppendMenuW(m,MF_SEPARATOR,0,0)) return 0; HWND hWnd=blGetMenuWindow(); if(hWnd) blRefreshMenuBar(hWnd); return 1; }
int _MenuItemSetEnabled(int itemId,int enabled){ HMENU m=blGetMenuItemParent(itemId); if(!m) return 0; EnableMenuItem(m,itemId,MF_BYCOMMAND|(enabled?MF_ENABLED:MF_GRAYED)); HWND hWnd=blGetMenuWindow(); if(hWnd) blRefreshMenuBar(hWnd); return 1; }
int _MenuItemSetChecked(int itemId,int checked){ HMENU m=blGetMenuItemParent(itemId); if(!m) return 0; CheckMenuItem(m,itemId,MF_BYCOMMAND|(checked?MF_CHECKED:MF_UNCHECKED)); HWND hWnd=blGetMenuWindow(); if(hWnd) blRefreshMenuBar(hWnd); return 1; }
int _MenuItemSetRadio(int itemId,int radio){ HMENU m=blGetMenuItemParent(itemId); if(!m) return 0; MENUITEMINFOW i={}; i.cbSize=sizeof(i); i.fMask=MIIM_FTYPE; if(!GetMenuItemInfoW(m,itemId,FALSE,&i)) return 0; if(radio) i.fType|=MFT_RADIOCHECK; else i.fType&=~MFT_RADIOCHECK; if(!SetMenuItemInfoW(m,itemId,FALSE,&i)) return 0; HWND hWnd=blGetMenuWindow(); if(hWnd) blRefreshMenuBar(hWnd); return 1; }
int _MenuItemClicked(int itemId){ if(g_blPendingMenuCommand==itemId){ g_blPendingMenuCommand=0; return 1; } return 0; }

// Apre la tendina di un menu come menu a comparsa, nel punto voluto. x,y sono
// in coordinate della finestra (le stesse di xmou/ymou), non dello schermo.
// TrackPopupMenu blocca finche' l'utente non sceglie o annulla: con TPM_RETURNCMD
// il comando torna qui invece di passare da WM_COMMAND, cosi' il risultato si
// legge poi con _MenuItemClicked come per la barra.
int _ShowPopup(int menuId,int x,int y){
    HMENU m=blGetMenuHandle(menuId); if(!m) return 0;
    HWND hWnd=blGetMenuWindow(); if(!hWnd) return 0;
    POINT pt={x,y}; ClientToScreen(hWnd,&pt);
    SetForegroundWindow(hWnd);
    int cmd=TrackPopupMenu(m,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,hWnd,0);
    if(cmd&&blIsKnownMenuItem(cmd)) g_blPendingMenuCommand=cmd;
    return 1;
}



