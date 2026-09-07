//--------------------------------------------------------------------------
//  Riproduzione di un .mp4 dentro una Image di mojo2, su iOS.
//
//  Stessa implementazione della versione macOS (AVAssetReader +
//  AVAssetReaderTrackOutput, fotogrammi kCVPixelFormatType_32BGRA, thread di
//  sfondo con doppio buffer) perche' l'API di AVFoundation/CoreMedia/
//  CoreVideo usata qui e' identica sulle due piattaforme. File separato
//  invece di uno importato da entrambe, per restare nello schema "un file
//  per piattaforma" del modulo (unica eccezione dichiarata e' blHttpRequests
//  -glfw.cpp). Per lo stesso motivo, un fix trovato qui va portato a mano
//  anche in blVideos-mac.cpp (o viceversa).
//
//  Il target ios (targets/ios/template) compila anch'esso un main.mm
//  (Objective-C++, vedi src/transcc/builders/ios.cxs), quindi questo .cpp
//  puo' usare sintassi Objective-C come i nativi macOS.
//
//  Framework: AVFoundation.framework e' gia' nel progetto Xcode del target
//  ios (usato anche da brl.admob), quindi blVideos.cxs NON lo ridichiara -
//  farlo duplicherebbe il riferimento nel progetto. CoreMedia.framework e
//  CoreVideo.framework invece non ci sono e li aggiunge il modulo con
//  #LIBS+=, che per QUESTO target (a differenza del target macOS Desktop) e'
//  un meccanismo gia' pronto: src/transcc/builders/ios.cxs legge #LIBS e
//  aggiunge i framework al progetto da solo, senza toccare l'installazione.
//
//  ATTENZIONE ARC: come il target macOS Desktop, anche il progetto Xcode di
//  questo target non ha CLANG_ENABLE_OBJC_ARC impostato, quindi e' spento:
//  stesso motivo per cui qui sotto gli oggetti Objective-C si rilasciano a
//  mano e le chiamate che tornano oggetti autoreleased stanno dentro
//  @autoreleasepool (il thread di decodifica e' un std::thread nudo, senza
//  il pool implicito di un NSThread/la run loop principale).
//
//  AUDIO: stesso schema di blVideos-mac.cpp (vedi li' per i dettagli) - un
//  secondo AVAssetReader sulla sola traccia audio, convertito a PCM 16 bit
//  stereo via outputSettings, thread di sfondo con coda di 4 buffer OpenAL.
//  OpenAL.framework e' gia' nel progetto Xcode del target ios (lo usa
//  mojo.audio), non serve aggiungere niente. Provato su iOS Simulator
//  insieme al video (vedi PORT-MAC-LINUX.md); un fix qui va portato a mano
//  anche in blVideos-mac.cpp, o viceversa.
//
//  MAI COMPILATO su un device vero, solo simulatore (vedi PORT-MAC-LINUX.md).
//--------------------------------------------------------------------------

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <CoreAudio/CoreAudioTypes.h>
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstring>


static double _blvNow(){

    using namespace std::chrono;
    return duration<double>( steady_clock::now().time_since_epoch() ).count();
}


class BBblVideo : public Object{
public:

    BBblVideo();
    ~BBblVideo();

    bool _Open( String path );
    void _Play();
    void _Pause();
    void _SetLoop( bool on );
    void _Close();

    bool _IsPlaying();
    bool _IsFinished();
    bool _HasFailed();

    int _Width();
    int _Height();

    bool _CopyFrame( BBDataBuffer *db );

    /* chiamate dai due thread, devono restare pubbliche */
    void _Decode();
    void _DecodeAudio();

private:

    bool _OpenReader( AVAssetReader **outReader,AVAssetReaderTrackOutput **outOutput );
    void _StoreFrame( CMSampleBufferRef sample );

    bool _OpenAudioReader( AVAssetReader **outReader,AVAssetReaderTrackOutput **outOutput );
    bool _FillAudioBuffer( ALuint alBuf );

    std::string _path;

    std::thread _thread;
    std::mutex _mtx;

    volatile bool _running;
    volatile bool _playing;
    volatile bool _loop;
    volatile bool _finished;
    volatile bool _failed;

    volatile int _w, _h;

    std::vector<unsigned char> _rgba;
    std::vector<unsigned char> _work;
    bool _hasNew;

