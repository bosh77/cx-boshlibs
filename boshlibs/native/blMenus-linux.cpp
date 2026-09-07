
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <vector>
#include <string>
#include <algorithm>

// X11/Xlib.h definisce Status/Bool/True/False/None come macro (#define Status
// int, ecc.). transcc mette tutti i .cpp e le classi generate in un solo
// main.cpp: senza l'#undef quei macro restano attivi anche piu' avanti nel
// file e rompono il primo metodo chiamato "Status" (o "Bool"...) che il
// compilatore Cerberus emette dopo, es. BBblHttpRequest::Status() diventa
// testualmente "int()". Qui dentro non servono nella forma macro.
#undef Status
#undef Bool
#undef True
#undef False
#undef None
struct N{int id,p,s,t;std::string x,ic;bool e,c,r;}; struct R{int id,y,h,t;std::string x,ic;bool e,c,r,sep;}; struct P{Window w;int m,x,y,w2,h2,hi;std::vector<R> rs;};
static int gid=1000,gseq=1,gcmd=0,groot=0,hbi=-1; static std::vector<N> ns; static std::vector<P> ps; static std::vector<int> tx,tw,tid; static Display*d=0; static Window pw=0,bw=0; static GC gc; static int mh=24; static bool mb1=false;
static std::string bls(String s){char*c=convertBBString(s);std::string r=c?c:"";if(c) free(c);return r;} static N* gn(int id){for(auto &n:ns) if(n.id==id) return &n; return 0;}
static bool initm(){ if(bw) return true; GLFWwindow*w=BBGlfwGame::GlfwGame()->GetGLFWwindow(); if(!w) return false; d=glfwGetX11Display(); pw=glfwGetX11Window(w); if(!d||!pw) return false; XWindowAttributes a; XGetWindowAttributes(d,pw,&a); bw=XCreateSimpleWindow(d,pw,0,0,a.width,mh,0,0,0xe8e8e8); XSelectInput(d,bw,ExposureMask); XMapWindow(d,bw); gc=XCreateGC(d,bw,0,0); return true; }
static std::vector<N*> kids(int p){ std::vector<N*> v; for(auto &n:ns) if(n.p==p) v.push_back(&n); std::sort(v.begin(),v.end(),[](N*a,N*b){return a->s<b->s;}); return v; }
static void blClosePopupsFrom(int l){ while((int)ps.size()>l){ XDestroyWindow(d,ps.back().w); ps.pop_back(); } if(!l) groot=0; XFlush(d); }
static int barhit(int x,int y){ if(y<0||y>=mh) return -1; for(size_t i=0;i<tid.size();++i) if(x>=tx[i]&&x<tx[i]+tw[i]) return (int)i; return -1; }
static int phit(P&p,int x,int y){ if(x<p.x||x>=p.x+p.w2||y<p.y||y>=p.y+p.h2) return -1; int py=y-p.y; for(size_t i=0;i<p.rs.size();++i) if(!p.rs[i].sep&&py>=p.rs[i].y&&py<p.rs[i].y+p.rs[i].h) return (int)i; return -1; }
static bool pin(P&p,int x,int y){ return x>=p.x&&x<p.x+p.w2&&y>=p.y&&y<p.y+p.h2; }
static bool pindeep(int lev,int x,int y){ for(size_t j=lev+1;j<ps.size();++j) if(pin(ps[j],x,y)) return true; return false; }
static void drawbar(){ if(!initm()) return; XWindowAttributes a; XGetWindowAttributes(d,pw,&a); XResizeWindow(d,bw,a.width,mh); XSetForeground(d,gc,0xe8e8e8); XFillRectangle(d,bw,gc,0,0,a.width,mh); tx.clear();tw.clear();tid.clear(); int x=8; for(auto *n:kids(0)){ tx.push_back(x); tid.push_back(n->id); int w=n->x.size()*8+20; tw.push_back(w); if(hbi==(int)tid.size()-1||groot==n->id){ XSetForeground(d,gc,0xc8dcff); XFillRectangle(d,bw,gc,x-4,3,w,18); } XSetForeground(d,gc,0x202020); XDrawString(d,bw,gc,x,16,n->x.c_str(),n->x.size()); x+=w; } XFlush(d); }
static void drawico(Window w,const std::string&fp,int dx,int dy){ if(fp.empty()) return; int iw=0,ih=0,idp=0; unsigned char*src=BBGlfwGame::GlfwGame()->LoadImageData(String(fp.c_str()),&iw,&ih,&idp); if(!src||iw<=0||ih<=0) return; int sw=16,sh=16; char*buf=(char*)malloc(sw*sh*4); if(!buf){ free(src); return; } for(int y=0;y<sh;++y) for(int x=0;x<sw;++x){ int sx=x*iw/sw,sy=y*ih/sh,si=(sy*iw+sx)*idp; unsigned char r=src[si],g=src[si+1],b=src[si+2],a=idp>3?src[si+3]:255; ((unsigned int*)buf)[y*sw+x]=a<20?0x00f8f8f8:((unsigned int)b)|((unsigned int)g<<8)|((unsigned int)r<<16); } XImage*im=XCreateImage(d,DefaultVisual(d,DefaultScreen(d)),DefaultDepth(d,DefaultScreen(d)),ZPixmap,0,buf,sw,sh,32,0); if(im){ XPutImage(d,w,gc,im,0,0,dx,dy,sw,sh); im->data=buf; XDestroyImage(im); } free(src); }
static void drawp(P&p){ XSetForeground(d,gc,0xf8f8f8); XFillRectangle(d,p.w,gc,0,0,p.w2,p.h2); for(size_t i=0;i<p.rs.size();++i){ auto&r=p.rs[i]; if(r.sep){ XSetForeground(d,gc,0x909090); XDrawLine(d,p.w,gc,8,r.y+3,p.w2-8,r.y+3); continue; } if((int)i==p.hi&&r.e){ XSetForeground(d,gc,0xc8dcff); XFillRectangle(d,p.w,gc,2,r.y,p.w2-4,r.h); } drawico(p.w,r.ic,8,r.y+3); std::string t=(r.r?(r.c?"(o) ":"( ) "):(r.c?"[x] ":"[ ] "))+r.x+(r.t==1?" >":""); XSetForeground(d,gc,r.e?0x202020:0x808080); XDrawString(d,p.w,gc,30,r.y+15,t.c_str(),t.size()); } XFlush(d); }
static void openp(int m,int x,int y,int l){ blClosePopupsFrom(l); auto ks=kids(m); if(ks.empty()) return; P p={0,m,x,y,280,0,-1,{}}; int h=4; for(auto*n:ks){ if(n->t==3) p.rs.push_back({0,h,8,3,"","",0,0,0,1}); else p.rs.push_back({n->id,h,22,n->t,n->x,n->ic,n->e,n->c,n->r,0}); h+=p.rs.back().h; } p.h2=h+2; p.w=XCreateSimpleWindow(d,pw,x,y,p.w2,p.h2,1,0x707070,0xf8f8f8); XSelectInput(d,p.w,ExposureMask); XMapRaised(d,p.w); ps.push_back(p); if(!l) groot=m; drawbar(); drawp(ps.back()); }
static void pollm(){ if(!initm()) return; XEvent e; while(XCheckWindowEvent(d,bw,ExposureMask,&e)) if(e.type==Expose) drawbar(); for(auto &p:ps) while(XCheckWindowEvent(d,p.w,ExposureMask,&e)) if(e.type==Expose) drawp(p); Window rr,cr; int rx,ry,wx,wy; unsigned int mask=0; if(!XQueryPointer(d,pw,&rr,&cr,&rx,&ry,&wx,&wy,&mask)) return; int nb=barhit(wx,wy); if(nb!=hbi){ hbi=nb; drawbar(); }
for(size_t i=0;i<ps.size();++i){ int nh=phit(ps[i],wx,wy); if(nh!=ps[i].hi){ ps[i].hi=nh; drawp(ps[i]); } if(nh>=0&&ps[i].rs[nh].t==1){ int sid=ps[i].rs[nh].id; if(i+1>=ps.size()||ps[i+1].m!=sid) openp(sid,ps[i].x+ps[i].w2-2,ps[i].y+ps[i].rs[nh].y,(int)i+1); } else if((int)ps.size()>i+1 && !pindeep((int)i,wx,wy)) blClosePopupsFrom((int)i+1); }
if(!ps.empty()&&nb>=0&&tid[nb]!=groot) openp(tid[nb],tx[nb],mh,0); bool down=(mask&Button1Mask)!=0; if(down&&!mb1){ bool used=0; for(size_t i=0;i<ps.size();++i){ int h=phit(ps[i],wx,wy); if(h>=0){ auto&r=ps[i].rs[h]; if(r.t==2&&r.e) gcmd=r.id; used=1; break; } } if(used|| (nb<0&&ps.empty()==0&&wy>=mh)){ blClosePopupsFrom(0); drawbar(); } else if(nb>=0) openp(tid[nb],tx[nb],mh,0); } mb1=down; }
static int addn(int p,String t,int tp,String ic=String()){ std::string s=bls(t); if(tp!=3&&s.empty()) return 0; ns.push_back({gid++,p,gseq++,tp,s,bls(ic),1,0,0}); drawbar(); return ns.back().id; }
int _AddMenu(String t){ return addn(0,t,0); } int _AddSubMenu(int m,String t){ return gn(m)?addn(m,t,1):0; } int _AddMenuItem(int m,String t){ return gn(m)?addn(m,t,2):0; } int _AddMenuItemWithIcon(int m,String t,String p){ return gn(m)?addn(m,t,2,p):0; } int _AddMenuSeparator(int m){ return gn(m)?addn(m,String(),3):0; }
int _MenuItemSetEnabled(int i,int v){ N*n=gn(i); if(!n) return 0; n->e=v!=0; for(auto &p:ps) drawp(p); return 1; } int _MenuItemSetChecked(int i,int v){ N*n=gn(i); if(!n) return 0; n->c=v!=0; for(auto &p:ps) drawp(p); return 1; } int _MenuItemSetRadio(int i,int v){ N*n=gn(i); if(!n) return 0; n->r=v!=0; for(auto &p:ps) drawp(p); return 1; } int _MenuItemClicked(int i){ pollm(); if(gcmd==i){ gcmd=0; return 1; } return 0; }


// Apre la tendina di un menu nel punto voluto, invece che sotto la barra.
// Riusa openp(), la stessa funzione che apre i popup della barra: da qui in
// poi ci pensa pollm(), che gia' gira a ogni giro e riempie gcmd.
int _ShowPopup(int menuId,int x,int y){
    if(!initm()) return 0;
    if(!gn(menuId)) return 0;
    blClosePopupsFrom(0);
    openp(menuId,x,y,0);
    return 1;
}


