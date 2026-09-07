
// ***** blHttpRequests - nativo html5 (XMLHttpRequest) *****
//
//  ATTENZIONE: nel browser vale la regola della stessa origine. Un indirizzo
//  su un altro dominio funziona solo se quel server manda
//  Access-Control-Allow-Origin - il test.php di prova lo fa.
//
//  Aggiunti rispetto al codice di partenza: il timeout (prima una richiesta
//  che non tornava restava appesa per sempre) e ontimeout.


function BBblHttpRequest(){
	this.response = {
		text: '',
		status: -1,
		length: 0
	}
	this._bytesSending = 0;
	this._totalToSend = 0;
	this._lastMethod = null;
	this._lastUrl = null;
	this._wantimage = false;
	this._rgba = null;
	this._iw = 0;
	this._ih = 0;
}


/*	Chiedendo un'immagine la risposta arriva come ArrayBuffer e si decodifica
	con createImageBitmap piu' un canvas fuori schermo: e' l'unico modo, nel
	browser, di arrivare ai pixel grezzi.

	La decodifica e' ASINCRONA, quindi running resta true fino a che non e'
	finita: se no il chiamante troverebbe la richiesta conclusa e l'immagine
	ancora vuota.															*/
BBblHttpRequest.prototype.WantImage=function( on ){
	this._wantimage = !!on;
	this._forceBinary = !!on;
}

BBblHttpRequest.prototype.ImageWidth=function(){ return this._iw; }
BBblHttpRequest.prototype.ImageHeight=function(){ return this._ih; }

BBblHttpRequest.prototype.ImagePixels=function( db ){

	if( !this._rgba || !db || !db.bytes ) return 0;
	if( db.length<this._rgba.length ) return 0;

	db.bytes.set( this._rgba,0 );

	return 1;
}

BBblHttpRequest.prototype._Decode=function( arr ){

	var self = this;

	self._iw = 0;
	self._ih = 0;
	self._rgba = null;

	try{

		var blob = new Blob( [arr] );

		createImageBitmap( blob ).then( function( bm ){

			try{
				var cv = document.createElement( "canvas" );
				cv.width = bm.width;
				cv.height = bm.height;

				var ctx = cv.getContext( "2d" );
				ctx.drawImage( bm,0,0 );

				var d = ctx.getImageData( 0,0,bm.width,bm.height ).data;

				/*	mojo2 vuole l'alfa PREMOLTIPLICATO. getImageData lo da'
					separato, quindi va moltiplicato a mano.				*/
				for( var i=0;i<d.length;i+=4 ){
					var a = d[i+3];
					if( a<255 ){
						d[i  ] = (d[i  ]*a/255)|0;
						d[i+1] = (d[i+1]*a/255)|0;
						d[i+2] = (d[i+2]*a/255)|0;
					}
				}

				self._rgba = d;
				self._iw = bm.width;
				self._ih = bm.height;

			}catch( e ){
				console.log( "[blHttp] decodifica: "+e );
			}

			self.running = false;

		} ).catch( function( e ){
			console.log( "[blHttp] decodifica: "+e );
			self.running = false;
		} );

	}catch( e ){
		console.log( "[blHttp] decodifica: "+e );
		self.running = false;
	}
}

