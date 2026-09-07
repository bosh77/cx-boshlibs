#if _WIN32

//--------------------------------------------------------------------------
//  Riproduzione di un .mp4 dentro una Image di mojo2, su Windows.
//
//  Usa Media Foundation, che fa gia' parte del sistema: nessuna libreria
//  esterna da distribuire. IMFSourceReader viene configurato per consegnare
//  fotogrammi RGB32, quindi la conversione dal formato del filmato la fa
//  Windows.
//
//  Struttura uguale alla versione Android: un thread di sfondo decodifica e
//  tiene pronto UN fotogramma gia' convertito, il thread che disegna si
//  limita a una memcpy dentro il DataBuffer. La prima versione faceva tutto
//  dentro _CopyFrame, cioe' sul thread di rendering: andava a scatti in
//  release e pianissimo in debug, perche' la conversione era un ciclo byte
//  per byte non ottimizzato.
//
//  Due accorgimenti sul costo della conversione:
//   - si lavora a parole da 32 bit invece che sui singoli byte (mojo2 vuole
//     R,G,B,A mentre RGB32 in memoria e' B,G,R,A: e' uno scambio di due byte
//     che si fa con qualche operazione logica per pixel)
//   - la conversione sta sul thread di decodifica, non su quello di disegno
//
//  AUDIO: un secondo IMFSourceReader indipendente, aperto sullo stesso file,
//  selezionato solo sul flusso audio e convertito a PCM 16 bit stereo (la
//  stessa idea di ENABLE_VIDEO_PROCESSING per il video: si chiede il formato
//  che serve e la catena di conversione la costruisce Media Foundation da
//  sola). Un secondo thread di sfondo tiene in coda un manciata di buffer
//  OpenAL e li riempie via via che vengono consumati (il classico schema di
//  streaming audio "coda di buffer"): l'audio si scandisce da solo, a tempo
//  reale, semplicemente lasciandolo suonare - non serve nessun orologio
//  manuale come per il video. Il contesto OpenAL e' quello che gia' apre
//  mojo.audio all'avvio dell'app (gxtkAudio, in mojo.glfw.cpp): e' globale
//  fra i thread in OpenAL (a differenza di un contesto OpenGL), quindi le
//  chiamate AL da questo thread di sfondo sono sicure senza doverne aprire
//  uno tutto nostro.
//
//  Non e' sincronizzazione fine (lip-sync da regia): video e audio partono
//  insieme da _Play() e ripartono insieme in loop, ma ognuno scorre col
//  proprio passo. Per un filmato dentro una Image questo basta.
//--------------------------------------------------------------------------

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <al.h>
#include <alc.h>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment( lib,"mfplat.lib" )
#pragma comment( lib,"mfuuid.lib" )
#pragma comment( lib,"openal32.lib" )
#endif


static bool _blvMFReady = false;

static void _blvStartMF(){

    if( _blvMFReady ) return;

    CoInitializeEx( 0,COINIT_MULTITHREADED );
    MFStartup( MF_VERSION,MFSTARTUP_LITE );

    _blvMFReady = true;
}

static std::wstring _blvWide( String s ){
    return std::wstring( s.Data(),s.Length() );
}

static double _blvNow(){

    static LARGE_INTEGER freq = { 0 };
    if( !freq.QuadPart ) QueryPerformanceFrequency( &freq );

    LARGE_INTEGER t;
    QueryPerformanceCounter( &t );

    return (double)t.QuadPart/(double)freq.QuadPart;
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

    IMFSourceReader *_OpenReader();
    IMFSourceReader *_OpenAudioReader();
    bool _ReadSize();
    void _StoreFrame( IMFSample *sample );
    bool _FillAudioBuffer( ALuint alBuf );

    std::wstring _path;

    HANDLE _thread;
    CRITICAL_SECTION _cs;
    bool _csReady;

    volatile bool _running;
    volatile bool _playing;
    volatile bool _loop;
    volatile bool _finished;
    volatile bool _failed;

    volatile int _w, _h;
    int _stride;

    /*  Due buffer che si scambiano: _work e' quello che il decoder riempie,
        _rgba quello pronto per chi disegna. Lo scambio e' uno scambio di
        puntatori, quindi dopo il primo fotogramma non si alloca piu' niente:
        allocarne uno nuovo ogni volta voleva dire, a 1024x768 e 60 fps,
        180 MB al secondo di sola gestione della memoria. */
    std::vector<unsigned char> _rgba;
    std::vector<unsigned char> _work;
    bool _hasNew;

    /*  Audio: thread, reader e coda OpenAL a parte, indipendenti dal video. */
    static const int AL_NBUF = 4;

    HANDLE _audioThread;
    volatile bool _audioRunning;
    bool _hasAudio;

    IMFSourceReader *_audioReader;
    int _audioChannels;
    int _audioRate;

    ALuint _alSource;
    ALuint _alBuffers[AL_NBUF];

    std::vector<unsigned char> _audioScratch;
};


