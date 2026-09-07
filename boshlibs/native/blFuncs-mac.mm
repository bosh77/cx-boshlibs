
//  Cartella per le impostazioni dell'app (~/Library/Application Support),
//  SENZA barra finale. Vedi la nota nella versione Windows: sta qui per non
//  dipendere da brl.process.
#import <Cocoa/Cocoa.h>

String _AppPathDataX(){

    NSArray *paths = NSSearchPathForDirectoriesInDomains( NSApplicationSupportDirectory,NSUserDomainMask,YES );

    NSString *dir = [paths firstObject];

    if( !dir ) return String();

    return String( [dir UTF8String] );
}
