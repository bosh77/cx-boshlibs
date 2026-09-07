
// blPrinter - nativo Linux: PostScript passato a CUPS con lpr.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

static FILE* g_blPrn=0; static std::string g_blPrnPath; static float g_blPW=595.0f,g_blPH=842.0f;
String _printername=L""; String _orientation=L"Verticale"; int _copies=1; String _coloribn=L"Colori"; int _papersize=9; float dpiX=72.0f,dpiY=72.0f,marginLeft=0,marginTop=0;
static std::string blps(String s){ char* c=convertBBString(s); std::string r=c?c:""; if(c) free(c); return r; }
static std::string blt(std::string s){ while(!s.empty()&&(s[0]==' '||s[0]=='\n'||s[0]=='\r'||s[0]=='\t')) s.erase(0,1); while(!s.empty()&&(s[s.size()-1]==' '||s[s.size()-1]=='\n'||s[s.size()-1]=='\r'||s[s.size()-1]=='\t')) s.erase(s.size()-1,1); return s; }
static std::string blq(const std::string&s){ std::string r="'"; for(size_t i=0;i<s.size();++i){ if(s[i]=='\'') r+="'\\''"; else r+=s[i]; } return r+"'"; }
static std::string blrun(const std::string& c){ std::string o; FILE* f=popen(c.c_str(),"r"); if(!f) return o; char b[256]; while(fgets(b,sizeof(b),f)) o+=b; pclose(f); return o; }
static bool blhas(const char* c){ return system((std::string("command -v ")+c+" >/dev/null 2>&1").c_str())==0; }
static std::vector<std::string> blprinters(){ std::vector<std::string> v; std::string o=blrun("lpstat -a 2>/dev/null"); for(size_t a=0;a<o.size();){ size_t b=o.find('\n',a); std::string s=blt(o.substr(a,b==std::string::npos?o.size()-a:b-a)); size_t p=s.find(' '); if(!s.empty()) v.push_back(p==std::string::npos?s:s.substr(0,p)); if(b==std::string::npos) break; a=b+1; } return v; }
static std::string bldef(){ std::string s=blt(blrun("lpstat -d 2>/dev/null")); size_t p=s.rfind(':'); return blt(p==std::string::npos?s:s.substr(p+1)); }
static std::string blask(const char*t,const char*m,const std::string& d){ bool g=getenv("DISPLAY")||getenv("WAYLAND_DISPLAY"); if(g&&blhas("zenity")){ std::string r=blt(blrun("zenity --entry --title="+blq(t)+" --text="+blq(m)+" --entry-text="+blq(d)+" 2>/dev/null")); if(!r.empty()) return r; } if(g&&blhas("kdialog")){ std::string r=blt(blrun("kdialog --inputbox "+blq(m)+" "+blq(d)+" --title "+blq(t)+" 2>/dev/null")); if(!r.empty()) return r; } return d; }
static std::string bllist(const char*t,const char*m,const std::vector<std::string>& v,const std::string& d){ if(v.empty()) return d; bool g=getenv("DISPLAY")||getenv("WAYLAND_DISPLAY"); if(g&&blhas("zenity")){ std::string c="zenity --list --title="+blq(t)+" --text="+blq(m)+" --column='Valore'"; for(size_t i=0;i<v.size();++i) c+=" "+blq(v[i]); std::string r=blt(blrun(c+" 2>/dev/null")); if(!r.empty()) return r; } if(g&&blhas("kdialog")){ std::string c="kdialog --menu "+blq(m)+" --title "+blq(t); for(size_t i=0;i<v.size();++i) c+=" "+blq(v[i])+" "+blq(v[i]); std::string r=blt(blrun(c+" 2>/dev/null")); if(!r.empty()) return r; } return d.empty()?v[0]:d; }
static const char* blpaper(){ if(_papersize==8) return "A3"; if(_papersize==1) return "Letter"; if(_papersize==5) return "Legal"; return "A4"; }
static void blpage(){ g_blPW=595; g_blPH=842; if(_papersize==8){ g_blPW=842; g_blPH=1191; } else if(_papersize==1){ g_blPW=612; g_blPH=792; } else if(_papersize==5){ g_blPW=612; g_blPH=1008; } if(blps(_orientation)=="Orizzontale"){ float t=g_blPW; g_blPW=g_blPH; g_blPH=t; } }
static std::string blesc(const std::string&s){ std::string o; for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='('||c==')'||c=='\\') o+='\\'; o+=c; } return o; }
static float U(float v,float p){ return v*p; }
static float PY(float v,float p){ return g_blPH-U(v,p); }
static void blhex(FILE* f,unsigned char v){ static const char* h="0123456789ABCDEF"; fputc(h[v>>4],f); fputc(h[v&15],f); }

