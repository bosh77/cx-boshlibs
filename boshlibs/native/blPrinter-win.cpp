

//----------------------  PRINTER  ----------------------

HDC printerDC = NULL;
HDC savedPrinterDC = NULL;

HGLOBAL hDevMode = NULL;
HGLOBAL hDevNames = NULL;

String _printername;
String _orientation;
int _copies;
String _coloribn;
int _papersize;

float dpiX;
float dpiY;

float marginLeft;
float marginTop; 

static HWND getDialogOwnerHwnd() {
    HWND hwnd = GetForegroundWindow();
    if (hwnd) return hwnd;
    return GetDesktopWindow();
}

static std::wstring getDefaultPrinterName() {
    DWORD size = 0;
    GetDefaultPrinterW(NULL, &size);
    if (size == 0) return L"";
    std::vector<wchar_t> buf(size);
    if (!GetDefaultPrinterW(&buf[0], &size)) return L"";
    return std::wstring(&buf[0]);
}

static void setPrinterNameFromDevNames() {
    if (!hDevNames) return;
    DEVNAMES* dn = (DEVNAMES*)GlobalLock(hDevNames);
    if (!dn) return;
    wchar_t* device = (wchar_t*)((BYTE*)dn + dn->wDeviceOffset);
    if (device && device[0]) _printername = String(device);
    GlobalUnlock(hDevNames);
}

static std::wstring getPrinterNameWide() {
    std::wstring name = convertBBStringToWide(_printername);
    if (!name.empty()) return name;
    if (hDevNames) {
        DEVNAMES* dn = (DEVNAMES*)GlobalLock(hDevNames);
        if (dn) {
            wchar_t* device = (wchar_t*)((BYTE*)dn + dn->wDeviceOffset);
            if (device && device[0]) name = device;
            GlobalUnlock(hDevNames);
        }
    }
    if (!name.empty()) return name;
    return getDefaultPrinterName();
}

static DEVMODEW* lockDevMode() {
    if (!hDevMode) return NULL;
    return (DEVMODEW*)GlobalLock(hDevMode);
}

static void unlockDevMode() {
    if (hDevMode) GlobalUnlock(hDevMode);
}

static HDC createPrinterDC() {
    DEVMODEW* dm = lockDevMode();
    HDC hdc = NULL;

    std::wstring name = getPrinterNameWide();

    if (!name.empty()) {
        hdc = CreateDCW(L"WINSPOOL", name.c_str(), NULL, dm);
        if (!hdc && dm) hdc = CreateDCW(L"WINSPOOL", name.c_str(), NULL, NULL);
    }

    if (hDevNames) {
        DEVNAMES* dn = (DEVNAMES*)GlobalLock(hDevNames);
        if (dn) {
            wchar_t* driver = (wchar_t*)((BYTE*)dn + dn->wDriverOffset);
            wchar_t* device = (wchar_t*)((BYTE*)dn + dn->wDeviceOffset);
            wchar_t* output = (wchar_t*)((BYTE*)dn + dn->wOutputOffset);

            if (device && device[0]) {
                const wchar_t* drv = (driver && driver[0]) ? driver : L"WINSPOOL";
                const wchar_t* port = (output && output[0]) ? output : NULL;
                hdc = CreateDCW(drv, device, port, dm);
                if (!hdc && dm) hdc = CreateDCW(drv, device, port, NULL);
            }
            GlobalUnlock(hDevNames);
        }
    }

    if (!hdc) {
        hdc = CreateDCW(L"WINSPOOL", NULL, NULL, dm);
        if (!hdc && dm) hdc = CreateDCW(L"WINSPOOL", NULL, NULL, NULL);
    }

    unlockDevMode();
    return hdc;
}

// float pixelsPerCm;





