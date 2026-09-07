import android.app.Activity;
import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.OpenableColumns;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/*	Implementazione Android dei requester (equivalente di brl.requesters, che
	esiste solo sul target glfw).

	Differenza sostanziale rispetto al desktop: su Android un dialogo NON puo'
	bloccare. Vive sull'UI thread mentre il gioco gira sul thread di render GL,
	quindi se si aspettasse il risultato si bloccherebbe il thread che deve
	disegnare il dialogo stesso (ANR). Ogni richiesta e' percio' divisa in due:
	una funzione che la avvia e ritorna subito, e il polling di _IsReady() a cui
	blRequesters.cxs fa da wrapper.

	Il color picker NON e' qui: Android non ha un dialogo di sistema per i colori
	(e nemmeno macOS e Linux ce l'hanno in brl.requesters), quindi e' disegnato
	con mojo2 dentro blRequesters.cxs ed e' identico su tutti i target.			*/

class BBblRequesters extends ActivityDelegate{

	static final int MODE_NONE = 0;
	static final int MODE_OPEN = 1;
	static final int MODE_SAVE = 2;
	static final int MODE_DIR = 3;

	static final Object _mutex = new Object();

	static BBblRequesters _delegate;
	static int _reqCode = -1;

	static boolean _busy;
	static boolean _ready;
	static int _resultInt;
	static String _resultStr = "";
	static int _mode = MODE_NONE;

	//***** Infrastruttura *****

	static void _EnsureDelegate(){
		if( _delegate!=null ) return;
		_delegate = new BBblRequesters();
		BBAndroidGame.AndroidGame().AddActivityDelegate( _delegate );
		_reqCode = BBAndroidGame.AndroidGame().AllocateActivityResultRequestCode();
	}

	static Activity _Activity(){
		BBAndroidGame game = BBAndroidGame.AndroidGame();
		if( game==null ) return null;
		return game.GetActivity();
	}

	/*	Ritorna false se c'e' gia' una richiesta aperta: due dialoghi
		sovrapposti renderebbero ambiguo il risultato del polling. */
	static boolean _Begin( int mode ){
		synchronized( _mutex ){
			if( _busy ) return false;
			_busy = true;
			_ready = false;
			_resultInt = 0;
			_resultStr = "";
			_mode = mode;
			return true;
		}
	}

	/*	Idempotente: se il dialogo e' gia' stato chiuso il secondo callback
		(per esempio onCancel dopo un click) non deve sovrascrivere nulla. */
	static void _Finish( int resultInt,String resultStr ){
		synchronized( _mutex ){
			if( !_busy ) return;
			_resultInt = resultInt;
			_resultStr = resultStr!=null ? resultStr : "";
			_ready = true;
			_busy = false;
			_mode = MODE_NONE;
		}
	}

	public static boolean _IsBusy(){
		synchronized( _mutex ){
			return _busy;
		}
	}

	public static boolean _IsReady(){
		synchronized( _mutex ){
			return _ready;
		}
	}

	public static int _ResultInt(){
		synchronized( _mutex ){
			return _resultInt;
		}
	}

	public static String _ResultStr(){
		synchronized( _mutex ){
			return _resultStr;
		}
	}

	public static void _Consume(){
		synchronized( _mutex ){
			_ready = false;
		}
	}

	//***** Dialoghi *****

	/*	Notify e' fire-and-forget come sul desktop: non occupa lo slot della
		richiesta corrente, cosi' la firma resta identica e i call site non
		vanno toccati. */
	public static void _Notify( final String title,final String text,final boolean serious ){

		final Activity activity = _Activity();
		if( activity==null ) return;

		activity.runOnUiThread( new Runnable(){
			public void run(){
				try{
					AlertDialog.Builder b = new AlertDialog.Builder( activity );
					b.setTitle( title );
					b.setMessage( text );
					b.setCancelable( true );
					b.setPositiveButton( "OK",null );
					b.show();
				}catch( Throwable t ){
				}
			}
		} );
	}

