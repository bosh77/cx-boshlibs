

//----------------------  FONTS  ----------------------

void MAXWIN() {

    HWND hWnd;
    hWnd=GetActiveWindow();
    ShowWindow(hWnd, SW_MAXIMIZE);

}



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

    struct FontEnumData {
    vector<string> fontNames;
};

// String _GetFontListString();
static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEX* lpelfe, NEWTEXTMETRICEX* lpntme, DWORD FontType, LPARAM lParam);
static int CALLBACK EnumFontFamProc(LOGFONT* lpLogFont, TEXTMETRIC* lpTextMetric, DWORD FontType, LPARAM lParam);

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

    CHOOSEFONT cf;
    LOGFONT lf;
    HFONT hfont;
    
    lf = {0};

    ZeroMemory(&lf, sizeof(lf));

    string fname = convertBBString(_fontname);

    memcpy( (LPSTR)&lf.lfFaceName, fname.c_str(), fname.length()+1 );

    
    HDC hdc = GetDC(GetActiveWindow());
    
    lf.lfHeight           = -MulDiv(_dimensione, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    lf.lfWidth            = 0;
    lf.lfEscapement       = 0;
    lf.lfOrientation      = 0;
    lf.lfWeight           = _peso;
    lf.lfItalic           = _ital;
    lf.lfUnderline        = FALSE;
    lf.lfStrikeOut        = FALSE;
    lf.lfCharSet          = DEFAULT_CHARSET;
    lf.lfOutPrecision     = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision    = CLIP_DEFAULT_PRECIS;
    lf.lfQuality          = DEFAULT_QUALITY;
    lf.lfPitchAndFamily   = DEFAULT_PITCH | FF_DONTCARE;


    ZeroMemory(&cf, sizeof(cf));
    

    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = GetActiveWindow();
    cf.lpLogFont = &lf;
    
    cf.Flags = CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_TTONLY;;


    if (ChooseFont(&cf)) {

    hfont = CreateFontIndirect(&lf);

    _fontname=lf.lfFaceName;
    
    _dimensione=-MulDiv(lf.lfHeight,72, GetDeviceCaps(hdc, LOGPIXELSY));

    _peso=lf.lfWeight;
    _ital=lf.lfItalic;
    

    }

    return 0;

}

String BBblFont::_GetFontListString() {

    static string result;
    FontEnumData enumData;
    
    // printf("Inizio enumerazione font...\n");
    
    HDC hdc = GetDC(NULL);
    if (hdc == NULL) {
        printf("ERRORE: Impossibile ottenere device context\n");
        return "";
    }
    
    // Usa EnumFontFamilies che enumera solo i nomi delle famiglie
    int result_enum = EnumFontFamilies(hdc, NULL, (FONTENUMPROC)EnumFontFamProc, (LPARAM)&enumData);
    
    // printf("EnumFontFamilies risultato: %d\n", result_enum);
    // printf("Font trovati: %zu\n", enumData.fontNames.size());
    
    ReleaseDC(NULL, hdc);
    
    // Costruisci la stringa
    result = "";
    for (size_t i = 0; i < enumData.fontNames.size(); i++) {
        if (i > 0) {
            result += ";";
        }
        result += enumData.fontNames[i];
    }
    
    if (result.empty()) {
        result = "Arial,Times New Roman,Courier New,Verdana,Tahoma";
    }
    
    // return "A,B,C";
    
    return String(result.c_str());

}

// Callback function per EnumFontFamiliesEx
int CALLBACK BBblFont::EnumFontFamExProc(
    ENUMLOGFONTEX* lpelfe,
    NEWTEXTMETRICEX* lpntme,
    DWORD FontType,
    LPARAM lParam
) {
    FontEnumData* data = (FontEnumData*)lParam;
    
    if (!data) {
        printf("ERRORE: data è NULL nel callback\n");
        return 0;
    }
    
    // Converti il nome del font
    string fontName;
    
    // Usa sempre la versione ANSI per semplicità
    fontName = string((char*)lpelfe->elfLogFont.lfFaceName);
    
    printf("Font trovato: %s\n", fontName.c_str());
    
    // Aggiungi il nome del font alla lista (evita duplicati)
    bool found = false;
    for (const auto& name : data->fontNames) {
        if (name == fontName) {
            found = true;
            break;
        }
    }
    
    if (!found && !fontName.empty()) {
        // data->fontNames.push_back(fontName);
        // printf("Font aggiunto: %s (totale: %zu)\n", fontName.c_str(), data->fontNames.size());
    }
    
    return 1; // Continua l'enumerazione
}

