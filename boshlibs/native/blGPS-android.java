import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Build;
import android.os.Bundle;

/*	La posizione, presa dal LocationManager di Android.

	Si chiedono DUE fornitori insieme, GPS e NETWORK, e si tiene la posizione
	migliore fra le due: il GPS e' preciso ma al chiuso non aggancia e puo'
	metterci un minuto; la rete (celle e wifi) risponde quasi subito con
	qualche centinaio di metri di errore. Cosi' la mappa si posiziona subito e
	poi si stringe da sola quando arriva il satellite.

	Il permesso ACCESS_FINE_LOCATION e' "pericoloso": dall'API 23 in poi non
	basta dichiararlo nel manifest, va CHIESTO all'utente mentre l'app gira.
	Qui lo si chiede una volta sola e poi si ricontrolla a ogni _State: non
	serve intercettare onRequestPermissionsResult (che l'Activity di Cerberus
	non gira a nessuno), perche' appena l'utente risponde la verifica del
	permesso cambia da sola.

	requestLocationUpdates vuole un thread con un Looper: il gioco gira sul
	thread di render GL, che non ce l'ha, quindi ogni chiamata che tocca il
	LocationManager passa da runOnUiThread. La lettura invece e' solo un paio
	di campi volatile, e si puo' fare da qualsiasi thread.					*/

class BBblGPS{

	static final int OFF      = 0;
	static final int WAITING  = 1;
	static final int FIX      = 2;
	static final int DENIED   = 3;
	static final int DISABLED = 4;

	/*	Ogni secondo e ogni 2 metri: e' una mappa, non un navigatore. Chiedere
		aggiornamenti piu' fitti scalda la batteria e non si vede.			*/
	static final long MIN_TIME_MS = 1000;
	static final float MIN_DIST_M = 2.0f;

	/*	Una posizione di rete vecchia di piu' di due minuti non e' piu' una
		risposta: e' un ricordo. Serve a non fidarsi del getLastKnownLocation
		di ieri.															*/
	static final long STALE_MS = 2*60*1000;

	static final int REQ_CODE = 7731;

	static final Object _mutex = new Object();

	static volatile boolean _on;
	static volatile boolean _asked;
	static volatile boolean _hasfix;

	static volatile float _lat;
	static volatile float _lon;
	static volatile float _acc;
	static volatile long  _when;

	static volatile String _provider = "";

	static LocationManager _lm;
	static LocationListener _gpsListener;
	static LocationListener _netListener;


	static Activity _Activity(){
		BBAndroidGame game = BBAndroidGame.AndroidGame();
		if( game==null ) return null;
		return game.GetActivity();
	}


	static boolean _Granted(){

		Activity a = _Activity();
		if( a==null ) return false;

		if( Build.VERSION.SDK_INT<23 ) return true;

		return a.checkSelfPermission( "android.permission.ACCESS_FINE_LOCATION" )
			==PackageManager.PERMISSION_GRANTED
			|| a.checkSelfPermission( "android.permission.ACCESS_COARSE_LOCATION" )
			==PackageManager.PERMISSION_GRANTED;
	}


	//***** API vista da Cerberus *****

	public static int _Start(){

		final Activity a = _Activity();
		if( a==null ) return 0;

		_on = true;

		if( !_Granted() ){

			/*	Una volta sola: richiederlo a ogni giro darebbe una raffica di
				finestre se l'utente ha gia' rifiutato.						*/
			if( !_asked && Build.VERSION.SDK_INT>=23 ){

				_asked = true;

				a.runOnUiThread( new Runnable(){
					public void run(){
						try{
							a.requestPermissions( new String[]{
								"android.permission.ACCESS_FINE_LOCATION",
								"android.permission.ACCESS_COARSE_LOCATION" },REQ_CODE );
						}catch( Throwable t ){
							System.out.println( "[blGPS] richiesta permesso: "+t );
						}
					}
				} );
			}

			return 0;
		}

		_Attach();

		return 1;
	}


	public static void _Stop(){

		_on = false;

		final Activity a = _Activity();
		if( a==null ) return;

		a.runOnUiThread( new Runnable(){
			public void run(){

				try{

					if( _lm!=null ){

						if( _gpsListener!=null ) _lm.removeUpdates( _gpsListener );
						if( _netListener!=null ) _lm.removeUpdates( _netListener );
					}

				}catch( Throwable t ){
					System.out.println( "[blGPS] stop: "+t );
				}

				_gpsListener = null;
				_netListener = null;
			}
		} );
	}


