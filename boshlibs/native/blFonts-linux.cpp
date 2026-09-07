#if defined(__has_include)
# if __has_include(<ft2build.h>)
#  include <ft2build.h>
# elif __has_include(<freetype2/ft2build.h>)
#  include <freetype2/ft2build.h>
# elif __has_include("/usr/include/freetype2/ft2build.h")
#  include "/usr/include/freetype2/ft2build.h"
# elif __has_include("/usr/local/include/freetype2/ft2build.h")
#  include "/usr/local/include/freetype2/ft2build.h"
# else
#  error "FreeType headers non trovati: manca ft2build.h nel toolchain o nei path standard Linux"
# endif
#else
# include <ft2build.h>
#endif
#include FT_FREETYPE_H

#if defined(__has_include)
# if __has_include(<fontconfig/fontconfig.h>)
#  include <fontconfig/fontconfig.h>
# elif __has_include("/usr/include/fontconfig/fontconfig.h")
#  include "/usr/include/fontconfig/fontconfig.h"
# elif __has_include("/usr/local/include/fontconfig/fontconfig.h")
#  include "/usr/local/include/fontconfig/fontconfig.h"
# else
#  error "Fontconfig headers non trovati: manca fontconfig/fontconfig.h nel toolchain o nei path standard Linux"
# endif
#else
# include <fontconfig/fontconfig.h>
#endif
#include <vector>
#include <string>
#include <stdint.h>
#include <algorithm>

#ifdef USE_QT_FONTDIALOG
#include <QApplication>
#include <QFontDialog>
#include <QFont>
#endif

class BBblFont : public Object{
public:
	BBblFont();
	~BBblFont();
    bool _New( String fontname, int dimensione, int peso, bool ital );
    int _SelectFont();
    String _GetFontName();
    int _GetFontSize();
    static String _GetFontListString();
private:
    String _fontname;
    int _dimensione;
    int _peso;
    bool _ital;
};

BBblFont::BBblFont(){
}

BBblFont::~BBblFont(){
}

bool BBblFont::_New( String fontname, int dimensione, int peso, bool ital ){
	_fontname=fontname;
    _dimensione=dimensione;
    _peso=peso;
    _ital=ital;
    return true;
}

String BBblFont::_GetFontName(){
    return String(_fontname);
}

int BBblFont::_GetFontSize(){
    return _dimensione;
}

static bool _FcHasBasicLatin(FcPattern* p){
    FcCharSet* cs = nullptr;
    if(FcPatternGetCharSet(p, FC_CHARSET, 0, &cs) != FcResultMatch || !cs) return false;
    return FcCharSetHasChar(cs, 'A') && FcCharSetHasChar(cs, 'a') && FcCharSetHasChar(cs, '0');
}

String BBblFont::_GetFontListString(){
    FcInit();
    FcPattern* pat = FcPatternCreate();
    FcObjectSet* os = FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_SCALABLE, FC_OUTLINE, FC_CHARSET, (char*)0);
    FcFontSet* fs = FcFontList(nullptr, pat, os);
    std::vector<std::string> names;
    std::string out;
    if(fs){
        for(int i=0;i<fs->nfont;++i){
            FcPattern* fp = fs->fonts[i];
            FcChar8* fam = nullptr;
            FcChar8* file = nullptr;
            FcBool scalable = FcFalse, outline = FcFalse;
            if(FcPatternGetString(fp, FC_FAMILY, 0, &fam) != FcResultMatch || !fam) continue;
            if(FcPatternGetString(fp, FC_FILE, 0, &file) != FcResultMatch || !file) continue;
            FcPatternGetBool(fp, FC_SCALABLE, 0, &scalable);
            FcPatternGetBool(fp, FC_OUTLINE, 0, &outline);
            if(!scalable || !outline) continue;
            if(!_FcHasBasicLatin(fp)) continue;
            std::string cur = (const char*)fam;
            if(cur.empty()) continue;
            if(std::find(names.begin(), names.end(), cur) != names.end()) continue;
            names.push_back(cur);
        }
        FcFontSetDestroy(fs);
    }
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);
    std::sort(names.begin(), names.end());
    for(size_t i=0;i<names.size();++i){
        if(i) out += ";";
        out += names[i];
    }
    if(out.empty()) out = "Sans";
    return String(out.c_str());
}