void _ShowSettings() {

    PRINTDLGW pd = {0};
    
    pd.lStructSize = sizeof(PRINTDLGW);
    pd.hwndOwner = getDialogOwnerHwnd();
    pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE | PD_PRINTSETUP;
    pd.hDevMode = hDevMode;
    pd.hDevNames = hDevNames;

    if (PrintDlgW(&pd))
    {
        if (savedPrinterDC) {
            DeleteDC(savedPrinterDC);
            savedPrinterDC = NULL;
        }

        if (pd.hDevMode && pd.hDevMode != hDevMode) {
            if (hDevMode) GlobalFree(hDevMode);
            hDevMode = pd.hDevMode;
        }

        if (pd.hDevNames && pd.hDevNames != hDevNames) {
            if (hDevNames) GlobalFree(hDevNames);
            hDevNames = pd.hDevNames;
        }

        setPrinterNameFromDevNames();

        DEVMODEW* dm = lockDevMode();
        if (dm)
        {
            if (_printername.Length() == 0 && dm->dmDeviceName[0])
                _printername = dm->dmDeviceName;
            _orientation = dm->dmOrientation == DMORIENT_PORTRAIT ? String(L"Verticale", 8) : String(L"Orizzontale", 11);
            _copies = dm->dmCopies;
            _coloribn = dm->dmColor == DMCOLOR_COLOR ? String(L"Colori", 6) : String(L"B/N", 3);
            _papersize = dm->dmPaperSize;
            unlockDevMode();
        }

        if (pd.hDC) savedPrinterDC = pd.hDC;
    }
    else
    {
        if (pd.hDevMode && pd.hDevMode != hDevMode) GlobalFree(pd.hDevMode);
        if (pd.hDevNames && pd.hDevNames != hDevNames) GlobalFree(pd.hDevNames);
        if (pd.hDC) DeleteDC(pd.hDC);
    }
}

String _GetSettings() {

    String RETSET=_printername + "@" + _orientation + "@" + _copies + "@" + _coloribn + "@" + _papersize;

    return RETSET;

}


float _StartPrintDocument() {

    float pixelsPerCm = 0;

    if (printerDC) {
        DeleteDC(printerDC);
        printerDC = NULL;
    }

    if (savedPrinterDC) {
        printerDC = savedPrinterDC;
        savedPrinterDC = NULL;
    } else {
        printerDC = createPrinterDC();
    }

    if (!printerDC) {
        std::wstring msg = L"Impossibile aprire la stampante";
        std::wstring pname = getPrinterNameWide();
        if (!pname.empty()) {
            msg += L":\n";
            msg += pname;
        }
        msg += L"\n\nPremi C e seleziona la stampante locale, poi Z per stampare.";
        MessageBoxW(getDialogOwnerHwnd(), msg.c_str(), L"Errore stampa", MB_OK | MB_ICONERROR);
        return 0;
    }

    dpiX = GetDeviceCaps(printerDC, LOGPIXELSX);
    dpiY = GetDeviceCaps(printerDC, LOGPIXELSY);

    marginLeft = GetDeviceCaps(printerDC, PHYSICALOFFSETX);
    marginTop = GetDeviceCaps(printerDC, PHYSICALOFFSETY);

    pixelsPerCm = (float)(dpiX / 2.54);

    DOCINFOW di = {0};
    di.cbSize = sizeof(DOCINFOW);
    di.lpszDocName = L"Stampa";

    if (StartDocW(printerDC, &di) <= 0) {
        MessageBoxW(getDialogOwnerHwnd(), L"StartDoc fallito.", L"Errore stampa", MB_OK | MB_ICONERROR);
        DeleteDC(printerDC);
        printerDC = NULL;
        return 0;
    }

    if (StartPage(printerDC) <= 0) {
        MessageBoxW(getDialogOwnerHwnd(), L"StartPage fallito.", L"Errore stampa", MB_OK | MB_ICONERROR);
        EndDoc(printerDC);
        DeleteDC(printerDC);
        printerDC = NULL;
        return 0;
    }

    return pixelsPerCm;
}

void _PrintRect(int x1, int y1, int x2, int y2, int wl, int rr, int gg, int bb, float pxc) {

        // pixelsPerCm=3;
        

        float linex1 =(float)x1 * pxc;  // 5 cm
        float liney1 =(float)y1 * pxc; // 3 cm
        float linex2 =(float)x2 * pxc;  // 5 cm
        float liney2 =(float)y2 * pxc; // 3 cm

        float spess = wl*pxc;

        // SetTextColor(printerDC, RGB(255, 0, 0));

       
        // Rectangle(printerDC, marginLeft+10, marginTop+180, marginLeft + rectWidth+10, marginTop + rectHeight+10);

        
        
        // Crea penna rossa
        // HPEN redPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
        HPEN redPen = CreatePen(PS_SOLID, spess, RGB(rr, gg, bb)); 
        HPEN oldPen = (HPEN)SelectObject(printerDC, redPen);

        HBRUSH oldBrush = (HBRUSH)SelectObject(printerDC, GetStockObject(NULL_BRUSH));

        //--------PROVA RECT--------
        Rectangle(printerDC, linex1, liney1, linex2, liney2);

        // Line(printerDC, linex1, liney1,linex1+ linex2, liney1+ liney2);


        // Graphics* graphics = new Graphics(hdcPrint);
        // Pen* pen = new Pen(Color(255, 0, 0, 0));
        // graphics->DrawLine(pen, 50, 50, 350, 550);

}

