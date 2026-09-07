import android.app.Activity;
import android.graphics.Color;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

/*	Tastiera nativa Android, quella vera: con correttore, suggerimenti ed emoji.

	Perche' non basta EnableKeyboard() di mojo. La tastiera di sistema mostra
	suggerimenti e correttore solo se chi ha il fuoco le offre una vera
	InputConnection - in pratica un EditText. La GameView di Cerberus e' una
	GLSurfaceView e ne restituisce una finta; anzi, il target dichiara
	apposta

		outAttrs.inputType = ...|TYPE_TEXT_FLAG_NO_SUGGESTIONS

	con tanto di commento "voodoo to disable various undesirable soft keyboard
	features such as predictive text". I caratteri arrivano poi da
	KeyEvent.getUnicodeChar(), che per le emoji torna 0: quindi con quella
	strada le emoji non si possono proprio ricevere.

	Qui si aggiunge al layout dell'Activity un EditText invisibile (1x1,
	trasparente, senza cursore) e gli si da' il fuoco: la tastiera crede di
	scrivere in un campo di testo normale e si comporta come tale. Il testo si
	legge da un TextWatcher.

	Threading: il TextWatcher gira sull'UI thread, il gioco legge dal thread di
	render GL, quindi il testo sta dietro un lock. Le operazioni che toccano le
	view passano da runOnUiThread, come in blRequesters-android.java.			*/

class BBblKeyboard{

	static final Object _mutex = new Object();

	static EditText _edit;
	static boolean _visible;

	/*	Cambia a ogni modifica del testo. L'app confronta con l'ultimo valore
		letto per sapere se e' successo qualcosa, senza doversi tenere una
		copia della stringa.												*/
	static int _version;

	static String _text = "";


	static Activity _Activity(){
		BBAndroidGame game = BBAndroidGame.AndroidGame();
		if( game==null ) return null;
		return game.GetActivity();
	}

	static InputMethodManager _Imm( Activity a ){
		return (InputMethodManager)a.getSystemService( Activity.INPUT_METHOD_SERVICE );
	}


	/*	L'EditText vero e proprio. Non si vede: e' grande 1x1 e trasparente,
		serve solo a dare alla tastiera qualcosa in cui scrivere.		*/
	static EditText _Ensure( Activity activity ){

		if( _edit!=null ) return _edit;

		View root = activity.findViewById( android.R.id.content );
		if( !(root instanceof ViewGroup) ) return null;

		EditText e = new EditText( activity );

		/*	inputType SENZA NO_SUGGESTIONS: e' proprio quello che vogliamo
			diverso dalla GameView. Con TEXT_FLAG_AUTO_CORRECT la tastiera
			accende correttore e suggerimenti; le emoji arrivano come
			normale testo, non come KeyEvent.							*/
		e.setInputType( InputType.TYPE_CLASS_TEXT
			| InputType.TYPE_TEXT_FLAG_AUTO_CORRECT
			| InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
			| InputType.TYPE_TEXT_FLAG_MULTI_LINE );

		e.setImeOptions( EditorInfo.IME_FLAG_NO_FULLSCREEN );

		e.setBackgroundColor( Color.TRANSPARENT );
		e.setTextColor( Color.TRANSPARENT );
		e.setCursorVisible( false );
		e.setGravity( Gravity.TOP );
		e.setFocusable( true );
		e.setFocusableInTouchMode( true );

		e.addTextChangedListener( new TextWatcher(){

			public void beforeTextChanged( CharSequence s, int a, int b, int c ){}
			public void onTextChanged( CharSequence s, int a, int b, int c ){}

			public void afterTextChanged( Editable s ){
				synchronized( _mutex ){
					_text = s.toString();
					_version++;
				}
			}
		} );

		((ViewGroup)root).addView( e, new FrameLayout.LayoutParams( 1, 1 ) );

		_edit = e;

		return _edit;
	}


	//***** API chiamata da Cerberus *****

	static int _Show(){

		final Activity activity = _Activity();
		if( activity==null ) return 0;

		activity.runOnUiThread( new Runnable(){
			public void run(){

				EditText e = _Ensure( activity );
				if( e==null ) return;

				e.setVisibility( View.VISIBLE );
				e.requestFocus();

				_Imm( activity ).showSoftInput( e, InputMethodManager.SHOW_FORCED );

				synchronized( _mutex ){ _visible = true; }
			}
		} );

		return 1;
	}


	static int _Hide(){

		final Activity activity = _Activity();
		if( activity==null ) return 0;

		activity.runOnUiThread( new Runnable(){
			public void run(){

				if( _edit==null ) return;

				_Imm( activity ).hideSoftInputFromWindow( _edit.getWindowToken(), 0 );

				_edit.clearFocus();

				synchronized( _mutex ){ _visible = false; }
			}
		} );

		return 1;
	}


	static int _IsVisible(){
		synchronized( _mutex ){ return _visible ? 1 : 0; }
	}


	static String _GetText(){
		synchronized( _mutex ){ return _text; }
	}


	static int _Version(){
		synchronized( _mutex ){ return _version; }
	}


	/*	Cambia il testo dall'app (per esempio per svuotarlo). Il TextWatcher
		scatta lo stesso e aggiorna _text, quindi non serve farlo qui.	*/
	static int _SetText( final String txt ){

		final Activity activity = _Activity();
		if( activity==null ) return 0;

		activity.runOnUiThread( new Runnable(){
			public void run(){

				EditText e = _Ensure( activity );
				if( e==null ) return;

				e.setText( txt );
				e.setSelection( e.getText().length() );
			}
		} );

		return 1;
	}
}