    /*  Audio: thread, reader e coda OpenAL a parte, indipendenti dal video. */
    static const int AL_NBUF = 4;

    std::thread _audioThread;
    volatile bool _audioRunning;
    bool _hasAudio;

    AVAssetReader *_audioReader;
    AVAssetReaderTrackOutput *_audioOutput;
    int _audioChannels;
    int _audioRate;

    ALuint _alSource;
    ALuint _alBuffers[AL_NBUF];
};


BBblVideo::BBblVideo():
    _running( false ),_playing( false ),_loop( false ),_finished( false ),_failed( false ),
    _w( 0 ),_h( 0 ),_hasNew( false ),
    _audioRunning( false ),_hasAudio( false ),
    _audioReader( nil ),_audioOutput( nil ),_audioChannels( 0 ),_audioRate( 0 ),_alSource( 0 ){

    for( int i=0;i<AL_NBUF;++i ) _alBuffers[i] = 0;
}

BBblVideo::~BBblVideo(){

    _Close();
}


bool BBblVideo::_OpenReader( AVAssetReader **outReader,AVAssetReaderTrackOutput **outOutput ){

    bool ok = false;

    @autoreleasepool{

        NSString *nsPath = [NSString stringWithUTF8String: _path.c_str()];
        NSURL *url = [NSURL fileURLWithPath: nsPath];
        AVURLAsset *asset = [AVURLAsset URLAssetWithURL: url options: nil];

        NSArray<AVAssetTrack*> *tracks = [asset tracksWithMediaType: AVMediaTypeVideo];

        if( tracks.count>0 ){

            AVAssetTrack *track = tracks[0];

            NSDictionary *settings = @{ (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) };

            NSError *err = nil;
            AVAssetReader *r = [[AVAssetReader alloc] initWithAsset: asset error: &err];

            if( r ){

                AVAssetReaderTrackOutput *o = [[AVAssetReaderTrackOutput alloc] initWithTrack: track outputSettings: settings];
                o.alwaysCopiesSampleData = NO;

                if( [r canAddOutput: o] ){

                    [r addOutput: o];

                    if( [r startReading] ){
                        *outReader = r;
                        *outOutput = o;
                        ok = true;
                    }else{
                        [o release];
                        [r release];
                    }

                }else{
                    [o release];
                    [r release];
                }

            }
        }
    }

    return ok;
}


/*  Reader indipendente, solo per l'audio: stesso file, thread diverso da
    quello video. Torna false se il file non ha una traccia audio o la
    conversione a PCM fallisce - il chiamante allora continua senza audio,
    stessa filosofia del modulo (il video funziona comunque). */
bool BBblVideo::_OpenAudioReader( AVAssetReader **outReader,AVAssetReaderTrackOutput **outOutput ){

    bool ok = false;

    @autoreleasepool{

        NSString *nsPath = [NSString stringWithUTF8String: _path.c_str()];
        NSURL *url = [NSURL fileURLWithPath: nsPath];
        AVURLAsset *asset = [AVURLAsset URLAssetWithURL: url options: nil];

        NSArray<AVAssetTrack*> *tracks = [asset tracksWithMediaType: AVMediaTypeAudio];

        if( tracks.count>0 ){

            AVAssetTrack *track = tracks[0];

            /*  Frequenza sorgente, letta dalla traccia PRIMA della
                conversione: non forziamo AVSampleRateKey, quindi la
                conversione a PCM mantiene la frequenza originale (stessa
                idea di Windows, che rilegge MF_MT_AUDIO_SAMPLES_PER_SECOND
                dopo aver negoziato il tipo). */
            double srcRate = 44100.0;

            NSArray *fmts = track.formatDescriptions;

            if( fmts.count>0 ){

                CMFormatDescriptionRef desc = (CMFormatDescriptionRef)fmts[0];
                const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription( (CMAudioFormatDescriptionRef)desc );

                if( asbd && asbd->mSampleRate>0 ) srcRate = asbd->mSampleRate;
            }

            /*  PCM 16 bit, stereo: il formato piu' semplice per
                AL_FORMAT_STEREO16. Il numero di canali sorgente puo' essere
                qualunque, anche 5.1: si chiede comunque stereo, ed e'
                AVFoundation a fare il mix. */
            NSDictionary *settings = @{
                AVFormatIDKey: @(kAudioFormatLinearPCM),
                AVLinearPCMBitDepthKey: @16,
                AVLinearPCMIsFloatKey: @NO,
                AVLinearPCMIsBigEndianKey: @NO,
                AVLinearPCMIsNonInterleaved: @NO,
                AVNumberOfChannelsKey: @2
            };

            AVAssetReader *r = [[AVAssetReader alloc] initWithAsset: asset error: nil];

            if( r ){

                AVAssetReaderTrackOutput *o = [[AVAssetReaderTrackOutput alloc] initWithTrack: track outputSettings: settings];
                o.alwaysCopiesSampleData = NO;

                if( [r canAddOutput: o] ){

                    [r addOutput: o];

                    if( [r startReading] ){

                        _audioChannels = 2;
                        _audioRate = (int)srcRate;

                        *outReader = r;
                        *outOutput = o;
                        ok = true;

                    }else{
                        [o release];
                        [r release];
                    }

                }else{
                    [o release];
                    [r release];
                }
            }
        }
    }

    return ok;
}


