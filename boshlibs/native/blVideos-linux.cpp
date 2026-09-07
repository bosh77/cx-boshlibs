//--------------------------------------------------------------------------
//  Riproduzione di un .mp4 dentro una Image di mojo2, su Linux.
//
//  A differenza di Windows (Media Foundation) e macOS (AVFoundation) qui non
//  c'e' un decoder di sistema pronto: si usa FFmpeg (libavformat/libavcodec
//  per demux+decodifica, libswscale per convertire il fotogramma in RGBA,
//  libswresample per l'audio) - libreria di sistema separata, va installata
//  (vedi PORT-MAC-LINUX.md). L'audio esce su OpenAL, come su Windows.
//
//  Struttura identica alle altre due versioni desktop: un thread di sfondo
//  decodifica il video e tiene pronto UN fotogramma gia' convertito in RGBA
//  (doppio buffer a scambio di puntatori: _rgba/_work), il thread che
//  disegna si limita a una memcpy (_CopyFrame). A differenza della versione
//  macOS (che deve RIAPRIRE il reader a ogni loop, perche' AVAssetReader non
//  si riavvolge) qui il decoder si apre una volta sola in _Open e il loop si
//  fa con una vera seek (av_seek_frame + avcodec_flush_buffers): e' la
//  strada normale con FFmpeg ed e' piu' leggera.
//
//  AUDIO: un secondo AVFormatContext/AVCodecContext indipendente, aperto
//  sullo stesso file ma selezionato solo sulla traccia audio - stessa idea
//  dei due IMFSourceReader indipendenti della versione Windows: usare due
//  contesti separati invece di condividerne uno solo fra i due thread evita
//  qualunque rischio di accesso concorrente alla stessa struttura FFmpeg (un
//  AVFormatContext non e' pensato per essere letto da due thread insieme).
//  swresample converte a PCM 16 bit stereo (qualunque sia il layout/formato
//  sorgente), un secondo thread di sfondo tiene in coda 4 buffer OpenAL e li
//  rimpiazza via via che vengono consumati (streaming a coda di buffer,
//  stesso schema di Windows/Android). Il contesto OpenAL e' quello che
//  mojo.audio apre gia' all'avvio dell'app (gxtkAudio, in mojo.glfw.cpp): e'
//  globale fra i thread in OpenAL (a differenza di un contesto OpenGL),
//  quindi le chiamate AL da questo thread di sfondo sono sicure senza
//  doverne aprire uno nostro.
//
//  Non e' sincronizzazione fine (lip-sync da regia): video e audio partono
//  insieme da _Play() e ripartono insieme in loop, ma ognuno scorre col
//  proprio passo. Per un filmato dentro una Image basta.
//--------------------------------------------------------------------------

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}

#include <AL/al.h>
#include <AL/alc.h>

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

    bool _OpenVideo();
    bool _OpenAudio();
    void _StoreFrame( AVFrame *frame );
    bool _FillAudioBuffer( ALuint alBuf );

    std::string _path;

    std::thread _thread;
    std::thread _audioThread;
    std::mutex _mtx;

    volatile bool _running;
    volatile bool _audioRunning;
    volatile bool _playing;
    volatile bool _loop;
    volatile bool _finished;
    volatile bool _failed;

    volatile int _w, _h;

    /*  Due buffer che si scambiano: _work e' quello che il decoder riempie,
        _rgba quello pronto per chi disegna (stesso schema delle versioni
        Windows/macOS: dopo il primo fotogramma non si alloca piu' niente). */
    std::vector<unsigned char> _rgba;
    std::vector<unsigned char> _work;
    bool _hasNew;

    AVFormatContext *_vfmt;
    AVCodecContext *_vdec;
    int _vStreamIdx;
    SwsContext *_vsws;

    /*  Audio: contesto e thread a parte, indipendenti dal video. Se il file
        non ha una traccia audio (o qualcosa nella catena fallisce) si
        prosegue senza: non e' _failed, il video sopra funziona comunque. */
    static const int AL_NBUF = 4;

    volatile bool _hasAudio;
    int _audioChannels;
    int _audioRate;

    AVFormatContext *_afmt;
    AVCodecContext *_adec;
    int _aStreamIdx;
    SwrContext *_aswr;

    ALuint _alSource;
    ALuint _alBuffers[AL_NBUF];

    std::vector<unsigned char> _audioScratch;
};


