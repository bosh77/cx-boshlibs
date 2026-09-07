
//----------------------  REQUESTERS - WINDOWS  ----------------------
//
// Dialoghi nativi Win32, senza dipendere da brl.requesters ne' da
// tinyfiledialogs: il modulo deve poter stare in piedi da solo.
//
// La logica (soprattutto la costruzione del filtro dei file e il callback
// di SHBrowseForFolder) segue quella collaudata di
// modules/brl/native/requesters.cpp dell'installazione Cerberus X.
//
// Gli include stanno qui e non in boshlib.cpp: cosi' blRequesters funziona
// anche in un'app che non importa blFonts. Sono tutti con header guard,
// quindi includerli due volte non da' fastidio.

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>


// La finestra che aveva il fuoco prima del dialogo: senza rimetterlo a posto
// la finestra GL resta "spenta" alla chiusura del requester.
static HWND blreq_focHwnd;

static void blreq_begin() {

    blreq_focHwnd = GetFocus();

}

static void blreq_end() {

    SetFocus(blreq_focHwnd);

}


static int blreq_panel(String title, String text, int flags) {

    blreq_begin();

    int n = MessageBoxW(GetActiveWindow(), text.ToCString<WCHAR>(), title.ToCString<WCHAR>(), flags);

    blreq_end();

    return n;

}


// Copia una String Cerberus in un buffer WCHAR terminato da zero. Serve
// perche' OPENFILENAMEW e BROWSEINFOW tengono i puntatori per tutta la
// durata della chiamata: va liberato dopo, non prima.
static WCHAR *blreq_tmpW(String str) {

    WCHAR *p = (WCHAR*)malloc(str.Length() * 2 + 2);

    memcpy(p, str.Data(), str.Length() * 2);

    p[str.Length()] = 0;

    return p;

}


void _blReqNotify(String title, String text, bool serious) {

    int flags = (serious ? MB_ICONWARNING : MB_ICONINFORMATION) | MB_OK | MB_APPLMODAL | MB_TOPMOST;

    blreq_panel(title, text, flags);

}


// 1 = OK, 0 = Cancel
int _blReqConfirm(String title, String text, bool serious) {

    int flags = (serious ? MB_ICONWARNING : MB_ICONINFORMATION) | MB_OKCANCEL | MB_APPLMODAL | MB_TOPMOST;

    if (blreq_panel(title, text, flags) == IDOK) return 1;

    return 0;

}


// 1 = Yes, 0 = No, -1 = Cancel
int _blReqProceed(String title, String text, bool serious) {

    int flags = (serious ? MB_ICONWARNING : MB_ICONINFORMATION) | MB_YESNOCANCEL | MB_APPLMODAL | MB_TOPMOST;

    int n = blreq_panel(title, text, flags);

    if (n == IDYES) return 1;
    if (n == IDNO) return 0;

    return -1;

}


// Filtro nel formato del modulo: "CX File(*.cxs):cxs;All Files(*.*):*"
// Windows lo vuole come coppie descrizione\0pattern\0 chiuse da \0, quindi
// ':' diventa "\0*." , ';' diventa "\0" e ',' diventa ";*." (piu' estensioni
// nella stessa voce). Senza ':' si accetta anche la forma breve "cxs".
String _blReqFile(String title, String exts, bool save, String path) {

    String file, dir;

    path = path.Replace("/", "\\");

    int i = path.FindLast("\\");

    if (i != -1) {
        dir = path.Slice(0, i);
        file = path.Slice(i + 1);
    } else {
        file = path;
    }

    if (file.Length() > MAX_PATH) return String();

    if (exts.Length()) {

        if (exts.Find(":") == -1) {
            exts = String("Files\0*.") + exts;
        } else {
            exts = exts.Replace(":", String("\0*.", 3));
        }

        exts = exts.Replace(";", String("\0", 1));
        exts = exts.Replace(",", ";*.") + String("\0", 1);

    }

    WCHAR buf[MAX_PATH + 1];

    memcpy(buf, file.Data(), file.Length() * 2);

    buf[file.Length()] = 0;

    OPENFILENAMEW of = { sizeof(of) };

    of.hwndOwner = GetActiveWindow();
    of.lpstrTitle = blreq_tmpW(title);
    of.lpstrFilter = blreq_tmpW(exts);
    of.lpstrFile = buf;
    of.lpstrInitialDir = dir.Length() ? blreq_tmpW(dir) : 0;
    of.nMaxFile = MAX_PATH;
    of.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    blreq_begin();

    String str;

    if (save) {

        of.lpstrDefExt = L"";
        of.Flags |= OFN_OVERWRITEPROMPT;

        if (GetSaveFileNameW(&of)) str = String(buf);

    } else {

        of.Flags |= OFN_FILEMUSTEXIST;

        if (GetOpenFileNameW(&of)) str = String(buf);

    }

    blreq_end();

    free((void*)of.lpstrTitle);
    free((void*)of.lpstrFilter);
    free((void*)of.lpstrInitialDir);

    return str;

}


// Parte gia' posizionato sulla cartella passata e mostra il path corrente
// nella barra di stato mentre si naviga.
static int CALLBACK blreq_browseCB(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData) {

    wchar_t szPath[MAX_PATH];

    switch (uMsg) {

    case BFFM_INITIALIZED:
        SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, pData);
        break;

    case BFFM_SELCHANGED:
        if (SHGetPathFromIDListW((LPITEMIDLIST)lp, szPath)) {
            SendMessageW(hwnd, BFFM_SETSTATUSTEXTW, 0, (LPARAM)szPath);
        }
        break;

    }

    return 0;

}


String _blReqDir(String title, String dir) {

    dir = dir.Replace("/", "\\");

    BROWSEINFOW bi = { 0 };

    WCHAR buf[MAX_PATH], *p;

    GetFullPathNameW(dir.ToCString<WCHAR>(), MAX_PATH, buf, &p);

    bi.hwndOwner = GetActiveWindow();
    bi.lpszTitle = blreq_tmpW(title);
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = blreq_browseCB;
    bi.lParam = (LPARAM)buf;

    blreq_begin();

    String str;

    ITEMIDLIST *idlist = SHBrowseForFolderW(&bi);

    if (idlist) {

        SHGetPathFromIDListW(idlist, buf);

        str = String(buf);

    }

    blreq_end();

    free((void*)bi.lpszTitle);

    return str;

}