	static boolean _ShowDialog( final String title,final String text,
			final String pos,final String neg,final String neu,
			final int vpos,final int vneg,final int vneu,final int vcancel ){

		final Activity activity = _Activity();
		if( activity==null ){
			_Finish( vcancel,"" );
			return true;
		}

		activity.runOnUiThread( new Runnable(){
			public void run(){
				try{
					AlertDialog.Builder b = new AlertDialog.Builder( activity );
					b.setTitle( title );
					b.setMessage( text );
					b.setCancelable( true );

					b.setPositiveButton( pos,new DialogInterface.OnClickListener(){
						public void onClick( DialogInterface d,int w ){
							_Finish( vpos,"" );
						}
					} );

					if( neg!=null ){
						b.setNegativeButton( neg,new DialogInterface.OnClickListener(){
							public void onClick( DialogInterface d,int w ){
								_Finish( vneg,"" );
							}
						} );
					}

					if( neu!=null ){
						b.setNeutralButton( neu,new DialogInterface.OnClickListener(){
							public void onClick( DialogInterface d,int w ){
								_Finish( vneu,"" );
							}
						} );
					}

					b.setOnCancelListener( new DialogInterface.OnCancelListener(){
						public void onCancel( DialogInterface d ){
							_Finish( vcancel,"" );
						}
					} );

					b.show();

				}catch( Throwable t ){
					_Finish( vcancel,"" );
				}
			}
		} );

		return true;
	}

	/*	Stessa semantica di bbConfirm su Windows/macOS: OK=1, tutto il resto=0. */
	public static boolean _Confirm( String title,String text,boolean serious ){
		if( !_Begin( MODE_NONE ) ) return false;
		return _ShowDialog( title,text,"OK","Cancel",null,1,0,0,0 );
	}

	/*	Stessa semantica di bbProceed su Windows/macOS: Yes=1, No=0, Cancel=-1. */
	public static boolean _Proceed( String title,String text,boolean serious ){
		if( !_Begin( MODE_NONE ) ) return false;
		return _ShowDialog( title,text,"Yes","No","Cancel",1,0,-1,-1 );
	}

	//***** Storage Access Framework *****

	static String _BaseName( String path ){
		if( path==null ) return "";
		String p = path.replace( '\\','/' );
		int i = p.lastIndexOf( '/' );
		if( i>=0 ) p = p.substring( i+1 );
		return p;
	}

	public static boolean _OpenFile( final String title,final String filter,final String path ){

		if( !_Begin( MODE_OPEN ) ) return false;

		final Activity activity = _Activity();
		if( activity==null ){
			_Finish( 0,"" );
			return true;
		}

		activity.runOnUiThread( new Runnable(){
			public void run(){
				try{
					_EnsureDelegate();

					Intent intent;
					if( Build.VERSION.SDK_INT>=Build.VERSION_CODES.KITKAT ){
						intent = new Intent( Intent.ACTION_OPEN_DOCUMENT );
						intent.addFlags( Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION );
					}else{
						intent = new Intent( Intent.ACTION_GET_CONTENT );
					}

					intent.addCategory( Intent.CATEGORY_OPENABLE );
					/*	Niente filtro MIME: le estensioni sorgente (.cxs, .asm...)
						non hanno un MIME registrato e un filtro nasconderebbe
						proprio i file che servono. */
					intent.setType( "*/*" );
					intent.addFlags( Intent.FLAG_GRANT_READ_URI_PERMISSION );

					activity.startActivityForResult( Intent.createChooser( intent,title ),_reqCode );

				}catch( Throwable t ){
					_Finish( 0,"" );
				}
			}
		} );

		return true;
	}

