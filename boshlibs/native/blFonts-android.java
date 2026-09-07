import java.io.File;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.os.Build;

/*	Implementazione Android dei font nativi (equivalente di
	blFonts-win.cpp / blFonts-mac.cpp / blFonts-linux.cpp).

	Su Android non esiste una API pubblica per enumerare i font di sistema per
	nome di famiglia, percio' le famiglie vengono ricavate leggendo la tabella
	'name' (nameID 1 = family, nameID 2 = subfamily) dei file .ttf/.otf presenti
	nelle directory di sistema. Il nome cosi' ottenuto viene mappato sul file, in
	modo che Typeface.createFromFile() carichi esattamente il font scelto.

	Il layout dell'atlante e il formato di dataPixel sono identici a quelli degli
	altri target: 224 caratteri (32..255) disposti su 7 righe da 32, dataPixel[0]
	e dataPixel[1] contengono larghezza e altezza, poi 4 byte per pixel in ordine
	B,G,R,A con glifo nero su fondo bianco (blFonts.cxs ricava l'alpha da
	255-componente, e AdjustFONT legge il byte di offset 2 della cella).		*/

class BBGetFont{

	int[] dataPixel = new int[2];
	int hFont;
	int[] xFont = new int[224];
	int[] yFont = new int[224];
	int[] wFont = new int[224];

	void _New(){
	}
}

class BBblFont{

	String _fontname = "";
	int _dimensione;
	int _peso;
	boolean _ital;

	boolean _New( String fontname, int dimensione, int peso, boolean ital ){
		_fontname = fontname;
		_dimensione = dimensione;
		_peso = peso;
		_ital = ital;
		return true;
	}

	String _GetFontName(){
		return _fontname;
	}

	int _GetFontSize(){
		return _dimensione;
	}

	/*	Android non offre un font chooser di sistema: la selezione avviene tramite
		la finestra interna (MYSELECTFONT). Qui il font corrente resta invariato. */
	int _SelectFont(){
		return 0;
	}

	//***** Enumerazione dei font di sistema *****

	static final String[] _FONT_DIRS = {
		"/system/fonts",
		"/system/font",
		"/data/fonts",
		"/product/fonts",
		"/system/product/fonts",
		"/vendor/fonts"
	};

	/*	Famiglie "logiche" sempre disponibili tramite Typeface.create(): fanno da
		rete di sicurezza se la scansione delle directory non produce nulla. */
	static final String[] _LOGICAL_FAMILIES = {
		"sans-serif",
		"serif",
		"monospace"
	};

	static LinkedHashMap<String,String> _fontFiles;
	static HashMap<String,Typeface> _typefaces = new HashMap<String,Typeface>();

	static void _BuildFontMap(){

		if( _fontFiles!=null ) return;

		LinkedHashMap<String,String> map = new LinkedHashMap<String,String>();
		HashMap<String,Boolean> isRegular = new HashMap<String,Boolean>();

		for( int d=0;d<_FONT_DIRS.length;++d ){

			File dir = new File( _FONT_DIRS[d] );
			File[] files = null;
			try{
				files = dir.listFiles();
			}catch( Throwable t ){
			}
			if( files==null ) continue;

			for( int i=0;i<files.length;++i ){

				File f = files[i];
				if( !f.isFile() ) continue;

				String lower = f.getName().toLowerCase();
				if( !lower.endsWith( ".ttf" ) && !lower.endsWith( ".otf" ) ) continue;

				String[] names = _ReadFontNames( f );
				if( names==null ) continue;

				String family = names[0];
				if( family==null ) continue;
				family = family.trim();
				if( family.length()==0 ) continue;

				String sub = names[1]==null ? "" : names[1].trim();
				boolean regular = sub.length()==0 || sub.equalsIgnoreCase( "Regular" ) || sub.equalsIgnoreCase( "Book" );

				/*	A parita' di famiglia tengo il primo file trovato, ma lo
					sostituisco se quello nuovo e' lo stile "Regular" e quello
					gia' memorizzato non lo era. */
				if( map.containsKey( family ) ){
					Boolean cur = isRegular.get( family );
					if( !regular || ( cur!=null && cur.booleanValue() ) ) continue;
				}

				map.put( family,f.getAbsolutePath() );
				isRegular.put( family,Boolean.valueOf( regular ) );
			}
		}

		for( int i=0;i<_LOGICAL_FAMILIES.length;++i ){
			if( !map.containsKey( _LOGICAL_FAMILIES[i] ) ) map.put( _LOGICAL_FAMILIES[i],"" );
		}

		_fontFiles = map;
	}

