
// ***** blHttpRequests - nativo glfw (Windows, Linux, macOS) *****
//
//  Fa il lavoro libcurl, che sta gia' dentro OGNI build del target glfw3
//  (targets/glfw3/template/curl): non e' una dipendenza nuova, esattamente
//  come tinyfiledialogs per i requester.
//
//  Il thread e' quello del runtime (BBThread): Start() chiama Run__UNSAFE__
//  su un thread di sfondo, IsRunning() dice se ha finito. Il chiamante non si
//  blocca mai.
//
//  Differenze rispetto al codice di partenza, tutte volute:
//
//  - il testo va e viene in UTF-8 VERO. Prima si usava wcstombs in uscita e
//    si allargava byte per byte in entrata: cioe' latin-1. Una risposta con
//    una lettera accentata tornava sbagliata, e il php di prova dichiara
//    charset=utf-8.
//
//  - i byte mandati e ricevuti li conta una funzione di avanzamento di curl,
//    che gira sul thread del lavoro. Prima BytesSending() chiamava
//    curl_easy_getinfo dal thread del disegno su un handle che l'altro thread
//    stava usando - e, finita la richiesta, su un handle gia' distrutto.
//
//  - l'handle di curl si distrugge alla richiesta dopo o nel distruttore, non
//    appena il thread finisce.
//
//  - curl_global_init una volta sola invece che a ogni richiesta.
//
//  - un verbo diverso da GET e POST (PUT, DELETE, PATCH...) adesso arriva
//    davvero al server.

// Su Linux curl viene dai pacchetti del sistema, altrove da quello che
// Cerberus si porta dietro.
#ifdef __linux__
#include <curl/curl.h>
#include "curl/include/curl.h"
#else
#include "curl/include/curl.h"
#endif

#include <string>
#include <vector>


// Da String (Char = wchar_t) a UTF-8.
//
// Su Windows Char e' a 16 bit, quindi un carattere fuori dal piano base
// arriva come coppia surrogata e va rimesso insieme; su Linux e macOS Char e'
// gia' a 32 bit e il ramo non serve.
static std::string blhttp_ToUtf8( String s ){

	std::string out;

	int n = s.Length();
	const Char *d = s.Data();

	for( int i=0;i<n;++i ){

		unsigned int cp = (unsigned int)d[i];

		if( sizeof(Char)==2 && cp>=0xD800 && cp<=0xDBFF && i+1<n ){

			unsigned int lo = (unsigned int)d[i+1];

			if( lo>=0xDC00 && lo<=0xDFFF ){
				cp = 0x10000 + ((cp-0xD800)<<10) + (lo-0xDC00);
				++i;
			}
		}

		if( cp<0x80 ){
			out += (char)cp;
		}else if( cp<0x800 ){
			out += (char)(0xC0|(cp>>6));
			out += (char)(0x80|(cp&0x3F));
		}else if( cp<0x10000 ){
			out += (char)(0xE0|(cp>>12));
			out += (char)(0x80|((cp>>6)&0x3F));
			out += (char)(0x80|(cp&0x3F));
		}else{
			out += (char)(0xF0|(cp>>18));
			out += (char)(0x80|((cp>>12)&0x3F));
			out += (char)(0x80|((cp>>6)&0x3F));
			out += (char)(0x80|(cp&0x3F));
		}
	}

	return out;
}