BBblHttpRequest.prototype.Open=function( requestMethod, url, timeout, verifycert, verifyhost ){
	// timeout e' in secondi come sulle altre piattaforme; verifycert e
	// verifyhost non hanno senso qui: e' il browser a decidere, e non si
	// puo' scavalcarlo. Ci sono per avere la stessa firma ovunque.
	if ( !this.xhr ) this.xhr=new XMLHttpRequest();

	// //IE9
	// if (window.XDomainRequest) {
	// 	var location = document.createElement('a');
	// 	location.href = url;

	// 	if ( location.hostname !== window.location.hostname){
	// 		if ( !('withCredentials' in this.xhr) && !(this.xhr instanceof XDomainRequest) ){
	// 			this.xhr=new XDomainRequest();
	// 		}
	// 	} else if (this.xhr instanceof XDomainRequest) {
	// 		this.xhr=new XMLHttpRequest();
	// 	}
	// }

	var request = this;

	if (!this.xhr.onload) {
		this.xhr.onload = function (e) {
			request.response.status = (e.target.status) ? e.target.status : 200;
			// Gestione risposta binaria o testuale (solo instanceof ArrayBuffer, più robusto)
			if (e.target.response instanceof ArrayBuffer) {
				var arr = new Uint8Array(e.target.response);
				request.response.length = arr.length;
				if (request._wantimage) {
					/*	Chiedendo un'immagine non si costruiscono ne' l'array
						di interi ne' il testo: sarebbero un quarto di mega
						buttato via subito dopo. running resta true finche' la
						decodifica non ha finito.							*/
					request._Decode(arr);
					return;
				}
				request.response.bytes = Array.from(arr);
				try {
					request.response.text = new TextDecoder().decode(arr);
				} catch (err) {
					request.response.text = "";
				}
			} else {
				request.response.text = e.target.responseText;
				request.response.length = e.target.responseText ? e.target.responseText.length : 0;
				request.response.bytes = undefined;
			}
			request.running = false;
		}
	}

	if ( !this.xhr.onprogress ){
		this.xhr.onprogress=function(e){
			if (e.lengthComputable) request.response.length = e.loaded;
		}
	}

	if ( !this.xhr.ontimeout ){
		this.xhr.ontimeout=function(e){
			request.response.status=0;
			request.running=false;
		}
	}

	if ( !this.xhr.onerror ){
		this.xhr.onerror=function(e){
			request.response.status=(e.target.status) ? e.target.status : 0;
			request.running=false;
		}
	}

	this.response.text='';
	this.response.status=-1;
	this.response.length=0;

	// Salva metodo e url per eventuale riapertura
	this._lastMethod = requestMethod;
	this._lastUrl = url;

	this.xhr.open( requestMethod, url );
	if( timeout>0 ){ try{ this.xhr.timeout = timeout*1000; }catch(e){} }
	// Imposta responseType binario se serve DOPO open (standard e compatibilità)
	if (this._forceBinary) {
		try { this.xhr.responseType = "arraybuffer"; } catch (e) {}
	} else {
		try { this.xhr.responseType = ""; } catch (e) {}
	}
	// Reset bytes sending counters on new request
	this._bytesSending = 0;
	this._totalToSend = 0;
}

BBblHttpRequest.prototype.Discard=function(){
	if ( this.xhr ) this.xhr.abort();
	this.response=null;
	this.xhr=null;
}

BBblHttpRequest.prototype.SetHeader=function( name, value ){

	if ( !this.xhr || !this.xhr.setRequestHeader ) return;

	/*	Il browser NON lascia impostare certe intestazioni: le decide lui, ed
		e' una regola di sicurezza, non un capriccio. User-Agent e' fra quelle,
		e blHttpRequest lo mette in OGNI richiesta - senza questo filtro ogni
		richiesta scriveva "Refused to set unsafe header" nella console.		*/
	var n = ( "" + name ).toLowerCase();

	if ( n == "user-agent" || n == "host" || n == "connection" ||
	     n == "referer" || n == "origin" || n == "cookie" ||
	     n == "content-length" || n == "date" || n == "expect" ||
	     n == "keep-alive" || n == "te" || n == "trailer" ||
	     n == "transfer-encoding" || n == "upgrade" || n == "via" ||
	     n.indexOf( "proxy-" ) == 0 || n.indexOf( "sec-" ) == 0 ) return;

	try { this.xhr.setRequestHeader( name, value ); } catch ( e ) {}
}

BBblHttpRequest.prototype.Send=function(){
	this.data=this.encoding=null;
	this._bytesSending = 0;
	this._totalToSend = 0;
	this.Start();
}