	static String _GetFontListString(){

		_BuildFontMap();

		ArrayList<String> names = new ArrayList<String>( _fontFiles.keySet() );
		Collections.sort( names );

		StringBuilder out = new StringBuilder();
		for( int i=0;i<names.size();++i ){
			if( i>0 ) out.append( ";" );
			out.append( names.get( i ) );
		}

		if( out.length()==0 ) return "sans-serif";
		return out.toString();
	}

	static String _FindFontFile( String family ){

		_BuildFontMap();

		String path = _fontFiles.get( family );
		if( path!=null ) return path;

		for( Map.Entry<String,String> e : _fontFiles.entrySet() ){
			if( e.getKey().equalsIgnoreCase( family ) ) return e.getValue();
		}

		return null;
	}

	/*	Font dell'APPLICAZIONE invece che di sistema. Due forme accettate:

		  "cerberus://data/xxx.ttf"   file dentro la cartella dei dati, che sul
		                              target Android diventa un asset dell'apk
		  "/percorso/xxx.ttf"         file sul filesystem del dispositivo

		Attenzione: perche' il .ttf finisca davvero negli assets serve
		#BINARY_FILES+="*.ttf" nel sorgente principale, altrimenti il builder
		lo scarta senza dire niente.								*/
	static Typeface _TypefaceFromPath( String name ){

		if( name==null || name.length()==0 ) return null;

		if( name.startsWith( "cerberus://data/" ) ){

			BBAndroidGame game = BBAndroidGame.AndroidGame();
			if( game==null ) return null;

			android.app.Activity activity = game.GetActivity();
			if( activity==null ) return null;

			/* stessa regola di BBAndroidGame.PathToAssetPath */
			String asset = "cerberus/"+name.substring( 16 );

			try{
				Typeface tf = Typeface.createFromAsset( activity.getAssets(),asset );
				if( tf!=null ) System.out.println( "[blFonts] font caricato dagli asset: "+asset );
				return tf;
			}catch( Throwable t ){
				System.out.println( "[blFonts] ASSET NON TROVATO: "+asset+"  ("+t+")" );
				return null;
			}
		}

		String lower = name.toLowerCase();

		if( lower.endsWith( ".ttf" ) || lower.endsWith( ".otf" ) || lower.endsWith( ".ttc" ) ){
			try{
				File f = new File( name );
				if( f.isFile() ){
					System.out.println( "[blFonts] font caricato da file: "+name );
					return Typeface.createFromFile( f );
				}
				System.out.println( "[blFonts] FILE NON TROVATO: "+name );
			}catch( Throwable t ){
			}
		}

		return null;
	}

	static Typeface _FindTypeface( String family ){

		if( family==null ) family = "";

		Typeface cached = _typefaces.get( family );
		if( cached!=null ) return cached;

		/*	Prima si prova come font dell'applicazione: se il nome e' un path
			non ha senso cercarlo fra le famiglie di sistema. */
		Typeface tf = _TypefaceFromPath( family );

		if( tf==null ){

			String path = _FindFontFile( family );
			if( path!=null && path.length()>0 ){
				try{
					tf = Typeface.createFromFile( path );
				}catch( Throwable t ){
					tf = null;
				}
			}
		}

		if( tf==null && family.length()>0 ){
			try{
				tf = Typeface.create( family,Typeface.NORMAL );
			}catch( Throwable t ){
				tf = null;
			}
		}

		if( tf==null ) tf = Typeface.DEFAULT;

		_typefaces.put( family,tf );
		return tf;
	}

	//***** Costruzione dell'atlante *****

	/*	Stessa mappatura usata dagli altri target: i codici 128..159 seguono la
		codepage del progetto, da 160 in poi vale Latin-1. */
	static int _CP1252ToUnicode( int ch ){

		switch( ch ){
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
		}
		return ch;
	}

	static boolean _HasGlyph( Paint paint,String s ){
		if( Build.VERSION.SDK_INT>=23 ){
			try{
				return paint.hasGlyph( s );
			}catch( Throwable t ){
			}
		}
		return true;
	}