	public static int _State(){

		if( !_on ) return OFF;

		if( !_Granted() ) return DENIED;

		/*	Il permesso puo' essere arrivato dopo lo _Start: appena c'e' ci si
			attacca, senza che l'app debba richiamare niente.				*/
		if( _gpsListener==null && _netListener==null ) _Attach();

		if( _hasfix ) return FIX;

		if( _lm!=null ){

			boolean gps = false;
			boolean net = false;

			try{ gps = _lm.isProviderEnabled( LocationManager.GPS_PROVIDER ); }catch( Throwable t ){}
			try{ net = _lm.isProviderEnabled( LocationManager.NETWORK_PROVIDER ); }catch( Throwable t ){}

			if( !gps && !net ) return DISABLED;
		}

		return WAITING;
	}


	public static float _Lat(){ return _lat; }
	public static float _Lon(){ return _lon; }
	public static float _Acc(){ return _acc; }

	public static String _Provider(){ return _provider; }


	/*	Da quanti millisecondi non arriva niente di nuovo. L'app puo' dire
		"posizione di 40 secondi fa" invece di far credere che sia adesso.	*/
	public static int _Age(){

		if( !_hasfix ) return 0;

		long d = System.currentTimeMillis()-_when;

		if( d<0 ) d = 0;
		if( d>2000000000L ) d = 2000000000L;

		return (int)d;
	}


	//***** dentro *****

	static void _Attach(){

		final Activity a = _Activity();
		if( a==null ) return;

		a.runOnUiThread( new Runnable(){
			public void run(){

				try{

					if( _lm==null ) _lm = (LocationManager)a.getSystemService( Context.LOCATION_SERVICE );
					if( _lm==null ) return;

					if( _gpsListener==null ){

						_gpsListener = _NewListener( "gps" );

						try{
							_lm.requestLocationUpdates( LocationManager.GPS_PROVIDER,
								MIN_TIME_MS,MIN_DIST_M,_gpsListener );
						}catch( Throwable t ){
							System.out.println( "[blGPS] gps non disponibile: "+t );
						}
					}

					if( _netListener==null ){

						_netListener = _NewListener( "rete" );

						try{
							_lm.requestLocationUpdates( LocationManager.NETWORK_PROVIDER,
								MIN_TIME_MS,MIN_DIST_M,_netListener );
						}catch( Throwable t ){
							System.out.println( "[blGPS] rete non disponibile: "+t );
						}
					}

					/*	Qualcosa da mostrare subito, mentre il primo vero fix
						arriva: la mappa si apre gia' nel posto giusto.		*/
					_TryLast( LocationManager.GPS_PROVIDER,"gps" );
					_TryLast( LocationManager.NETWORK_PROVIDER,"rete" );

				}catch( SecurityException e ){

					System.out.println( "[blGPS] permesso mancante: "+e );

				}catch( Throwable t ){

					System.out.println( "[blGPS] avvio: "+t );
				}
			}
		} );
	}


	static void _TryLast( String provider,String nome ){

		try{

			Location loc = _lm.getLastKnownLocation( provider );

			if( loc==null ) return;

			/*	Solo se e' recente: una posizione vecchia manderebbe la mappa
				dall'altra parte d'Italia.									*/
			if( System.currentTimeMillis()-loc.getTime()>STALE_MS ) return;

			_Take( loc,nome );

		}catch( Throwable t ){}
	}


	static LocationListener _NewListener( final String nome ){

		return new LocationListener(){

			public void onLocationChanged( Location loc ){
				_Take( loc,nome );
			}

			public void onProviderEnabled( String p ){}
			public void onProviderDisabled( String p ){}

			/*	Deprecato dall'API 29 ma ancora dichiarato dall'interfaccia
				sotto: va lasciato, se no non compila con minSdk basso.		*/
			public void onStatusChanged( String p,int status,Bundle extras ){}
		};
	}


	static void _Take( Location loc,String nome ){

		if( loc==null ) return;

		synchronized( _mutex ){

			float acc = loc.hasAccuracy() ? loc.getAccuracy() : 9999.0f;

			/*	Le due fonti arrivano mescolate. Si tiene la nuova se e' piu'
				precisa di quella che c'e', oppure se quella che c'e' e'
				invecchiata: se no una rete che dichiara una precisione bassa
				bloccherebbe per sempre il GPS, e viceversa un GPS agganciato
				una volta sola resterebbe li' anche da fermo da mezz'ora.	*/
			boolean meglio = !_hasfix
				|| acc<=_acc
				|| System.currentTimeMillis()-_when>STALE_MS;

			if( !meglio ) return;

			_lat = (float)loc.getLatitude();
			_lon = (float)loc.getLongitude();
			_acc = acc;
			_when = loc.getTime();
			_provider = nome;

			_hasfix = true;
		}
	}
}