int BBblFont::_SelectFont(){
    std::string family = convertBBString(_fontname);
#ifdef USE_QT_FONTDIALOG
    int argc = 0;
    char* argvv[] = { (char*)0 };
    QApplication* app = qApp;
    bool ownsApp = false;
    if (!app) { app = new QApplication(argc, argvv); ownsApp = true; }
    QFont initial(QString::fromStdString(family), _dimensione);
    initial.setWeight(_peso >= 700 ? QFont::Bold : QFont::Normal);
    initial.setItalic(_ital);
    bool ok = false;
    QFont font = QFontDialog::getFont(&ok, initial, nullptr);
    if (ok){
        _fontname = String(font.family().toStdString().c_str());
        int ps = font.pointSize();
        if (ps > 0) _dimensione = ps;
        _peso = font.weight() >= QFont::Bold ? 700 : 400;
        _ital = font.italic();
    }
    if (ownsApp) { delete app; }
#endif
    FcInit();
    FcPattern* pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)family.c_str());
    int fcWeight = FC_WEIGHT_NORMAL;
    if (_peso >= 700) fcWeight = FC_WEIGHT_BOLD;
    else if (_peso <= 300) fcWeight = FC_WEIGHT_LIGHT;
    FcPatternAddInteger(pat, FC_WEIGHT, fcWeight);
    FcPatternAddInteger(pat, FC_SLANT, _ital ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    FcConfigSubstitute(nullptr, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult result;
    FcPattern* match = FcFontMatch(nullptr, pat, &result);
    if (match){
        FcChar8* fam = nullptr;
        if (FcPatternGetString(match, FC_FAMILY, 0, &fam) == FcResultMatch && fam){
            _fontname = String((const char*)fam);
        }
        FcPatternDestroy(match);
    }
    FcPatternDestroy(pat);
    return 0;
}

class BBGetFont : public Object{
public:
    BBGetFont();
	~BBGetFont();
    void _New();
    Array<int> dataPixel;
    int hFont;
    Array<int> xFont;
    Array<int> yFont;
    Array<int> wFont;
private:
};

BBGetFont::BBGetFont(){
}

BBGetFont::~BBGetFont(){
}

static uint32_t _BlendOnWhite(uint8_t a){
    uint8_t c = (uint8_t)(255 - a);
    return (uint32_t)(255 << 24 | c << 16 | c << 8 | c);
}

static int _FTAdvanceX(FT_GlyphSlot g){
    return (int)((g->advance.x) >> 6);
}

static uint32_t* _PixelPtr(std::vector<uint32_t>& buf,int w,int x,int y){
    return &buf[(y*w)+x];
}

static int _Clamp(int v,int lo,int hi){
    if(v<lo) return lo;
    if(v>hi) return hi;
    return v;
}

static int _CP1252ToUnicode(int ch){
    if (ch >= 128 && ch <= 255) {
        switch (ch) {
            case 128: return 0x00C7;
            case 129: return 0x00FC;
            case 130: return 0x00E9;
            case 131: return 0x00E2;
            case 132: return 0x00E4;
            case 133: return 0x00E0;
            case 134: return 0x00E5;
            case 135: return 0x00E7;
            case 136: return 0x00EA;
            case 137: return 0x00EB;
            case 138: return 0x00E8;
            case 139: return 0x00EF;
            case 140: return 0x00EE;
            case 141: return 0x00EC;
            case 142: return 0x00C4;
            case 143: return 0x00C5;
            case 144: return 0x00C9;
            case 145: return 0x00E6;
            case 146: return 0x00C6;
            case 147: return 0x00F4;
            case 148: return 0x00F6;
            case 149: return 0x00F2;
            case 150: return 0x00FB;
            case 151: return 0x00F9;
            case 152: return 0x00FF;
            case 153: return 0x00D6;
            case 154: return 0x00DC;
            case 155: return 0x00A2;
            case 156: return 0x00A3;
            case 157: return 0x00A5;
            case 158: return 0x20A7;
            case 159: return 0x0192;
            case 160: return 0x00A0;
            case 161: return 0x00A1;
            case 162: return 0x00A2;
            case 163: return 0x00A3;
            case 164: return 0x00A4;
            case 165: return 0x00A5;
            case 166: return 0x00A6;
            case 167: return 0x00A7;
            case 168: return 0x00A8;
            case 169: return 0x00A9;
            case 170: return 0x00AA;
            case 171: return 0x00AB;
            case 172: return 0x00AC;
            case 173: return 0x00AD;
            case 174: return 0x00AE;
            case 175: return 0x00AF;
            case 176: return 0x00B0;
            case 177: return 0x00B1;
            case 178: return 0x00B2;
            case 179: return 0x00B3;
            case 180: return 0x00B4;
            case 181: return 0x00B5;
            case 182: return 0x00B6;
            case 183: return 0x00B7;
            case 184: return 0x00B8;
            case 185: return 0x00B9;
            case 186: return 0x00BA;
            case 187: return 0x00BB;
            case 188: return 0x00BC;
            case 189: return 0x00BD;
            case 190: return 0x00BE;
            case 191: return 0x00BF;
            case 192: return 0x00C0;
            case 193: return 0x00C1;
            case 194: return 0x00C2;
            case 195: return 0x00C3;
            case 196: return 0x00C4;
            case 197: return 0x00C5;
            case 198: return 0x00C6;
            case 199: return 0x00C7;
            case 200: return 0x00C8;
            case 201: return 0x00C9;
            case 202: return 0x00CA;
            case 203: return 0x00CB;
            case 204: return 0x00CC;
            case 205: return 0x00CD;
            case 206: return 0x00CE;
            case 207: return 0x00CF;
            case 208: return 0x00D0;
            case 209: return 0x00D1;
            case 210: return 0x00D2;
            case 211: return 0x00D3;
            case 212: return 0x00D4;
            case 213: return 0x00D5;
            case 214: return 0x00D6;
            case 215: return 0x00D7;
            case 216: return 0x00D8;
            case 217: return 0x00D9;
            case 218: return 0x00DA;
            case 219: return 0x00DB;
            case 220: return 0x00DC;
            case 221: return 0x00DD;
            case 222: return 0x00DE;
            case 223: return 0x00DF;
            case 224: return 0x00E0;
            case 225: return 0x00E1;
            case 226: return 0x00E2;
            case 227: return 0x00E3;
            case 228: return 0x00E4;
            case 229: return 0x00E5;
            case 230: return 0x00E6;
            case 231: return 0x00E7;
            case 232: return 0x00E8;
            case 233: return 0x00E9;
            case 234: return 0x00EA;
            case 235: return 0x00EB;
            case 236: return 0x00EC;
            case 237: return 0x00ED;
            case 238: return 0x00EE;
            case 239: return 0x00EF;
            case 240: return 0x00F0;
            case 241: return 0x00F1;
            case 242: return 0x00F2;
            case 243: return 0x00F3;
            case 244: return 0x00F4;
            case 245: return 0x00F5;
            case 246: return 0x00F6;
            case 247: return 0x00F7;
            case 248: return 0x00F8;
            case 249: return 0x00F9;
            case 250: return 0x00FA;
            case 251: return 0x00FB;
            case 252: return 0x00FC;
            case 253: return 0x00FD;
            case 254: return 0x00FE;
            case 255: return 0x00FF;
            default: break;
        }
    }
    return ch;
}

static std::string _FindFontFile(std::string family){
    FcInit();
    FcPattern* pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)family.c_str());
    FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
    FcPatternAddBool(pat, FC_OUTLINE, FcTrue);
    FcConfigSubstitute(nullptr, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult result;
    FcPattern* font = FcFontMatch(nullptr, pat, &result);
    std::string path;
    if(font){
        FcChar8* file = nullptr;
        if(_FcHasBasicLatin(font) && FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file){
            path = std::string((const char*)file);
        }
        FcPatternDestroy(font);
    }
    FcPatternDestroy(pat);
    return path;
}

