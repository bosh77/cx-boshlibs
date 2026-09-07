import android.app.Activity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupMenu;
import android.widget.RelativeLayout;
import java.util.ArrayList;

/*	Implementazione Android dei menu.

	Su Android NON esiste una barra dei menu sempre visibile come sul desktop:
	l'Activity di Cerberus usa il tema Theme.NoTitleBar.Fullscreen, quindi non
	c'e' nemmeno una ActionBar dove appoggiare l'overflow a tre puntini. Il
	menu si apre percio' a comando, come PopupMenu, ancorato a una view
	invisibile che spostiamo nel punto voluto sopra la superficie GL.

	La costruzione resta identica al desktop (AddMenu/AddItem/AddSeparator...):
	quello che cambia e' che la barra non si vede e l'app deve chiamare
	ShowPopup() da un proprio pulsante.

	Le voci vivono in un albero di nodi tenuto qui, e il vero android.view.Menu
	viene riempito solo al momento dell'apertura: PopupMenu costruisce il suo
	Menu da zero ogni volta, quindi gli id e gli stati (spunta, abilitato,
	radio) devono stare da questa parte.

	Threading: la costruzione arriva dal thread di render GL, l'apertura del
	popup deve avvenire sull'UI thread. Gli id vengono assegnati subito, cosi'
	le funzioni restano sincrone come sul desktop; solo l'apertura passa da
	runOnUiThread. Stesso schema di blRequesters-android.java.				*/

class BBblMenus{

	//***** Albero delle voci *****

	static final int T_MENU = 0;    // menu di primo livello (una tendina)
	static final int T_SUB  = 1;    // sottomenu
	static final int T_ITEM = 2;    // voce cliccabile
	static final int T_SEP  = 3;    // separatore

	static class Node{
		int id;
		int parent;         // 0 = radice
		int type;
		String title = "";
		boolean enabled = true;
		boolean checked = false;
		boolean radio = false;
	}

	static final Object _mutex = new Object();

	static ArrayList<Node> _nodes = new ArrayList<Node>();
	static int _nextId = 1000;

	/*	Comando in attesa di essere letto da _MenuItemClicked. Ne basta uno:
		fra un tocco e l'altro l'app fa almeno un giro di OnUpdate.			*/
	static int _pending = 0;

	static View _anchor;


	static Activity _Activity(){
		BBAndroidGame game = BBAndroidGame.AndroidGame();
		if( game==null ) return null;
		return game.GetActivity();
	}

	static Node _Find( int id ){
		synchronized( _mutex ){
			for( Node n : _nodes ) if( n.id==id ) return n;
		}
		return null;
	}

	static ArrayList<Node> _Children( int parent ){
		ArrayList<Node> v = new ArrayList<Node>();
		synchronized( _mutex ){
			for( Node n : _nodes ) if( n.parent==parent ) v.add( n );
		}
		return v;
	}

	static int _Add( int parent, int type, String title ){
		synchronized( _mutex ){
			Node n = new Node();
			n.id = _nextId++;
			n.parent = parent;
			n.type = type;
			n.title = title!=null ? title : "";
			_nodes.add( n );
			return n.id;
		}
	}

	//***** Costruzione (chiamata dal thread di gioco, ritorna subito) *****

	static int _AddMenu( String title ){
		if( title==null || title.length()==0 ) return 0;
		return _Add( 0, T_MENU, title );
	}

	static int _AddSubMenu( int menuId, String title ){
		if( menuId==0 || title==null || title.length()==0 ) return 0;
		if( _Find( menuId )==null ) return 0;
		return _Add( menuId, T_SUB, title );
	}

	static int _AddMenuItem( int menuId, String title ){
		if( menuId==0 || title==null || title.length()==0 ) return 0;
		if( _Find( menuId )==null ) return 0;
		return _Add( menuId, T_ITEM, title );
	}

	/*	L'icona si accetta ma si ignora: Android non disegna le icone nei
		PopupMenu, e forzarle vorrebbe dire usare MenuPopupHelper via
		reflection, che e' API interna e cambia fra versioni.				*/
	static int _AddMenuItemWithIcon( int menuId, String title, String iconPath ){
		return _AddMenuItem( menuId, title );
	}

	static int _AddMenuSeparator( int menuId ){
		if( menuId==0 ) return 0;
		if( _Find( menuId )==null ) return 0;
		return _Add( menuId, T_SEP, "" )!=0 ? 1 : 0;
	}

	//***** Stato delle voci *****

	static int _MenuItemSetEnabled( int itemId, int enabled ){
		Node n = _Find( itemId );
		if( n==null ) return 0;
		n.enabled = enabled!=0;
		return 1;
	}

	static int _MenuItemSetChecked( int itemId, int checked ){
		Node n = _Find( itemId );
		if( n==null ) return 0;
		n.checked = checked!=0;
		return 1;
	}

	static int _MenuItemSetRadio( int itemId, int radio ){
		Node n = _Find( itemId );
		if( n==null ) return 0;
		n.radio = radio!=0;
		return 1;
	}

	static int _MenuItemClicked( int itemId ){
		synchronized( _mutex ){
			if( _pending==itemId ){ _pending = 0; return 1; }
		}
		return 0;
	}

	//***** Apertura del popup *****

	/*	Una view di 1x1 pixel, trasparente, dentro il layout dell'Activity:
		serve solo come punto di ancoraggio, PopupMenu ne ha bisogno di una.	*/
	static View _EnsureAnchor( Activity activity ){
		if( _anchor!=null ) return _anchor;
		View root = activity.findViewById( android.R.id.content );
		if( !(root instanceof ViewGroup) ) return null;
		ViewGroup vg = (ViewGroup)root;
		View v = new View( activity );
		vg.addView( v, new ViewGroup.LayoutParams( 1, 1 ) );
		_anchor = v;
		return _anchor;
	}

	static void _Fill( Menu m, int parent ){
		for( Node n : _Children( parent ) ){
			if( n.type==T_SEP ){
				/*	android.view.Menu non ha separatori. Si usano i gruppi, che
					su alcune versioni disegnano una riga; dove non lo fanno la
					voce semplicemente non c'e' e non si perde niente.		*/
				continue;
			}
			if( n.type==T_SUB ){
				SubMenu sm = m.addSubMenu( n.title );
				_Fill( sm, n.id );
				continue;
			}
			MenuItem it = m.add( Menu.NONE, n.id, Menu.NONE, n.title );
			it.setEnabled( n.enabled );
			if( n.checked || n.radio ){
				it.setCheckable( true );
				it.setChecked( n.checked );
			}
		}
	}

	static int _ShowPopup( final int menuId, final int x, final int y ){

		final Activity activity = _Activity();
		if( activity==null ) return 0;
		if( _Find( menuId )==null ) return 0;

		activity.runOnUiThread( new Runnable(){
			public void run(){

				View anchor = _EnsureAnchor( activity );
				if( anchor==null ) return;

				/*	L'ancora si sposta dove ha chiesto l'app. Le coordinate
					arrivano in pixel dello schermo, gli stessi che usa mojo2
					su Android, quindi non serve convertire niente.			*/
				anchor.setX( x );
				anchor.setY( y );

				PopupMenu pm = new PopupMenu( activity, anchor );
				_Fill( pm.getMenu(), menuId );

				pm.setOnMenuItemClickListener( new PopupMenu.OnMenuItemClickListener(){
					public boolean onMenuItemClick( MenuItem item ){
						synchronized( _mutex ){ _pending = item.getItemId(); }
						return true;
					}
				} );

				pm.show();
			}
		} );

		return 1;
	}
}
