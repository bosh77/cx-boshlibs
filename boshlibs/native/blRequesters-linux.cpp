
//----------------------  REQUESTERS - LINUX  ----------------------
//
// Dialoghi tramite tinyfiledialogs, che il target glfw3 compila gia' dentro
// ogni build Linux (vedi targets/glfw3/template/gcc_linux/Makefile), quindi
// non aggiunge nessuna dipendenza nuova: niente GTK, niente pkg-config.
// Qui i prototipi si dichiarano da soli invece di passare per brl.requesters.
//
// tinyfiledialogs a sua volta si appoggia a zenity / kdialog / matedialog se
// li trova, altrimenti ripiega su una finestra in console.

#include <string.h>

#include <tinyfiledialogs.h>


static String::CString<char> blreq_cstr(const String &t) {

    return t.ToCString<char>();

}


void _blReqNotify(String title, String text, bool serious) {

    tinyfd_messageBox(blreq_cstr(title), blreq_cstr(text), "ok", serious ? "error" : "info", 1);

}


// 1 = OK, 0 = Cancel
int _blReqConfirm(String title, String text, bool serious) {

    return tinyfd_messageBox(blreq_cstr(title), blreq_cstr(text), "okcancel", serious ? "error" : "info", 1);

}


// 1 = Yes, 0 = No, -1 = Cancel
//
// tinyfd con "yesnocancel" torna 0 = cancel, 1 = yes, 2 = no: la conversione
// si fa qui, una volta sola, cosi' il Cerberus non ha bisogno di rami per
// piattaforma e la semantica e' identica a Windows e macOS.
int _blReqProceed(String title, String text, bool serious) {

    int r = tinyfd_messageBox(blreq_cstr(title), blreq_cstr(text), "yesnocancel", serious ? "error" : "info", 1);

    if (r == 1) return 1;
    if (r == 2) return 0;

    return -1;

}


//---------------------  filtro dei file  ---------------------
//
// Il modulo passa il filtro come  "Descrizione(*.a *.b):a,b;Altra(*.c):c"
// mentre tinyfd vuole un elenco di pattern ("*.a","*.b") piu' una sola
// descrizione. Se fra le estensioni compare "*" il filtro si spegne del
// tutto (mostra tutti i file).

#define BLREQ_MAXFILTERS 32
#define BLREQ_FILTERLEN  64


// "cxs" -> "*.cxs"   "*.cxs" -> "*.cxs"   "*" -> 0, niente filtro
static int blreq_pattern(char *dst, const char *src, int len) {

    if (len == 1 && src[0] == '*') return 0;

    int n = 0;

    if (src[0] != '*') { dst[n++] = '*'; dst[n++] = '.'; }

    for (int i = 0; i < len && n < BLREQ_FILTERLEN - 1; ++i) dst[n++] = src[i];

    dst[n] = 0;

    return 1;

}


// Ritorna quanti pattern ha trovato; 0 significa "tutti i file".
static int blreq_buildFilter(String exts, char pats[BLREQ_MAXFILTERS][BLREQ_FILTERLEN], char const **pptr, char *desc, int descsize) {

    desc[0] = 0;

    if (!exts.Length()) return 0;

    char raw[1024];

    exts.ToCString<char>(raw, sizeof(raw));

    int len = 0; while (raw[len]) ++len;

    int count = 0, i0 = 0;

    while (i0 < len && count < BLREQ_MAXFILTERS) {

        int i2 = i0; while (i2 < len && raw[i2] != ';') ++i2;   // fine del gruppo

        int i1 = i0; while (i1 < i2 && raw[i1] != ':') ++i1;    // ':' del gruppo

        if (i1 < i2) {

            if (!desc[0]) {

                int n = 0;

                for (int i = i0; i < i1 && raw[i] != '(' && n < descsize - 1; ++i) desc[n++] = raw[i];

                while (n > 0 && desc[n - 1] == ' ') --n;

                desc[n] = 0;

            }

            ++i1;

        } else {

            i1 = i0;    // gruppo senza descrizione

        }

        while (i1 < i2 && count < BLREQ_MAXFILTERS) {

            int i3 = i1; while (i3 < i2 && raw[i3] != ',' && raw[i3] != ' ') ++i3;

            if (i3 > i1) {

                if (!blreq_pattern(pats[count], raw + i1, i3 - i1)) return 0;

                ++count;

            }

            i1 = i3 + 1;

        }

        i0 = i2 + 1;

    }

    if (!desc[0]) strcpy(desc, "Files");

    for (int i = 0; i < count; ++i) pptr[i] = pats[i];

    return count;

}


String _blReqFile(String title, String exts, bool save, String path) {

    if (path == "") path = ".";

    char pats[BLREQ_MAXFILTERS][BLREQ_FILTERLEN];
    char const *ptrs[BLREQ_MAXFILTERS];
    char desc[128];

    int num = blreq_buildFilter(exts, pats, ptrs, desc, sizeof(desc));

    char const * const *patterns = num ? ptrs : 0;
    char const *description = num ? desc : 0;

    char const *ps;

    if (save) {
        ps = tinyfd_saveFileDialog(blreq_cstr(title), blreq_cstr(path), num, patterns, description);
    } else {
        ps = tinyfd_openFileDialog(blreq_cstr(title), blreq_cstr(path), num, patterns, description, 0);
    }

    if (!ps) return String();

    return String(ps);

}


String _blReqDir(String title, String dir) {

    if (dir == "") dir = ".";

    char const *ps = tinyfd_selectFolderDialog(blreq_cstr(title), blreq_cstr(dir));

    if (!ps) return String();

    return String(ps);

}
