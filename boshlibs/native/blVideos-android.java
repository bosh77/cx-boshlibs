import android.content.res.AssetFileDescriptor;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Build;
import java.nio.ByteBuffer;

/*	Riproduzione di un .mp4 dentro una Image di mojo2.

	L'idea: un thread di sfondo decodifica con MediaCodec e tiene pronto UN
	fotogramma convertito in RGBA. Il gioco, sul suo thread, chiama _CopyFrame
	che lo travasa nel DataBuffer, e da li' blVideo.cxs fa Image.WritePixels.
	Cosi' il video diventa una normale Image: si disegna con Canvas.DrawRect,
	si scala, si ruota, ci si disegna sopra.

	Perche' non una VideoView sovrapposta: starebbe SOPRA la superficie GL e
	non sarebbe componibile con il resto del disegno.

	Perche' non SurfaceTexture: darebbe una texture OES, che mojo2 non sa
	campionare (servirebbe uno shader samplerExternalOES).

	Il prezzo di questa strada e' la conversione YUV->RGBA in Java e il
	trasferimento di w*h*4 byte a fotogramma: va bene per risoluzioni
	contenute, non per un 1080p.

	AUDIO: un secondo MediaExtractor + MediaCodec indipendente, sulla sola
	traccia audio, su un secondo thread; il PCM che ne esce va in un AudioTrack
	in MODE_STREAM. E' lo stesso schema della versione Windows (dove i buffer
	sono quelli di OpenAL): l'audio si scandisce DA SOLO: quando il buffer
	dell'AudioTrack e' pieno la write non accetta altro, e il thread aspetta -
	non serve nessun orologio manuale come per il video.

	Le write sono NON bloccanti (WRITE_NON_BLOCKING, API 21 come il resto del
	file): con quelle bloccanti, mettere in pausa avrebbe potuto lasciare il
	thread appeso dentro una write, e _Close sarebbe rimasta ad aspettarlo.

	Non e' sincronizzazione fine (lip-sync da regia): video e audio partono
	insieme da _Play() e ripartono insieme in loop, ma ognuno scorre col
	proprio passo. Per un filmato dentro una Image questo basta.				*/

class BBblVideo{

	/*	getOutputImage e COLOR_FormatYUV420Flexible sono API 21. Sotto, il
		video non parte e _Open ritorna false. */
	static final int MIN_SDK = 21;

	final Object _lock = new Object();

	Thread _thread;

	/*	L'audio ha thread, estrattore e decoder tutti suoi: il video ha la sua
		scansione a tempo sui presentation time, l'audio si fa dettare il ritmo
		dall'AudioTrack. Tenerli separati evita che uno rallenti l'altro. */
	Thread _audioThread;
	volatile boolean _audioRunning;

	volatile boolean _running;
	volatile boolean _playing;
	volatile boolean _loop;
	volatile boolean _finished;
	volatile boolean _failed;

	volatile int _w;
	volatile int _h;

	String _path = "";

	/* fotogramma pronto, gia' in RGBA */
	byte[] _rgba;
	boolean _hasNew;

	//***** API vista da Cerberus *****

	public boolean _Open( String path ){

		_Close();

		if( Build.VERSION.SDK_INT<MIN_SDK ){
			System.out.println( "[blVideo] serve Android 5.0 (API 21), qui e' API "+Build.VERSION.SDK_INT );
			_failed = true;
			return false;
		}

		_path = path!=null ? path : "";
		_failed = false;
		_finished = false;
		_w = 0;
		_h = 0;

		/*	La dimensione serve subito al chiamante per creare la Image, ma la
			sa solo l'estrattore: la leggo qui, in modo sincrono, prima di
			avviare il thread. */
		if( !_ReadSize() ){
			_failed = true;
			return false;
		}

		_running = true;

		_thread = new Thread( new Runnable(){
			public void run(){
				_Decode();
			}
		} );

		_thread.setName( "blVideo" );
		_thread.start();

		/*	L'audio e' un extra: se il file non ha traccia audio, o il decoder
			non parte, _DecodeAudio esce subito e il video prosegue muto. Non e'
			_failed. */
		_audioRunning = true;

		_audioThread = new Thread( new Runnable(){
			public void run(){
				_DecodeAudio();
			}
		} );

		_audioThread.setName( "blVideoAudio" );
		_audioThread.start();

		return true;
	}

	public void _Play(){
		_playing = true;
	}

	public void _Pause(){
		_playing = false;
	}

	public void _SetLoop( boolean on ){
		_loop = on;
	}

	public boolean _IsPlaying(){
		return _playing;
	}