/*  Font caricato da un FILE dell'applicazione invece che da una famiglia
    installata. Qui costa pochissimo: FreeType lavora gia' su un percorso, e
    l'unica cosa da fare e' saltare fontconfig quando il nome E' un file.

    Accetta "cerberus://data/xxx.ttf" (risolto accanto all'eseguibile) e i
    percorsi normali.                                                       */
static bool _EndsWithNoCase(const std::string &s, const char *suf, size_t n){
    if(s.length() < n) return false;
    for(size_t i = 0; i < n; ++i){
        char c = s[s.length()-n+i];
        if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if(c != suf[i]) return false;
    }
    return true;
}

/*  Se il nome e' un file di font ne ritorna il percorso reale, altrimenti "".
    Non verifica l'esistenza: ci pensa FT_New_Face, che deve comunque aprirlo. */
static std::string _FontFilePath(String fontname){

    std::string n = convertBBString(fontname);

    if(!_EndsWithNoCase(n, ".ttf", 4) && !_EndsWithNoCase(n, ".otf", 4) && !_EndsWithNoCase(n, ".ttc", 4)) return std::string();

    return std::string(convertBBString(BBGame::Game()->PathToFilePath(fontname)));
}

BBGetFont* _CreateFont(String fontname, int fontSize){
    BBGetFont* _newfont=new BBGetFont();
    std::string fontName = convertBBString(fontname);

    FT_Library ft = nullptr;
    if(FT_Init_FreeType(&ft)) return _newfont;

    FT_Face face = nullptr;

    /*  Prima si prova come file dell'applicazione ("cerberus://data/x.ttf" o
        un percorso normale): se il nome E' un file, cercarlo fra le famiglie
        di fontconfig non avrebbe senso. */
    std::string appPath = _FontFilePath(fontname);

    if(!appPath.empty()) FT_New_Face(ft, appPath.c_str(), 0, &face);

    if(!face){

        std::string fontPath = _FindFontFile(fontName);
        if(fontPath.empty() && fontName != "Sans") fontPath = _FindFontFile("Sans");

        if(fontPath.empty() || FT_New_Face(ft, fontPath.c_str(), 0, &face)){
            FT_Done_FreeType(ft);
            return _newfont;
        }
    }
    if(FT_Set_Pixel_Sizes(face, 0, fontSize)){
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return _newfont;
    }
    int ascent = (int)(face->size->metrics.ascender >> 6);
    int descent = (int)(- (face->size->metrics.descender >> 6));
    int charHeight = (int)(face->size->metrics.height >> 6);

    std::vector<int> charWidths(224);
    int maxRowWidth = 0;
    int currentRowWidth = 0;

    /* Quanto salgono e scendono DAVVERO i glifi rispetto alla linea di base.
       Prima ogni riga dell'atlante era alta charHeight (l'interlinea del font)
       e il glifo veniva disegnato a "ascent - bitmap_top": per i glifi piu'
       alti dell'ascent - le maiuscole accentate - quel valore e' NEGATIVO e il
       glifo finiva sopra la propria cella, cioe' nel fondo della cella della
       riga precedente. Quei pixel estranei venivano poi inclusi da AdjustFONT
       nell'altezza comune e comparivano come piccoli segni sotto i caratteri
       (visibile su Linux con "Nimbus Mono PS").
       Misurando maxTop e maxBelow la riga e' alta quanto basta e nessun glifo
       esce dalla propria cella. */
    int maxTop = 0;
    int maxBelow = 0;
    for (int i = 0; i < 224; i++) {
        int code = _CP1252ToUnicode(i + 32);
        FT_UInt gi = FT_Get_Char_Index(face, code);
        if(!gi || FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT)){
            charWidths[i] = std::max(1, fontSize / 2);
            currentRowWidth += charWidths[i];
            if ((i + 1) % 32 == 0) {
                if (currentRowWidth > maxRowWidth) maxRowWidth = currentRowWidth;
                currentRowWidth = 0;
            }
            continue;
        }
        int wi = _FTAdvanceX(face->glyph) + 1;
        charWidths[i] = wi;

        /* 26.6 fixed point: arrotondo per eccesso per non perdere una riga */
        int gTop   = (int)((face->glyph->metrics.horiBearingY + 63) >> 6);
        int gBelow = (int)((face->glyph->metrics.height - face->glyph->metrics.horiBearingY + 63) >> 6);
        if (gTop   > maxTop)   maxTop   = gTop;
        if (gBelow > maxBelow) maxBelow = gBelow;

        currentRowWidth += wi;
        if ((i + 1) % 32 == 0) {
            if (currentRowWidth > maxRowWidth) maxRowWidth = currentRowWidth;
            currentRowWidth = 0;
        }
    }
    if (currentRowWidth > 0) {
        if (currentRowWidth > maxRowWidth) maxRowWidth = currentRowWidth;
    }

    int charsPerRow = 32;
    int numRows = 7;

    if (maxTop   < ascent)  maxTop   = ascent;
    if (maxBelow < descent) maxBelow = descent;
    int rowPitch = maxTop + maxBelow;
    if (rowPitch < charHeight) rowPitch = charHeight;
    int baseline = maxTop;

    int atlasWidth = maxRowWidth + (int)(64*(fontSize/10.0f));
    int atlasHeight = numRows * rowPitch;

    std::vector<uint32_t> pixels(atlasWidth * atlasHeight, 0xFFFFFFFFu);

    _newfont->xFont = Array<int>(224);
    _newfont->yFont = Array<int>(224);
    _newfont->wFont = Array<int>(224);

    int charIndex = 0;
    int currentX = 0;
    int currentY = 0;

    for (int i = 32; i < 256; i++) {
        int code = _CP1252ToUnicode(i);
        FT_UInt gi = FT_Get_Char_Index(face, code);
        if(!gi || FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT) || FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)){
            int row = charIndex / charsPerRow;
            int col = charIndex % charsPerRow;
            if (col == 0) {
                currentX = 0;
                currentY = row * rowPitch;
            }
            _newfont->xFont[charIndex] = currentX;
            _newfont->yFont[charIndex] = currentY;
            _newfont->wFont[charIndex] = charWidths[charIndex];
            currentX += _newfont->wFont[charIndex] + (int)(2*(fontSize/10.0f));
            charIndex++;
            continue;
        }
        int row = charIndex / charsPerRow;
        int col = charIndex % charsPerRow;
        if (col == 0) {
            currentX = 0;
            currentY = row * rowPitch;
        }
        _newfont->xFont[charIndex] = currentX;
        _newfont->yFont[charIndex] = currentY;
        _newfont->wFont[charIndex] = charWidths[charIndex];
        int dstX = currentX + face->glyph->bitmap_left;
        int dstY = currentY + baseline - face->glyph->bitmap_top;
        int bw = face->glyph->bitmap.width;
        int bh = face->glyph->bitmap.rows;
        int pitch = face->glyph->bitmap.pitch;
        for(int yy=0; yy<bh; ++yy){
            for(int xx=0; xx<bw; ++xx){
                int sx = xx;
                int sy = yy;
                uint8_t a = face->glyph->bitmap.buffer[sy * pitch + sx];
                if(a){
                    int x = dstX + xx;
                    int y = dstY + yy;
                    /* Fuori dalla PROPRIA cella non si scrive: prima _Clamp
                       spalmava i pixel in eccesso sul bordo dell'atlante,
                       sporcando le celle vicine. */
                    if (x < 0 || x >= atlasWidth) continue;
                    if (y < currentY || y >= currentY + rowPitch) continue;
                    if (y < 0 || y >= atlasHeight) continue;
                    *_PixelPtr(pixels, atlasWidth, x, y) = _BlendOnWhite(a);
                }
            }
        }
        currentX += _newfont->wFont[charIndex] + (int)(2*(fontSize/10.0f));
        charIndex++;
    }

    int dataSize = atlasWidth * atlasHeight * 4;
    _newfont->dataPixel = Array<int>(dataSize + 2);
    _newfont->dataPixel[0] = atlasWidth;
    _newfont->dataPixel[1] = atlasHeight;
    const uint8_t* raw = (const uint8_t*)pixels.data();
    for (int i = 0; i < dataSize; i++) {
        _newfont->dataPixel[i + 2] = raw[i];
    }
    _newfont->hFont = rowPitch;

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    return _newfont;
}