static DWORD WINAPI _blvThreadProc( LPVOID param ){

    CoInitializeEx( 0,COINIT_MULTITHREADED );

    ( (BBblVideo*)param )->_Decode();

    CoUninitialize();

    return 0;
}

static DWORD WINAPI _blvAudioThreadProc( LPVOID param ){

    CoInitializeEx( 0,COINIT_MULTITHREADED );

    ( (BBblVideo*)param )->_DecodeAudio();

    CoUninitialize();

    return 0;
}


BBblVideo::BBblVideo():
    _thread( 0 ),_csReady( false ),
    _running( false ),_playing( false ),_loop( false ),_finished( false ),_failed( false ),
    _w( 0 ),_h( 0 ),_stride( 0 ),_hasNew( false ),
    _audioThread( 0 ),_audioRunning( false ),_hasAudio( false ),
    _audioReader( 0 ),_audioChannels( 0 ),_audioRate( 0 ),_alSource( 0 ){

    InitializeCriticalSection( &_cs );
    _csReady = true;

    for( int i=0;i<AL_NBUF;++i ) _alBuffers[i] = 0;
}

BBblVideo::~BBblVideo(){

    _Close();

    if( _csReady ){
        DeleteCriticalSection( &_cs );
        _csReady = false;
    }
}


/*  MFCreateSourceReaderFromURL e' l'UNICA funzione che serve da
    mfreadwrite.dll. La sua libreria di import (libmfreadwrite.a) non c'e' in
    tutte le distribuzioni MinGW - TDM-GCC ha mfplat e mfuuid ma non quella -
    quindi la funzione si prende a runtime con GetProcAddress: cosi' il modulo
    linka sia con MSVC sia con MinGW, e la DLL c'e' su qualunque Windows in cui
    Media Foundation esista.                                                */
typedef HRESULT (STDAPICALLTYPE *PFN_MFCreateSourceReaderFromURL)( LPCWSTR,IMFAttributes*,IMFSourceReader** );

static PFN_MFCreateSourceReaderFromURL blvid_ReaderFromURL(){

    static PFN_MFCreateSourceReaderFromURL fn = 0;
    static bool tried = false;

    if( !tried ){
        tried = true;
        HMODULE h = LoadLibraryW( L"mfreadwrite.dll" );
        if( h ) fn = (PFN_MFCreateSourceReaderFromURL)GetProcAddress( h,"MFCreateSourceReaderFromURL" );
    }

    return fn;
}


