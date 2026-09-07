
// ***** blHttpRequests - nativo iOS (NSURLSession) *****
//
//  Portato dal progetto POST_request_test cambiando solo il nome della classe
//  (BBHttpRequest -> BBblHttpRequest, se no si scontrerebbe con quella di
//  brl.httprequest) e la firma di Open, che adesso prende timeout e controllo
//  del certificato come su tutte le altre piattaforme.
//
//  NON e' mai stato compilato dentro boshlibs: qui non c'e' un Mac. Va
//  provato prima di fidarsene.

// ***** HttpRequest.h *****

// Cambia la dichiarazione del delegate:
@interface BBblHttpRequestDelegate : NSObject <NSURLSessionTaskDelegate, NSURLSessionDataDelegate>
@property (nonatomic, assign) class BBblHttpRequest* httpRequest;
@end

class BBblHttpRequest : public BBThread{

    public:

    BBblHttpRequest();
    ~BBblHttpRequest();
        
    void Open( String req,String url,int timeout,bool verifycert,bool verifyhost );
    void SetHeader( String name,String value );
    void Send();
    void SendText( String text,String encoding );
    void SendData( NSData* data );
    void SendBytes( Array<int> datax, int length );

    /*  Su iOS la decodifica dell'immagine non c'e' ancora: i metodi esistono
        perche' il blocco Extern e' unico per tutte le piattaforme, ma non
        fanno niente e ToImage() torna Null.                                */
    void WantImage( bool on ){}
    int ImageWidth(){ return 0; }
    int ImageHeight(){ return 0; }
    int ImagePixels( BBDataBuffer *db ){ return 0; }

    String ResponseText();
    Array<int> ResponseBytes();

    int Status();
    int BytesReceived();
    
    int BytesSending();
    int TotalBytesToSend();
    bool IsUploading();

    void SetProgressCallback(void (*callback)(int64_t sent, int64_t total));

    //private:

    NSMutableURLRequest *_req;
    NSURLSession *_session;
    NSURLSessionTask *_task;
    BBblHttpRequestDelegate *_delegate;
    NSData *_postData;
    NSMutableData *_receivedData; // <--- AGGIUNGI QUESTA LINEA
    
    String _response;
    int _status;
    int _recv;
    int64_t _sendx;
    int64_t _totalToSend;
    bool _isUploading;
    
    void (*_progressCallback)(int64_t sent, int64_t total);

    void Run__UNSAFE__();
    
    void OnProgressUpdate(int64_t bytesSent, int64_t totalBytesSent, int64_t totalExpected);
    void OnUploadComplete(NSData* responseData, NSURLResponse* response, NSError* error);
};

// ***** HttpRequest.cpp *****