/* ------------------------------------------------------------------------
   _CreateTextImage

   Disegna UNA scritta e restituisce solo quella, invece dell'atlante con
   tutti i 224 caratteri: l'immagine e' grande quanto la scritta.

   L'immagine e' costruita SIMMETRICA rispetto alla linea di base - sopra e
   sotto c'e' lo stesso spazio - cosi' chi disegna la centra nella riga e
   tutti i font risultano allineati sulla stessa base, comprese le famiglie
   con metriche insolite. Lo spazio in piu' e' trasparente.

   dataPixel esce come [ larghezza, altezza, poi B,G,R,A per ogni pixel ],
   nero su bianco: e' il lato Cerberus che inverte e ne ricava l'alfa.
   ------------------------------------------------------------------------ */
BBGetFont* _CreateTextImage( String fontname, int fontSize, String text, int alignx ){

    BBGetFont *_newfont = new BBGetFont();

    if( fontSize<1 ) fontSize = 1;

    FT_Library ft = nullptr;
    if( FT_Init_FreeType( &ft ) ) return _newfont;

    FT_Face face = nullptr;

    /* prima si prova come file dell'applicazione, poi fra le famiglie */
    std::string appPath = _FontFilePath( fontname );

    if( !appPath.empty() ) FT_New_Face( ft,appPath.c_str(),0,&face );

    if( !face ){

        std::string fontName = convertBBString( fontname );
        std::string fontPath = _FindFontFile( fontName );

        if( fontPath.empty() && fontName!="Sans" ) fontPath = _FindFontFile( "Sans" );

        if( fontPath.empty() || FT_New_Face( ft,fontPath.c_str(),0,&face ) ){
            FT_Done_FreeType( ft );
            return _newfont;
        }
    }

    if( FT_Set_Pixel_Sizes( face,0,fontSize ) ){
        FT_Done_Face( face );
        FT_Done_FreeType( ft );
        return _newfont;
    }

    int ascent  = (int)( face->size->metrics.ascender>>6 );
    int descent = (int)( -( face->size->metrics.descender>>6 ) );
    int lineH   = (int)( face->size->metrics.height>>6 );

    if( ascent<0 ) ascent = 0;
    if( descent<0 ) descent = 0;
    if( lineH<1 ) lineH = ascent+descent;
    if( lineH<1 ) lineH = fontSize;

    /* il testo arriva gia' in Unicode: si spezza sui ritorni a capo */
    std::vector< std::vector<int> > lines;
    std::vector<int> cur;

    int nch = text.Length();

    for( int i=0;i<nch;++i ){
        int c = (int)text[i];
        if( c==13 ) c = 10;
        if( c==10 ){ lines.push_back( cur ); cur.clear(); }
        else cur.push_back( c );
    }

    lines.push_back( cur );

    int pad = fontSize/4;
    if( pad<4 ) pad = 4;

    /* larghezza di ogni riga, sommando gli avanzamenti */
    int maxWidth = 1;
    std::vector<int> lw( lines.size(),0 );

    for( size_t l=0;l<lines.size();++l ){

        int w = 0;

        for( size_t k=0;k<lines[l].size();++k ){

            FT_UInt gi = FT_Get_Char_Index( face,lines[l][k] );

            if( !gi || FT_Load_Glyph( face,gi,FT_LOAD_DEFAULT ) ){
                w += fontSize/2;
                continue;
            }

            w += _FTAdvanceX( face->glyph );
        }

        if( w<1 ) w = 1;

        lw[l] = w;

        if( w>maxWidth ) maxWidth = w;
    }

    int half = ascent>descent ? ascent : descent;
    int side = half+pad;

    int atlasWidth  = maxWidth+pad*2;
    int atlasHeight = side*2+( (int)lines.size()-1 )*lineH;

    if( atlasWidth<1 ) atlasWidth = 1;
    if( atlasHeight<1 ) atlasHeight = 1;

    std::vector<uint32_t> pixels( atlasWidth*atlasHeight,0xFFFFFFFFu );

    for( size_t l=0;l<lines.size();++l ){

        int penX = pad;

        if( alignx==1 ) penX = pad+( maxWidth-lw[l] )/2;
        else if( alignx==2 ) penX = pad+( maxWidth-lw[l] );

        int baseline = side+(int)l*lineH;

        for( size_t k=0;k<lines[l].size();++k ){

            FT_UInt gi = FT_Get_Char_Index( face,lines[l][k] );

            if( !gi || FT_Load_Glyph( face,gi,FT_LOAD_DEFAULT ) ||
                FT_Render_Glyph( face->glyph,FT_RENDER_MODE_NORMAL ) ){
                penX += fontSize/2;
                continue;
            }

            int dstX = penX+face->glyph->bitmap_left;
            int dstY = baseline-face->glyph->bitmap_top;

            int bw = face->glyph->bitmap.width;
            int bh = face->glyph->bitmap.rows;
            int pitch = face->glyph->bitmap.pitch;

            for( int yy=0;yy<bh;++yy ){
                for( int xx=0;xx<bw;++xx ){

                    uint8_t a = face->glyph->bitmap.buffer[ yy*pitch+xx ];

                    if( !a ) continue;

                    int x = dstX+xx;
                    int y = dstY+yy;

                    if( x<0 || x>=atlasWidth ) continue;
                    if( y<0 || y>=atlasHeight ) continue;

                    *_PixelPtr( pixels,atlasWidth,x,y ) = _BlendOnWhite( a );
                }
            }

            penX += _FTAdvanceX( face->glyph );
        }
    }

    int dataSize = atlasWidth*atlasHeight*4;

    _newfont->dataPixel = Array<int>( dataSize+2 );
    _newfont->dataPixel[0] = atlasWidth;
    _newfont->dataPixel[1] = atlasHeight;

    const uint8_t *raw = (const uint8_t*)pixels.data();

    for( int i=0;i<dataSize;++i ) _newfont->dataPixel[i+2] = raw[i];

    _newfont->hFont = lineH;

    FT_Done_Face( face );
    FT_Done_FreeType( ft );

    return _newfont;
}