/*  Legge UN sample audio e lo carica in un buffer OpenAL gia' generato.
    Torna false a fine flusso (senza loop) o su errore vero: il chiamante
    allora smette di rimettere in coda, non e' un fallimento del video. In
    loop ricrea il reader (AVAssetReader non si riavvolge, stessa storia del
    video) e riprova subito sul nuovo giro. */
bool BBblVideo::_FillAudioBuffer( ALuint alBuf ){

    CMSampleBufferRef sample = nil;

    @autoreleasepool{
        sample = [_audioOutput copyNextSampleBuffer];
    }

    if( !sample ){

        if( !_loop || !_audioRunning ) return false;

        @autoreleasepool{
            if( _audioOutput ) [_audioOutput release];
            if( _audioReader ) [_audioReader release];
        }

        _audioOutput = nil;
        _audioReader = nil;

        AVAssetReader *newReader = nil;
        AVAssetReaderTrackOutput *newOutput = nil;

        if( !_OpenAudioReader( &newReader,&newOutput ) ) return false;

        _audioReader = newReader;
        _audioOutput = newOutput;

        return _FillAudioBuffer( alBuf );
    }

    CMBlockBufferRef blockBuffer = nil;
    AudioBufferList audioBufferList;

    OSStatus st = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sample,NULL,&audioBufferList,sizeof(audioBufferList),
        NULL,NULL,kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,&blockBuffer );

    if( st==noErr && audioBufferList.mNumberBuffers>0 ){

        AudioBuffer &ab = audioBufferList.mBuffers[0];

        if( ab.mData && ab.mDataByteSize>0 ){

            ALenum format = _audioChannels>=2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

            alBufferData( alBuf,format,ab.mData,(ALsizei)ab.mDataByteSize,(ALsizei)_audioRate );

            /*  _alSource/_alBuffers li tocca solo questo thread: _Close()
                aspetta che il thread finisca (join) prima di toccare AL,
                quindi qui non serve _mtx. */
            alSourceQueueBuffers( _alSource,1,&alBuf );
        }
    }

    if( blockBuffer ) CFRelease( blockBuffer );
    CFRelease( sample );

    return true;
}


void BBblVideo::_DecodeAudio(){

    alGetError();  /* pulisce eventuali errori precedenti, cosi' i controlli sotto sono affidabili */

    alGenSources( 1,&_alSource );
    alGenBuffers( AL_NBUF,_alBuffers );

    if( alGetError()!=AL_NO_ERROR || !_alSource ){
        /* niente sorgente/buffer: niente audio, ma il video prosegue lo stesso */
        return;
    }

    /*  Precarica tutti i buffer prima di avviare la sorgente, cosi' non
        parte "a vuoto" in attesa del primo _FillAudioBuffer. */
    for( int i=0;i<AL_NBUF;++i ) _FillAudioBuffer( _alBuffers[i] );

    bool wasPlaying = false;
    bool streamDone = false;

    while( _audioRunning ){

        if( !_playing ){

            if( wasPlaying ){
                alSourcePause( _alSource );
                wasPlaying = false;
            }

            std::this_thread::sleep_for( std::chrono::milliseconds( 15 ) );
            continue;
        }

        if( !wasPlaying ){

            /*  Alla ripartenza (Play dopo Pause, o dopo un loop che ha
                ririempito la coda) la sorgente va rimessa in moto. */
            ALint state = 0;
            alGetSourcei( _alSource,AL_SOURCE_STATE,&state );
            if( state!=AL_PLAYING ) alSourcePlay( _alSource );

            wasPlaying = true;
        }

        if( !streamDone ){

            ALint processed = 0;
            alGetSourcei( _alSource,AL_BUFFERS_PROCESSED,&processed );

            while( processed-- > 0 ){

                ALuint buf = 0;

                alSourceUnqueueBuffers( _alSource,1,&buf );

                if( buf && !_FillAudioBuffer( buf ) ) streamDone = true;
            }
        }

        /*  AL puo' fermarsi da solo se per un attimo la coda resta vuota
            (macchina lenta): se c'e' ancora roba in coda si fa ripartire,
            invece di restare muti in silenzio per il resto del filmato. */
        ALint state = 0;
        alGetSourcei( _alSource,AL_SOURCE_STATE,&state );
        if( state!=AL_PLAYING ){
            ALint queued = 0;
            alGetSourcei( _alSource,AL_BUFFERS_QUEUED,&queued );
            if( queued>0 ) alSourcePlay( _alSource );
            else if( streamDone ) break;  /* finito davvero, niente da riprodurre */
        }

        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }
}