IMFSourceReader *BBblVideo::_OpenReader(){

    /* ENABLE_VIDEO_PROCESSING fa fare a Media Foundation la conversione
       verso RGB32, qualunque sia il formato del filmato. */
    IMFAttributes *attr = 0;
    if( FAILED( MFCreateAttributes( &attr,1 ) ) ) return 0;
    attr->SetUINT32( MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE );

    PFN_MFCreateSourceReaderFromURL createReader = blvid_ReaderFromURL();

    if( !createReader ){ attr->Release(); return 0; }

    IMFSourceReader *reader = 0;
    HRESULT hr = createReader( _path.c_str(),attr,&reader );
    attr->Release();

    if( FAILED( hr ) || !reader ) return 0;

    reader->SetStreamSelection( (DWORD)MF_SOURCE_READER_ALL_STREAMS,FALSE );
    reader->SetStreamSelection( (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE );

    IMFMediaType *want = 0;
    if( FAILED( MFCreateMediaType( &want ) ) ){ reader->Release(); return 0; }

    want->SetGUID( MF_MT_MAJOR_TYPE,MFMediaType_Video );
    want->SetGUID( MF_MT_SUBTYPE,MFVideoFormat_RGB32 );

    hr = reader->SetCurrentMediaType( (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,want );
    want->Release();

    if( FAILED( hr ) ){ reader->Release(); return 0; }

    return reader;
}


/*  Reader indipendente, solo per l'audio: stesso file, thread diverso da
    quello video, cosi' i due non si intralciano - il video ha la sua
    scansione a tempo con le Sleep, l'audio deve solo tenere piena la coda
    OpenAL appena c'e' posto. Torna 0 se il file non ha una traccia audio, e
    in quel caso il chiamante continua senza (stessa filosofia del modulo:
    fuori dal desktop/senza nativo i riquadri restano vuoti, qui l'audio
    resta muto, ma il video funziona lo stesso). */
IMFSourceReader *BBblVideo::_OpenAudioReader(){

    IMFAttributes *attr = 0;
    if( FAILED( MFCreateAttributes( &attr,1 ) ) ) return 0;

    PFN_MFCreateSourceReaderFromURL createReader = blvid_ReaderFromURL();

    if( !createReader ){ attr->Release(); return 0; }

    IMFSourceReader *reader = 0;
    HRESULT hr = createReader( _path.c_str(),attr,&reader );
    attr->Release();

    if( FAILED( hr ) || !reader ) return 0;

    reader->SetStreamSelection( (DWORD)MF_SOURCE_READER_ALL_STREAMS,FALSE );

    /* se il file non ha un flusso audio questa fallisce ed e' la conferma
       che non c'e' niente da riprodurre, non un errore da segnalare */
    hr = reader->SetStreamSelection( (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,TRUE );
    if( FAILED( hr ) ){ reader->Release(); return 0; }

    IMFMediaType *want = 0;
    if( FAILED( MFCreateMediaType( &want ) ) ){ reader->Release(); return 0; }

    /*  PCM 16 bit, stereo: il formato piu' semplice per AL_FORMAT_STEREO16.
        Media Foundation inserisce da solo il resampler/mixer necessario
        (per l'audio lo fa sempre, non serve un flag ENABLE come per il
        video). Il numero di canali sorgente puo' essere qualunque, anche
        5.1: si chiede comunque stereo. */
    want->SetGUID( MF_MT_MAJOR_TYPE,MFMediaType_Audio );
    want->SetGUID( MF_MT_SUBTYPE,MFAudioFormat_PCM );
    want->SetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE,16 );
    want->SetUINT32( MF_MT_AUDIO_NUM_CHANNELS,2 );

    hr = reader->SetCurrentMediaType( (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,want );
    want->Release();

    if( FAILED( hr ) ){ reader->Release(); return 0; }

    IMFMediaType *cur = 0;
    if( FAILED( reader->GetCurrentMediaType( (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,&cur ) ) || !cur ){
        reader->Release();
        return 0;
    }

    UINT32 ch = 2,rate = 44100;
    cur->GetUINT32( MF_MT_AUDIO_NUM_CHANNELS,&ch );
    cur->GetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND,&rate );
    cur->Release();

    _audioChannels = (int)ch;
    _audioRate = (int)rate;

    return reader;
}


bool BBblVideo::_ReadSize(){

    IMFSourceReader *reader = _OpenReader();
    if( !reader ) return false;

    bool ok = false;

    IMFMediaType *cur = 0;
    if( SUCCEEDED( reader->GetCurrentMediaType( (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,&cur ) ) && cur ){

        UINT32 w = 0,h = 0;
        MFGetAttributeSize( cur,MF_MT_FRAME_SIZE,&w,&h );
        _w = (int)w;
        _h = (int)h;

        /* passo di riga: se e' negativo l'immagine e' memorizzata capovolta */
        UINT32 st = 0;
        if( SUCCEEDED( cur->GetUINT32( MF_MT_DEFAULT_STRIDE,&st ) ) ) _stride = (int)(INT32)st;
        else _stride = 0;

        cur->Release();

        ok = _w>0 && _h>0;
    }

    reader->Release();

    return ok;
}


bool BBblVideo::_Open( String path ){

    _Close();
    _blvStartMF();

    _failed = false;
    _finished = false;
    _hasNew = false;

    /* "cerberus://data/x.mp4" -> percorso reale accanto all'eseguibile */
    _path = _blvWide( BBGame::Game()->PathToFilePath( path ) );

    /* la dimensione serve subito al chiamante per creare la Image */
    if( !_ReadSize() ){
        bbPrint( String( "[blVideo] apertura fallita: " )+path );
        _failed = true;
        return false;
    }

    bbPrint( String( "[blVideo] " )+path+" "+String( _w )+"x"+String( _h ) );

    _running = true;

    DWORD id = 0;
    _thread = CreateThread( 0,0,_blvThreadProc,this,0,&id );

    if( !_thread ){
        _running = false;
        _failed = true;
        return false;
    }

    /*  L'audio e' un extra: se il file non ne ha, o qualcosa nella catena
        PCM fallisce, si continua senza. Non e' _failed, il video sopra
        funziona comunque. */
    _audioReader = _OpenAudioReader();

    if( _audioReader ){

        bbPrint( String( "[blVideo] audio: " )+_audioChannels+"ch "+_audioRate+"Hz" );

        _hasAudio = true;
        _audioRunning = true;

        DWORD aid = 0;
        _audioThread = CreateThread( 0,0,_blvAudioThreadProc,this,0,&aid );

        if( !_audioThread ){
            _audioRunning = false;
            _hasAudio = false;
            _audioReader->Release();
            _audioReader = 0;
        }
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

    if( _thread ){
        WaitForSingleObject( _thread,1000 );
        CloseHandle( _thread );
        _thread = 0;
    }

    if( _audioThread ){
        WaitForSingleObject( _audioThread,1000 );
        CloseHandle( _audioThread );
        _audioThread = 0;
    }

    /*  Il thread audio a questo punto e' gia' finito (join sopra), quindi le
        chiamate AL qui non si accavallano con quelle del thread. */
    if( _alSource ){
        alSourceStop( _alSource );
        alSourcei( _alSource,AL_BUFFER,0 );  /* sgancia le code rimaste */
        alDeleteSources( 1,&_alSource );
        _alSource = 0;
    }

    bool anyBuf = false;
    for( int i=0;i<AL_NBUF;++i ) if( _alBuffers[i] ) anyBuf = true;
    if( anyBuf ){
        alDeleteBuffers( AL_NBUF,_alBuffers );
        for( int i=0;i<AL_NBUF;++i ) _alBuffers[i] = 0;
    }

    if( _audioReader ){
        _audioReader->Release();
        _audioReader = 0;
    }

    _hasAudio = false;

    if( _csReady ){
        EnterCriticalSection( &_cs );
        _rgba.clear();
        _hasNew = false;
        LeaveCriticalSection( &_cs );
    }

    _w = 0;
    _h = 0;
    _stride = 0;
}

bool BBblVideo::_IsPlaying(){ return _playing; }
bool BBblVideo::_IsFinished(){ return _finished; }
bool BBblVideo::_HasFailed(){ return _failed; }
int  BBblVideo::_Width(){ return _w; }
int  BBblVideo::_Height(){ return _h; }


/*  Il thread che disegna fa solo questo: una memcpy. */
bool BBblVideo::_CopyFrame( BBDataBuffer *db ){

    if( !db || !_csReady ) return false;

    bool got = false;

    EnterCriticalSection( &_cs );

    if( _hasNew && !_rgba.empty() ){

        void *dst = db->WritePointer( 0 );
        int n = (int)_rgba.size();

        if( dst && db->Length()>=n ){
            memcpy( dst,&_rgba[0],n );
            _hasNew = false;
            got = true;
        }
    }

    LeaveCriticalSection( &_cs );

    return got;
}


void BBblVideo::_StoreFrame( IMFSample *sample ){

    IMFMediaBuffer *mbuf = 0;
    if( FAILED( sample->ConvertToContiguousBuffer( &mbuf ) ) || !mbuf ) return;

    BYTE *src = 0;
    DWORD maxLen = 0,curLen = 0;

    if( FAILED( mbuf->Lock( &src,&maxLen,&curLen ) ) ){
        mbuf->Release();
        return;
    }

    int w = _w, h = _h;

    if( w>0 && h>0 ){

        /*  Il passo di riga VERO del buffer non e' sempre w*4, e
            MF_MT_DEFAULT_STRIDE (letto in _ReadSize, in _stride) non e'
            sempre affidabile: il Video Processor MFT (attivato da
            MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING per la conversione a
            RGB32) puo' allineare la riga a un multiplo di 16 pixel - i
            blocchi H.264 - anche quando il fotogramma "visibile" non lo e',
            e continua a dichiarare lo stride del fotogramma visibile, non
            quello vero del buffer.

            Scoperto e VERIFICATO scrivendo un fotogramma su disco (non solo
            dedotto) su un video 1468x818 (non multiplo di 16): l'attributo
            dichiarava un passo di 5872 (1468*4) e con quello l'immagine
            usciva a righe storte, sempre peggio scendendo verso il basso -
            il "sballato" di questo bug. Il buffer vero era invece largo
            1472 px per riga (5888 byte, = 1468 arrotondato a 16) e alto
            832 righe (818 arrotondato a 16): con quel passo l'immagine e'
            uscita pulita. Provato anche IMF2DBuffer::Lock2D, che in teoria
            darebbe il passo vero senza bisogno di indovinarlo: su questo
            decoder (Video Processor MFT, buffer di sistema non 2D) non e'
            mai disponibile, quindi non e' una strada percorribile qui.

            La riprova che rende il calcolo affidabile e non un'ipotesi a
            caso: se passo e altezza arrotondati a 16 moltiplicati fra loro
            tornano ESATTI con la lunghezza vera del buffer (curLen), e'
            quello il passo giusto - e' una coincidenza troppo precisa per
            essere un caso. Con 1024x768 (gia' multiplo di 16) l'arrotondato
            e il dichiarato sono lo stesso numero, quindi il calcolo non
            cambia nulla per i video che gia' funzionavano. Se il confronto
            non torna (decoder senza allineamento a blocchi) si ripiega su
            quanto dichiara MF, con un controllo che non si legga comunque
            fuori dal buffer.                                              */
        int w16 = ( w+15 ) & ~15;
        int h16 = ( h+15 ) & ~15;
        long long alignedTotal = (long long)w16*4*h16;

        int pitch;

        if( alignedTotal == (long long)curLen ){

            pitch = w16*4;

        } else {

            int declared = _stride ? ( _stride<0 ? -_stride : _stride ) : w*4;

            pitch = ( (long long)declared*h <= (long long)curLen ) ? declared : w*4;
        }

        if( (long long)pitch*h <= (long long)curLen ){

            size_t need = (size_t)w*h*4;
            if( _work.size()!=need ) _work.resize( need );

            for( int y=0;y<h;++y ){

                /* passo negativo: la prima riga in memoria e' l'ultima a video */
                const unsigned char *srow = _stride<0 ? src+(size_t)( h-1-y )*pitch : src+(size_t)y*pitch;

                const unsigned int *s32 = (const unsigned int*)srow;
                unsigned int *d32 = (unsigned int*)( &_work[0]+(size_t)y*w*4 );

                for( int x=0;x<w;++x ){

                    /*  in memoria RGB32 e' B,G,R,A; letto come parola little
                        endian vale B | G<<8 | R<<16 | A<<24.
                        mojo2 vuole R,G,B,A: bastano tre spostamenti.  */
                    unsigned int v = s32[x];

                    d32[x] = ( ( v>>16 ) & 0x000000FFu ) |
                             (   v       & 0x0000FF00u ) |
                             ( ( v<<16 ) & 0x00FF0000u ) |
                                           0xFF000000u;
                }
            }

            EnterCriticalSection( &_cs );
            _rgba.swap( _work );
            _hasNew = true;
            LeaveCriticalSection( &_cs );
        }
    }

    mbuf->Unlock();
    mbuf->Release();
}


void BBblVideo::_Decode(){

    IMFSourceReader *reader = _OpenReader();

    if( !reader ){
        _failed = true;
        return;
    }

    double startTime = -1;

    while( _running ){

        if( !_playing ){
            Sleep( 15 );
            startTime = -1;      /* alla ripresa il tempo riparte da qui */
            continue;
        }

        DWORD flags = 0;
        LONGLONG ts = 0;
        IMFSample *sample = 0;

        HRESULT hr = reader->ReadSample( (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,0,&flags,&ts,&sample );

        if( FAILED( hr ) ){
            if( sample ) sample->Release();
            _failed = true;
            break;
        }

        if( flags & MF_SOURCE_READERF_ENDOFSTREAM ){

            if( sample ) sample->Release();

            if( _loop ){

                PROPVARIANT var;
                PropVariantInit( &var );
                var.vt = VT_I8;
                var.hVal.QuadPart = 0;
                reader->SetCurrentPosition( GUID_NULL,var );
                PropVariantClear( &var );

                startTime = -1;
                continue;
            }

            _finished = true;
            _playing = false;
            continue;
        }

        if( !sample ) continue;

        /* i tempi di Media Foundation sono in unita' da 100 ns */
        double frameTime = (double)ts/10000000.0;

        double now = _blvNow();
        if( startTime<0 ) startTime = now-frameTime;

        double late = now-( startTime+frameTime );

        /*  Se siamo rimasti molto indietro si RIAGGANCIA l'orologio invece di
            scartare il fotogramma.
            E' il punto che prima bloccava il filmato: l'orologio era ancorato
            al primo fotogramma, e appena il decoder accumulava piu' di 150 ms
            di ritardo ogni fotogramma successivo risultava in ritardo e
            veniva buttato per sempre. Il thread continuava a decodificare a
            vuoto e l'immagine restava congelata.
            Riagganciando, se la macchina non ce la fa il filmato scorre un po'
            piu' lento del dovuto, ma non si ferma mai. */
        if( late>0.25 ){
            startTime = now-frameTime;
            late = 0;
        }

        /*  In anticipo: si aspetta il momento giusto, altrimenti il filmato
            scorrerebbe alla velocita' del decoder invece che alla sua. */
        if( late<0 ){

            double wait = -late;

            while( wait>0 && _running && _playing ){
                DWORD ms = (DWORD)( wait*1000.0 );
                if( ms>50 ) ms = 50;
                Sleep( ms>0 ? ms : 1 );
                wait = ( startTime+frameTime )-_blvNow();
            }
        }

        _StoreFrame( sample );

        sample->Release();
    }

    reader->Release();
}


/*  Legge UN sample audio da _audioReader e lo carica in un buffer OpenAL
    gia' generato. Torna false a fine flusso (senza loop) o su errore: il
    chiamante allora smette di rimettere in coda, non e' un fallimento del
    video. In loop riavvolge da solo, come fa _Decode per il video. */
bool BBblVideo::_FillAudioBuffer( ALuint alBuf ){

    DWORD flags = 0;
    LONGLONG ts = 0;
    IMFSample *sample = 0;

    HRESULT hr = _audioReader->ReadSample( (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,0,&flags,&ts,&sample );

    if( FAILED( hr ) ){
        if( sample ) sample->Release();
        return false;
    }

    if( flags & MF_SOURCE_READERF_ENDOFSTREAM ){

        if( sample ) sample->Release();

        if( !_loop ) return false;

        PROPVARIANT var;
        PropVariantInit( &var );
        var.vt = VT_I8;
        var.hVal.QuadPart = 0;
        _audioReader->SetCurrentPosition( GUID_NULL,var );
        PropVariantClear( &var );

        return _FillAudioBuffer( alBuf );  /* riprova subito sul nuovo giro */
    }

    if( !sample ) return true;  /* nessun campione pronto ora, non e' fine flusso */

    IMFMediaBuffer *mbuf = 0;
    if( FAILED( sample->ConvertToContiguousBuffer( &mbuf ) ) || !mbuf ){
        sample->Release();
        return true;
    }

    BYTE *src = 0;
    DWORD curLen = 0;

    if( SUCCEEDED( mbuf->Lock( &src,0,&curLen ) ) && curLen>0 ){

        ALenum format = _audioChannels>=2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

        alBufferData( alBuf,format,src,(ALsizei)curLen,(ALsizei)_audioRate );

        mbuf->Unlock();

        /* _alSource/_alBuffers li tocca solo questo thread: _Close() fa il
           suo pulizia AL solo DOPO aver aspettato che questo thread finisca,
           quindi qui non serve _cs. */
        alSourceQueueBuffers( _alSource,1,&alBuf );
    }

    mbuf->Release();
    sample->Release();

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

            Sleep( 15 );
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
            invece di restare mute in silenzio per il resto del filmato. */
        ALint state = 0;
        alGetSourcei( _alSource,AL_SOURCE_STATE,&state );
        if( state!=AL_PLAYING ){
            ALint queued = 0;
            alGetSourcei( _alSource,AL_BUFFERS_QUEUED,&queued );
            if( queued>0 ) alSourcePlay( _alSource );
            else if( streamDone ) break;  /* finito davvero, niente da riprodurre */
        }

        Sleep( 10 );
    }
}

#endif
