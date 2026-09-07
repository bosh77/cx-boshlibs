
//CODICE DA CREARE PER SISTEMA OPERATIVO MAC OS, IDENTICO A blPrinter-win.cpp

#import <Cocoa/Cocoa.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <vector>
#include <cstring>

static NSPrintInfo* gPrintInfo=nil;
static PMPrintSession gSession=0; static PMPrintSettings gSettings=0; static PMPageFormat gFormat=0; static CGContextRef gCtx=0; static bool gPage=false;
String _printername=L""; String _orientation=L"Verticale"; int _copies=1; String _coloribn=L"Colori"; int _papersize=0; float dpiX=72.0f,dpiY=72.0f,marginLeft=0,marginTop=0;

static void syncpm(){ gSession=0; gSettings=0; gFormat=0; if(!gPrintInfo) return; gSession=(PMPrintSession)[gPrintInfo PMPrintSession]; gSettings=(PMPrintSettings)[gPrintInfo PMPrintSettings]; gFormat=(PMPageFormat)[gPrintInfo PMPageFormat]; }
static void resetpm(){ if(gPrintInfo){ [gPrintInfo release]; gPrintInfo=nil; } gSession=0; gSettings=0; gFormat=0; gCtx=0; gPage=false; }
static CGRect pg(){ PMRect r={0,0,595,842}; if(gFormat) PMGetAdjustedPageRect(gFormat,&r); return CGRectMake(r.left,r.top,r.right-r.left,r.bottom-r.top); }
static inline CGFloat U(float v,float p){ return (CGFloat)(v*p); }
static inline void RGBf(float r,float g,float b){ if(!gCtx) return; CGContextSetRGBStrokeColor(gCtx,r/255.0f,g/255.0f,b/255.0f,1); CGContextSetRGBFillColor(gCtx,r/255.0f,g/255.0f,b/255.0f,1); }