bool BBblVideo::_Open( String path ){

    _Close();

    _failed = false;
    _finished = false;
    _hasNew = false;

    /* "cerberus://data/x.mp4" -> percorso reale dentro il bundle dell'app */
    _path = convertBBString( BBGame::Game()->PathToFilePath( path ) );

    bool sizeOk = false;

    @autoreleasepool{

        NSString *nsPath = [NSString stringWithUTF8String: _path.c_str()];
        NSURL *url = [NSURL fileURLWithPath: nsPath];
        AVURLAsset *asset = [AVURLAsset URLAssetWithURL: url options: nil];

        NSArray<AVAssetTrack*> *tracks = [asset tracksWithMediaType: AVMediaTypeVideo];

        if( tracks.count>0 ){

            CGSize sz = tracks[0].naturalSize;

            _w = (int)sz.width;
            _h = (int)sz.height;

            sizeOk = _w>0 && _h>0;
        }
    }

    if( !sizeOk ){
        bbPrint( String( "[blVideo] apertura fallita: " )+path );
        _failed = true;
        return false;
    }

    bbPrint( String( "[blVideo] " )+path+" "+String( _w )+"x"+String( _h ) );

    _running = true;

    _thread = std::thread( &BBblVideo::_Decode,this );

    AVAssetReader *aReader = nil;
    AVAssetReaderTrackOutput *aOutput = nil;

    if( _OpenAudioReader( &aReader,&aOutput ) ){

        _audioReader = aReader;
        _audioOutput = aOutput;

        bbPrint( String( "[blVideo] audio: " )+_audioChannels+"ch "+_audioRate+"Hz" );

        _hasAudio = true;
        _audioRunning = true;

        _audioThread = std::thread( &BBblVideo::_DecodeAudio,this );
    }

    return true;
}


void BBblVideo::_Play(){
    _finished = false;
    _playing = true;
}

void BBblVideo::_Pause(){
    _playing = false;
}

void BBblVideo::_SetLoop( bool on ){
    _loop = on;
}

void BBblVideo::_Close(){

    _running = false;
    _audioRunning = false;
    _playing = false;

    if( _thread.joinable() ) _thread.join();
    if( _audioThread.joinable() ) _audioThread.join();

    if( _alSource ){
        alSourceStop( _alSource );
        alSourcei( _alSource,AL_BUFFER,0 );
        alDeleteSources( 1,&_alSource );
        _alSource = 0;
    }

    bool anyBuf = false;
    for( int i=0;i<AL_NBUF;++i ) if( _alBuffers[i] ) anyBuf = true;
    if( anyBuf ){
        alDeleteBuffers( AL_NBUF,_alBuffers );
        for( int i=0;i<AL_NBUF;++i ) _alBuffers[i] = 0;
    }

    if( _audioReader || _audioOutput ){

        @autoreleasepool{
            if( _audioOutput ) [_audioOutput release];
            if( _audioReader ) [_audioReader release];
        }

        _audioOutput = nil;
        _audioReader = nil;
    }

    _hasAudio = false;

    {
        std::lock_guard<std::mutex> lk( _mtx );
        _rgba.clear();
        _hasNew = false;
    }

    _w = 0;
    _h = 0;
}