	static BBGetFont _CreateFont( String fontname,int fontSize ){

		BBGetFont _newfont = new BBGetFont();

		if( fontSize<1 ) fontSize = 1;

		Paint paint = new Paint( Paint.ANTI_ALIAS_FLAG );
		paint.setSubpixelText( true );
		paint.setTypeface( _FindTypeface( fontname ) );
		paint.setTextSize( fontSize );
		paint.setColor( Color.BLACK );
		paint.setStyle( Paint.Style.FILL );
		paint.setTextAlign( Paint.Align.LEFT );

		Paint.FontMetricsInt fm = paint.getFontMetricsInt();
		int ascent = -fm.ascent;
		int descent = fm.descent;
		int charHeight = fm.descent-fm.ascent;

		String[] glyphs = new String[224];
		boolean[] hasGlyph = new boolean[224];
		int[] charWidths = new int[224];

		int maxRowWidth = 0;
		int currentRowWidth = 0;

		/*	Quanto salgono e scendono DAVVERO i glifi rispetto alla linea di base.
			Dimensionando la riga su questi valori (e non sull'interlinea del font)
			nessun glifo esce dalla propria cella: le maiuscole accentate sono piu'
			alte dell'ascent e finirebbero nel fondo della cella della riga
			precedente, dove AdjustFONT le scambierebbe per parte del carattere. */
		int maxTop = 0;
		int maxBelow = 0;

		Rect bounds = new Rect();

		for( int i=0;i<224;++i ){

			int code = _CP1252ToUnicode( i+32 );
			String s = new String( Character.toChars( code ) );
			glyphs[i] = s;

			boolean ok = _HasGlyph( paint,s );
			hasGlyph[i] = ok;

			int wi;
			if( !ok ){
				wi = Math.max( 1,fontSize/2 );
			}else{
				wi = (int)Math.floor( paint.measureText( s ) )+1;
				if( wi<1 ) wi = 1;

				paint.getTextBounds( s,0,s.length(),bounds );
				if( !bounds.isEmpty() ){
					int gTop = -bounds.top;
					int gBelow = bounds.bottom;
					if( gTop>maxTop ) maxTop = gTop;
					if( gBelow>maxBelow ) maxBelow = gBelow;
				}
			}

			charWidths[i] = wi;
			currentRowWidth += wi;

			if( ((i+1)%32)==0 ){
				if( currentRowWidth>maxRowWidth ) maxRowWidth = currentRowWidth;
				currentRowWidth = 0;
			}
		}
		if( currentRowWidth>maxRowWidth ) maxRowWidth = currentRowWidth;

		int charsPerRow = 32;
		int numRows = 7;

		if( maxTop<ascent ) maxTop = ascent;
		if( maxBelow<descent ) maxBelow = descent;
		int rowPitch = maxTop+maxBelow;
		if( rowPitch<charHeight ) rowPitch = charHeight;
		if( rowPitch<1 ) rowPitch = 1;
		int baseline = maxTop;

		int gap = (int)( 2*(fontSize/10.0f) );
		int atlasWidth = maxRowWidth+(int)( 64*(fontSize/10.0f) );
		int atlasHeight = numRows*rowPitch;
		if( atlasWidth<1 ) atlasWidth = 1;

		Bitmap bmp;
		try{
			bmp = Bitmap.createBitmap( atlasWidth,atlasHeight,Bitmap.Config.ARGB_8888 );
		}catch( Throwable t ){
			return _newfont;
		}

		Canvas cv = new Canvas( bmp );
		cv.drawColor( Color.WHITE );

		_newfont.xFont = new int[224];
		_newfont.yFont = new int[224];
		_newfont.wFont = new int[224];

		int charIndex = 0;
		int currentX = 0;
		int currentY = 0;

		for( int i=32;i<256;++i ){

			int row = charIndex/charsPerRow;
			int col = charIndex%charsPerRow;
			if( col==0 ){
				currentX = 0;
				currentY = row*rowPitch;
			}

			_newfont.xFont[charIndex] = currentX;
			_newfont.yFont[charIndex] = currentY;
			_newfont.wFont[charIndex] = charWidths[charIndex];

			if( hasGlyph[charIndex] ){
				/*	Il clip tiene il glifo dentro la propria riga: senza, i pixel
					in eccesso sporcherebbero le celle sopra e sotto. */
				int save = cv.save();
				cv.clipRect( 0,currentY,atlasWidth,currentY+rowPitch );
				cv.drawText( glyphs[charIndex],currentX,currentY+baseline,paint );
				cv.restoreToCount( save );
			}

			currentX += _newfont.wFont[charIndex]+gap;
			charIndex++;
		}

		int numPixels = atlasWidth*atlasHeight;
		int[] px = new int[numPixels];
		bmp.getPixels( px,0,atlasWidth,0,0,atlasWidth,atlasHeight );
		bmp.recycle();

		int[] data = new int[numPixels*4+2];
		data[0] = atlasWidth;
		data[1] = atlasHeight;

		int j = 2;
		for( int i=0;i<numPixels;++i ){
			/*	Disegnando nero opaco su bianco i tre canali sono uguali: basta
				leggerne uno e replicarlo in B,G,R come fanno i target nativi. */
			int g = (px[i]>>16)&0xFF;
			data[j] = g;
			data[j+1] = g;
			data[j+2] = g;
			data[j+3] = 255;
			j += 4;
		}

		_newfont.dataPixel = data;
		_newfont.hFont = rowPitch;

		return _newfont;
	}