@implementation BBblHttpRequestDelegate

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data {
    if (self.httpRequest && self.httpRequest->_receivedData) {
        [self.httpRequest->_receivedData appendData:data];
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task 
   didSendBodyData:(int64_t)bytesSent 
    totalBytesSent:(int64_t)totalBytesSent 
totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend {
    if (self.httpRequest) {
        self.httpRequest->OnProgressUpdate(bytesSent, totalBytesSent, totalBytesExpectedToSend);
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task 
didCompleteWithError:(NSError *)error {
    if (self.httpRequest) {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse*)task.response;
        NSData *responseData = self.httpRequest->_receivedData;
        self.httpRequest->OnUploadComplete(responseData, httpResponse, error);
    }
}

@end

BBblHttpRequest::BBblHttpRequest():_req( 0 ),_session( 0 ),_task( 0 ),_delegate( 0 ),_postData( 0 ),
                              _receivedData( 0 ),
                              _status( -1 ),_recv( 0 ),_sendx( 0 ),_totalToSend( 0 ),
                              _isUploading( false ),_progressCallback( nullptr ){
}

BBblHttpRequest::~BBblHttpRequest(){
    if( _delegate ){
        [_delegate release];
        _delegate = 0;
    }
    if( _session ){
        [_session release];
        _session = 0;
    }
    if( _postData ){
        [_postData release];
        _postData = 0;
    }
    if( _req ){
        [_req release];
        _req = 0;
    }
    if( _receivedData ){
        [_receivedData release];
        _receivedData = 0;
    }
}

void BBblHttpRequest::Open( String req,String url,int timeout,bool verifycert,bool verifyhost ){
    
    _req=[[NSMutableURLRequest alloc] init];
    
    [_req setHTTPMethod:req.ToNSString()];
    [_req setURL:[NSURL URLWithString:url.ToNSString()]];

    if( [_req respondsToSelector:@selector(setAllowsCellularAccess:)] ){
        [_req setAllowsCellularAccess:YES];
    }
    
    // Crea la sessione URL e il delegate
    if( !_session ){
        NSURLSessionConfiguration *config = [NSURLSessionConfiguration defaultSessionConfiguration];
        _delegate = [[BBblHttpRequestDelegate alloc] init];
        _delegate.httpRequest = this;
        _session = [NSURLSession sessionWithConfiguration:config
                                                 delegate:_delegate
                                            delegateQueue:nil];
    }
    
    _response="";
    _status=-1;
    _recv=0;
    _sendx=0;
    _totalToSend=0;
    _isUploading=false;

    if( _receivedData ){
        [_receivedData release];
    }
    _receivedData = [[NSMutableData alloc] init];
}

void BBblHttpRequest::SetHeader( String name,String value ){
    [_req setValue:value.ToNSString() forHTTPHeaderField:name.ToNSString()];
}

void BBblHttpRequest::Send(){
    Start();
}

void BBblHttpRequest::SendText( String text,String encoding ){
    if( _postData ){
        [_postData release];
    }
    _postData = [[text.ToNSString() dataUsingEncoding:NSUTF8StringEncoding] retain];
    _totalToSend = [_postData length];
    Start();
}

void BBblHttpRequest::SendBytes( Array<int> datax, int length ){
    if( _postData ){
        [_postData release];
    }
    if( length > 0 ) {
        // Copia i valori int in un buffer di unsigned char (byte)
        unsigned char* buf = (unsigned char*)malloc(length);
        for (int i = 0; i < length; ++i) {
            buf[i] = (unsigned char)(datax[i] & 0xFF);
        }
        _postData = [[NSData alloc] initWithBytes:buf length:length];
        free(buf);
        _totalToSend = [_postData length];
    } else {
        _postData = 0;
        _totalToSend = 0;
    }
    Start();
}

void BBblHttpRequest::SendData( NSData* data ){
    if( _postData ){
        [_postData release];
    }
    _postData = [data retain];
    _totalToSend = [_postData length];
    Start();
}

void BBblHttpRequest::Run__UNSAFE__(){

    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];

    _isUploading = true;
    
    if( _postData ){
        _task = [_session uploadTaskWithRequest:_req fromData:_postData];
    } else {
        _task = [_session dataTaskWithRequest:_req];
    }
    
    [_task resume];
    
    while( _isUploading ){
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    }
    
    [pool release];
    
    if( _req ){
        [_req release];
        _req = 0;
    }
}

void BBblHttpRequest::OnProgressUpdate(int64_t bytesSent, int64_t totalBytesSent, int64_t totalExpected){
    _sendx = totalBytesSent;
    _totalToSend = totalExpected;
    
    if( _progressCallback ){
        _progressCallback(totalBytesSent, totalExpected);
    }
    
    printf("Progress: %lld / %lld bytes (%.1f%%)\n", 
           totalBytesSent, totalExpected, 
           (double)totalBytesSent / totalExpected * 100.0);
}

void BBblHttpRequest::OnUploadComplete(NSData* responseData, NSURLResponse* response, NSError* error){
    _isUploading = false;
    
    if( error ){
        _status = -1;
        _response = String([[error localizedDescription] UTF8String]);
    } else if( response ){
        NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
        _status = [httpResponse statusCode];
        if( responseData ){
            _response = String([[NSString alloc] initWithData:responseData encoding:NSUTF8StringEncoding]);
            _recv = [responseData length];
        }
    }
}

String BBblHttpRequest::ResponseText(){
    return _response;
}

Array<int> BBblHttpRequest::ResponseBytes(){
    if (!_receivedData || [_receivedData length] == 0) {
        return Array<int>(0);
    }
    int len = (int)[_receivedData length];
    Array<int> arr(len);
    const unsigned char* bytes = (const unsigned char*)[_receivedData bytes];
    for (int i = 0; i < len; ++i) {
        arr[i] = (int)bytes[i];
    }
    return arr;
}


int BBblHttpRequest::Status(){
    return _status;
}

int BBblHttpRequest::BytesReceived(){
    return _recv;
}

int BBblHttpRequest::BytesSending(){
    return (int)_sendx;
}

int BBblHttpRequest::TotalBytesToSend(){
    return (int)_totalToSend;
}

bool BBblHttpRequest::IsUploading(){
    return _isUploading;
}

void BBblHttpRequest::SetProgressCallback(void (*callback)(int64_t sent, int64_t total)){
    _progressCallback = callback;
}