	public static boolean _SaveFile( final String title,final String filter,final String path ){

		if( !_Begin( MODE_SAVE ) ) return false;

		final Activity activity = _Activity();
		if( activity==null || Build.VERSION.SDK_INT<Build.VERSION_CODES.KITKAT ){
			_Finish( 0,"" );
			return true;
		}

		activity.runOnUiThread( new Runnable(){
			public void run(){
				try{
					_EnsureDelegate();

					Intent intent = new Intent( Intent.ACTION_CREATE_DOCUMENT );
					intent.addCategory( Intent.CATEGORY_OPENABLE );
					intent.setType( "*/*" );

					String name = _BaseName( path );
					if( name.length()>0 ) intent.putExtra( Intent.EXTRA_TITLE,name );

					intent.addFlags( Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION );

					activity.startActivityForResult( intent,_reqCode );

				}catch( Throwable t ){
					_Finish( 0,"" );
				}
			}
		} );

		return true;
	}

	public static boolean _RequestDir( final String title,final String path ){

		if( !_Begin( MODE_DIR ) ) return false;

		final Activity activity = _Activity();
		if( activity==null || Build.VERSION.SDK_INT<Build.VERSION_CODES.LOLLIPOP ){
			_Finish( 0,"" );
			return true;
		}

		activity.runOnUiThread( new Runnable(){
			public void run(){
				try{
					_EnsureDelegate();

					Intent intent = new Intent( Intent.ACTION_OPEN_DOCUMENT_TREE );
					intent.addFlags( Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION );

					activity.startActivityForResult( intent,_reqCode );

				}catch( Throwable t ){
					_Finish( 0,"" );
				}
			}
		} );

		return true;
	}

	public void onActivityResult( int requestCode,int resultCode,Intent data ){

		if( requestCode!=_reqCode ) return;

		String uriString = "";
		int ok = 0;

		if( resultCode==Activity.RESULT_OK && data!=null ){

			Uri uri = data.getData();
			if( uri!=null ){

				uriString = uri.toString();
				ok = 1;

				/*	Senza il permesso persistente l'URI vale solo finche' il
					processo resta vivo: al riavvio dell'app il progetto
					aperto non sarebbe piu' leggibile. */
				if( Build.VERSION.SDK_INT>=Build.VERSION_CODES.KITKAT ){
					try{
						int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
						if( flags==0 ) flags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
						_Activity().getContentResolver().takePersistableUriPermission( uri,flags );
					}catch( Throwable t ){
					}
				}
			}
		}

		_Finish( ok,uriString );
	}

	//***** Lettura/scrittura degli URI restituiti dal SAF *****

	/*	Il SAF restituisce content:// e non un path: os.LoadString e os.SaveString
		non ci arrivano. Queste funzioni sono il minimo indispensabile perche'
		l'editor possa aprire, salvare e sorvegliare un documento scelto
		dall'utente. */

	static Uri _ParseUri( String uriString ){
		if( uriString==null || uriString.length()==0 ) return null;
		try{
			return Uri.parse( uriString );
		}catch( Throwable t ){
			return null;
		}
	}

	public static String _UriDisplayName( String uriString ){

		Activity activity = _Activity();
		Uri uri = _ParseUri( uriString );
		if( activity==null || uri==null ) return "";

		Cursor cursor = null;
		try{
			cursor = activity.getContentResolver().query( uri,null,null,null,null );
			if( cursor!=null && cursor.moveToFirst() ){
				int column = cursor.getColumnIndex( OpenableColumns.DISPLAY_NAME );
				if( column>=0 ){
					String name = cursor.getString( column );
					if( name!=null && name.length()>0 ) return name;
				}
			}
		}catch( Throwable t ){
		}finally{
			if( cursor!=null ){
				try{
					cursor.close();
				}catch( Throwable t ){
				}
			}
		}

		String last = uri.getLastPathSegment();
		return last!=null ? last : "";
	}