void _ShowSettings(){ std::string def=bldef(); std::vector<std::string> p=blprinters(); if(p.empty()&&!def.empty()) p.push_back(def); std::string cur=blps(_printername); if(cur.empty()||cur=="Default") cur=def; std::string pr=bllist("Stampa","Seleziona stampante",p,cur); if(pr.empty()) pr=blask("Stampa","Nome stampante CUPS",cur); if(!pr.empty()) _printername=String(pr.c_str()); std::string cp=blask("Stampa","Numero copie",std::to_string(_copies>0?_copies:1)); int n=atoi(cp.c_str()); if(n>0) _copies=n; std::vector<std::string> o; o.push_back("Verticale"); o.push_back("Orizzontale"); std::string ov=bllist("Stampa","Orientamento",o,blps(_orientation)); if(!ov.empty()) _orientation=String(ov.c_str()); std::vector<std::string> c; c.push_back("Colori"); c.push_back("B/N"); std::string cv=bllist("Stampa","Modalita colore",c,blps(_coloribn)); if(!cv.empty()) _coloribn=String(cv.c_str()); std::vector<std::string> pa; pa.push_back("A4"); pa.push_back("A3"); pa.push_back("Letter"); pa.push_back("Legal"); std::string pv=bllist("Stampa","Formato carta",pa,blpaper()); _papersize=pv=="A3"?8:pv=="Letter"?1:pv=="Legal"?5:9; blpage(); }
String _GetSettings(){ return _printername+"@"+_orientation+"@"+_copies+"@"+_coloribn+"@"+_papersize; }
float _StartPrintDocument(){ if(g_blPrn){ fclose(g_blPrn); g_blPrn=0; } if(_printername==L"") _ShowSettings(); if(_printername==L""){ std::string def=bldef(); if(!def.empty()) _printername=String(def.c_str()); } blpage(); char tmp[256]; snprintf(tmp,sizeof(tmp),"/tmp/blprinter_%d.ps",(int)getpid()); g_blPrnPath=tmp; g_blPrn=fopen(g_blPrnPath.c_str(),"wb"); if(!g_blPrn) return dpiX/2.54f; fprintf(g_blPrn,"%%!PS-Adobe-3.0\n%%%%Pages: 1\n%%%%BoundingBox: 0 0 %d %d\n%%%%EndComments\n1 setlinejoin 1 setlinecap\n",(int)g_blPW,(int)g_blPH); return dpiX/2.54f; }
void _PrintRect(int x1,int y1,int x2,int y2,int wl,int rr,int gg,int bb,float pxc){ if(!g_blPrn) return; float x=U(x1<x2?x1:x2,pxc), y=PY(y1>y2?y1:y2,pxc), w=U((x2>x1?x2-x1:x1-x2),pxc), h=U((y2>y1?y2-y1:y1-y2),pxc); fprintf(g_blPrn,"%.3f %.3f %.3f setrgbcolor %.3f setlinewidth newpath %.3f %.3f moveto %.3f 0 rlineto 0 %.3f rlineto %.3f 0 rlineto closepath stroke\n",rr/255.0f,gg/255.0f,bb/255.0f,U(wl,pxc),x,y,w,h,-w); }
void _PrintLine(int x1,int y1,int x2,int y2,int wl,int rr,int gg,int bb,float pxc){ if(!g_blPrn) return; fprintf(g_blPrn,"%.3f %.3f %.3f setrgbcolor %.3f setlinewidth newpath %.3f %.3f moveto %.3f %.3f lineto stroke\n",rr/255.0f,gg/255.0f,bb/255.0f,U(wl,pxc),U(x1,pxc),PY(y1,pxc),U(x2,pxc),PY(y2,pxc)); }
void _PrintText(float x1,float y1,Array<int> pixels,int width,int height,float rr,float gg,float bb,float ksize,float pxc){ if(!g_blPrn||width<=0||height<=0) return; float dw=width*pxc*ksize, dh=height*pxc*ksize; fprintf(g_blPrn,"gsave %.3f %.3f translate %.3f %.3f scale /picstr %d string def %d %d 8 [%d 0 0 -%d 0 %d] {currentfile picstr readhexstring pop} false 3 colorimage\n",U(x1,pxc),PY(y1,pxc)-dh,dw,dh,width*3,width,height,width,height,height); unsigned char cr=(unsigned char)(rr*255.0f),cg=(unsigned char)(gg*255.0f),cb=(unsigned char)(bb*255.0f); for(int i=0;i<width*height;++i){ bool on=(unsigned int)pixels[i]!=0; blhex(g_blPrn,on?cr:255); blhex(g_blPrn,on?cg:255); blhex(g_blPrn,on?cb:255); if((i&15)==15) fputc('\n',g_blPrn); } fprintf(g_blPrn,"\ngrestore\n"); }
void _EndPrintDocument(){ if(!g_blPrn) return; fprintf(g_blPrn,"showpage\n%%%%EOF\n"); fclose(g_blPrn); g_blPrn=0; std::string cmd="lpr "; std::string pr=blps(_printername); if(!pr.empty()&&pr!="Default") cmd+="-P "+blq(pr)+" "; if(_copies>1) cmd+="-# "+std::to_string(_copies)+" "; cmd+="-o media="+std::string(blpaper())+" "; cmd+="-o orientation-requested="+std::string(blps(_orientation)=="Orizzontale"?"4":"3")+" "; cmd+="-o ColorModel="+std::string(blps(_coloribn)=="B/N"?"Gray":"RGB")+" "; cmd+=blq(g_blPrnPath)+" >/dev/null 2>&1"; system(cmd.c_str()); unlink(g_blPrnPath.c_str()); g_blPrnPath.clear(); }