	//***** Lettura della tabella 'name' di un file sfnt *****

	/*	Ritorna [family(nameID 1), subfamily(nameID 2)] oppure null. */
	static String[] _ReadFontNames( File file ){

		RandomAccessFile raf = null;
		try{

			raf = new RandomAccessFile( file,"r" );

			long base = 0;
			int tag = raf.readInt();
			if( tag==0x74746366 ){		// 'ttcf': collezione, uso il primo font
				raf.skipBytes( 4 );		// version
				int numFonts = raf.readInt();
				if( numFonts<1 ) return null;
				base = ((long)raf.readInt())&0xFFFFFFFFL;
				raf.seek( base );
				tag = raf.readInt();
			}
			if( tag!=0x00010000 && tag!=0x4F54544F ) return null;	// sfnt / 'OTTO'

			int numTables = raf.readUnsignedShort();
			raf.skipBytes( 6 );		// searchRange, entrySelector, rangeShift

			long nameOffset = -1;
			for( int i=0;i<numTables;++i ){
				int t = raf.readInt();
				raf.skipBytes( 4 );		// checksum
				long off = ((long)raf.readInt())&0xFFFFFFFFL;
				raf.skipBytes( 4 );		// length
				if( t==0x6E616D65 ){	// 'name'
					nameOffset = off;
					break;
				}
			}
			if( nameOffset<0 ) return null;

			raf.seek( nameOffset );
			raf.skipBytes( 2 );		// format
			int count = raf.readUnsignedShort();
			int stringOffset = raf.readUnsignedShort();

			String family = null, subfamily = null;
			int familyScore = -1, subfamilyScore = -1;

			int[] platform = new int[count];
			int[] encoding = new int[count];
			int[] language = new int[count];
			int[] nameId = new int[count];
			int[] length = new int[count];
			int[] offset = new int[count];

			for( int i=0;i<count;++i ){
				platform[i] = raf.readUnsignedShort();
				encoding[i] = raf.readUnsignedShort();
				language[i] = raf.readUnsignedShort();
				nameId[i] = raf.readUnsignedShort();
				length[i] = raf.readUnsignedShort();
				offset[i] = raf.readUnsignedShort();
			}

			for( int i=0;i<count;++i ){

				if( nameId[i]!=1 && nameId[i]!=2 ) continue;
				if( length[i]<=0 ) continue;

				int score = _NameScore( platform[i],encoding[i],language[i] );
				if( score<0 ) continue;
				if( nameId[i]==1 && score<=familyScore ) continue;
				if( nameId[i]==2 && score<=subfamilyScore ) continue;

				byte[] buf = new byte[length[i]];
				raf.seek( nameOffset+stringOffset+offset[i] );
				raf.readFully( buf );

				String s = new String( buf,platform[i]==1 ? "ISO-8859-1" : "UTF-16BE" );
				s = s.replace( String.valueOf( (char)0 ),"" ).replace( ";","" ).trim();
				if( s.length()==0 ) continue;

				if( nameId[i]==1 ){
					family = s;
					familyScore = score;
				}else{
					subfamily = s;
					subfamilyScore = score;
				}
			}

			if( family==null ) return null;
			return new String[]{ family,subfamily };

		}catch( Throwable t ){
			return null;
		}finally{
			if( raf!=null ){
				try{
					raf.close();
				}catch( Throwable t ){
				}
			}
		}
	}