BBblHttpRequest.prototype.SendText=function( data, encoding ){
	this.data = data;
	this.encoding = encoding;
	// Calculate total bytes to send
	if (typeof data === 'string') {
		// Assume UTF-8 encoding
		this._totalToSend = new TextEncoder().encode(data).length;
	} else if (data && data.byteLength !== undefined) {
		this._totalToSend = data.byteLength;
	} else {
		this._totalToSend = 0;
	}
	this._bytesSending = 0;
	this._forceBinary = false;
	this.Start();
}


BBblHttpRequest.prototype.SendBytes = function (datax, ln) {
   // datax: array di int (0-255), ln: numero di byte da inviare
   if (!Array.isArray(datax) || typeof ln !== 'number' || ln < 0) {
	   this.data = null;
	   this._totalToSend = 0;
	   this._bytesSending = 0;
	   this._forceBinary = false;
	   return;
   }

   //print ("1");

   // Crea un Uint8Array dai primi ln elementi di datax
   var bytes = new Uint8Array(ln);
   for (var i = 0; i < ln; ++i) {
	   bytes[i] = datax[i] & 0xFF;
   }

   //print (bytes.byteLength);

   this.data = bytes; // Passa direttamente Uint8Array
   this.encoding = null;
   this._totalToSend = bytes.byteLength;
   this._bytesSending = 0;
   this._forceBinary = true;
   // Richiama Open per assicurare che responseType sia corretto
   if (this._lastMethod && this._lastUrl) {
	   this.Open(this._lastMethod, this._lastUrl);
   }
   this.Start();
}

BBblHttpRequest.prototype.Start=function(){
	if (this.xhr) {
		this.running = true;
		var self = this;
		// Attach upload progress event if available
		if (this.xhr.upload && !this._uploadProgressAttached) {
			this.xhr.upload.onprogress = function (e) {
				if (e.lengthComputable) {
					self._bytesSending = e.loaded;
					self._totalToSend = e.total;
				}
			};
			this._uploadProgressAttached = true;
		}
		// WantImage arriva DOPO Open, quindi il responseType va rimesso qui:
		// e' legale fra open() e send().
		if (this._wantimage) { try { this.xhr.responseType = "arraybuffer"; } catch (e) {} }
		this.xhr.send(this.data);
		// Reset _forceBinary dopo ogni invio, a meno che non si stia
		// chiedendo un'immagine: li' serve ancora al prossimo Open.
		if( !this._wantimage ) this._forceBinary = false;
	}
}

BBblHttpRequest.prototype.BytesReceived=function(){
	return this.response.length;
}

BBblHttpRequest.prototype.ResponseText=function(){
	return this.response.text;
}

BBblHttpRequest.prototype.ResponseBytes=function(){
	// Restituisci sempre un array di int (0-255)
	if (this.response && Array.isArray(this.response.bytes)) {
		// Già array di int
		return this.response.bytes.map(function(b) { return b & 0xFF; });
	}
	// Fallback: se la risposta è testo, codifica in UTF-8 e restituisci array di int
	if (typeof this.response.text === 'string') {
		return Array.from(new TextEncoder().encode(this.response.text)).map(function(b) { return b & 0xFF; });
	}
	return [];

}

BBblHttpRequest.prototype.Status=function(){
	return this.response.status;
}

BBblHttpRequest.prototype.IsRunning=function(){
	return this.running;
}

BBblHttpRequest.prototype.BytesSending = function () {
	if (this._totalToSend > 0) {
		return Math.min(this._bytesSending, this._totalToSend);
	}
	// Fallback: if no progress event, try to estimate from data
	if (typeof this.data === 'string') {
		return new TextEncoder().encode(this.data).length;
	} else if (this.data && this.data.byteLength !== undefined) {
		return this.data.byteLength;
	}
	return 0;
}