void _PrintLine(int x1, int y1, int x2, int y2, int wl, int rr, int gg, int bb, float pxc) {

        // pixelsPerCm=3;
        

        float linex1 =(float)x1 * pxc;  // 5 cm
        float liney1 =(float)y1 * pxc; // 3 cm
        float linex2 =(float)x2 * pxc;  // 5 cm
        float liney2 =(float)y2 * pxc; // 3 cm

        float spess = wl*pxc;

        // SetTextColor(printerDC, RGB(255, 0, 0));

       
        // Rectangle(printerDC, marginLeft+10, marginTop+180, marginLeft + rectWidth+10, marginTop + rectHeight+10);

        
        
        // Crea penna rossa
        // HPEN redPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
        HPEN redPen = CreatePen(PS_SOLID, spess, RGB(rr, gg, bb)); 
        HPEN oldPen = (HPEN)SelectObject(printerDC, redPen);

        HBRUSH oldBrush = (HBRUSH)SelectObject(printerDC, GetStockObject(NULL_BRUSH));
        
        //--------PROVA RECT--------
        // Rectangle(printerDC, linex1, liney1,linex1+ linex2, liney1+ liney2);

        // Line(printerDC, linex1, liney1,linex1+ linex2, liney1+ liney2);
        // Sposta il "cursore" al punto di partenza
        MoveToEx(printerDC, linex1, liney1, NULL);
        
        // Disegna linea fino al punto finale
        LineTo(printerDC, linex2, liney2);

        // Graphics* graphics = new Graphics(hdcPrint);
        // Pen* pen = new Pen(Color(255, 0, 0, 0));
        // graphics->DrawLine(pen, 50, 50, 350, 550);

}