// Da UTF-8 a String. Un byte che non fa parte di una sequenza valida si tiene
// com'e' invece di buttare via tutto: una risposta mezza rotta si legge lo
// stesso.
static String blhttp_FromUtf8( const char *p,int n ){

	if( n<=0 ) return String();

	std::vector<Char> out;
	out.reserve( n );

	int i = 0;

	while( i<n ){

		unsigned char c = (unsigned char)p[i];

		unsigned int cp = c;
		int extra = 0;

		if( c<0x80 ){
			extra = 0;
		}else if( (c&0xE0)==0xC0 ){
			cp = c&0x1F; extra = 1;
		}else if( (c&0xF0)==0xE0 ){
			cp = c&0x0F; extra = 2;
		}else if( (c&0xF8)==0xF0 ){
			cp = c&0x07; extra = 3;
		}

		if( i+extra>=n ){
			cp = c;
			extra = 0;
		}

		for( int k=1;k<=extra;++k ){

			unsigned char cc = (unsigned char)p[i+k];

			if( (cc&0xC0)!=0x80 ){
				cp = c;
				extra = 0;
				break;
			}

			cp = (cp<<6) | (cc&0x3F);
		}

		i += extra+1;

		if( sizeof(Char)==2 && cp>0xFFFF ){

			cp -= 0x10000;
			out.push_back( (Char)(0xD800 + (cp>>10)) );
			out.push_back( (Char)(0xDC00 + (cp&0x3FF)) );

		}else{

			out.push_back( (Char)cp );
		}
	}

	if( out.empty() ) return String();

	return String( &out[0],(int)out.size() );
}


class BBblHttpRequest : public BBThread{

public:

	BBblHttpRequest();
	~BBblHttpRequest();

	void Open( String verb,String url,int timeout,bool verifycert,bool verifyhost );
	void SetHeader( String name,String value );

	void Send();
	void SendText( String text,String encoding );
	void SendBytes( Array<int> data,int ln );

	void WantImage( bool on );
	int ImageWidth();
	int ImageHeight();
	int ImagePixels( BBDataBuffer *db );

	String ResponseText();
	Array<int> ResponseBytes();

	int Status();
	int BytesReceived();
	int BytesSending();

private:

	CURL *_curl;
	struct curl_slist *_header;

	std::vector<char> _response;
	std::string _body;

	long _status;

	// Scritti dal thread del lavoro, letti da quello del disegno: interi
	// semplici, e la lettura di un valore vecchio di un fotogramma non fa
	// nessun danno a una barra di avanzamento.
	volatile int _recv;
	volatile int _sent;

	// Immagine gia' decodificata, se e' stata chiesta.
	bool _wantimage;

	std::vector<unsigned char> _rgba;

	volatile int _iw;
	volatile int _ih;

	void Cleanup();
	void DecodeImage();

	static size_t DataCallback( void *buf,size_t size,size_t nmemb,void *userp );
	size_t DataCallbackImpl( void *buf,size_t size,size_t nmemb );

	static int ProgressCallback( void *userp,curl_off_t dltotal,curl_off_t dlnow,curl_off_t ultotal,curl_off_t ulnow );

	void Run__UNSAFE__();
};


BBblHttpRequest::BBblHttpRequest():_curl( 0 ),_header( 0 ),_status( -1 ),_recv( 0 ),_sent( 0 ),_wantimage( false ),_iw( 0 ),_ih( 0 ){
}


BBblHttpRequest::~BBblHttpRequest(){

	Cleanup();
}


void BBblHttpRequest::Cleanup(){

	if( _header ){
		curl_slist_free_all( _header );
		_header = 0;
	}

	if( _curl ){
		curl_easy_cleanup( _curl );
		_curl = 0;
	}
}


size_t BBblHttpRequest::DataCallback( void *buf,size_t size,size_t nmemb,void *userp ){

	return static_cast<BBblHttpRequest*>( userp )->DataCallbackImpl( buf,size,nmemb );
}


size_t BBblHttpRequest::DataCallbackImpl( void *buf,size_t size,size_t nmemb ){

	size_t plus = size*nmemb;

	const char *src = (const char*)buf;

	_response.insert( _response.end(),src,src+plus );

	_recv = (int)_response.size();

	return plus;
}


int BBblHttpRequest::ProgressCallback( void *userp,curl_off_t dltotal,curl_off_t dlnow,curl_off_t ultotal,curl_off_t ulnow ){

	BBblHttpRequest *self = static_cast<BBblHttpRequest*>( userp );

	self->_sent = (int)ulnow;

	// Zero = vai avanti. Qualsiasi altro numero fa abortire il trasferimento.
	return 0;
}


