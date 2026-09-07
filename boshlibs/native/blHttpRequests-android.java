import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/*	blHttpRequests - nativo Android (HttpURLConnection).

	Gira su BBThread: Start() lancia Run__UNSAFE__ sul thread di sfondo,
	IsRunning() dice quando ha finito.

	Differenze rispetto al codice di partenza, tutte volute:

	- una richiesta SENZA corpo (la GET) non esplode piu'. Prima Send()
	  azzerava il testo ma lasciava sendbt a null, e il ramo else faceva
	  sendbt.length: NullPointerException dentro il thread, e la richiesta
	  restava li' senza dire niente.

	- il codice di risposta si legge SEMPRE, anche su 404 o 500. Prima
	  getInputStream lanciava e _status restava -1: un errore del server era
	  indistinguibile da un cavo staccato.

	- su risposta di errore si legge getErrorStream, che e' dove il server
	  mette il messaggio.

	- timeout di connessione e di lettura, che prima non c'erano: senza, una
	  rete che non risponde tiene il thread appeso per sempre.

	- _con puo' essere null se Open e' fallita, e disconnect() non ci va
	  sopra a occhi chiusi.															*/

class BBblHttpRequest extends BBThread{

	HttpURLConnection _con;

	String _response = "";
	int[] _responsebytes;

	int _status = -1;

	volatile int _recv;
	volatile int _sent;

	String _sendText;
	String _encoding;

	byte[] _sendBytes;

	/*	Immagine gia' decodificata, se e' stata chiesta. */
	boolean _wantimage;

	byte[] _rgba;

	volatile int _iw;
	volatile int _ih;

	/*	A blocchi di un kilobyte, cosi' BytesSending() si muove davvero
		mandando roba grossa invece di saltare da 0 al totale.				*/
	static final int CHUNK = 1024;


	void Open( String verb,String url,int timeout,boolean verifycert,boolean verifyhost ){

		_response = "";
		_responsebytes = null;
		_status = -1;
		_recv = 0;
		_sent = 0;
		_sendText = null;
		_sendBytes = null;
		_rgba = null;
		_iw = 0;
		_ih = 0;

		try{

			URL turl = new URL( url );

			_con = (HttpURLConnection)turl.openConnection();
			_con.setRequestMethod( verb );

			if( timeout>0 ){
				_con.setConnectTimeout( timeout*1000 );
				_con.setReadTimeout( timeout*1000 );
			}

			/*	verifycert e verifyhost non si toccano: su Android il
				controllo del certificato lo fa il sistema e va bene cosi'.
				I due parametri ci sono per avere la stessa firma ovunque. */

		}catch( Exception ex ){

			System.out.println( "[blHttp] apertura "+url+": "+ex );
			_con = null;
		}
	}


	void SetHeader( String name,String value ){

		if( _con!=null ) _con.setRequestProperty( name,value );
	}


	void Send(){

		_sendText = null;
		_sendBytes = null;

		Start();
	}


	void SendText( String text,String encoding ){

		_sendText = text;
		_encoding = encoding;
		_sendBytes = null;

		Start();
	}


	void SendBytes( int[] data,int ln ){

		_sendText = null;

		if( data==null || ln<0 ) ln = 0;
		if( data!=null && ln>data.length ) ln = data.length;

		_sendBytes = new byte[ln];

		for( int i=0;i<ln;++i ) _sendBytes[i] = (byte)(data[i]&0xFF);

		Start();
	}


	String ResponseText(){
		return _response;
	}


	int[] ResponseBytes(){

		if( _responsebytes==null ) return new int[0];

		return _responsebytes;
	}


	void WantImage( boolean on ){
		_wantimage = on;
	}


	int ImageWidth(){ return _iw; }

	int ImageHeight(){ return _ih; }


	int ImagePixels( BBDataBuffer db ){

		if( db==null || _rgba==null ) return 0;

		java.nio.ByteBuffer dst = db.GetByteBuffer();

		if( dst==null || dst.capacity()<_rgba.length ) return 0;

		dst.position( 0 );
		dst.put( _rgba,0,_rgba.length );
		dst.position( 0 );

		return 1;
	}


	int Status(){ return _status; }