void _PrintImage(float x1, float y1, String fn, float pxc) {

        // pixelsPerCm=3;
    
    // HDC screenDC = GetDC(GetActiveWindow());
    HDC screenDC = GetDC(HWND_DESKTOP);
    // int referenceDpi = GetDeviceCaps(screenDC, LOGPIXELSX);

    float dpiXSCR = GetDeviceCaps(screenDC, LOGPIXELSX);
    float dpiYSCR = GetDeviceCaps(screenDC, LOGPIXELSY);

    ReleaseDC(NULL, screenDC);

    pxc=pxc/dpiX*dpiXSCR;
    
    pxc=pxc*1.04195435;

        //  pxc=pxc/10.0f; //*2.54f;

        // pxc=pxc*0.84;

        float linex1 =x1 * pxc; // - marginLeft*pxc;  // 5 cm
        float liney1 =y1 * pxc; // - marginTop*pxc; // 3 cm
        // float linex2 =(float)x2 * pxc;  // 5 cm
        // float liney2 =(float)y2 * pxc; // 3 cm

        // float spess = wl*pxc;

        // SetTextColor(printerDC, RGB(255, 0, 0));

       
        // Rectangle(printerDC, marginLeft+10, marginTop+180, marginLeft + rectWidth+10, marginTop + rectHeight+10);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    wstring filenam2 = convertBBStringToWide(fn);

    std::wcout << filenam2 << std::endl;

    // filenam2=L"D:/my/_APP/cxnativesdemo/cxnativesdemo.data/knight.png";
    // Carica PNG

//     int bufferSize = sizeof(argbBuffer);

//     BYTE* argbBuffer2 = new BYTE[bufferSize];
// //c++ ciclo semplice per copiare da Array  a BYTE[]
//     for (int i = 0; i < bufferSize; i++) {
//     // arr[i+2]= pixelData[i];
//     // output.push_back(pixelData[i]);
//     // argbBuffer2[i]=argbBuffer[i];
// argbBuffer2[i] = reinterpret_cast<BYTE*>(argbBuffer)[i];

//     }

//     // Crea bitmap da pixel ARGB
//     Image* image = CreateImageFromARGB(argbBuffer2, width, height);

    Image image((LPWSTR)filenam2.c_str());

    
    // Crea Graphics legato al printer HDC
    Graphics graphics(printerDC);

    // Dimensione fisica → Pixel del dispositivo:
    // int devicePixels = (int)(dimensioneCm * pxc);

    // Pixel del dispositivo → Dimensione fisica:
    // float dimensioneCm = devicePixels / pxc;

    float wimg=image.GetWidth()*pxc;
    float himg=image.GetHeight()*pxc;


    // Calcola dimensioni fisiche basate sul DPI di riferimento
    // float kk=2.54f;

    

    // std::wcout << L"DPI: " << dpiX << L"x" << dpiY << std::endl;
    // std::wcout << L"DPISCR: " << dpiXSCR << L"x" << dpiYSCR << std::endl;

    // float XCm = (float)(x1) / (referenceDpi / 2.54f)*10.0f*kk;
    // float YCm = (float)(y1) / (referenceDpi / 2.54f)*10.0f*kk;
    // float widthCm = (float)(image.GetWidth()) / (referenceDpi / 2.54f)*10.0f*kk;
    // float heightCm = (float)(image.GetHeight()) / (referenceDpi / 2.54f)*10.0f*kk;
    // ReleaseDC(NULL, screenDC);

    ImageAttributes imageAttributes;
        
        // Esempio: trasparenza al 50%
        ColorMatrix colorMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  // Alpha al 50%
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
        };

    // imageAttributes.SetColorMatrix(&colorMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeDefault);

    HBRUSH redB = CreateSolidBrush(RGB(255, 255, 255)); 
    // HPEN oldPen = (HPEN)SelectObject(printerDC, redB);

    HBRUSH oldBrush = (HBRUSH)SelectObject(printerDC, redB);


    //https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-drawimage(image_constrect__int_int_int_int_unit_constimageattributes_drawimageabort_void)

    // RECT bounds = gdiplusToken.rclBounds;

    // Disegna l'immagine nella posizione/dimensione desiderata
    Rect destRect(linex1, liney1, wimg, himg);
    // Rect destRect(XCm, YCm, widthCm, heightCm);

    

    graphics.DrawImage(&image, destRect, 0,0,image.GetWidth(),image.GetHeight(), UnitPixel, &imageAttributes);

    
    // Chiude GDI+
    // GdiplusShutdown(gdiplusToken);
        
        // Crea penna rossa
        // HPEN redPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
        // HPEN redPen = CreatePen(PS_SOLID, spess, RGB(rr, gg, bb)); 
        // HPEN oldPen = (HPEN)SelectObject(printerDC, redPen);

        // HBRUSH oldBrush = (HBRUSH)SelectObject(printerDC, GetStockObject(NULL_BRUSH));

        // //--------PROVA RECT--------
        // Rectangle(printerDC, linex1, liney1,linex1+ linex2, liney1+ liney2);

        // Line(printerDC, linex1, liney1,linex1+ linex2, liney1+ liney2);


        // Graphics* graphics = new Graphics(hdcPrint);
        // Pen* pen = new Pen(Color(255, 0, 0, 0));
        // graphics->DrawLine(pen, 50, 50, 350, 550);

}