	public boolean _IsFinished(){
		return _finished;
	}

	public boolean _HasFailed(){
		return _failed;
	}

	public int _Width(){
		return _w;
	}

	public int _Height(){
		return _h;
	}

	public void _Close(){

		_running = false;
		_audioRunning = false;
		_playing = false;

		Thread t = _thread;
		_thread = null;

		if( t!=null ){
			try{
				t.join( 500 );
			}catch( Throwable e ){
			}
		}

		/*	L'AudioTrack lo rilascia il thread audio nel suo finally: e' lui il
			padrone, e rilasciarlo da qui mentre quello ci sta ancora scrivendo
			dentro sarebbe un crash. Qui si aspetta e basta. */
		Thread at = _audioThread;
		_audioThread = null;

		if( at!=null ){
			try{
				at.join( 500 );
			}catch( Throwable e ){
			}
		}

		synchronized( _lock ){
			_rgba = null;
			_hasNew = false;
		}
	}

	/*	Travasa l'ultimo fotogramma nel DataBuffer del chiamante.
		false se non ce n'e' uno nuovo: in quel caso non si tocca la texture. */
	public boolean _CopyFrame( BBDataBuffer db ){

		synchronized( _lock ){

			if( !_hasNew || _rgba==null || db==null ) return false;

			ByteBuffer dst = db.GetByteBuffer();
			if( dst==null || dst.capacity()<_rgba.length ) return false;

			dst.position( 0 );
			dst.put( _rgba,0,_rgba.length );
			dst.position( 0 );

			_hasNew = false;
			return true;
		}
	}

	//***** Interno *****

	/*	"cerberus://data/x.mp4" -> asset "cerberus/x.mp4", come fa
		BBAndroidGame.PathToAssetPath. Altrimenti percorso sul filesystem. */
	boolean _SetSource( MediaExtractor ex ) throws Exception{

		if( _path.startsWith( "cerberus://data/" ) ){

			BBAndroidGame game = BBAndroidGame.AndroidGame();
			if( game==null ) return false;

			android.app.Activity activity = game.GetActivity();
			if( activity==null ) return false;

			String asset = "cerberus/"+_path.substring( 16 );

			AssetFileDescriptor afd = activity.getAssets().openFd( asset );
			ex.setDataSource( afd.getFileDescriptor(),afd.getStartOffset(),afd.getLength() );
			afd.close();

			return true;
		}

		ex.setDataSource( _path );
		return true;
	}

	static int _VideoTrack( MediaExtractor ex ){

		for( int i=0;i<ex.getTrackCount();++i ){
			MediaFormat f = ex.getTrackFormat( i );
			String mime = f.getString( MediaFormat.KEY_MIME );
			if( mime!=null && mime.startsWith( "video/" ) ) return i;
		}
		return -1;
	}

	static int _AudioTrack( MediaExtractor ex ){

		for( int i=0;i<ex.getTrackCount();++i ){
			MediaFormat f = ex.getTrackFormat( i );
			String mime = f.getString( MediaFormat.KEY_MIME );
			if( mime!=null && mime.startsWith( "audio/" ) ) return i;
		}
		return -1;
	}

	boolean _ReadSize(){

		MediaExtractor ex = null;

		try{
			ex = new MediaExtractor();
			if( !_SetSource( ex ) ) return false;

			int track = _VideoTrack( ex );
			if( track<0 ){
				System.out.println( "[blVideo] nessuna traccia video in "+_path );
				return false;
			}

			MediaFormat f = ex.getTrackFormat( track );
			_w = f.getInteger( MediaFormat.KEY_WIDTH );
			_h = f.getInteger( MediaFormat.KEY_HEIGHT );

			System.out.println( "[blVideo] "+_path+"  "+_w+"x"+_h+"  "+f.getString( MediaFormat.KEY_MIME ) );

			return _w>0 && _h>0;

		}catch( Throwable t ){
			System.out.println( "[blVideo] apertura fallita: "+t );
			return false;
		}finally{
			if( ex!=null ){
				try{
					ex.release();
				}catch( Throwable t ){
				}
			}
		}
	}

