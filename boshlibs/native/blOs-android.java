import android.app.Activity;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/*	Parte nativa del modulo os per Android.

	brl.filesystem copre gia' FileType / FileSize / FileTime / RealPath /
	LoadDir / CreateDir / DeleteFile / CopyFile su Android, quindi qui restano
	solo i pezzi che mancano: lettura e scrittura di testo e il path
	dell'applicazione.

	La codifica e' UTF-8 come String::Load / String::Save del target desktop,
	cosi' un sorgente scritto sul PC e riletto sul telefono e' identico.		*/

class BBblOs{

	static Activity _Activity(){
		BBAndroidGame game = BBAndroidGame.AndroidGame();
		if( game==null ) return null;
		return game.GetActivity();
	}

	/*	Su Android non esiste il path dell'eseguibile. Restituisco un file
		fittizio dentro la cartella privata dell'app, cosi' os.ExtractDir(
		os.AppPath() ) - che e' l'uso che ne fa l'IDE - da' una directory
		scrivibile. */
	public static String _AppPath(){

		Activity activity = _Activity();
		if( activity==null ) return "";

		return activity.getFilesDir().getAbsolutePath()+"/app";
	}

	public static String _LoadString( String path ){

		if( path==null || path.length()==0 ) return "";

		InputStream in = null;
		try{
			in = new FileInputStream( path );

			ByteArrayOutputStream out = new ByteArrayOutputStream();
			byte[] buffer = new byte[8192];
			for( ;; ){
				int count = in.read( buffer );
				if( count<0 ) break;
				out.write( buffer,0,count );
			}

			return new String( out.toByteArray(),"UTF-8" );

		}catch( Throwable t ){
			return "";
		}finally{
			if( in!=null ){
				try{
					in.close();
				}catch( Throwable t ){
				}
			}
		}
	}

	/*	Stessi codici di ritorno del target desktop: 0 se e' andata bene. */
	public static int _SaveString( String text,String path ){

		if( path==null || path.length()==0 ) return -1;

		OutputStream out = null;
		try{
			File file = new File( path );

			File dir = file.getParentFile();
			if( dir!=null && !dir.exists() ) dir.mkdirs();

			out = new FileOutputStream( file );
			out.write( text.getBytes( "UTF-8" ) );
			out.flush();

			return 0;

		}catch( Throwable t ){
			return -1;
		}finally{
			if( out!=null ){
				try{
					out.close();
				}catch( Throwable t ){
				}
			}
		}
	}
}