void _PrintText(float x1, float y1, Array<int> pixels, int width, int height, float rr, float gg, float bb, float ksize, float pxc) {

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);


    //CREA IMAGE DA Array<int>
    // Crea un Bitmap dai dati dell'array
    // Bitmap* bmp = new Bitmap(width, height, PixelFormat32bppARGB);
    
    // // Blocca i bit del bitmap per scrivere i pixel
    // BitmapData bmpData;
    // Rect rect(0, 0, width, height);
    // bmp->LockBits(&rect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    
    // // Copia i dati dall'Array<int> al bitmap
    // int* destPixels = (int*)bmpData.Scan0;
    // for (int i = 0; i < width * height; i++) {
    //     destPixels[i] = pixels[i];
    // }
    
    // // Sblocca i bit
    // bmp->UnlockBits(&bmpData);
    
    // // Crea un oggetto Image (non puntatore) dal bitmap
    // // Bitmap image(width, height, PixelFormat32bppARGB);
    
    // Crea direttamente il bitmap finale (senza bitmap temporaneo)
    Bitmap image(width, height, PixelFormat32bppARGB);
    
    // Blocca i bit del bitmap per scrivere i pixel
    BitmapData bmpData;
    Rect rect(0, 0, width, height);
    Status result = image.LockBits(&rect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    
    if (result == Ok) {
        // Copia i dati dall'Array<int> al bitmap
        int* destPixels = (int*)bmpData.Scan0;
        for (int i = 0; i < width * height; i++) {
            destPixels[i] = pixels[i];
        }
        
        // Sblocca i bit
        image.UnlockBits(&bmpData);
        
        // Ora puoi usare 'image' con graphics.DrawImage
        // Graphics* g = Graphics::FromHDC(printerDC);
        // g->DrawImage(&image, x1, y1, width * pxc, height * pxc);
        // delete g;
        std::wcout << L"OK!" << std::endl;
    }
    
    // // Bitmap temporanea dai pixel
    // Bitmap bmp(width, height, PixelFormat32bppARGB);
    // BitmapData bmpData;
    // Rect rect(0, 0, width, height);

    // bmp.LockBits(&rect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    // memcpy(bmpData.Scan0, source.data(), source.size() * sizeof(int));
    // bmp.UnlockBits(&bmpData);

    // // Salva in PNG dentro uno stream in memoria
    // CLSID pngClsid = GetEncoderClsid(L"image/png");
    // IStream* pStream = nullptr;
    // CreateStreamOnHGlobal(NULL, TRUE, &pStream);
    // bmp.Save(pStream, &pngClsid, nullptr);

    // // Riavvolgi lo stream
    // LARGE_INTEGER liZero = {};
    // pStream->Seek(liZero, STREAM_SEEK_SET, NULL);

    // // Carica come Image* da PNG
    // Image* image = Image::FromStream(pStream);

    // // Ora hai un Image* da usare
    // Image* image = bmp;

    // // --- esempio: disegnare su un'altra bitmap ---
    // Bitmap canvas(200, 200, PixelFormat32bppARGB);
    // Graphics g(&canvas);

    // g.Clear(Color(255, 0, 0, 0)); // sfondo nero
    // g.DrawImage(image, 50, 50, width, height);


        // pixelsPerCm=3;
    
    // HDC screenDC = GetDC(GetActiveWindow());
    HDC screenDC = GetDC(HWND_DESKTOP);
    // int referenceDpi = GetDeviceCaps(screenDC, LOGPIXELSX);

    float dpiXSCR = GetDeviceCaps(screenDC, LOGPIXELSX);
    float dpiYSCR = GetDeviceCaps(screenDC, LOGPIXELSY);

    ReleaseDC(NULL, screenDC);

    pxc=pxc/dpiX*dpiXSCR;
    
    pxc=pxc*1.04;

        //  pxc=pxc/10.0f; //*2.54f;

        // pxc=pxc*0.84;

        float linex1 =x1 * pxc; // - marginLeft*pxc;  // 5 cm
        float liney1 =y1 * pxc; // - marginTop*pxc; // 3 cm
        // float linex2 =(float)x2 * pxc;  // 5 cm
        // float liney2 =(float)y2 * pxc; // 3 cm

        // float spess = wl*pxc;

        // SetTextColor(printerDC, RGB(255, 0, 0));

       
        // Rectangle(printerDC, marginLeft+10, marginTop+180, marginLeft + rectWidth+10, marginTop + rectHeight+10);

    
    // string filenam = convertBBString(fn);

    // filenam="D:/my/_APP/cxnativesdemo/cxnativesdemo.data/knight.png";
    // Carica PNG

    // wstring filenam2(filenam.begin(), filenam.end());

//     int bufferSize = sizeof(argbBuffer);

//     BYTE* argbBuffer2 = new BYTE[bufferSize];
// //c++ ciclo semplice per copiare da Array  a BYTE[]
//     for (int i = 0; i < bufferSize; i++) {
//     // arr[i+2]= pixelData[i];
//     // output.push_back(pixelData[i]);
//     // argbBuffer2[i]=argbBuffer[i];
// argbBuffer2[i] = reinterpret_cast<BYTE*>(argbBuffer)[i];

//     }

//     // Crea bitmap da pixel ARGB
//     Image* image = CreateImageFromARGB(argbBuffer2, width, height);

    // Image image((LPWSTR)filenam2.c_str());

    
    // Crea Graphics legato al printer HDC
    Graphics graphics(printerDC);

    // Dimensione fisica → Pixel del dispositivo:
    // int devicePixels = (int)(dimensioneCm * pxc);

    // Pixel del dispositivo → Dimensione fisica:
    // float dimensioneCm = devicePixels / pxc;

    float wimg=image.GetWidth()*pxc  * ksize;
    float himg=image.GetHeight()*pxc  * ksize;


    // Calcola dimensioni fisiche basate sul DPI di riferimento
    // float kk=2.54f;

    

    std::wcout << L"DPI: " << dpiX << L"x" << dpiY << std::endl;
    std::wcout << L"DPISCR: " << dpiXSCR << L"x" << dpiYSCR << std::endl;

    // float XCm = (float)(x1) / (referenceDpi / 2.54f)*10.0f*kk;
    // float YCm = (float)(y1) / (referenceDpi / 2.54f)*10.0f*kk;
    // float widthCm = (float)(image.GetWidth()) / (referenceDpi / 2.54f)*10.0f*kk;
    // float heightCm = (float)(image.GetHeight()) / (referenceDpi / 2.54f)*10.0f*kk;
    // ReleaseDC(NULL, screenDC);

    ImageAttributes imageAttributes;
        
        // Esempio: trasparenza al 50%
        ColorMatrix colorMatrix = {
            rr, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, gg, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, bb, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  // Alpha al 50%
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
        };

    imageAttributes.SetColorMatrix(&colorMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeDefault);

    HBRUSH redB = CreateSolidBrush(RGB(255, 255, 255)); 
    // HPEN oldPen = (HPEN)SelectObject(printerDC, redB);

    HBRUSH oldBrush = (HBRUSH)SelectObject(printerDC, redB);


    //https://learn.microsoft.com/en-us/windows/win32/api/gdiplusgraphics/nf-gdiplusgraphics-graphics-drawimage(image_constrect__int_int_int_int_unit_constimageattributes_drawimageabort_void)

    // RECT bounds = gdiplusToken.rclBounds;

    // Disegna l'immagine nella posizione/dimensione desiderata
    Rect destRect(linex1, liney1, wimg, himg);
    // Rect destRect(XCm, YCm, widthCm, heightCm);

    

    graphics.DrawImage(&image, destRect, 0,0,image.GetWidth(),image.GetHeight(), UnitPixel, &imageAttributes);

    
    // Chiude GDI+
    // GdiplusShutdown(gdiplusToken);
        
        // Crea penna rossa
        // HPEN redPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 255));
        // HPEN redPen = CreatePen(PS_SOLID, spess, RGB(rr, gg, bb)); 
        // HPEN oldPen = (HPEN)SelectObject(printerDC, redPen);

        // HBRUSH oldBrush = (HBRUSH)SelectObject(printerDC, GetStockObject(NULL_BRUSH));

        // //--------PROVA RECT--------
        // Rectangle(printerDC, linex1, liney1,linex1+ linex2, liney1+ liney2);

        // Line(printerDC, linex1, liney1,linex1+ linex2, liney1+ liney2);


        // Graphics* graphics = new Graphics(hdcPrint);
        // Pen* pen = new Pen(Color(255, 0, 0, 0));
        // graphics->DrawLine(pen, 50, 50, 350, 550);

}



void _EndPrintDocument() {

    if (!printerDC) return;

    EndPage(printerDC);
    EndDoc(printerDC);
    DeleteDC(printerDC);
    printerDC = NULL;
}








//  Cartella in cui sta l'eseguibile, con la barra finale.
//
//  Sta qui e non in brl.process perche' quel modulo non e' implementato su
//  Android ("Native Process class not implemented") e perche' boshlibs non
//  deve dipendere dai moduli dell'installazione: tutto il codice sta nel
//  modulo. Serve alla stampa, che rilegge le immagini da disco e non capisce
//  i percorsi "cerberus://data/".
String _AppDir(){

    WCHAR buf[MAX_PATH+1];

    DWORD n = GetModuleFileNameW( 0,buf,MAX_PATH );

    if( !n ) return String();

    buf[n] = 0;

    for( int i=(int)n-1;i>=0;--i ){
        // 92 e' il codice del backslash: scritto come numero per non
        // doverlo passare attraverso gli escape di piu' strumenti.
        if( buf[i]==(WCHAR)92 || buf[i]==L'/' ){ buf[i+1]=0; break; }
    }

    return String( buf );
}