bool BBblVideo::_IsPlaying(){ return _playing; }
bool BBblVideo::_IsFinished(){ return _finished; }
bool BBblVideo::_HasFailed(){ return _failed; }
int  BBblVideo::_Width(){ return _w; }
int  BBblVideo::_Height(){ return _h; }


/*  Il thread che disegna fa solo questo: una memcpy. */
bool BBblVideo::_CopyFrame( BBDataBuffer *db ){

    if( !db ) return false;

    bool got = false;

    std::lock_guard<std::mutex> lk( _mtx );

    if( _hasNew && !_rgba.empty() ){

        void *dst = db->WritePointer( 0 );
        int n = (int)_rgba.size();

        if( dst && db->Length()>=n ){
            memcpy( dst,&_rgba[0],n );
            _hasNew = false;
            got = true;
        }
    }

    return got;
}


void BBblVideo::_StoreFrame( CMSampleBufferRef sample ){

    CVImageBufferRef pix = CMSampleBufferGetImageBuffer( sample );
    if( !pix ) return;

    CVPixelBufferLockBaseAddress( pix,kCVPixelBufferLock_ReadOnly );

    int w = (int)CVPixelBufferGetWidth( pix );
    int h = (int)CVPixelBufferGetHeight( pix );
    size_t stride = CVPixelBufferGetBytesPerRow( pix );
    unsigned char *src = (unsigned char*)CVPixelBufferGetBaseAddress( pix );

    if( src && w>0 && h>0 ){

        size_t need = (size_t)w*h*4;
        if( _work.size()!=need ) _work.resize( need );

        for( int y=0;y<h;++y ){

            const unsigned int *s32 = (const unsigned int*)( src+(size_t)y*stride );
            unsigned int *d32 = (unsigned int*)( &_work[0]+(size_t)y*w*4 );

            for( int x=0;x<w;++x ){

                /* vedi blVideos-mac.cpp: stesso scambio BGRA -> RGBA, alfa
                   forzato a pieno perche' un fotogramma video e' opaco */
                unsigned int v = s32[x];

                d32[x] = ( ( v>>16 ) & 0x000000FFu ) |
                         (   v       & 0x0000FF00u ) |
                         ( ( v<<16 ) & 0x00FF0000u ) |
                                       0xFF000000u;
            }
        }

        std::lock_guard<std::mutex> lk( _mtx );
        _rgba.swap( _work );
        _hasNew = true;
    }

    CVPixelBufferUnlockBaseAddress( pix,kCVPixelBufferLock_ReadOnly );
}


void BBblVideo::_Decode(){

    AVAssetReader *reader = nil;
    AVAssetReaderTrackOutput *output = nil;

    if( !_OpenReader( &reader,&output ) ){
        _failed = true;
        return;
    }

    double startTime = -1;

    while( _running ){

        if( !_playing ){
            std::this_thread::sleep_for( std::chrono::milliseconds( 15 ) );
            startTime = -1;
            continue;
        }

        CMSampleBufferRef sample = nil;

        @autoreleasepool{
            sample = [output copyNextSampleBuffer];
        }

        if( !sample ){

            @autoreleasepool{
                [output release];
                [reader release];
            }
            reader = nil;
            output = nil;

            if( _loop && _running ){

                if( !_OpenReader( &reader,&output ) ){
                    _failed = true;
                    break;
                }

                startTime = -1;
                continue;
            }

            _finished = true;
            _playing = false;
            continue;
        }

        CMTime ts = CMSampleBufferGetPresentationTimeStamp( sample );
        double frameTime = CMTimeGetSeconds( ts );

        double now = _blvNow();
        if( startTime<0 ) startTime = now-frameTime;

        double late = now-( startTime+frameTime );

        if( late>0.25 ){
            startTime = now-frameTime;
            late = 0;
        }

        if( late<0 ){

            double wait = -late;

            while( wait>0 && _running && _playing ){
                double ms = wait*1000.0;
                if( ms>50 ) ms = 50;
                std::this_thread::sleep_for( std::chrono::milliseconds( ms>0 ? (int)ms : 1 ) );
                wait = ( startTime+frameTime )-_blvNow();
            }
        }

        _StoreFrame( sample );

        CFRelease( sample );
    }

    if( reader ){
        @autoreleasepool{
            if( output ) [output release];
            [reader release];
        }
    }
}