BBblVideo::BBblVideo():
    _running( false ),_audioRunning( false ),_playing( false ),_loop( false ),_finished( false ),_failed( false ),
    _w( 0 ),_h( 0 ),_hasNew( false ),
    _vfmt( 0 ),_vdec( 0 ),_vStreamIdx( -1 ),_vsws( 0 ),
    _hasAudio( false ),_audioChannels( 0 ),_audioRate( 0 ),
    _afmt( 0 ),_adec( 0 ),_aStreamIdx( -1 ),_aswr( 0 ),
    _alSource( 0 ){

    for( int i=0;i<AL_NBUF;++i ) _alBuffers[i] = 0;
}

BBblVideo::~BBblVideo(){

    _Close();
}


bool BBblVideo::_OpenVideo(){

    if( avformat_open_input( &_vfmt,_path.c_str(),0,0 )<0 ) return false;

    if( avformat_find_stream_info( _vfmt,0 )<0 ){
        avformat_close_input( &_vfmt );
        return false;
    }

    const AVCodec *codec = 0;
    _vStreamIdx = av_find_best_stream( _vfmt,AVMEDIA_TYPE_VIDEO,-1,-1,&codec,0 );

    if( _vStreamIdx<0 || !codec ){
        avformat_close_input( &_vfmt );
        return false;
    }

    _vdec = avcodec_alloc_context3( codec );
    if( !_vdec ){
        avformat_close_input( &_vfmt );
        return false;
    }

    avcodec_parameters_to_context( _vdec,_vfmt->streams[_vStreamIdx]->codecpar );

    if( avcodec_open2( _vdec,codec,0 )<0 ){
        avcodec_free_context( &_vdec );
        avformat_close_input( &_vfmt );
        return false;
    }

    _vsws = sws_getContext( _vdec->width,_vdec->height,_vdec->pix_fmt,
                             _vdec->width,_vdec->height,AV_PIX_FMT_RGBA,
                             SWS_BILINEAR,0,0,0 );

    if( !_vsws ){
        avcodec_free_context( &_vdec );
        avformat_close_input( &_vfmt );
        return false;
    }

    return true;
}


/*  Torna false se il file non ha una traccia audio, o se qualcosa nella
    catena di apertura fallisce: il chiamante allora continua senza, non e'
    un errore da segnalare (stessa filosofia di _OpenAudioReader su
    Windows). */
bool BBblVideo::_OpenAudio(){

    if( avformat_open_input( &_afmt,_path.c_str(),0,0 )<0 ) return false;

    if( avformat_find_stream_info( _afmt,0 )<0 ){
        avformat_close_input( &_afmt );
        return false;
    }

    const AVCodec *codec = 0;
    _aStreamIdx = av_find_best_stream( _afmt,AVMEDIA_TYPE_AUDIO,-1,-1,&codec,0 );

    if( _aStreamIdx<0 || !codec ){
        avformat_close_input( &_afmt );
        _aStreamIdx = -1;
        return false;
    }

    _adec = avcodec_alloc_context3( codec );
    if( !_adec ){
        avformat_close_input( &_afmt );
        _aStreamIdx = -1;
        return false;
    }

    avcodec_parameters_to_context( _adec,_afmt->streams[_aStreamIdx]->codecpar );

    if( avcodec_open2( _adec,codec,0 )<0 ){
        avcodec_free_context( &_adec );
        avformat_close_input( &_afmt );
        _aStreamIdx = -1;
        return false;
    }

    /*  PCM 16 bit stereo: il formato piu' semplice per AL_FORMAT_STEREO16.
        Il layout/formato/frequenza sorgente puo' essere qualunque (anche
        5.1): swresample costruisce da solo la conversione, si tiene invece
        la frequenza originale (nessun resampling di frequenza necessario,
        OpenAL accetta qualunque frequenza in alBufferData). */
    AVChannelLayout outLayout;
    av_channel_layout_from_mask( &outLayout,AV_CH_LAYOUT_STEREO );

    int rc = swr_alloc_set_opts2( &_aswr,
                                   &outLayout,AV_SAMPLE_FMT_S16,_adec->sample_rate,
                                   &_adec->ch_layout,_adec->sample_fmt,_adec->sample_rate,
                                   0,0 );

    av_channel_layout_uninit( &outLayout );

    if( rc<0 || !_aswr || swr_init( _aswr )<0 ){
        if( _aswr ) swr_free( &_aswr );
        avcodec_free_context( &_adec );
        avformat_close_input( &_afmt );
        _aStreamIdx = -1;
        return false;
    }

    _audioChannels = 2;
    _audioRate = _adec->sample_rate;

    return true;
}


