
//  Cartella per le impostazioni dell'app (Roaming AppData), SENZA barra
//  finale: es. "C:\Users\nome\AppData\Roaming". Sta qui e non in
//  brl.process perche' quel modulo non e' implementato su Android ("Native
//  Process class not implemented") e perche' boshlibs non deve dipendere
//  dai moduli dell'installazione: tutto il codice sta nel modulo.
//
//  SHGetFolderPathW (non la piu' recente SHGetKnownFolderPath) apposta:
//  quella vuole _WIN32_WINNT >= 0x0600, che MinGW non alza di default, e
//  fallisce con "was not declared in this scope". Questa e' quella usata
//  gia' con successo altrove nel modulo con le stesse toolchain.
#include <shlobj.h>

String _AppPathDataX(){

    WCHAR path[MAX_PATH];

    if( FAILED( SHGetFolderPathW( NULL,CSIDL_APPDATA,NULL,0,path ) ) ) return String();

    return String( path );
}
