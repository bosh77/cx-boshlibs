
//CODICE DA CREARE PER SISTEMA OPERATIVO MAC OS, IDENTICO A blMenus-win.cpp

#import <Cocoa/Cocoa.h>
#include <vector>

struct blMenuEntry{ int id; NSMenu* handle; };
struct blMenuItemEntry{ int id; NSMenuItem* handle; };
static int g_blMenuNextId=1000,g_blPendingMenuCommand=0;
static std::vector<blMenuEntry> g_blMenus; static std::vector<blMenuItemEntry> g_blMenuItems;

@interface blMenuTarget:NSObject -(void)onMenu:(id)sender; @end
@implementation blMenuTarget -(void)onMenu:(id)sender{ g_blPendingMenuCommand=(int)[sender tag]; } @end
static blMenuTarget* g_blMenuTarget=nil;

static NSString* bb2ns(String s){ char* c=convertBBString(s); NSString* n=c?[NSString stringWithUTF8String:c]:@""; if(c) free(c); return n?n:@""; }
static void blEnsure(){ if(!g_blMenuTarget) g_blMenuTarget=[blMenuTarget new]; [NSApplication sharedApplication]; }
static NSMenu* blBar(){ blEnsure(); NSMenu* m=[NSApp mainMenu]; if(!m){ m=[[NSMenu alloc] initWithTitle:@""]; [NSApp setMainMenu:m]; } return m; }
static NSMenu* blGetMenu(int id){ for(auto &m:g_blMenus) if(m.id==id) return m.handle; return nil; }
static NSMenuItem* blGetItem(int id){ for(auto &it:g_blMenuItems) if(it.id==id) return it.handle; return nil; }
static int blAddItemCore(int menuId,String title,NSString* icon){ NSMenu* m=blGetMenu(menuId); if(!m) return 0; int id=g_blMenuNextId++; NSMenuItem* it=[[NSMenuItem alloc] initWithTitle:bb2ns(title) action:@selector(onMenu:) keyEquivalent:@""]; [it setTarget:g_blMenuTarget]; [it setTag:id]; if([icon length]){ NSImage* img=[[NSImage alloc] initWithContentsOfFile:icon]; if(img) [it setImage:img]; } [m addItem:it]; blMenuItemEntry entry; entry.id=id; entry.handle=it; g_blMenuItems.push_back(entry); return id; }

int _AddMenu(String title){ NSMenu* bar=blBar(); NSMenuItem* root=[[NSMenuItem alloc] initWithTitle:bb2ns(title) action:nil keyEquivalent:@""]; NSMenu* sub=[[NSMenu alloc] initWithTitle:bb2ns(title)]; [bar addItem:root]; [bar setSubmenu:sub forItem:root]; int id=g_blMenuNextId++; blMenuEntry entry; entry.id=id; entry.handle=sub; g_blMenus.push_back(entry); return id; }
int _AddSubMenu(int menuId,String title){ NSMenu* parent=blGetMenu(menuId); if(!parent) return 0; NSMenuItem* root=[[NSMenuItem alloc] initWithTitle:bb2ns(title) action:nil keyEquivalent:@""]; NSMenu* sub=[[NSMenu alloc] initWithTitle:bb2ns(title)]; [parent addItem:root]; [parent setSubmenu:sub forItem:root]; int id=g_blMenuNextId++; blMenuEntry entry; entry.id=id; entry.handle=sub; g_blMenus.push_back(entry); return id; }
int _AddMenuItem(int menuId,String title){ return blAddItemCore(menuId,title,@""); }
int _AddMenuItemWithIcon(int menuId,String title,String iconPath){ return blAddItemCore(menuId,title,bb2ns(iconPath)); }
int _AddMenuSeparator(int menuId){ NSMenu* m=blGetMenu(menuId); if(!m) return 0; [m addItem:[NSMenuItem separatorItem]]; return 1; }
int _MenuItemSetEnabled(int itemId,int enabled){ NSMenuItem* it=blGetItem(itemId); if(!it) return 0; [it setEnabled:enabled!=0]; return 1; }
int _MenuItemSetChecked(int itemId,int checked){ NSMenuItem* it=blGetItem(itemId); if(!it) return 0; [it setState:checked?NSControlStateValueOn:NSControlStateValueOff]; return 1; }
int _MenuItemSetRadio(int itemId,int radio){ NSMenuItem* it=blGetItem(itemId); if(!it) return 0; [it setOnStateImage:radio?[NSImage imageNamed:NSImageNameStatusAvailable]:nil]; return 1; }
int _MenuItemClicked(int itemId){ if(g_blPendingMenuCommand==itemId){ g_blPendingMenuCommand=0; return 1; } return 0; }


// Apre la tendina di un menu come menu a comparsa. NSView usa l'origine in
// basso a sinistra, mojo2 in alto a sinistra: la y va rovesciata.
int _ShowPopup(int menuId,int x,int y){
    NSMenu* m=blGetMenu(menuId); if(!m) return 0;
    NSWindow* w=[NSApp keyWindow]; if(!w) return 0;
    NSView* v=[w contentView]; if(!v) return 0;
    NSPoint pt=NSMakePoint(x,[v bounds].size.height-y);
    [m popUpMenuPositioningItem:nil atLocation:pt inView:v];
    return 1;
}


