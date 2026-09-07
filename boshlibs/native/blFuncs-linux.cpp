
//  Cartella per le impostazioni dell'app: la home dell'utente, SENZA barra
//  finale (su Linux non esiste un vero equivalente di "Roaming AppData":
//  tocca all'app aggiungere il proprio "/.nomeapp" o "/.config/nomeapp",
//  come gia' fa CerbIDE con SettingsPath + "/.CerbIDE"). Vedi la nota nella
//  versione Windows: sta qui per non dipendere da brl.process.
#include <pwd.h>
#include <unistd.h>

String _AppPathDataX(){

    struct passwd *pw = getpwuid( getuid() );

    if( !pw || !pw->pw_dir ) return String();

    return String( pw->pw_dir );
}