bool BBblVideo::_Open( String path ){

    _Close();

    _failed = false;
    _finished = false;
    _hasNew = false;

    /* "cerberus://data/x.mp4" -> percorso reale accanto all'eseguibile */
    _path = convertBBString( BBGame::Game()->PathToFilePath( path ) );

    if( !_OpenVideo() ){
        bbPrint( String( "[blVideo] apertura fallita: " )+path );
        _failed = true;
        return false;
    }

    _w = _vdec->width;
    _h = _vdec->height;

    bbPrint( String( "[blVideo] " )+path+" "+String( _w )+"x"+String( _h ) );

    _running = true;
    _thread = std::thread( &BBblVideo::_Decode,this );

    if( _OpenAudio() ){

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

    /*  I thread sono gia' finiti (join sopra): le chiamate AL/FFmpeg qui non
        si accavallano con quelle dei thread di sfondo. */
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

    if( _vsws ){ sws_freeContext( _vsws ); _vsws = 0; }
    if( _vdec ) avcodec_free_context( &_vdec );
    if( _vfmt ) avformat_close_input( &_vfmt );
    _vStreamIdx = -1;

    if( _aswr ) swr_free( &_aswr );
    if( _adec ) avcodec_free_context( &_adec );
    if( _afmt ) avformat_close_input( &_afmt );
    _aStreamIdx = -1;
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


void BBblVideo::_StoreFrame( AVFrame *frame ){

    int w = _w, h = _h;
    if( w<1 || h<1 ) return;

    size_t need = (size_t)w*h*4;
    if( _work.size()!=need ) _work.resize( need );

    uint8_t *dst[4] = { &_work[0],0,0,0 };
    int dstStride[4] = { w*4,0,0,0 };

    sws_scale( _vsws,frame->data,frame->linesize,0,h,dst,dstStride );

    /*  swscale non garantisce sempre alfa=255 quando il formato sorgente
        (YUV) non ha un canale alfa proprio: un fotogramma video e' pero'
        sempre opaco, e mojo2 vuole l'alfa premoltiplicato - un pixel con
        alfa a 0 sparirebbe (vedi CLAUDE.md). Stessa cautela gia' presa a
        mano nelle versioni Windows/macOS (li' scambiando i canali BGRA a
        mano forzavano 0xFF; qui la conversione la fa swscale, ma l'alfa si
        forza comunque). */
    for( size_t i=3;i<need;i+=4 ) _work[i] = 0xFF;

    std::lock_guard<std::mutex> lk( _mtx );
    _rgba.swap( _work );
    _hasNew = true;
}


void BBblVideo::_Decode(){

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    double startTime = -1;

    while( _running ){

        if( !_playing ){
            std::this_thread::sleep_for( std::chrono::milliseconds( 15 ) );
            startTime = -1;      /* alla ripresa il tempo riparte da qui */
            continue;
        }

        int rc = av_read_frame( _vfmt,pkt );

        if( rc<0 ){

            /* fine flusso (o errore, indistinguibile qui): a differenza di
               AVAssetReader (macOS) un AVFormatContext si puo' riavvolgere
               con una vera seek, senza doverlo riaprire */
            av_packet_unref( pkt );

            if( _loop ){
                av_seek_frame( _vfmt,_vStreamIdx,0,AVSEEK_FLAG_BACKWARD );
                avcodec_flush_buffers( _vdec );
                startTime = -1;
                continue;
            }

            _finished = true;
            _playing = false;
            continue;
        }

        if( pkt->stream_index != _vStreamIdx ){
            av_packet_unref( pkt );
            continue;
        }

        if( avcodec_send_packet( _vdec,pkt )<0 ){
            av_packet_unref( pkt );
            continue;
        }

        av_packet_unref( pkt );

        while( avcodec_receive_frame( _vdec,frame )==0 ){

            double frameTime = ( frame->pts==AV_NOPTS_VALUE ) ? 0.0 :
                (double)frame->pts * av_q2d( _vfmt->streams[_vStreamIdx]->time_base );

            double now = _blvNow();
            if( startTime<0 ) startTime = now-frameTime;

            double late = now-( startTime+frameTime );

            /*  Se siamo rimasti molto indietro si RIAGGANCIA l'orologio
                invece di scartare il fotogramma (stesso motivo delle altre
                due versioni: senza, un ritardo temporaneo blocca il filmato
                per sempre). */
            if( late>0.25 ){
                startTime = now-frameTime;
                late = 0;
            }

            if( late<0 ){

                double wait = -late;

                while( wait>0 && _running && _playing ){
                    int ms = (int)( wait*1000.0 );
                    if( ms>50 ) ms = 50;
                    std::this_thread::sleep_for( std::chrono::milliseconds( ms>0 ? ms : 1 ) );
                    wait = ( startTime+frameTime )-_blvNow();
                }
            }

            _StoreFrame( frame );

            av_frame_unref( frame );
        }
    }

    av_frame_free( &frame );
    av_packet_free( &pkt );
}


/*  Decodifica audio finche' lo scratch non arriva a una dimensione
    ragionevole per un buffer OpenAL (un singolo frame FFmpeg e' in genere
    piccolo, ~1024 campioni: accodarne uno per volta consumerebbe la coda
    troppo in fretta). Torna false solo se il flusso e' finito DAVVERO e non
    c'e' loop: il chiamante allora smette di rimettere in coda, non e' un
    fallimento del video. In loop riavvolge da solo con una seek, come fa
    _Decode per il video. Il contatore guard evita un giro infinito nel caso
    limite di un file che continua a fallire la lettura anche dopo la seek
    (file audio corrotto). */
bool BBblVideo::_FillAudioBuffer( ALuint alBuf ){

    const size_t TARGET = 32768;  /* byte, PCM16 stereo: qualche centinaio di ms */

    _audioScratch.clear();

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    bool any = false;
    int guard = 0;

    while( _audioScratch.size()<TARGET && guard<64 ){

        int rc = av_read_frame( _afmt,pkt );

        if( rc<0 ){

            av_packet_unref( pkt );
            ++guard;

            if( !_loop ) break;

            av_seek_frame( _afmt,_aStreamIdx,0,AVSEEK_FLAG_BACKWARD );
            avcodec_flush_buffers( _adec );
            continue;
        }

        if( pkt->stream_index != _aStreamIdx ){
            av_packet_unref( pkt );
            continue;
        }

        if( avcodec_send_packet( _adec,pkt )<0 ){
            av_packet_unref( pkt );
            continue;
        }

        av_packet_unref( pkt );

        while( avcodec_receive_frame( _adec,frame )==0 ){

            int outSamples = swr_get_out_samples( _aswr,frame->nb_samples );

            if( outSamples<0 ){
                av_frame_unref( frame );
                continue;
            }

            size_t before = _audioScratch.size();
            _audioScratch.resize( before+(size_t)outSamples*4 );  /* S16 stereo = 4 byte/campione */

            uint8_t *outPtr = &_audioScratch[before];

            int got = swr_convert( _aswr,&outPtr,outSamples,(const uint8_t**)frame->data,frame->nb_samples );

            av_frame_unref( frame );

            if( got<0 ){
                _audioScratch.resize( before );
                continue;
            }

            _audioScratch.resize( before+(size_t)got*4 );

            any = true;
        }
    }

    av_frame_free( &frame );
    av_packet_free( &pkt );

    if( !any || _audioScratch.empty() ) return false;

    ALenum format = AL_FORMAT_STEREO16;

    alBufferData( alBuf,format,&_audioScratch[0],(ALsizei)_audioScratch.size(),(ALsizei)_audioRate );

    /* _alSource lo tocca solo questo thread mentre gira: _Close() fa la sua
       pulizia AL solo DOPO aver aspettato che questo thread finisca */
    alSourceQueueBuffers( _alSource,1,&alBuf );

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
            invece di restare mute in silenzio per il resto del filmato. */
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