	int BytesReceived(){ return _recv; }

	int BytesSending(){ return _sent; }


	void Run__UNSAFE__(){

		if( _con==null ) return;

		try{

			byte[] body = null;

			if( _sendText!=null ){

				String enc = (_encoding!=null && _encoding.length()>0) ? _encoding : "UTF-8";

				/*	Cerberus dice "utf8", Java vuole "UTF-8". */
				if( enc.equalsIgnoreCase( "utf8" ) ) enc = "UTF-8";

				body = _sendText.getBytes( enc );

			}else if( _sendBytes!=null ){

				body = _sendBytes;
			}

			/*	Niente corpo = GET: non si apre nemmeno il flusso in uscita,
				se no la richiesta diventerebbe una POST vuota.				*/
			if( body!=null ){

				_con.setDoOutput( true );
				_con.setFixedLengthStreamingMode( body.length );

				OutputStream out = _con.getOutputStream();

				for( int i=0;i<body.length;i+=CHUNK ){

					int len = Math.min( CHUNK,body.length-i );

					out.write( body,i,len );

					_sent += len;
				}

				out.flush();
				out.close();
			}

			/*	Il codice PRIMA di leggere: e' l'unico modo di distinguere un
				404 da una rete caduta.										*/
			_status = _con.getResponseCode();

			InputStream in = null;

			try{
				in = _con.getInputStream();
			}catch( Exception ex ){
				/*	Su 4xx e 5xx getInputStream lancia: il corpo con il
					messaggio del server sta nell'altro flusso.				*/
				in = _con.getErrorStream();
			}

			if( in!=null ){

				ByteArrayOutputStream out = new ByteArrayOutputStream( 4096 );

				byte[] buf = new byte[4096];

				for(;;){

					int n = in.read( buf );
					if( n<0 ) break;

					out.write( buf,0,n );

					_recv += n;
				}

				in.close();

				byte[] raw = out.toByteArray();

				if( _wantimage ){

					/*	Chiedendo un'immagine NON si costruiscono la stringa
						e l'array di interi: per un tassello sarebbero un
						quarto di mega di caratteri piu' un mega di int, tutti
						buttati via subito dopo.							*/
					_Decode( raw );

				}else{

					_response = new String( raw,"UTF-8" );

					_responsebytes = new int[raw.length];

					for( int i=0;i<raw.length;++i ) _responsebytes[i] = raw[i]&0xFF;
				}
			}

		}catch( Throwable t ){

			System.out.println( "[blHttp] "+t );

		}finally{

			try{ _con.disconnect(); }catch( Throwable t ){}
		}
	}


	/*	La decodifica si fa QUI, sul thread del lavoro: cosi' il gioco non
		perde un fotogramma quando arriva un tassello di mappa. Sul thread del
		disegno resta solo la copia dei pixel nella texture.					*/
	void _Decode( byte[] raw ){

		Bitmap bm = BitmapFactory.decodeByteArray( raw,0,raw.length );

		if( bm==null ) return;

		int w = bm.getWidth();
		int h = bm.getHeight();

		if( w<1 || h<1 ){
			bm.recycle();
			return;
		}

		int[] argb = new int[w*h];
		bm.getPixels( argb,0,w,0,0,w,h );
		bm.recycle();

		byte[] rgba = new byte[w*h*4];

		for( int i=0;i<w*h;++i ){

			int c = argb[i];

			int a = (c>>>24)&255;
			int r = (c>>16)&255;
			int g = (c>>8)&255;
			int b = c&255;

			/*	mojo2 vuole l'alfa PREMOLTIPLICATO: un pixel bianco con alfa 0
				resterebbe bianco pieno. Sui tasselli, che sono opachi, non
				cambia niente; su un png con trasparenza si', eccome.		*/
			if( a<255 ){
				r = r*a/255;
				g = g*a/255;
				b = b*a/255;
			}

			int o = i*4;

			rgba[o  ] = (byte)r;
			rgba[o+1] = (byte)g;
			rgba[o+2] = (byte)b;
			rgba[o+3] = (byte)a;
		}

		_rgba = rgba;
		_iw = w;
		_ih = h;
	}
}
