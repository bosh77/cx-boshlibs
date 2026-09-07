
//CODICE DA CREARE PER SISTEMA OPERATIVO MAC OS, IDENTICO A blDrawlist-win.cpp

#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#include <vector>
#include <cmath>

static CFStringRef bldt_CFStringFromBB(String s){ return CFStringCreateWithCharacters(0,(const UniChar*)s.Data(),s.Length()); }
static CTFontRef bldt_CreateFont(String fontname,float size){ CFStringRef name=bldt_CFStringFromBB(fontname); CTFontRef font=CTFontCreateWithName(name,size,0); CFRelease(name); if(!font) font=CTFontCreateWithName(CFSTR("Helvetica"),size,0); return font; }
static CFAttributedStringRef bldt_CreateAttr(CFStringRef str,CTFontRef font){ CFMutableAttributedStringRef attr=CFAttributedStringCreateMutable(0,0); CFAttributedStringReplaceString(attr,CFRangeMake(0,0),str); CFAttributedStringSetAttribute(attr,CFRangeMake(0,CFStringGetLength(str)),kCTFontAttributeName,font); return attr; }
static CGContextRef bldt_CreateBitmapContext(int w,int h,std::vector<unsigned char> &rgba){ if(w<1) w=1; if(h<1) h=1; rgba.assign(w*h*4,255); CGColorSpaceRef cs=CGColorSpaceCreateDeviceRGB(); CGContextRef ctx=CGBitmapContextCreate(rgba.data(),w,h,8,w*4,cs,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big); CGColorSpaceRelease(cs); if(!ctx) return 0; CGContextSetRGBFillColor(ctx,1,1,1,1); CGContextFillRect(ctx,CGRectMake(0,0,w,h)); CGContextSetRGBFillColor(ctx,0,0,0,1); CGContextSetTextMatrix(ctx,CGAffineTransformIdentity); return ctx; }

Array<int> _CreateText(String txt,String fo,int sz){
	if(sz<1) sz=16;
	CFStringRef text=bldt_CFStringFromBB(txt);
	CTFontRef font=bldt_CreateFont(fo,(float)sz);
	CFAttributedStringRef attr=bldt_CreateAttr(text,font);
	CTLineRef line=CTLineCreateWithAttributedString(attr);
	CGFloat ascent=0,descent=0,leading=0;
	double tw=CTLineGetTypographicBounds(line,&ascent,&descent,&leading);
	int width=(int)std::ceil(tw)+2; if(width<1) width=1;
	int height=(int)std::ceil(ascent+descent+leading)+2; if(height<1) height=1;
	std::vector<unsigned char> rgba;
	CGContextRef ctx=bldt_CreateBitmapContext(width,height,rgba);
	if(ctx){ CGContextSetTextPosition(ctx,1,(CGFloat)(descent+1)); CTLineDraw(line,ctx); CGContextRelease(ctx); }
	int dataSize=width*height*4;
	Array<int> arr=Array<int>(dataSize+2);
	arr[0]=width; arr[1]=height;
	for(int i=0;i<dataSize;++i) arr[i+2]=rgba[i];
	CFRelease(line); CFRelease(attr); CFRelease(font); CFRelease(text);
	return arr;
}