	/*	Preferisco le stringhe Windows in inglese, poi le Unicode, poi le Mac. */
	static int _NameScore( int platform,int encoding,int language ){
		if( platform==3 && language==0x409 ) return 3;
		if( platform==3 ) return 2;
		if( platform==0 ) return 1;
		if( platform==1 && encoding==0 ) return 0;
		return -1;
	}

	/*	_CreateTextImage

		Disegna UNA scritta e restituisce solo quella, invece dell'atlante con
		tutti i 224 caratteri: l'immagine e' grande quanto la scritta.

		dataPixel esce come [ larghezza, altezza, poi B,G,R,A per ogni pixel ],
		nero su bianco, esattamente come la versione Windows, cosi' il lato
		Cerberus e' uno solo per tutti i target. */
	static BBGetFont _CreateTextImage( String fontname,int fontSize,String text,int alignx ){

		BBGetFont _newfont = new BBGetFont();

		if( fontSize<1 ) fontSize = 1;
		if( text==null || text.length()==0 ) text = " ";

		text = text.replace( "\r","\n" );

		String[] lines = text.split( "\n",-1 );

		if( lines.length==0 ) lines = new String[]{ " " };

		Paint paint = new Paint( Paint.ANTI_ALIAS_FLAG );
		paint.setSubpixelText( true );
		paint.setTypeface( _FindTypeface( fontname ) );
		paint.setTextSize( fontSize );
		paint.setColor( Color.BLACK );
		paint.setStyle( Paint.Style.FILL );
		paint.setTextAlign( Paint.Align.LEFT );

		Paint.FontMetricsInt fm = paint.getFontMetricsInt();

		int lineH = fm.descent-fm.ascent;
		if( lineH<1 ) lineH = fontSize;

		int pad = fontSize/4;
		if( pad<4 ) pad = 4;

		/*	L'immagine viene costruita SIMMETRICA rispetto alla linea di base:
			sopra e sotto c'e' lo stesso spazio. Cosi' chi disegna centra
			l'immagine nella riga e tutti i font risultano allineati sulla
			stessa linea di base, anche quelli con metriche insolite. Lo
			spazio in piu' e' trasparente, quindi non si vede. */
		int mAsc = -fm.ascent;
		int mDesc = fm.descent;

		int half = mAsc>mDesc ? mAsc : mDesc;
		int side = half+pad;

		int maxWidth = 1;
		int[] lw = new int[ lines.length ];

		for( int i=0;i<lines.length;++i ){
			String s = lines[i].length()==0 ? " " : lines[i];
			int w = (int)Math.ceil( paint.measureText( s ) );
			if( w<1 ) w = 1;
			lw[i] = w;
			if( w>maxWidth ) maxWidth = w;
		}

		int aw = maxWidth+pad*2;
		int ah = side*2+( lines.length-1 )*lineH;

		Bitmap bmp = Bitmap.createBitmap( aw,ah,Bitmap.Config.ARGB_8888 );
		Canvas cv = new Canvas( bmp );

		cv.drawColor( Color.WHITE );

		for( int i=0;i<lines.length;++i ){

			String s = lines[i].length()==0 ? " " : lines[i];

			int x = pad;

			if( alignx==1 ) x = pad+( maxWidth-lw[i] )/2;
			else if( alignx==2 ) x = pad+( maxWidth-lw[i] );

			cv.drawText( s,x,side+i*lineH,paint );
		}

		int n = aw*ah;

		int[] px = new int[ n ];
		bmp.getPixels( px,0,aw,0,0,aw,ah );
		bmp.recycle();

		_newfont.dataPixel = new int[ n*4+2 ];
		_newfont.dataPixel[0] = aw;
		_newfont.dataPixel[1] = ah;

		int j = 2;

		for( int i=0;i<n;++i ){
			int c = px[i];
			_newfont.dataPixel[j  ] =   c      & 0xFF;
			_newfont.dataPixel[j+1] = ( c>>8  ) & 0xFF;
			_newfont.dataPixel[j+2] = ( c>>16 ) & 0xFF;
			_newfont.dataPixel[j+3] = 255;
			j += 4;
		}

		_newfont.hFont = lineH;

		return _newfont;
	}

}
