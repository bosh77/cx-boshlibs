#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>
#include <vector>
#include <string>
#include <stdint.h>
 
#import <Cocoa/Cocoa.h>


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

int BBblFont::_SelectFont(){

    //[self showFontPanel:nil];

    //showFontPanel;

    
// ... nel tuo metodo (es. viewDidLoad)

// NSFontManager *fontManager = [NSFontManager sharedFontManager];
// NSArray<NSString *> *fontFamilyNames = [fontManager availableFontFamilies];

// // I nomi delle famiglie sono solitamente ordinati.
// // Puoi iterare sull'array per usarli:
// for (NSString *familyName in fontFamilyNames) {
//     NSLog(@"Famiglia Font: %@", familyName);
// }

    return 0;
}

String BBblFont::_GetFontListString() {
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSArray<NSString *> *fontFamilyNames = [fontManager availableFontFamilies];

    String fontListString = "";
    bool first = true;

    for (NSString *familyName in fontFamilyNames) {
        if (!first) {
            fontListString += ";";
        }
        fontListString += String([familyName UTF8String]);
        first = false;
    }

    return fontListString;
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
    Array<int> hGlyph;
private:
};

BBGetFont::BBGetFont(){
}

BBGetFont::~BBGetFont(){
}

static UniChar _MapCP1252ToUnicode(int ch){
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
    return (UniChar)ch;
}

/*  Font caricato da un FILE dell'applicazione invece che da una famiglia
    installata. Su macOS non serve registrare niente nel sistema: si crea un
    CGFont dal file e lo si passa a CoreText.

    Accetta "cerberus://data/xxx.ttf" (risolto dentro il bundle, accanto
    all'eseguibile) e i percorsi normali.                                   */
static bool _EndsWithNoCase(const std::string &s, const char *suf, size_t n){
    if(s.length() < n) return false;
    for(size_t i = 0; i < n; ++i){
        char c = s[s.length()-n+i];
        if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if(c != suf[i]) return false;
    }
    return true;
}

static CTFontRef _CTFontFromFile(String fontname, int fontSize){

    std::string n = convertBBString(fontname);

    if(!_EndsWithNoCase(n, ".ttf", 4) && !_EndsWithNoCase(n, ".otf", 4) && !_EndsWithNoCase(n, ".ttc", 4)) return nullptr;

    std::string path = convertBBString(BBGame::Game()->PathToFilePath(fontname));

    CGDataProviderRef prov = CGDataProviderCreateWithFilename(path.c_str());
    if(!prov) return nullptr;

    CGFontRef cg = CGFontCreateWithDataProvider(prov);
    CGDataProviderRelease(prov);
    if(!cg) return nullptr;

    CTFontRef ct = CTFontCreateWithGraphicsFont(cg, (CGFloat)fontSize, nullptr, nullptr);
    CGFontRelease(cg);

    return ct;
}