//======================================================================
//  IMMAGINI DENTRO IL POSTSCRIPT
//
//  Il nativo Linux produce PostScript e lo passa a CUPS, quindi l'immagine va
//  decodificata qui e incorporata come RGB grezzo con l'operatore
//  "colorimage". Si appoggia col bordo superiore alla y chiesta, esattamente
//  come nelle versioni Mac e Windows.
//
//  A decodificare e' stb_image, che sta GIA' dentro ogni build del target
//  glfw3: main.h fa #include <stb_image.h>, il Makefile compila stb_image.o
//  ed e' lo stesso codice con cui mojo carica le immagini. Non e' una
//  dipendenza nuova e non c'e' niente da aggiungere al Makefile.
//
//  Prima qui c'erano un inflate, un decoder PNG e un decoder JPEG baseline
//  scritti a mano: piu' di cinquecento righe che nessuno aveva mai verificato,
//  ed erano il sospettato numero uno quando le due immagini dell'esempio non
//  venivano stampate. Sono state tolte il 2026-09-06.
//======================================================================


//  Carica un'immagine dal disco e la restituisce come RGB, 3 byte per pixel:
//  e' quello che si mette nel PostScript.
//
//  Si chiedono a stb QUATTRO canali e si appiattisce a mano su bianco, invece
//  di chiedergliene tre: con tre stb scarta il canale alfa senza comporlo, e
//  un png con trasparenza uscirebbe con il colore che sta sotto ai pixel
//  invisibili - di solito nero. Il bianco e' il colore della carta.
static bool blLoadRGB(const std::string& path,int&w,int&h,std::vector<unsigned char>&rgb){

    w=0; h=0;
    rgb.clear();

    int comp=0;

    unsigned char* px=stbi_load(path.c_str(),&w,&h,&comp,4);

    if(!px) return false;

    if(w<1||h<1){ stbi_image_free(px); return false; }

    rgb.resize((size_t)w*h*3);

    for(int i=0;i<w*h;++i){

        int a=px[i*4+3];

        if(a>=255){

            rgb[i*3+0]=px[i*4+0];
            rgb[i*3+1]=px[i*4+1];
            rgb[i*3+2]=px[i*4+2];

        }else{

            rgb[i*3+0]=(unsigned char)((px[i*4+0]*a+255*(255-a))/255);
            rgb[i*3+1]=(unsigned char)((px[i*4+1]*a+255*(255-a))/255);
            rgb[i*3+2]=(unsigned char)((px[i*4+2]*a+255*(255-a))/255);
        }
    }

    stbi_image_free(px);

    return true;
}


static void blhash(FILE* f,unsigned char v){ static const char* h="0123456789ABCDEF"; fputc(h[v>>4],f); fputc(h[v&15],f); }

void _PrintImage(float x1,float y1,String fn,float pxc){
    if(!g_blPrn) return;
    std::string path=blps(fn);
    int w=0,h=0;
    std::vector<unsigned char> rgb;
    if(!blLoadRGB(path,w,h,rgb)){
        std::string s=blesc(path);
        fprintf(g_blPrn,"0.6 0.6 0.6 setrgbcolor /Helvetica findfont 8 scalefont setfont %.3f %.3f moveto (%s) show\n",U(x1,pxc),PY(y1,pxc),s.c_str());
        return;
    }
    float dw=w*pxc, dh=h*pxc;
    fprintf(g_blPrn,"gsave %.3f %.3f translate %.3f %.3f scale /picstr %d string def %d %d 8 [%d 0 0 -%d 0 %d] {currentfile picstr readhexstring pop} false 3 colorimage\n",
        U(x1,pxc),PY(y1,pxc)-dh,dw,dh,w*3,w,h,w,h,h);
    size_t hx=0;
    for(size_t i=0;i<rgb.size();i++){
        blhash(g_blPrn,rgb[i]);
        hx++;
        if(hx>=60){ fputc('\n',g_blPrn); hx=0; }
    }
    if(hx) fputc('\n',g_blPrn);
    fprintf(g_blPrn,"\ngrestore\n");
}




//  Cartella dell'eseguibile, con la barra finale. Vedi la nota nella versione
//  Windows: sta qui per non dipendere da brl.process.
String _AppDir(){

    char buf[4096];

    ssize_t n = readlink( "/proc/self/exe",buf,sizeof(buf)-1 );

    if( n<=0 ) return String();

    buf[n] = 0;

    for( int i=(int)n-1;i>=0;--i ){
        if( buf[i]=='/' ){ buf[i+1]=0; break; }
    }

    return String( buf );
}
