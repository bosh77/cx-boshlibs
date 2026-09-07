
//----------------------  REQUESTERS - macOS  ----------------------
//
// Cocoa nativo. Su macOS il target glfw3 NON compila tinyfiledialogs (sta
// solo nei Makefile gcc_winnt e gcc_linux), e comunque su mac ripiegherebbe
// su osascript, che dalle versioni recenti chiede il permesso di
// automazione: i pannelli Cocoa sono la strada giusta.
//
// E' un file .mm (Objective-C++), come i macf1.mm / fontlist-mac.mm gia'
// usati negli altri moduli.
//
// Rispetto al vecchio codice di brl.requesters qui si usano le API moderne
// (NSAlert, setDirectoryURL:, runModal) invece di NSRunAlertPanel e
// runModalForDirectory:file:types:, che sono deprecate da parecchie versioni.

#import <Cocoa/Cocoa.h>


// La finestra che aveva il fuoco prima del pannello.
static NSWindow *blreq_keyWin;

static void blreq_begin() {

    blreq_keyWin = [NSApp keyWindow];

    if (!blreq_keyWin) [NSApp activateIgnoringOtherApps:YES];

}

static void blreq_end() {

    if (blreq_keyWin) [blreq_keyWin makeKeyWindow];

}


// Pannello con un massimo di tre pulsanti. Ritorna l'indice del pulsante
// premuto (0 = il primo, quello di destra e predefinito).
static int blreq_alert(String title, String text, bool serious, NSString *b0, NSString *b1, NSString *b2) {

    blreq_begin();

    NSAlert *alert = [[NSAlert alloc] init];

    [alert setMessageText:title.ToNSString()];
    [alert setInformativeText:text.ToNSString()];
    [alert setAlertStyle:(serious ? NSAlertStyleCritical : NSAlertStyleInformational)];

    if (b0) [alert addButtonWithTitle:b0];
    if (b1) [alert addButtonWithTitle:b1];
    if (b2) [alert addButtonWithTitle:b2];

    NSModalResponse n = [alert runModal];

    [alert release];

    blreq_end();

    return (int)(n - NSAlertFirstButtonReturn);

}


void _blReqNotify(String title, String text, bool serious) {

    blreq_alert(title, text, serious, @"OK", 0, 0);

}


// 1 = OK, 0 = Cancel
int _blReqConfirm(String title, String text, bool serious) {

    if (blreq_alert(title, text, serious, @"OK", @"Cancel", 0) == 0) return 1;

    return 0;

}


// 1 = Yes, 0 = No, -1 = Cancel
int _blReqProceed(String title, String text, bool serious) {

    int n = blreq_alert(title, text, serious, @"Yes", @"No", @"Cancel");

    if (n == 0) return 1;
    if (n == 1) return 0;

    return -1;

}


// Dal filtro "Descrizione(*.a *.b):a,b;Altra(*.c):c" ricava l'elenco delle
// estensioni. Se compare "*" torna nil: vuol dire "tutti i file".
static NSMutableArray *blreq_exts(String filter) {

    if (!filter.Length()) return 0;

    NSMutableArray *arr = [NSMutableArray arrayWithCapacity:10];

    int i0 = 0;

    while (i0 < filter.Length()) {

        int i1 = filter.Find(":", i0) + 1;

        if (!i1) break;

        int i2 = filter.Find(";", i1);

        if (i2 == -1) i2 = filter.Length();

        while (i1 < i2) {

            int i3 = filter.Find(",", i1);

            if (i3 == -1 || i3 > i2) i3 = i2;

            String ext = filter.Slice(i1, i3);

            if (ext == "*") return 0;    // niente filtro

            if (ext.Length()) [arr addObject:ext.ToNSString()];

            i1 = i3 + 1;

        }

        i0 = i2 + 1;

    }

    if ([arr count] == 0) return 0;

    return arr;

}


String _blReqFile(String title, String filter, bool save, String path) {

    String file, dir;

    int i = path.FindLast("/");

    if (i != -1) {
        dir = path.Slice(0, i);
        file = path.Slice(i + 1);
    } else {
        file = path;
    }

    NSMutableArray *nsexts = blreq_exts(filter);

    blreq_begin();

    String str;

    if (save) {

        NSSavePanel *panel = [NSSavePanel savePanel];

        if (title.Length()) [panel setTitle:title.ToNSString()];
        if (dir.Length()) [panel setDirectoryURL:[NSURL fileURLWithPath:dir.ToNSString() isDirectory:YES]];
        if (file.Length()) [panel setNameFieldStringValue:file.ToNSString()];

        if (nsexts) {
            [panel setAllowedFileTypes:nsexts];
            [panel setAllowsOtherFileTypes:NO];
        }

        if ([panel runModal] == NSModalResponseOK) str = String([[panel URL] path]);

    } else {

        NSOpenPanel *panel = [NSOpenPanel openPanel];

        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];

        if (title.Length()) [panel setTitle:title.ToNSString()];
        if (dir.Length()) [panel setDirectoryURL:[NSURL fileURLWithPath:dir.ToNSString() isDirectory:YES]];
        if (file.Length()) [panel setNameFieldStringValue:file.ToNSString()];

        if (nsexts) [panel setAllowedFileTypes:nsexts];

        if ([panel runModal] == NSModalResponseOK) str = String([[panel URL] path]);

    }

    blreq_end();

    return str;

}


String _blReqDir(String title, String dir) {

    NSOpenPanel *panel = [NSOpenPanel openPanel];

    [panel setCanChooseFiles:NO];
    [panel setCanChooseDirectories:YES];
    [panel setCanCreateDirectories:YES];
    [panel setAllowsMultipleSelection:NO];

    if (title.Length()) [panel setTitle:title.ToNSString()];
    if (dir.Length()) [panel setDirectoryURL:[NSURL fileURLWithPath:dir.ToNSString() isDirectory:YES]];

    blreq_begin();

    String str;

    if ([panel runModal] == NSModalResponseOK) str = String([[panel URL] path]);

    blreq_end();

    return str;

}