	void _Decode(){

		MediaExtractor ex = null;
		MediaCodec dec = null;

		try{

			ex = new MediaExtractor();
			if( !_SetSource( ex ) ) return;

			int track = _VideoTrack( ex );
			if( track<0 ) return;

			ex.selectTrack( track );

			MediaFormat fmt = ex.getTrackFormat( track );
			String mime = fmt.getString( MediaFormat.KEY_MIME );

			fmt.setInteger( MediaFormat.KEY_COLOR_FORMAT,MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible );

			dec = MediaCodec.createDecoderByType( mime );
			dec.configure( fmt,null,null,0 );
			dec.start();

			MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

			boolean inputDone = false;
			long startNanos = -1;

			while( _running ){

				if( !_playing ){
					Thread.sleep( 15 );
					continue;
				}

				//--- alimenta il decoder

				if( !inputDone ){

					int inIdx = dec.dequeueInputBuffer( 5000 );

					if( inIdx>=0 ){

						ByteBuffer ib = dec.getInputBuffer( inIdx );
						int n = ib!=null ? ex.readSampleData( ib,0 ) : -1;

						if( n<0 ){
							dec.queueInputBuffer( inIdx,0,0,0,MediaCodec.BUFFER_FLAG_END_OF_STREAM );
							inputDone = true;
						}else{
							dec.queueInputBuffer( inIdx,0,n,ex.getSampleTime(),0 );
							ex.advance();
						}
					}
				}

				//--- raccogli i fotogrammi

				int outIdx = dec.dequeueOutputBuffer( info,5000 );

				if( outIdx<0 ) continue;

				if( (info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0 ){

					dec.releaseOutputBuffer( outIdx,false );

					if( _loop ){
						ex.seekTo( 0,MediaExtractor.SEEK_TO_CLOSEST_SYNC );
						dec.flush();
						inputDone = false;
						startNanos = -1;
						continue;
					}

					_finished = true;
					_playing = false;
					continue;
				}

				if( info.size>0 ){

					/*	Ritmo: i fotogrammi si consegnano secondo il loro
						presentation time, altrimenti il filmato scorrerebbe
						alla velocita' del decoder. */
					long ptNanos = info.presentationTimeUs*1000L;

					if( startNanos<0 ) startNanos = System.nanoTime()-ptNanos;

					long wait = ( startNanos+ptNanos )-System.nanoTime();

					if( wait>0 ) Thread.sleep( wait/1000000L,(int)( wait%1000000L ) );

					android.media.Image img = dec.getOutputImage( outIdx );

					if( img!=null ){
						_StoreFrame( img );
						img.close();
					}
				}

				dec.releaseOutputBuffer( outIdx,false );
			}

		}catch( Throwable t ){
			System.out.println( "[blVideo] decodifica interrotta: "+t );
			_failed = true;
		}finally{

			if( dec!=null ){
				try{
					dec.stop();
				}catch( Throwable t ){
				}
				try{
					dec.release();
				}catch( Throwable t ){
				}
			}

			if( ex!=null ){
				try{
					ex.release();
				}catch( Throwable t ){
				}
			}
		}
	}

	/*	YUV_420_888 -> RGBA. I piani hanno passi (stride) propri che vanno
		rispettati: su molti dispositivi U e V sono interlacciati con
		pixelStride 2, e la riga e' piu' lunga della larghezza utile.

		E soprattutto: si copia SOLO il rettangolo di ritaglio (getCropRect),
		non tutta l'immagine del decoder. Un decoder H.264 lavora a blocchi di
		16 pixel e restituisce un'immagine arrotondata a quella griglia, con
		accanto il ritaglio che dice qual e' la parte buona: un video 1468x818
		esce come 1472x832 con ritaglio (0,0,1468,818). Prendendo getWidth()/
		getHeight() ci si porta dietro le bande di riempimento E si dichiara al
		chiamante una dimensione che non e' quella del filmato - blVideos.cxs
		crea la Image su _Width()/_Height(), quindi DrawFit calcolava le
		proporzioni sui numeri sbagliati e DrawPart ritagliava nel punto
		sbagliato. E' lo stesso inganno del passo di riga sulla versione
		Windows, in un altro vestito. */
	void _StoreFrame( android.media.Image img ){

		int cx = 0;
		int cy = 0;

		int w = img.getWidth();
		int h = img.getHeight();

		android.graphics.Rect crop = img.getCropRect();

		if( crop!=null && crop.width()>0 && crop.height()>0 ){
			cx = crop.left;
			cy = crop.top;
			w = crop.width();
			h = crop.height();
		}

		if( w<=0 || h<=0 ) return;

		_w = w;
		_h = h;

		android.media.Image.Plane[] p = img.getPlanes();
		if( p==null || p.length<3 ) return;

		ByteBuffer yb = p[0].getBuffer();
		ByteBuffer ub = p[1].getBuffer();
		ByteBuffer vb = p[2].getBuffer();

		int yrs = p[0].getRowStride();
		int urs = p[1].getRowStride();
		int vrs = p[2].getRowStride();
		int ups = p[1].getPixelStride();
		int vps = p[2].getPixelStride();

		byte[] out = new byte[ w*h*4 ];

		byte[] yrow = new byte[ yrs ];
		byte[] urow = new byte[ urs ];
		byte[] vrow = new byte[ vrs ];

		int lastUvRow = -1;

		for( int y=0;y<h;++y ){

			/* riga nell'immagine del decoder, non nel ritaglio */
			int srcY = cy+y;

			int yoff = srcY*yrs;
			if( yoff+yrs>yb.capacity() ) break;
			yb.position( yoff );
			yb.get( yrow,0,yrs );

			int uvRow = srcY>>1;

			if( uvRow!=lastUvRow ){

				int uoff = uvRow*urs;
				int voff = uvRow*vrs;

				if( uoff+urs<=ub.capacity() ){
					ub.position( uoff );
					ub.get( urow,0,urs );
				}
				if( voff+vrs<=vb.capacity() ){
					vb.position( voff );
					vb.get( vrow,0,vrs );
				}

				lastUvRow = uvRow;
			}

			int o = y*w*4;

			for( int x=0;x<w;++x ){

				int srcX = cx+x;

				int Y = ( srcX<yrs ? yrow[srcX] : 0 ) & 0xFF;

				int uvCol = ( srcX>>1 )*ups;
				int vvCol = ( srcX>>1 )*vps;

				int U = ( uvCol<urs ? urow[uvCol] : 0 ) & 0xFF;
				int V = ( vvCol<vrs ? vrow[vvCol] : 0 ) & 0xFF;

				int yv = Y-16;
				if( yv<0 ) yv = 0;

				int uu = U-128;
				int vv = V-128;

				int r = ( 1192*yv+1634*vv ) >> 10;
				int g = ( 1192*yv-833*vv-400*uu ) >> 10;
				int b = ( 1192*yv+2066*uu ) >> 10;

				if( r<0 ) r = 0; else if( r>255 ) r = 255;
				if( g<0 ) g = 0; else if( g>255 ) g = 255;
				if( b<0 ) b = 0; else if( b>255 ) b = 255;

				out[o]   = (byte)r;
				out[o+1] = (byte)g;
				out[o+2] = (byte)b;
				out[o+3] = (byte)255;

				o += 4;
			}
		}

		synchronized( _lock ){
			_rgba = out;
			_hasNew = true;
		}
	}

	//***** Audio *****

	/*	Crea l'AudioTrack sul formato che il decoder dichiara DAVVERO in uscita
		(non su quello della traccia): il decoder puo' cambiarlo, ed e' per
		questo che si aspetta INFO_OUTPUT_FORMAT_CHANGED o il primo buffer.
		Torna null se il dispositivo non lo concede: in quel caso niente audio,
		ma il video va avanti lo stesso. */
	AudioTrack _MakeAudioTrack( MediaFormat f ){

		try{

			if( f==null ) return null;

			int rate = f.getInteger( MediaFormat.KEY_SAMPLE_RATE );
			int ch = f.getInteger( MediaFormat.KEY_CHANNEL_COUNT );

			if( rate<=0 || ch<=0 ) return null;

			int cfg = ch>=2 ? AudioFormat.CHANNEL_OUT_STEREO : AudioFormat.CHANNEL_OUT_MONO;

			int min = AudioTrack.getMinBufferSize( rate,cfg,AudioFormat.ENCODING_PCM_16BIT );

			/*	getMinBufferSize torna ERROR (-1) o ERROR_BAD_VALUE (-2) se la
				combinazione non e' supportata. */
			if( min<=0 ) return null;

			/*	Quattro volte il minimo: con il minimo secco basta un intoppo
				del sistema per svuotare il buffer e sentire un buco. */
			AudioTrack track = new AudioTrack(
				AudioManager.STREAM_MUSIC,
				rate,
				cfg,
				AudioFormat.ENCODING_PCM_16BIT,
				min*4,
				AudioTrack.MODE_STREAM );

			if( track.getState()!=AudioTrack.STATE_INITIALIZED ){
				track.release();
				return null;
			}

			System.out.println( "[blVideo] audio: "+ch+"ch "+rate+"Hz" );

			return track;

		}catch( Throwable t ){
			System.out.println( "[blVideo] audio non disponibile: "+t );
			return null;
		}
	}

	/*	Scrive tutto il blocco PCM, un pezzo per volta. Quando l'AudioTrack e'
		pieno la write torna 0: si aspetta un attimo e si riprova, ed e' proprio
		quell'attesa a dare il ritmo al thread audio.

		Si passa direttamente il ByteBuffer del decoder invece di copiarlo in un
		byte[]: una copia in meno per blocco, e soprattutto questa write e' API
		21 come il resto del file, mentre quella su byte[] con writeMode e' API
		23 e su un 5.0/5.1 darebbe NoSuchMethodError. */
	void _WritePcm( AudioTrack track,ByteBuffer pcm,int size ) throws Exception{

		int left = size;

		while( left>0 && _audioRunning && _playing ){

			int n = track.write( pcm,left,AudioTrack.WRITE_NON_BLOCKING );

			if( n<0 ) return;              /* errore: si molla questo blocco */

			if( n==0 ){
				Thread.sleep( 5 );
				continue;
			}

			left -= n;
		}
	}

	void _DecodeAudio(){

		MediaExtractor ex = null;
		MediaCodec dec = null;
		AudioTrack track = null;

		try{

			ex = new MediaExtractor();
			if( !_SetSource( ex ) ) return;

			int idx = _AudioTrack( ex );

			/* nessuna traccia audio: non e' un errore, il filmato e' muto */
			if( idx<0 ) return;

			ex.selectTrack( idx );

			MediaFormat fmt = ex.getTrackFormat( idx );
			String mime = fmt.getString( MediaFormat.KEY_MIME );

			dec = MediaCodec.createDecoderByType( mime );
			dec.configure( fmt,null,null,0 );
			dec.start();

			MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

			boolean inputDone = false;

			while( _audioRunning ){

				if( !_playing ){

					if( track!=null && track.getPlayState()==AudioTrack.PLAYSTATE_PLAYING ) track.pause();

					Thread.sleep( 15 );
					continue;
				}

				if( track!=null && track.getPlayState()!=AudioTrack.PLAYSTATE_PLAYING ) track.play();

				//--- alimenta il decoder

				if( !inputDone ){

					int inIdx = dec.dequeueInputBuffer( 5000 );

					if( inIdx>=0 ){

						ByteBuffer ib = dec.getInputBuffer( inIdx );
						int n = ib!=null ? ex.readSampleData( ib,0 ) : -1;

						if( n<0 ){
							dec.queueInputBuffer( inIdx,0,0,0,MediaCodec.BUFFER_FLAG_END_OF_STREAM );
							inputDone = true;
						}else{
							dec.queueInputBuffer( inIdx,0,n,ex.getSampleTime(),0 );
							ex.advance();
						}
					}
				}

				//--- raccogli il PCM

				int outIdx = dec.dequeueOutputBuffer( info,5000 );

				if( outIdx==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED ){

					if( track==null ){
						track = _MakeAudioTrack( dec.getOutputFormat() );
						if( track!=null ) track.play();
					}
					continue;
				}

				if( outIdx<0 ) continue;

				if( ( info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM )!=0 ){

					dec.releaseOutputBuffer( outIdx,false );

					if( _loop ){

						/*	Niente track.flush() qui: mentre la traccia suona e'
							documentato come no-op, e comunque quello che c'e'
							ancora in coda deve finire di suonare - e' proprio
							cio' che fa ripartire il giro senza un buco. */
						ex.seekTo( 0,MediaExtractor.SEEK_TO_CLOSEST_SYNC );
						dec.flush();
						inputDone = false;
						continue;
					}

					/*	Fine dell'audio: chi decide _finished e' il video, non
						questo thread - le due tracce possono avere durate un po'
						diverse. */
					break;
				}

				if( info.size>0 ){

					if( track==null ){
						track = _MakeAudioTrack( dec.getOutputFormat() );
						if( track!=null ) track.play();
					}

					ByteBuffer ob = dec.getOutputBuffer( outIdx );

					if( ob!=null && track!=null ){

						ob.position( info.offset );
						ob.limit( info.offset+info.size );

						_WritePcm( track,ob,info.size );
					}
				}

				dec.releaseOutputBuffer( outIdx,false );
			}

		}catch( Throwable t ){
			System.out.println( "[blVideo] audio interrotto: "+t );
		}finally{

			if( track!=null ){
				try{
					track.pause();
					track.flush();
					track.stop();
				}catch( Throwable t ){
				}
				try{
					track.release();
				}catch( Throwable t ){
				}
			}

			if( dec!=null ){
				try{
					dec.stop();
				}catch( Throwable t ){
				}
				try{
					dec.release();
				}catch( Throwable t ){
				}
			}

			if( ex!=null ){
				try{
					ex.release();
				}catch( Throwable t ){
				}
			}
		}
	}
}