void BBblHttpRequest::Open( String verb,String url,int timeout,bool verifycert,bool verifyhost ){

	// Una volta sola per tutto il programma: curl_global_init non e' pensata
	// per essere chiamata a ogni richiesta, e men che meno da piu' thread.
	static bool inited = false;

	if( !inited ){
		curl_global_init( CURL_GLOBAL_DEFAULT );
		inited = true;
	}

	// Se l'oggetto viene riusato, prima si butta via quello di prima.
	Cleanup();

	_response.clear();
	_body.clear();
	_rgba.clear();

	_iw = 0;
	_ih = 0;

	_status = -1;
	_recv = 0;
	_sent = 0;

	_curl = curl_easy_init();

	if( !_curl ) return;

	std::string urlc = blhttp_ToUtf8( url );
	std::string verbc = blhttp_ToUtf8( verb );

	// curl copia le stringhe che gli si passano (da 7.17 in poi), quindi non
	// c'e' bisogno di tenerle vive noi.
	curl_easy_setopt( _curl,CURLOPT_URL,urlc.c_str() );

	curl_easy_setopt( _curl,CURLOPT_WRITEDATA,this );
	curl_easy_setopt( _curl,CURLOPT_WRITEFUNCTION,&BBblHttpRequest::DataCallback );

	curl_easy_setopt( _curl,CURLOPT_XFERINFODATA,this );
	curl_easy_setopt( _curl,CURLOPT_XFERINFOFUNCTION,&BBblHttpRequest::ProgressCallback );
	curl_easy_setopt( _curl,CURLOPT_NOPROGRESS,0L );

	// Senza questo, su Unix curl userebbe i segnali per il timeout della
	// risoluzione dei nomi, e con i thread e' un guaio.
	curl_easy_setopt( _curl,CURLOPT_NOSIGNAL,1L );

	if( timeout>0 ) curl_easy_setopt( _curl,CURLOPT_TIMEOUT,(long)timeout );

	// I redirect si seguono, come fa qualsiasi altro client.
	curl_easy_setopt( _curl,CURLOPT_FOLLOWLOCATION,1L );
	curl_easy_setopt( _curl,CURLOPT_MAXREDIRS,5L );

	curl_easy_setopt( _curl,CURLOPT_SSL_VERIFYPEER,verifycert ? 1L : 0L );
	curl_easy_setopt( _curl,CURLOPT_SSL_VERIFYHOST,verifyhost ? 2L : 0L );

	if( verbc=="GET" ){

		curl_easy_setopt( _curl,CURLOPT_HTTPGET,1L );

	}else if( verbc!="POST" ){

		// POST lo accende gia' CURLOPT_POSTFIELDS; per PUT, DELETE, PATCH...
		// va detto per esteso, se no partirebbero tutte come GET.
		curl_easy_setopt( _curl,CURLOPT_CUSTOMREQUEST,verbc.c_str() );
	}
}


void BBblHttpRequest::SetHeader( String name,String value ){

	if( !_curl ) return;

	std::string nv = blhttp_ToUtf8( name )+": "+blhttp_ToUtf8( value );

	_header = curl_slist_append( _header,nv.c_str() );

	curl_easy_setopt( _curl,CURLOPT_HTTPHEADER,_header );
}


void BBblHttpRequest::Send(){

	if( !_curl ) return;

	Start();
}


void BBblHttpRequest::SendText( String text,String encoding ){

	if( !_curl ) return;

	// Il corpo lo teniamo noi e diamo a curl il puntatore: cosi' non ci sono
	// due copie di una stringa che puo' essere di parecchi mega.
	_body = blhttp_ToUtf8( text );

	curl_easy_setopt( _curl,CURLOPT_POSTFIELDSIZE,(long)_body.size() );
	curl_easy_setopt( _curl,CURLOPT_POSTFIELDS,_body.c_str() );

	Start();
}


void BBblHttpRequest::SendBytes( Array<int> data,int ln ){

	if( !_curl ) return;

	if( ln<0 ) ln = 0;
	if( ln>data.Length() ) ln = data.Length();

	_body.resize( ln );

	for( int i=0;i<ln;++i ) _body[i] = (char)(unsigned char)(data[i]&255);

	curl_easy_setopt( _curl,CURLOPT_POSTFIELDSIZE,(long)ln );

	// COPYPOSTFIELDS no: il corpo lo teniamo vivo noi in _body, e su 5 MB una
	// copia in piu' si sente.
	curl_easy_setopt( _curl,CURLOPT_POSTFIELDS,ln ? _body.data() : "" );

	Start();
}


