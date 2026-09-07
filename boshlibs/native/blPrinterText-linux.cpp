
//CODICE DA CREARE PER SISTEMA OPERATIVO LINUX, IDENTICO A blDrawlist-win.cpp

#if defined(__has_include)
# if __has_include(<ft2build.h>)
#  include <ft2build.h>
# elif __has_include(<freetype2/ft2build.h>)
#  include <freetype2/ft2build.h>
# endif
#endif
#include FT_FREETYPE_H
#if defined(__has_include)
# if __has_include(<fontconfig/fontconfig.h>)
#  include <fontconfig/fontconfig.h>
# endif
#endif
#include <vector>
#include <string>

static bool bldl_haslatin(FcPattern* p){ FcCharSet* cs=0; return FcPatternGetCharSet(p,FC_CHARSET,0,&cs)==FcResultMatch && cs && FcCharSetHasChar(cs,'A'); }
static std::string bldl_fontfile(std::string family){ FcInit(); FcPattern* pat=FcPatternCreate(); FcPatternAddString(pat,FC_FAMILY,(const FcChar8*)family.c_str()); FcPatternAddBool(pat,FC_SCALABLE,FcTrue); FcPatternAddBool(pat,FC_OUTLINE,FcTrue); FcConfigSubstitute(0,pat,FcMatchPattern); FcDefaultSubstitute(pat); FcResult r; FcPattern* font=FcFontMatch(0,pat,&r); std::string path; if(font){ FcChar8* file=0; if(bldl_haslatin(font) && FcPatternGetString(font,FC_FILE,0,&file)==FcResultMatch && file) path=(const char*)file; FcPatternDestroy(font);} FcPatternDestroy(pat); return path; }
static unsigned bldl_utf8(const std::string&s,size_t &i){ unsigned char c=(unsigned char)s[i++]; if(c<0x80) return c; if((c>>5)==0x6 && i<s.size()) return ((c&31)<<6)|((unsigned char)s[i++]&63); if((c>>4)==0xE && i+1<s.size()){ unsigned a=(unsigned char)s[i++],b=(unsigned char)s[i++]; return ((c&15)<<12)|((a&63)<<6)|(b&63);} if((c>>3)==0x1E && i+2<s.size()){ unsigned a=(unsigned char)s[i++],b=(unsigned char)s[i++],d=(unsigned char)s[i++]; return ((c&7)<<18)|((a&63)<<12)|((b&63)<<6)|(d&63);} return '?'; }

Array<int> _CreateText(String txt,String fo,int sz){ if(sz<1) sz=16; char* tf=convertBBString(fo); std::string font=tf?tf:""; if(tf) free(tf); std::string path=bldl_fontfile(font); if(path.empty()&&font!="Sans") path=bldl_fontfile("Sans"); Array<int> arr(6); arr[0]=1; arr[1]=1; arr[2]=arr[3]=arr[4]=arr[5]=255; FT_Library ft=0; FT_Face face=0; if(path.empty()||FT_Init_FreeType(&ft)||FT_New_Face(ft,path.c_str(),0,&face)||FT_Set_Pixel_Sizes(face,0,sz)) return arr; int asc=(int)(face->size->metrics.ascender>>6), desc=(int)(-(face->size->metrics.descender>>6)); if(asc<1) asc=sz; if(desc<1) desc=1; char* tt=convertBBString(txt); std::string text=tt?tt:""; if(tt) free(tt); int w=2; for(size_t i=0;i<text.size();){ unsigned code=bldl_utf8(text,i); if(!FT_Load_Char(face,code,FT_LOAD_DEFAULT)) w+=(int)(face->glyph->advance.x>>6); else w+=sz/2; } int h=asc+desc+2; std::vector<unsigned char> rgba(w*h*4,255); int pen=1; for(size_t i=0;i<text.size();){ unsigned code=bldl_utf8(text,i); if(FT_Load_Char(face,code,FT_LOAD_RENDER)){ pen+=sz/2; continue; } FT_GlyphSlot g=face->glyph; int dx=pen+g->bitmap_left, dy=asc-g->bitmap_top+1; for(int y=0;y<(int)g->bitmap.rows;++y) for(int x=0;x<(int)g->bitmap.width;++x){ int px=dx+x,py=dy+y; if(px<0||py<0||px>=w||py>=h) continue; unsigned char a=g->bitmap.buffer[y*g->bitmap.pitch+x]; int j=(py*w+px)*4; unsigned char c=255-a; rgba[j]=c; rgba[j+1]=c; rgba[j+2]=c; rgba[j+3]=255; } pen+=(int)(g->advance.x>>6); }
FT_Done_Face(face); FT_Done_FreeType(ft); arr=Array<int>(w*h*4+2); arr[0]=w; arr[1]=h; for(int i=0;i<w*h*4;++i) arr[i+2]=rgba[i]; return arr; }