int CALLBACK BBblFont::EnumFontFamProc(
    LOGFONT* lpLogFont,
    TEXTMETRIC* lpTextMetric,
    DWORD FontType,
    LPARAM lParam
) {
    FontEnumData* data = (FontEnumData*)lParam;
    
    if (!data) return 0;
    
    string fontName = string((char*)lpLogFont->lfFaceName);
    
    // Controlla duplicati
    bool found = false;
    for (const auto& name : data->fontNames) {
        if (name == fontName) {
            found = true;
            break;
        }
    }
    
    if (!found && !fontName.empty()) {
        data->fontNames.push_back(fontName);
        // printf("Font famiglia aggiunta: %s\n", fontName.c_str());
    }
    
    return 1;
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



//--------------------------------------------------------------------------
//  FONT CARICATO DA UN FILE DELL'APPLICAZIONE
//
//  CreateFont vuole il NOME della famiglia, non un percorso. Per usare un
//  .ttf che viaggia con l'applicazione servono due passi:
//    1) registrarlo nel processo con AddFontResourceEx
//    2) chiederlo per il nome scritto DENTRO al file, nella tabella 'name'
//       del formato sfnt (nameID 1 = famiglia)
//  E' la stessa lettura che fa la versione Android.
//
//  Accetta "cerberus://data/xxx.ttf" (risolto accanto all'eseguibile) e i
//  percorsi normali. Con qualsiasi altro nome non fa nulla e si prosegue
//  come prima, per famiglia installata nel sistema.
//--------------------------------------------------------------------------

static std::wstring _blfWide( String s ){
    return std::wstring( s.Data(), s.Length() );
}

static bool _blfEndsWith( const std::wstring &s, const wchar_t *suf, size_t sufLen ){
    if( s.length() < sufLen ) return false;
    for( size_t i = 0; i < sufLen; ++i ){
        wchar_t c = s[ s.length() - sufLen + i ];
        if( c >= L'A' && c <= L'Z' ) c = (wchar_t)( c - L'A' + L'a' );
        if( c != suf[i] ) return false;
    }
    return true;
}

static unsigned int _blfU16( const unsigned char *p ){
    return ( (unsigned int)p[0] << 8 ) | (unsigned int)p[1];
}

static unsigned int _blfU32( const unsigned char *p ){
    return ( (unsigned int)p[0] << 24 ) | ( (unsigned int)p[1] << 16 ) | ( (unsigned int)p[2] << 8 ) | (unsigned int)p[3];
}

/*  Preferisco le stringhe Windows in inglese, poi le Unicode, poi le Mac. */
static int _blfNameScore( unsigned int platform, unsigned int encoding, unsigned int language ){
    if( platform == 3 && language == 0x409 ) return 3;
    if( platform == 3 ) return 2;
    if( platform == 0 ) return 1;
    if( platform == 1 && encoding == 0 ) return 0;
    return -1;
}

static std::wstring _blfFaceNameFromFile( const std::wstring &path ){

    std::wstring result;

    FILE *fp = _wfopen( path.c_str(), L"rb" );
    if( !fp ) return result;

    fseek( fp, 0, SEEK_END );
    long len = ftell( fp );
    fseek( fp, 0, SEEK_SET );

    if( len < 12 ){ fclose( fp ); return result; }

    std::vector<unsigned char> buf( (size_t)len );
    size_t got = fread( &buf[0], 1, (size_t)len, fp );
    fclose( fp );

    if( got != (size_t)len ) return result;

    const unsigned char *d = &buf[0];
    size_t n = (size_t)len;

    size_t base = 0;
    unsigned int tag = _blfU32( d );

    if( tag == 0x74746366 ){                    /* 'ttcf': collezione, primo font */
        if( n < 16 ) return result;
        base = (size_t)_blfU32( d + 12 );
        if( base + 12 > n ) return result;
        tag = _blfU32( d + base );
    }

    if( tag != 0x00010000 && tag != 0x4F54544F ) return result;

    unsigned int numTables = _blfU16( d + base + 4 );
    size_t dir = base + 12;
    if( dir + (size_t)numTables * 16 > n ) return result;

    size_t nameOff = 0;

    for( unsigned int i = 0; i < numTables; ++i ){
        const unsigned char *rec = d + dir + (size_t)i * 16;
        if( _blfU32( rec ) == 0x6E616D65 ){     /* 'name' */
            nameOff = (size_t)_blfU32( rec + 8 );
            break;
        }
    }

    if( !nameOff || nameOff + 6 > n ) return result;

    unsigned int count  = _blfU16( d + nameOff + 2 );
    size_t       strOff = (size_t)_blfU16( d + nameOff + 4 );

    if( nameOff + 6 + (size_t)count * 12 > n ) return result;

    int best = -1;

    for( unsigned int i = 0; i < count; ++i ){

        const unsigned char *r = d + nameOff + 6 + (size_t)i * 12;

        unsigned int platform = _blfU16( r );
        unsigned int encoding = _blfU16( r + 2 );
        unsigned int language = _blfU16( r + 4 );
        unsigned int nameId   = _blfU16( r + 6 );
        unsigned int length   = _blfU16( r + 8 );
        size_t       offset   = (size_t)_blfU16( r + 10 );

        if( nameId != 1 || !length ) continue;

        int score = _blfNameScore( platform, encoding, language );
        if( score <= best ) continue;

        size_t at = nameOff + strOff + offset;
        if( at + length > n ) continue;

        std::wstring s;

        if( platform == 1 ){
            for( unsigned int k = 0; k < length; ++k ) s += (wchar_t)d[ at + k ];
        }else{
            for( unsigned int k = 0; k + 1 < length; k += 2 ) s += (wchar_t)( ( (unsigned int)d[at+k] << 8 ) | (unsigned int)d[at+k+1] );
        }

        if( s.empty() ) continue;

        best = score;
        result = s;
    }

    return result;
}

/*  Ritorna il nome di famiglia da passare a CreateFont, registrando il file
    la prima volta. Stringa vuota se il nome non e' un file di font.        */
static std::wstring _blfRegisterFontFile( String fontname ){

    std::wstring name = _blfWide( fontname );

    if( !_blfEndsWith( name, L".ttf", 4 ) &&
        !_blfEndsWith( name, L".otf", 4 ) &&
        !_blfEndsWith( name, L".ttc", 4 ) ) return std::wstring();

    /* "cerberus://data/x.ttf" -> percorso reale accanto all'eseguibile */
    std::wstring path = _blfWide( BBGame::Game()->PathToFilePath( fontname ) );

    /* un file va registrato una volta sola */
    static std::vector<std::wstring> _blfDonePaths;
    static std::vector<std::wstring> _blfDoneFaces;

    for( size_t i = 0; i < _blfDonePaths.size(); ++i ){
        if( _blfDonePaths[i] == path ) return _blfDoneFaces[i];
    }

    std::wstring face = _blfFaceNameFromFile( path );

    if( face.length() ){
        if( !AddFontResourceExW( path.c_str(), FR_PRIVATE, 0 ) ) face.clear();
    }

    _blfDonePaths.push_back( path );
    _blfDoneFaces.push_back( face );

    return face;
}


BBGetFont* _CreateFont(String fontname, int fontSize){


    BBGetFont* _newfont=new BBGetFont();

    
    HDC hdc = GetDC(GetActiveWindow());
    string fontName = convertBBString(fontname);
    
    
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    
    
    int scaledFontSize = fontSize;
    
    
    HFONT hFont = 0;

    /* se il nome e' un .ttf/.otf dell'applicazione lo registro e uso il nome
       di famiglia letto dentro al file */
    std::wstring faceFromFile = _blfRegisterFontFile( fontname );

    if( faceFromFile.length() ){
        hFont = CreateFontW(-scaledFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, faceFromFile.c_str());
    }

    /* comportamento di sempre: famiglia installata nel sistema */
    if( !hFont ){
        hFont = CreateFont(-scaledFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontName.c_str());
    }
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    int charHeight = tm.tmHeight + 6;
    
    //charHeight = tm.tmAscent + tm.tmDescent; //tm.tmInternalLeading + MulDiv(22,GetDeviceCaps(hdc,LOGPIXELSY),72);

    std::vector<int> charWidths(224);
    int totalWidth = 0;
    int maxRowWidth = 0;
    int currentRowWidth = 0;
    
    int validCharIndex = 0;
    for (int i = 0; i < 224; i++) {
        wchar_t ch = static_cast<wchar_t>(i + 32);
        
        
        if (ch >= 128 && ch <= 255) {
        
            switch (ch) {
                // Caratteri 128-159
                case 128: ch = 0x00C7; break;  // Ç - C con cediglia
                case 129: ch = 0x00FC; break;  // ü - u con dieresi
                case 130: ch = 0x00E9; break;  // é - e con accento acuto
                case 131: ch = 0x00E2; break;  // â - a con circonflesso
                case 132: ch = 0x00E4; break;  // ä - a con dieresi
                case 133: ch = 0x00E0; break;  // à - a con accento grave
                case 134: ch = 0x00E5; break;  // å - a con anello
                case 135: ch = 0x00E7; break;  // ç - c con cediglia
                case 136: ch = 0x00EA; break;  // ê - e con circonflesso
                case 137: ch = 0x00EB; break;  // ë - e con dieresi
                case 138: ch = 0x00E8; break;  // è - e con accento grave
                case 139: ch = 0x00EF; break;  // ï - i con dieresi
                case 140: ch = 0x00EE; break;  // î - i con circonflesso
                case 141: ch = 0x00EC; break;  // ì - i con accento grave
                case 142: ch = 0x00C4; break;  // Ä - A con dieresi
                case 143: ch = 0x00C5; break;  // Å - A con anello
                case 144: ch = 0x00C9; break;  // É - E con accento acuto
                case 145: ch = 0x00E6; break;  // æ - ae legatura
                case 146: ch = 0x00C6; break;  // Æ - AE legatura
                case 147: ch = 0x00F4; break;  // ô - o con circonflesso
                case 148: ch = 0x00F6; break;  // ö - o con dieresi
                case 149: ch = 0x00F2; break;  // ò - o con accento grave
                case 150: ch = 0x00FB; break;  // û - u con circonflesso
                case 151: ch = 0x00F9; break;  // ù - u con accento grave
                case 152: ch = 0x00FF; break;  // ÿ - y con dieresi
                case 153: ch = 0x00D6; break;  // Ö - O con dieresi
                case 154: ch = 0x00DC; break;  // Ü - U con dieresi
                case 155: ch = 0x00A2; break;  // ¢ - simbolo cent
                case 156: ch = 0x00A3; break;  // £ - simbolo sterlina
                case 157: ch = 0x00A5; break;  // ¥ - simbolo yen
                case 158: ch = 0x20A7; break;  // ₧ - simbolo peseta
                case 159: ch = 0x0192; break;  // ƒ - f con gancio
                // Caratteri 160-255 (mappatura esplicita CP1252)
                case 160: ch = 0x00A0; break;  //   - spazio non-breaking
                case 161: ch = 0x00A1; break;  // ¡ - punto esclamativo rovesciato
                case 162: ch = 0x00A2; break;  // ¢ - simbolo cent
                case 163: ch = 0x00A3; break;  // £ - simbolo sterlina
                case 164: ch = 0x00A4; break;  // ¤ - simbolo valuta generica
                case 165: ch = 0x00A5; break;  // ¥ - simbolo yen
                case 166: ch = 0x00A6; break;  // ¦ - barra verticale spezzata
                case 167: ch = 0x00A7; break;  // § - simbolo sezione
                case 168: ch = 0x00A8; break;  // ¨ - dieresi
                case 169: ch = 0x00A9; break;  // © - simbolo copyright
                case 170: ch = 0x00AA; break;  // ª - indicatore ordinale femminile
                case 171: ch = 0x00AB; break;  // « - virgolette angolari doppie sinistra
                case 172: ch = 0x00AC; break;  // ¬ - simbolo negazione
                case 173: ch = 0x00AD; break;  // ­ - trattino soft
                case 174: ch = 0x00AE; break;  // ® - simbolo marchio registrato
                case 175: ch = 0x00AF; break;  // ¯ - macron
                case 176: ch = 0x00B0; break;  // ° - simbolo grado
                case 177: ch = 0x00B1; break;  // ± - più o meno
                case 178: ch = 0x00B2; break;  // ² - due al quadrato
                case 179: ch = 0x00B3; break;  // ³ - tre al cubo
                case 180: ch = 0x00B4; break;  // ´ - accento acuto
                case 181: ch = 0x00B5; break;  // µ - simbolo micro
                case 182: ch = 0x00B6; break;  // ¶ - simbolo paragrafo
                case 183: ch = 0x00B7; break;  // · - punto medio
                case 184: ch = 0x00B8; break;  // ¸ - cediglia
                case 185: ch = 0x00B9; break;  // ¹ - uno al quadrato
                case 186: ch = 0x00BA; break;  // º - indicatore ordinale maschile
                case 187: ch = 0x00BB; break;  // » - virgolette angolari doppie destra
                case 188: ch = 0x00BC; break;  // ¼ - un quarto
                case 189: ch = 0x00BD; break;  // ½ - un mezzo
                case 190: ch = 0x00BE; break;  // ¾ - tre quarti
                case 191: ch = 0x00BF; break;  // ¿ - punto interrogativo rovesciato
                case 192: ch = 0x00C0; break;  // À - A con accento grave
                case 193: ch = 0x00C1; break;  // Á - A con accento acuto
                case 194: ch = 0x00C2; break;  // Â - A con circonflesso
                case 195: ch = 0x00C3; break;  // Ã - A con tilde
                case 196: ch = 0x00C4; break;  // Ä - A con dieresi
                case 197: ch = 0x00C5; break;  // Å - A con anello
                case 198: ch = 0x00C6; break;  // Æ - AE legatura
                case 199: ch = 0x00C7; break;  // Ç - C con cediglia
                case 200: ch = 0x00C8; break;  // È - E con accento grave
                case 201: ch = 0x00C9; break;  // É - E con accento acuto
                case 202: ch = 0x00CA; break;  // Ê - E con circonflesso
                case 203: ch = 0x00CB; break;  // Ë - E con dieresi
                case 204: ch = 0x00CC; break;  // Ì - I con accento grave
                case 205: ch = 0x00CD; break;  // Í - I con accento acuto
                case 206: ch = 0x00CE; break;  // Î - I con circonflesso
                case 207: ch = 0x00CF; break;  // Ï - I con dieresi
                case 208: ch = 0x00D0; break;  // Ð - Eth
                case 209: ch = 0x00D1; break;  // Ñ - N con tilde
                case 210: ch = 0x00D2; break;  // Ò - O con accento grave
                case 211: ch = 0x00D3; break;  // Ó - O con accento acuto
                case 212: ch = 0x00D4; break;  // Ô - O con circonflesso
                case 213: ch = 0x00D5; break;  // Õ - O con tilde
                case 214: ch = 0x00D6; break;  // Ö - O con dieresi
                case 215: ch = 0x00D7; break;  // × - simbolo moltiplicazione
                case 216: ch = 0x00D8; break;  // Ø - O barrata
                case 217: ch = 0x00D9; break;  // Ù - U con accento grave
                case 218: ch = 0x00DA; break;  // Ú - U con accento acuto
                case 219: ch = 0x00DB; break;  // Û - U con circonflesso
                case 220: ch = 0x00DC; break;  // Ü - U con dieresi
                case 221: ch = 0x00DD; break;  // Ý - Y con accento acuto
                case 222: ch = 0x00DE; break;  // Þ - Thorn
                case 223: ch = 0x00DF; break;  // ß - eszett (beta tedesca)
                case 224: ch = 0x00E0; break;  // à - a con accento grave
                case 225: ch = 0x00E1; break;  // á - a con accento acuto
                case 226: ch = 0x00E2; break;  // â - a con circonflesso
                case 227: ch = 0x00E3; break;  // ã - a con tilde
                case 228: ch = 0x00E4; break;  // ä - a con dieresi
                case 229: ch = 0x00E5; break;  // å - a con anello
                case 230: ch = 0x00E6; break;  // æ - ae legatura
                case 231: ch = 0x00E7; break;  // ç - c con cediglia
                case 232: ch = 0x00E8; break;  // è - e con accento grave
                case 233: ch = 0x00E9; break;  // é - e con accento acuto
                case 234: ch = 0x00EA; break;  // ê - e con circonflesso
                case 235: ch = 0x00EB; break;  // ë - e con dieresi
                case 236: ch = 0x00EC; break;  // ì - i con accento grave
                case 237: ch = 0x00ED; break;  // í - i con accento acuto
                case 238: ch = 0x00EE; break;  // î - i con circonflesso
                case 239: ch = 0x00EF; break;  // ï - i con dieresi
                case 240: ch = 0x00F0; break;  // ð - eth
                case 241: ch = 0x00F1; break;  // ñ - n con tilde
                case 242: ch = 0x00F2; break;  // ò - o con accento grave
                case 243: ch = 0x00F3; break;  // ó - o con accento acuto
                case 244: ch = 0x00F4; break;  // ô - o con circonflesso
                case 245: ch = 0x00F5; break;  // õ - o con tilde
                case 246: ch = 0x00F6; break;  // ö - o con dieresi
                case 247: ch = 0x00F7; break;  // ÷ - simbolo divisione
                case 248: ch = 0x00F8; break;  // ø - o barrata
                case 249: ch = 0x00F9; break;  // ù - u con accento grave
                case 250: ch = 0x00FA; break;  // ú - u con accento acuto
                case 251: ch = 0x00FB; break;  // û - u con circonflesso
                case 252: ch = 0x00FC; break;  // ü - u con dieresi
                case 253: ch = 0x00FD; break;  // ý - y con accento acuto
                case 254: ch = 0x00FE; break;  // þ - thorn
                case 255: ch = 0x00FF; break;  // ÿ - y con dieresi
                default: break;
            }
        }
        
        // Calcola la larghezza usando il carattere mappato
        SIZE charSize;
        GetTextExtentPoint32W(hdc, &ch, 1, &charSize);
        charWidths[i] = charSize.cx + 1; // +1 pixel di padding tra caratteri
        
        currentRowWidth += charWidths[i];
        if ((i + 1) % 32 == 0) { // Fine riga ogni 32 caratteri
            maxRowWidth = max(maxRowWidth, currentRowWidth);
            currentRowWidth = 0;
        }
    }
    if (currentRowWidth > 0) { // Ultima riga parziale
        maxRowWidth = max(maxRowWidth, currentRowWidth);
    }
    
    
    int charsPerRow = 32;
    int numRows = 7; // (256-32)/32 = 7 righe
    int atlasWidth = maxRowWidth + 64*(fontSize/10.0f);
    int atlasHeight = numRows * charHeight;
    
    // Crea bitmap per l'atlas con supporto per rendering di alta qualità
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, atlasWidth, atlasHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    // Riempie sfondo bianco
    RECT rect = {0, 0, atlasWidth, atlasHeight};
    FillRect(hdcMem, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    
    // Seleziona il font nel DC di memoria e configura per rendering di alta qualità
    SelectObject(hdcMem, hFont);
    SetTextColor(hdcMem, RGB(0, 0, 0));
    SetBkMode(hdcMem, TRANSPARENT);
    
    // Abilita antialiasing per rendering di alta qualità (Windows Vista+)
    SetTextAlign(hdcMem, TA_LEFT | TA_TOP);
    SetMapMode(hdcMem, MM_TEXT);
    
    // Inizializza gli array per le posizioni
    _newfont->xFont = Array<int>(224);
    _newfont->yFont = Array<int>(224);
    _newfont->wFont = Array<int>(224);
    
    // Disegna tutti i caratteri e calcola le posizioni
    int charIndex = 0;
    int currentX = 0;
    int currentY = 0;
    
    for (int i = 32; i < 256; i++) {
        wchar_t ch = static_cast<wchar_t>(i);
        
        // Per i caratteri 128-159, usa la codifica corretta (CP1252/Latin-1)
        if (ch >= 128 && ch <= 255) {
            // Mappa i caratteri usando la codifica CP1252 (Windows Latin-1)
            switch (ch) {
                case 128: ch = 0x00C7; break;  // Ç - C con cediglia
                case 129: ch = 0x00FC; break;  // ü - u con dieresi
                case 130: ch = 0x00E9; break;  // é - e con accento acuto
                case 131: ch = 0x00E2; break;  // â - a con circonflesso
                case 132: ch = 0x00E4; break;  // ä - a con dieresi
                case 133: ch = 0x00E0; break;  // à - a con accento grave
                case 134: ch = 0x00E5; break;  // å - a con anello
                case 135: ch = 0x00E7; break;  // ç - c con cediglia
                case 136: ch = 0x00EA; break;  // ê - e con circonflesso
                case 137: ch = 0x00EB; break;  // ë - e con dieresi
                case 138: ch = 0x00E8; break;  // è - e con accento grave
                case 139: ch = 0x00EF; break;  // ï - i con dieresi
                case 140: ch = 0x00EE; break;  // î - i con circonflesso
                case 141: ch = 0x00EC; break;  // ì - i con accento grave
                case 142: ch = 0x00C4; break;  // Ä - A con dieresi
                case 143: ch = 0x00C5; break;  // Å - A con anello
                case 144: ch = 0x00C9; break;  // É - E con accento acuto
                case 145: ch = 0x00E6; break;  // æ - ae legatura
                case 146: ch = 0x00C6; break;  // Æ - AE legatura
                case 147: ch = 0x00F4; break;  // ô - o con circonflesso
                case 148: ch = 0x00F6; break;  // ö - o con dieresi
                case 149: ch = 0x00F2; break;  // ò - o con accento grave
                case 150: ch = 0x00FB; break;  // û - u con circonflesso
                case 151: ch = 0x00F9; break;  // ù - u con accento grave
                case 152: ch = 0x00FF; break;  // ÿ - y con dieresi
                case 153: ch = 0x00D6; break;  // Ö - O con dieresi
                case 154: ch = 0x00DC; break;  // Ü - U con dieresi
                case 155: ch = 0x00A2; break;  // ¢ - simbolo cent
                case 156: ch = 0x00A3; break;  // £ - simbolo sterlina
                case 157: ch = 0x00A5; break;  // ¥ - simbolo yen
                case 158: ch = 0x20A7; break;  // ₧ - simbolo peseta
                case 159: ch = 0x0192; break;  // ƒ - f con gancio
                // Caratteri 160-255 (mappatura esplicita CP1252)
                case 160: ch = 0x00A0; break;  //   - spazio non-breaking
                case 161: ch = 0x00A1; break;  // ¡ - punto esclamativo rovesciato
                case 162: ch = 0x00A2; break;  // ¢ - simbolo cent
                case 163: ch = 0x00A3; break;  // £ - simbolo sterlina
                case 164: ch = 0x00A4; break;  // ¤ - simbolo valuta generica
                case 165: ch = 0x00A5; break;  // ¥ - simbolo yen
                case 166: ch = 0x00A6; break;  // ¦ - barra verticale spezzata
                case 167: ch = 0x00A7; break;  // § - simbolo sezione
                case 168: ch = 0x00A8; break;  // ¨ - dieresi
                case 169: ch = 0x00A9; break;  // © - simbolo copyright
                case 170: ch = 0x00AA; break;  // ª - indicatore ordinale femminile
                case 171: ch = 0x00AB; break;  // « - virgolette angolari doppie sinistra
                case 172: ch = 0x00AC; break;  // ¬ - simbolo negazione
                case 173: ch = 0x00AD; break;  // ­ - trattino soft
                case 174: ch = 0x00AE; break;  // ® - simbolo marchio registrato
                case 175: ch = 0x00AF; break;  // ¯ - macron
                case 176: ch = 0x00B0; break;  // ° - simbolo grado
                case 177: ch = 0x00B1; break;  // ± - più o meno
                case 178: ch = 0x00B2; break;  // ² - due al quadrato
                case 179: ch = 0x00B3; break;  // ³ - tre al cubo
                case 180: ch = 0x00B4; break;  // ´ - accento acuto
                case 181: ch = 0x00B5; break;  // µ - simbolo micro
                case 182: ch = 0x00B6; break;  // ¶ - simbolo paragrafo
                case 183: ch = 0x00B7; break;  // · - punto medio
                case 184: ch = 0x00B8; break;  // ¸ - cediglia
                case 185: ch = 0x00B9; break;  // ¹ - uno al quadrato
                case 186: ch = 0x00BA; break;  // º - indicatore ordinale maschile
                case 187: ch = 0x00BB; break;  // » - virgolette angolari doppie destra
                case 188: ch = 0x00BC; break;  // ¼ - un quarto
                case 189: ch = 0x00BD; break;  // ½ - un mezzo
                case 190: ch = 0x00BE; break;  // ¾ - tre quarti
                case 191: ch = 0x00BF; break;  // ¿ - punto interrogativo rovesciato
                case 192: ch = 0x00C0; break;  // À - A con accento grave
                case 193: ch = 0x00C1; break;  // Á - A con accento acuto
                case 194: ch = 0x00C2; break;  // Â - A con circonflesso
                case 195: ch = 0x00C3; break;  // Ã - A con tilde
                case 196: ch = 0x00C4; break;  // Ä - A con dieresi
                case 197: ch = 0x00C5; break;  // Å - A con anello
                case 198: ch = 0x00C6; break;  // Æ - AE legatura
                case 199: ch = 0x00C7; break;  // Ç - C con cediglia
                case 200: ch = 0x00C8; break;  // È - E con accento grave
                case 201: ch = 0x00C9; break;  // É - E con accento acuto
                case 202: ch = 0x00CA; break;  // Ê - E con circonflesso
                case 203: ch = 0x00CB; break;  // Ë - E con dieresi
                case 204: ch = 0x00CC; break;  // Ì - I con accento grave
                case 205: ch = 0x00CD; break;  // Í - I con accento acuto
                case 206: ch = 0x00CE; break;  // Î - I con circonflesso
                case 207: ch = 0x00CF; break;  // Ï - I con dieresi
                case 208: ch = 0x00D0; break;  // Ð - Eth
                case 209: ch = 0x00D1; break;  // Ñ - N con tilde
                case 210: ch = 0x00D2; break;  // Ò - O con accento grave
                case 211: ch = 0x00D3; break;  // Ó - O con accento acuto
                case 212: ch = 0x00D4; break;  // Ô - O con circonflesso
                case 213: ch = 0x00D5; break;  // Õ - O con tilde
                case 214: ch = 0x00D6; break;  // Ö - O con dieresi
                case 215: ch = 0x00D7; break;  // × - simbolo moltiplicazione
                case 216: ch = 0x00D8; break;  // Ø - O barrata
                case 217: ch = 0x00D9; break;  // Ù - U con accento grave
                case 218: ch = 0x00DA; break;  // Ú - U con accento acuto
                case 219: ch = 0x00DB; break;  // Û - U con circonflesso
                case 220: ch = 0x00DC; break;  // Ü - U con dieresi
                case 221: ch = 0x00DD; break;  // Ý - Y con accento acuto
                case 222: ch = 0x00DE; break;  // Þ - Thorn
                case 223: ch = 0x00DF; break;  // ß - eszett (beta tedesca)
                case 224: ch = 0x00E0; break;  // à - a con accento grave
                case 225: ch = 0x00E1; break;  // á - a con accento acuto
                case 226: ch = 0x00E2; break;  // â - a con circonflesso
                case 227: ch = 0x00E3; break;  // ã - a con tilde
                case 228: ch = 0x00E4; break;  // ä - a con dieresi
                case 229: ch = 0x00E5; break;  // å - a con anello
                case 230: ch = 0x00E6; break;  // æ - ae legatura
                case 231: ch = 0x00E7; break;  // ç - c con cediglia
                case 232: ch = 0x00E8; break;  // è - e con accento grave
                case 233: ch = 0x00E9; break;  // é - e con accento acuto
                case 234: ch = 0x00EA; break;  // ê - e con circonflesso
                case 235: ch = 0x00EB; break;  // ë - e con dieresi
                case 236: ch = 0x00EC; break;  // ì - i con accento grave
                case 237: ch = 0x00ED; break;  // í - i con accento acuto
                case 238: ch = 0x00EE; break;  // î - i con circonflesso
                case 239: ch = 0x00EF; break;  // ï - i con dieresi
                case 240: ch = 0x00F0; break;  // ð - eth
                case 241: ch = 0x00F1; break;  // ñ - n con tilde
                case 242: ch = 0x00F2; break;  // ò - o con accento grave
                case 243: ch = 0x00F3; break;  // ó - o con accento acuto
                case 244: ch = 0x00F4; break;  // ô - o con circonflesso
                case 245: ch = 0x00F5; break;  // õ - o con tilde
                case 246: ch = 0x00F6; break;  // ö - o con dieresi
                case 247: ch = 0x00F7; break;  // ÷ - simbolo divisione
                case 248: ch = 0x00F8; break;  // ø - o barrata
                case 249: ch = 0x00F9; break;  // ù - u con accento grave
                case 250: ch = 0x00FA; break;  // ú - u con accento acuto
                case 251: ch = 0x00FB; break;  // û - u con circonflesso
                case 252: ch = 0x00FC; break;  // ü - u con dieresi
                case 253: ch = 0x00FD; break;  // ý - y con accento acuto
                case 254: ch = 0x00FE; break;  // þ - thorn
                case 255: ch = 0x00FF; break;  // ÿ - y con dieresi
                
                default: break;
            }
        }
        
        // Calcola posizione nel grid
        int row = charIndex / charsPerRow;
        int col = charIndex % charsPerRow;
        
        // Se inizia una nuova riga, resetta X
        if (col == 0) {
            currentX = 0;
            currentY = row * charHeight;
        }
        
        // Salva posizioni nell'oggetto font
        _newfont->xFont[charIndex] = currentX;
        _newfont->yFont[charIndex] = currentY;
        _newfont->wFont[charIndex] = charWidths[charIndex];
        
        // Disegna il carattere mappato correttamente
        TextOutW(hdcMem, currentX, currentY, &ch, 1);
        
        // Avanza X per il prossimo carattere ########
        currentX += charWidths[charIndex]+2*(fontSize/10.0f);
        
        charIndex++;
    }
    
    // Ottieni dati bitmap
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = atlasWidth;
    bmi.bmiHeader.biHeight = -atlasHeight; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    int dataSize = atlasWidth * atlasHeight * 4;
    std::vector<uint8_t> pixelData(dataSize);
    
    GetDIBits(hdc, hBitmap, 0, atlasHeight, pixelData.data(), &bmi, DIB_RGB_COLORS);
    
    // Crea array per i pixel
    _newfont->dataPixel = Array<int>(dataSize + 2);
    _newfont->dataPixel[0] = atlasWidth;
    _newfont->dataPixel[1] = atlasHeight;
    
    // Copia i dati pixel
    for (int i = 0; i < dataSize; i++) {
        _newfont->dataPixel[i + 2] = pixelData[i];
    }
    
    // Salva altezza font
    _newfont->hFont = charHeight;
    
    // Cleanup
    SelectObject(hdcMem, hOldBitmap);
    SelectObject(hdc, hOldFont);
    DeleteObject(hBitmap);
    DeleteObject(hFont);
    DeleteDC(hdcMem);
    ReleaseDC(GetActiveWindow(), hdc);

    //END MAKE FONT
    
    return _newfont;

}




/* ------------------------------------------------------------------------
   _CreateTextImage

   Disegna UNA scritta e restituisce solo quella, invece dell'atlante con
   tutti i 224 caratteri. Per una lista di nomi di font e' la strada giusta:
   l'immagine e' grande quanto la scritta, non quanto l'alfabeto.

   dataPixel esce come [ larghezza, altezza, poi B,G,R,A per ogni pixel ],
   nero su bianco: e' il lato Cerberus che inverte e ne ricava il canale
   alfa, cosi' il colore lo sceglie chi disegna.
   ------------------------------------------------------------------------ */
BBGetFont* _CreateTextImage( String fontname, int fontSize, String text, int alignx ){

    BBGetFont *_newfont = new BBGetFont();

    HDC hdc = GetDC( GetActiveWindow() );

    std::wstring txt = convertBBStringToWide( text );

    if( txt.empty() ) txt = L" ";

    /* i ritorni a capo diventano tutti '\n' e le righe vuote doppie spariscono */
    for( size_t i=0;i<txt.size();++i ) if( txt[i]==L'\r' ) txt[i] = L'\n';

    std::vector<std::wstring> lines;
    std::wstring cur;

    for( size_t i=0;i<txt.size();++i ){
        if( txt[i]==L'\n' ){ lines.push_back( cur ); cur.clear(); }
        else cur += txt[i];
    }

    lines.push_back( cur );

    if( lines.empty() ) lines.push_back( L" " );

    /* stesso trattamento di _CreateFont: se il nome e' un file .ttf/.otf
       dell'applicazione lo registro e uso il nome di famiglia vero */
    HFONT hFont = 0;

    std::wstring faceFromFile = _blfRegisterFontFile( fontname );

    if( faceFromFile.length() ){
        hFont = CreateFontW( -fontSize,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                             DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,
                             faceFromFile.c_str() );
    }

    if( !hFont ){
        std::wstring fname = convertBBStringToWide( fontname );
        hFont = CreateFontW( -fontSize,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                             DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,
                             fname.c_str() );
    }

    HFONT hOldFont = (HFONT)SelectObject( hdc,hFont );

    TEXTMETRICW tm;
    GetTextMetricsW( hdc,&tm );

    int pad = fontSize/4;
    if( pad<4 ) pad = 4;

    /* L'immagine viene costruita SIMMETRICA rispetto alla linea di base: sopra
       e sotto c'e' lo stesso spazio. Cosi' chi disegna puo' semplicemente
       centrare l'immagine nella riga e tutti i font risultano allineati sulla
       stessa linea di base, anche quelli con metriche insolite (per esempio
       Sans Serif Collection, che ha un'ascesa molto piu' alta della media e
       centrando il riquadro finirebbe spostato in alto). Lo spazio in piu' e'
       trasparente, quindi non si vede e non disturba le righe vicine. */
    int mAsc = tm.tmAscent;
    int mDesc = tm.tmDescent;

    int half = mAsc>mDesc ? mAsc : mDesc;
    int side = half+pad;

    int maxWidth = 1;

    std::vector<int> lineWidths( lines.size() );

    for( size_t i=0;i<lines.size();++i ){

        SIZE ts = { 0,0 };

        std::wstring line = lines[i].empty() ? std::wstring( L" " ) : lines[i];

        GetTextExtentPoint32W( hdc,line.c_str(),(int)line.size(),&ts );

        lineWidths[i] = ts.cx;

        if( ts.cx>maxWidth ) maxWidth = ts.cx;
    }

    int atlasWidth = maxWidth+pad*2;
    int atlasHeight = side*2+( (int)lines.size()-1 )*tm.tmHeight;

    HDC hdcMem = CreateCompatibleDC( hdc );
    HBITMAP hBitmap = CreateCompatibleBitmap( hdc,atlasWidth,atlasHeight );
    HBITMAP hOldBitmap = (HBITMAP)SelectObject( hdcMem,hBitmap );

    RECT rect = { 0,0,atlasWidth,atlasHeight };
    FillRect( hdcMem,&rect,(HBRUSH)GetStockObject( WHITE_BRUSH ) );

    SelectObject( hdcMem,hFont );
    SetTextColor( hdcMem,RGB( 0,0,0 ) );
    SetBkMode( hdcMem,TRANSPARENT );
    SetTextAlign( hdcMem,TA_LEFT|TA_BASELINE );

    for( size_t i=0;i<lines.size();++i ){

        std::wstring line = lines[i].empty() ? std::wstring( L" " ) : lines[i];

        int x = pad;

        if( alignx==1 ) x = pad+( maxWidth-lineWidths[i] )/2;
        else if( alignx==2 ) x = pad+( maxWidth-lineWidths[i] );

        TextOutW( hdcMem,x,side+(int)i*tm.tmHeight,line.c_str(),(int)line.size() );
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    bmi.bmiHeader.biWidth = atlasWidth;
    bmi.bmiHeader.biHeight = -atlasHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    int dataSize = atlasWidth*atlasHeight*4;

    std::vector<unsigned char> pixelData( dataSize );

    GetDIBits( hdc,hBitmap,0,atlasHeight,pixelData.data(),&bmi,DIB_RGB_COLORS );

    _newfont->dataPixel = Array<int>( dataSize+2 );
    _newfont->dataPixel[0] = atlasWidth;
    _newfont->dataPixel[1] = atlasHeight;

    for( int i=0;i<dataSize;++i ) _newfont->dataPixel[i+2] = pixelData[i];

    _newfont->hFont = tm.tmHeight;

    SelectObject( hdcMem,hOldBitmap );
    SelectObject( hdc,hOldFont );
    DeleteObject( hBitmap );
    DeleteObject( hFont );
    DeleteDC( hdcMem );
    ReleaseDC( GetActiveWindow(),hdc );

    return _newfont;
}