	public static boolean _UriExists( String uriString ){

		Activity activity = _Activity();
		Uri uri = _ParseUri( uriString );
		if( activity==null || uri==null ) return false;

		InputStream in = null;
		try{
			in = activity.getContentResolver().openInputStream( uri );
			return in!=null;
		}catch( Throwable t ){
			return false;
		}finally{
			if( in!=null ){
				try{
					in.close();
				}catch( Throwable t ){
				}
			}
		}
	}

	/*	Timbro per il file watcher: dimensione + data di modifica. "" se l'URI
		non e' piu' raggiungibile. */
	public static String _UriStamp( String uriString ){

		Activity activity = _Activity();
		Uri uri = _ParseUri( uriString );
		if( activity==null || uri==null ) return "";

		Cursor cursor = null;
		try{
			cursor = activity.getContentResolver().query( uri,null,null,null,null );
			if( cursor!=null && cursor.moveToFirst() ){

				long size = -1;
				long modified = -1;

				int cs = cursor.getColumnIndex( OpenableColumns.SIZE );
				if( cs>=0 && !cursor.isNull( cs ) ) size = cursor.getLong( cs );

				int cm = cursor.getColumnIndex( "last_modified" );
				if( cm>=0 && !cursor.isNull( cm ) ) modified = cursor.getLong( cm );

				return size+"-"+modified;
			}
		}catch( Throwable t ){
		}finally{
			if( cursor!=null ){
				try{
					cursor.close();
				}catch( Throwable t ){
				}
			}
		}

		return "";
	}

	/*	Dimensione in byte, -1 se ignota. */
	public static int _UriSize( String uriString ){

		Activity activity = _Activity();
		Uri uri = _ParseUri( uriString );
		if( activity==null || uri==null ) return -1;

		Cursor cursor = null;
		try{
			cursor = activity.getContentResolver().query( uri,null,null,null,null );
			if( cursor!=null && cursor.moveToFirst() ){
				int cs = cursor.getColumnIndex( OpenableColumns.SIZE );
				if( cs>=0 && !cursor.isNull( cs ) ) return (int)cursor.getLong( cs );
			}
		}catch( Throwable t ){
		}finally{
			if( cursor!=null ){
				try{
					cursor.close();
				}catch( Throwable t ){
				}
			}
		}

		return -1;
	}

	/*	Data di modifica in SECONDI, come lo stat() usato sul desktop: la
		colonna del provider e' in millisecondi e non entrerebbe in un Int. */
	public static int _UriTime( String uriString ){

		Activity activity = _Activity();
		Uri uri = _ParseUri( uriString );
		if( activity==null || uri==null ) return -1;

		Cursor cursor = null;
		try{
			cursor = activity.getContentResolver().query( uri,null,null,null,null );
			if( cursor!=null && cursor.moveToFirst() ){
				int cm = cursor.getColumnIndex( "last_modified" );
				if( cm>=0 && !cursor.isNull( cm ) ) return (int)( cursor.getLong( cm )/1000L );
			}
		}catch( Throwable t ){
		}finally{
			if( cursor!=null ){
				try{
					cursor.close();
				}catch( Throwable t ){
				}
			}
		}

		return -1;
	}

	public static String _UriReadText( String uriString ){

		Activity activity = _Activity();
		Uri uri = _ParseUri( uriString );
		if( activity==null || uri==null ) return "";

		InputStream in = null;
		try{
			in = activity.getContentResolver().openInputStream( uri );
			if( in==null ) return "";

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

	public static boolean _UriWriteText( String uriString,String text ){

		Activity activity = _Activity();
		Uri uri = _ParseUri( uriString );
		if( activity==null || uri==null ) return false;

		OutputStream out = null;
		try{
			ContentResolver resolver = activity.getContentResolver();

			/*	"wt" tronca: con "w" un salvataggio piu' corto del file
				precedente lascerebbe in coda i byte vecchi. */
			out = resolver.openOutputStream( uri,"wt" );
			if( out==null ) return false;

			out.write( text.getBytes( "UTF-8" ) );
			out.flush();
			return true;

		}catch( Throwable t ){
			return false;
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