void BBblHttpRequest::Run__UNSAFE__(){

	if( !_curl ) return;

	CURLcode res = curl_easy_perform( _curl );

	if( res==CURLE_OK ){

		curl_easy_getinfo( _curl,CURLINFO_RESPONSE_CODE,&_status );

	}else{

		// Meno di zero vuol dire "non e' nemmeno arrivata al server": il
		// codice di curl si vede nella console, cosi' si capisce se e' un
		// problema di rete, di nome o di certificato.
		_status = -1;

		printf( "[blHttp] curl: %s\n",curl_easy_strerror( res ) );
		fflush( stdout );
	}

	// La decodifica dell'immagine si fa QUI, sul thread del lavoro: cosi' il
	// gioco non perde un fotogramma quando arriva un tassello di mappa. Sul
	// thread del disegno resta solo la copia dei pixel nella texture.
	if( _wantimage && res==CURLE_OK ) DecodeImage();

	// L'handle NON si distrugge qui: il thread del disegno puo' ancora
	// chiamare BytesReceived o Status subito dopo. Se ne occupa la prossima
	// Open o il distruttore.
}


String BBblHttpRequest::ResponseText(){

	if( _response.empty() ) return String();

	return blhttp_FromUtf8( &_response[0],(int)_response.size() );
}


Array<int> BBblHttpRequest::ResponseBytes(){

	int n = (int)_response.size();

	Array<int> dt( n );

	for( int i=0;i<n;++i ) dt[i] = (unsigned char)_response[i];

	return dt;
}


int BBblHttpRequest::Status(){

	return (int)_status;
}


int BBblHttpRequest::BytesReceived(){

	return _recv;
}


int BBblHttpRequest::BytesSending(){

	return _sent;
}


void BBblHttpRequest::WantImage( bool on ){

	_wantimage = on;
}


// stb_image e' gia' dentro ogni build glfw3: main.h lo include, e' quello con
// cui mojo carica le immagini. Niente da aggiungere al progetto.
void BBblHttpRequest::DecodeImage(){

	_iw = 0;
	_ih = 0;
	_rgba.clear();

	if( _response.empty() ) return;

	int w = 0,h = 0,comp = 0;

	unsigned char *p = stbi_load_from_memory( (const unsigned char*)&_response[0],(int)_response.size(),&w,&h,&comp,4 );

	if( !p ) return;

	if( w<1 || h<1 ){
		stbi_image_free( p );
		return;
	}

	_rgba.resize( (size_t)w*h*4 );

	for( int i=0;i<w*h;++i ){

		int a = p[i*4+3];

		// mojo2 vuole l'alfa PREMOLTIPLICATO: un pixel bianco con alfa 0
		// resterebbe bianco pieno e la scritta uscirebbe dentro un rettangolo
		// opaco. Sui tasselli, che sono opachi, non cambia niente.
		if( a<255 ){
			_rgba[i*4+0] = (unsigned char)(p[i*4+0]*a/255);
			_rgba[i*4+1] = (unsigned char)(p[i*4+1]*a/255);
			_rgba[i*4+2] = (unsigned char)(p[i*4+2]*a/255);
		}else{
			_rgba[i*4+0] = p[i*4+0];
			_rgba[i*4+1] = p[i*4+1];
			_rgba[i*4+2] = p[i*4+2];
		}

		_rgba[i*4+3] = (unsigned char)a;
	}

	stbi_image_free( p );

	_iw = w;
	_ih = h;
}


int BBblHttpRequest::ImageWidth(){ return _iw; }

int BBblHttpRequest::ImageHeight(){ return _ih; }


int BBblHttpRequest::ImagePixels( BBDataBuffer *db ){

	if( !db || _rgba.empty() ) return 0;

	int n = (int)_rgba.size();

	if( db->Length()<n ) return 0;

	void *dst = db->WritePointer( 0 );
	if( !dst ) return 0;

	memcpy( dst,&_rgba[0],n );

	return 1;
}