BBGetFont* _CreateFont(String fontname, int fontSize){
    BBGetFont* _newfont=new BBGetFont();
    std::string fontName = convertBBString(fontname);

    /* prima si prova come file dell'applicazione */
    CTFontRef ctFont = _CTFontFromFile(fontname, fontSize);

    if(!ctFont){
        CFStringRef cfFontName = CFStringCreateWithCString(kCFAllocatorDefault, fontName.c_str(), kCFStringEncodingUTF8);
        ctFont = CTFontCreateWithName(cfFontName, (CGFloat)fontSize, nullptr);
        CFRelease(cfFontName);
    }

    CGFloat ascent = CTFontGetAscent(ctFont);
    CGFloat descent = CTFontGetDescent(ctFont);
    CGFloat leading = CTFontGetLeading(ctFont); (void)leading;
    // charHeight verrà calcolata dall'inchiostro reale dei glifi per evitare tagli
    int charHeight = 0;

    // Larghezze per 224 caratteri (codici 32..255), indicizzate con (code-32)
    std::vector<int> charWidths(224);
    int maxRowWidth = 0;
    int currentRowWidth = 0;

    // Estensione reale dell'inchiostro sopra/sotto la baseline su TUTTI i glifi:
    // le metriche ascent/descent non bastano (maiuscole accentate, parentesi, @...)
    int maxAbove = (int)ceil(ascent);
    int maxBelow = (int)ceil(descent);
    
    for (int i = 0; i < 224; i++) {
        int code = i + 32;
        UniChar uch = _MapCP1252ToUnicode(code);
        CFStringRef s = CFStringCreateWithCharacters(kCFAllocatorDefault, &uch, 1);
        CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(attrs, kCTFontAttributeName, ctFont);
        CFAttributedStringRef attrStr = CFAttributedStringCreate(kCFAllocatorDefault, s, attrs);
        CTLineRef line = CTLineCreateWithAttributedString(attrStr);
        CGFloat w = (CGFloat)CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
        int wi = (int)ceil(w) + 1;
        // salva in indice 0..223
        charWidths[i] = wi;
        // misura altezza reale del glifo per la riga
        CGRect gb = CTLineGetBoundsWithOptions(line, kCTLineBoundsUseGlyphPathBounds);
        if (gb.size.height > 0) {
            int above = (int)ceil(gb.origin.y + gb.size.height);
            int below = (int)ceil(-gb.origin.y);
            if (above > maxAbove) maxAbove = above;
            if (below > maxBelow) maxBelow = below;
        }
        currentRowWidth += wi+4;
        if ((i + 1) % 32 == 0) {
            if (currentRowWidth > maxRowWidth) maxRowWidth = currentRowWidth;
            currentRowWidth = 0;
        }
        CFRelease(line);
        CFRelease(attrStr);
        CFRelease(attrs);
        CFRelease(s);
    }
    if (currentRowWidth > 0) {
        if (currentRowWidth > maxRowWidth) maxRowWidth = currentRowWidth;
    }
    // AdjustFONT() (blFonts.cxs, ramo macos) scansiona la cella solo da y+PAD_TOP a
    // y+charHeight-1: riserviamo quel margine in alto e teniamo tutto l'inchiostro dentro.
    const int PAD_TOP = 7;   // deve restare >= KK1 di AdjustFONT
    const int PAD_BOT = 3;
    charHeight = PAD_TOP + maxAbove + maxBelow + PAD_BOT;

	int KK=14;
    int charsPerRow = 32;
    int numRows = 7;
    int atlasWidth = maxRowWidth + (int)(64*(fontSize/10.0f));
    int atlasHeight = numRows * charHeight;


    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    size_t bytesPerRow = atlasWidth * 4;
    void* data = calloc(atlasHeight+KK, bytesPerRow);
    CGContextRef ctx = CGBitmapContextCreate(data, atlasWidth, atlasHeight+KK, 8, bytesPerRow, cs, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);

    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    CGContextFillRect(ctx, CGRectMake(0, 0, atlasWidth, atlasHeight+KK));
    CGContextTranslateCTM(ctx, 0, 0);
    CGContextScaleCTM(ctx, 1, 1);
    // Assicura che la matrice del testo sia neutra e disegna come riempimento
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetTextDrawingMode(ctx, kCGTextFill);
    CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
    CFMutableDictionaryRef baseAttrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(baseAttrs, kCTFontAttributeName, ctFont);
    CFDictionaryAddValue(baseAttrs, kCTForegroundColorFromContextAttributeName, kCFBooleanTrue);

    // Tabelle 0..223 per codici 32..255
    _newfont->xFont = Array<int>(224);
    _newfont->yFont = Array<int>(224);
    _newfont->wFont = Array<int>(224);
    _newfont->hGlyph = Array<int>(224);

    int charIndex = 0;
    int currentX = 0;
    int currentY = 0;

    for (int i = 32; i < 256; i++) {
        UniChar uch = _MapCP1252ToUnicode(i);
        CFStringRef s = CFStringCreateWithCharacters(kCFAllocatorDefault, &uch, 1);
        CFAttributedStringRef attrStr = CFAttributedStringCreate(kCFAllocatorDefault, s, baseAttrs);
        CTLineRef line = CTLineCreateWithAttributedString(attrStr);
        int row = charIndex / charsPerRow;
        int col = charIndex % charsPerRow;
        // Reset di currentX/currentY a inizio riga (packing sequenziale, come su Windows:
        // una griglia a passo fisso taglierebbe i caratteri piu' larghi della media)
        if (col == 0) {
            currentX = 0;
            currentY = row * charHeight;
        }

        // Misura bound del glifo e gestisci sidebearing sinistro negativo (corsivi/script)
        CGRect drawBounds = CTLineGetBoundsWithOptions(line, kCTLineBoundsUseGlyphPathBounds);
        int leftBearing = (drawBounds.origin.x < 0) ? (int)ceil(-drawBounds.origin.x) : 0;
        // Scrive nelle tabelle usando indice 0..223 (charIndex): yFont = bordo superiore cella
        _newfont->xFont[charIndex] = currentX;
        _newfont->yFont[charIndex] = currentY;
        _newfont->wFont[charIndex] = charWidths[charIndex];
        _newfont->hGlyph[charIndex] = (int)ceil(drawBounds.size.height);
        // Baseline: il glifo piu' alto parte esattamente a currentY+PAD_TOP e il piu'
        // basso finisce entro la cella (il bitmap CG ha origine in basso a sinistra).
        int baselineRow = currentY + PAD_TOP + maxAbove;
        CGFloat baselineY = (CGFloat)((atlasHeight + KK) - 1 - baselineRow);
        CGContextSetTextPosition(ctx, (CGFloat)(currentX + leftBearing), baselineY);
        CTLineDraw(line, ctx);
        currentX += charWidths[charIndex] + (int)(2*(fontSize/10.0f));
        charIndex++;
        CFRelease(line);
        CFRelease(attrStr);
        CFRelease(s);
    }

    int dataSize = atlasWidth * (atlasHeight + KK) * 4;
    uint8_t* pixelData = (uint8_t*)CGBitmapContextGetData(ctx);
    _newfont->dataPixel = Array<int>(dataSize + 2);
    _newfont->dataPixel[0] = atlasWidth;
    _newfont->dataPixel[1] = atlasHeight + KK;

    // Copia diretta top-down (come prima): il contesto è già invertito
    
    size_t stride = atlasWidth * 4;
    for (int y = 0; y < atlasHeight+KK; ++y) {
        const uint8_t* srcRow = pixelData + (y * stride);
        int dstOffset = 2 + (y * stride);
        for (size_t x = 0; x < stride; ++x) {
            _newfont->dataPixel[dstOffset + x] = srcRow[x];
        }
    }

    // Specchia verticalmente i singoli caratteri nelle loro celle (per-glifo)
    // for (int i = 0; i < 224; ++i) {
    //     int gx = _newfont->xFont[i];
    //     int gyTop = _newfont->yFont[i];
    //     int gw = _newfont->wFont[i];
    //     int gh = _newfont->hGlyph[i];
    //     if (gw <= 0 || gh <= 1) continue;
    //     int gyBottom = gyTop + gh - 1;
    //     // Bound checks
    //     if (gx < 0 || gyTop < 0) continue;
    //     if (gx + gw > atlasWidth) gw = atlasWidth - gx;
    //     if (gyBottom >= atlasHeight) gh = atlasHeight - gyTop;
    //     for (int r = 0; r < gh / 2; ++r) {
    //         int rowTop = gyTop + r;
    //         int rowBottom = gyBottom - r;
    //         int topOff = 2 + rowTop * (int)stride + gx * 4;
    //         int botOff = 2 + rowBottom * (int)stride + gx * 4;
    //         for (int k = 0; k < gw * 4; ++k) {
    //             int tmp = _newfont->dataPixel[topOff + k];
    //             _newfont->dataPixel[topOff + k] = _newfont->dataPixel[botOff + k];
    //             _newfont->dataPixel[botOff + k] = tmp;
    //         }
    //     }
    // }

    _newfont->hFont = charHeight;

    CFRelease(baseAttrs);
    CGContextRelease(ctx);
    free(data);
    CFRelease(ctFont);

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

    std::string fontName = convertBBString( fontname );

    /* prima si prova come file dell'applicazione */
    CTFontRef ctFont = _CTFontFromFile( fontname,fontSize );

    if( !ctFont ){
        CFStringRef cfFontName = CFStringCreateWithCString( kCFAllocatorDefault,fontName.c_str(),kCFStringEncodingUTF8 );
        ctFont = CTFontCreateWithName( cfFontName,(CGFloat)fontSize,nullptr );
        CFRelease( cfFontName );
    }

    if( !ctFont ) return _newfont;

    int ascent  = (int)ceil( CTFontGetAscent( ctFont ) );
    int descent = (int)ceil( CTFontGetDescent( ctFont ) );
    int leading = (int)ceil( CTFontGetLeading( ctFont ) );

    if( ascent<0 ) ascent = 0;
    if( descent<0 ) descent = 0;

    int lineH = ascent+descent+leading;
    if( lineH<1 ) lineH = fontSize;

    /* il testo arriva gia' in UTF-16: si spezza sui ritorni a capo */
    std::vector< std::vector<UniChar> > lines;
    std::vector<UniChar> cur;

    int nch = text.Length();

    for( int i=0;i<nch;++i ){
        int c = (int)text[i];
        if( c==13 ) c = 10;
        if( c==10 ){ lines.push_back( cur ); cur.clear(); }
        else cur.push_back( (UniChar)c );
    }

    lines.push_back( cur );

    int pad = fontSize/4;
    if( pad<4 ) pad = 4;

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable( kCFAllocatorDefault,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks );
    CFDictionaryAddValue( attrs,kCTFontAttributeName,ctFont );
    CFDictionaryAddValue( attrs,kCTForegroundColorFromContextAttributeName,kCFBooleanTrue );

    /* una CTLine per riga, misurata subito */
    std::vector<CTLineRef> ctlines( lines.size(),nullptr );
    std::vector<int> lw( lines.size(),0 );

    int maxWidth = 1;

    for( size_t l=0;l<lines.size();++l ){

        CFStringRef s;

        if( lines[l].empty() ){
            UniChar sp = (UniChar)' ';
            s = CFStringCreateWithCharacters( kCFAllocatorDefault,&sp,1 );
        }else{
            s = CFStringCreateWithCharacters( kCFAllocatorDefault,lines[l].data(),(CFIndex)lines[l].size() );
        }

        CFAttributedStringRef as = CFAttributedStringCreate( kCFAllocatorDefault,s,attrs );
        CTLineRef line = CTLineCreateWithAttributedString( as );

        double w = CTLineGetTypographicBounds( line,nullptr,nullptr,nullptr );

        int wi = (int)ceil( w );
        if( wi<1 ) wi = 1;

        lw[l] = wi;
        if( wi>maxWidth ) maxWidth = wi;

        ctlines[l] = line;

        CFRelease( as );
        CFRelease( s );
    }

    int half = ascent>descent ? ascent : descent;
    int side = half+pad;

    int atlasWidth  = maxWidth+pad*2;
    int atlasHeight = side*2+( (int)lines.size()-1 )*lineH;

    if( atlasWidth<1 ) atlasWidth = 1;
    if( atlasHeight<1 ) atlasHeight = 1;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    size_t bytesPerRow = atlasWidth*4;
    void *data = calloc( atlasHeight,bytesPerRow );
    CGContextRef ctx = CGBitmapContextCreate( data,atlasWidth,atlasHeight,8,bytesPerRow,cs,kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little );
    CGColorSpaceRelease( cs );

    CGContextSetRGBFillColor( ctx,1,1,1,1 );
    CGContextFillRect( ctx,CGRectMake( 0,0,atlasWidth,atlasHeight ) );
    CGContextSetTextMatrix( ctx,CGAffineTransformIdentity );
    CGContextSetTextDrawingMode( ctx,kCGTextFill );
    CGContextSetRGBFillColor( ctx,0,0,0,1 );

    for( size_t l=0;l<lines.size();++l ){

        int x = pad;

        if( alignx==1 ) x = pad+( maxWidth-lw[l] )/2;
        else if( alignx==2 ) x = pad+( maxWidth-lw[l] );

        /* il bitmap di Core Graphics ha l'origine in basso a sinistra */
        int baselineRow = side+(int)l*lineH;
        CGFloat baselineY = (CGFloat)( atlasHeight-1-baselineRow );

        CGContextSetTextPosition( ctx,(CGFloat)x,baselineY );
        CTLineDraw( ctlines[l],ctx );
    }

    int dataSize = atlasWidth*atlasHeight*4;

    uint8_t *pixelData = (uint8_t*)CGBitmapContextGetData( ctx );

    _newfont->dataPixel = Array<int>( dataSize+2 );
    _newfont->dataPixel[0] = atlasWidth;
    _newfont->dataPixel[1] = atlasHeight;

    for( int i=0;i<dataSize;++i ) _newfont->dataPixel[i+2] = pixelData[i];

    _newfont->hFont = lineH;

    for( size_t l=0;l<lines.size();++l ) if( ctlines[l] ) CFRelease( ctlines[l] );

    CFRelease( attrs );
    CGContextRelease( ctx );
    free( data );
    CFRelease( ctFont );

    return _newfont;
}