void _ShowSettings(){
	resetpm();
	[NSApplication sharedApplication];
	gPrintInfo=[[NSPrintInfo sharedPrintInfo] copy];
	if(!gPrintInfo) return;
	NSPrintPanel* panel=[NSPrintPanel printPanel];
	if([panel runModalWithPrintInfo:gPrintInfo]!=NSModalResponseOK){ resetpm(); return; }
	syncpm();
	NSPrinter* p=[gPrintInfo printer];
	_printername=p?String([[p name] UTF8String]):L"";
	UInt32 cp=1; if(gSettings) PMGetCopies(gSettings,&cp); _copies=(int)cp;
	_orientation=[gPrintInfo orientation]==NSPaperOrientationLandscape?L"Orizzontale":L"Verticale";
	_papersize=(int)[gPrintInfo paperSize].height;
}
String _GetSettings(){ return _printername+"@"+_orientation+"@"+_copies+"@"+_coloribn+"@"+_papersize; }
float _StartPrintDocument(){ if(!gSession||!gSettings||!gFormat) _ShowSettings(); if(!gSession) return dpiX/2.54f; if(PMSessionBeginCGDocumentNoDialog(gSession,gSettings,gFormat)!=noErr) return dpiX/2.54f; if(PMSessionBeginPageNoDialog(gSession,gFormat,NULL)!=noErr) return dpiX/2.54f; gPage=true; PMSessionGetCGGraphicsContext(gSession,&gCtx); if(!gCtx) return dpiX/2.54f; CGRect r=pg(); marginLeft=r.origin.x; marginTop=r.origin.y; CGContextTranslateCTM(gCtx,-r.origin.x,CGRectGetMaxY(r)); CGContextScaleCTM(gCtx,1,-1); return dpiX/2.54f; }
void _PrintRect(int x1,int y1,int x2,int y2,int wl,int rr,int gg,int bb,float pxc){ if(!gCtx) return; RGBf(rr,gg,bb); CGContextSetLineWidth(gCtx,U(wl,pxc)); CGContextStrokeRect(gCtx,CGRectMake(U(x1,pxc),U(y1,pxc),U(x2-x1,pxc),U(y2-y1,pxc))); }
void _PrintLine(int x1,int y1,int x2,int y2,int wl,int rr,int gg,int bb,float pxc){ if(!gCtx) return; RGBf(rr,gg,bb); CGContextSetLineWidth(gCtx,U(wl,pxc)); CGContextBeginPath(gCtx); CGContextMoveToPoint(gCtx,U(x1,pxc),U(y1,pxc)); CGContextAddLineToPoint(gCtx,U(x2,pxc),U(y2,pxc)); CGContextStrokePath(gCtx); }
void _PrintImage(float x1,float y1,String fn,float pxc){ if(!gCtx) return; char* path=convertBBString(fn); CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8*)path,strlen(path),false); free(path); if(!url) return; CGImageSourceRef src=CGImageSourceCreateWithURL(url,NULL); CGImageRef img=src?CGImageSourceCreateImageAtIndex(src,0,NULL):0; if(img) CGContextDrawImage(gCtx,CGRectMake(U(x1,pxc),U(y1,pxc),CGImageGetWidth(img)*pxc,CGImageGetHeight(img)*pxc),img); if(img) CGImageRelease(img); if(src) CFRelease(src); CFRelease(url); }
void _PrintText(float x1,float y1,Array<int> pixels,int width,int height,float rr,float gg,float bb,float ksize,float pxc){ if(!gCtx||width<=0||height<=0) return; std::vector<unsigned char> rgba(width*height*4); unsigned char cr=(unsigned char)(rr*255.0f),cg=(unsigned char)(gg*255.0f),cb=(unsigned char)(bb*255.0f); for(int i=0;i<width*height;++i){ unsigned int p=(unsigned int)pixels[i]; int j=i*4; rgba[j]=cr; rgba[j+1]=cg; rgba[j+2]=cb; rgba[j+3]=(unsigned char)((p>>24)&255); } CGColorSpaceRef cs=CGColorSpaceCreateDeviceRGB(); CGDataProviderRef pr=CGDataProviderCreateWithData(NULL,rgba.data(),rgba.size(),NULL); CGImageRef img=CGImageCreate(width,height,8,32,width*4,cs,kCGImageAlphaLast|kCGBitmapByteOrderDefault,pr,NULL,false,kCGRenderingIntentDefault); if(img) CGContextDrawImage(gCtx,CGRectMake(U(x1,pxc),U(y1,pxc),width*pxc*ksize,height*pxc*ksize),img); if(img) CGImageRelease(img); if(pr) CGDataProviderRelease(pr); if(cs) CGColorSpaceRelease(cs); }
void _EndPrintDocument(){ if(!gSession) return; if(gPage) PMSessionEndPageNoDialog(gSession); PMSessionEndDocumentNoDialog(gSession); resetpm(); }





//  Cartella che CONTIENE "data", con la barra finale. Vedi la nota nella
//  versione Windows: sta qui per non dipendere da brl.process.
//
//  Su Windows e Linux e' la cartella dell'eseguibile; su macOS NO. L'eseguibile
//  sta in CerberusGame.app/Contents/MacOS, mentre "data" e' una risorsa del
//  bundle e finisce in CerberusGame.app/Contents/Resources/data (nel progetto
//  Xcode del target la cartella e' dichiarata "data in Resources"). La prima
//  versione di questa funzione tornava la cartella che CONTIENE il .app: un
//  percorso che non esiste, e le immagini non venivano stampate - la stampante
//  rilegge il file da disco e non lo trovava.
//
//  Qui si fa la stessa cosa che fa Cerberus per risolvere "cerberus://data/":
//  BBGlfwGame::PathToFilePath prende la cartella dell'eseguibile e, se finisce
//  per ".app/Contents/MacOS", la sostituisce con "Resources". resourcePath da'
//  quel percorso in una riga, e per un binario NON impacchettato torna la
//  cartella dell'eseguibile, cioe' la cosa giusta anche in quel caso.
String _AppDir(){

    NSString* p = [[NSBundle mainBundle] resourcePath];

    if( !p ) return String();

    return String( [[p stringByAppendingString:@"/"] UTF8String] );
}
