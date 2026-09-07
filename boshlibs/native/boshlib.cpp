
// Include comuni a tutti i target: convertBBString / convertBBStringToWide
// usano std::wstring, malloc e wcstombs anche fuori dal ramo _WIN32.
#include <string>
#include <cstdlib>
#include <cwchar>

#if _WIN32

#include <fstream>
#include <iostream>
#include <tchar.h>
#include <vector>
#include <string>
#include <windows.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include <commdlg.h>

#include <sstream>

// #include <cairo/cairo.h>

#include <gdiplus.h>
using namespace Gdiplus;

#pragma comment (lib,"Gdiplus.lib")

using namespace std;


#endif


char *convertBBString(String string){
  int srclength = string.Length();
  char *result = (char *)malloc(srclength+1);
  std::wstring string_ws(string.Data(), srclength);
  wcstombs(result, string_ws.c_str(), srclength+1);
  return result;
}



// Nuova funzione per gestire Unicode correttamente
std::wstring convertBBStringToWide(String string){
  int srclength = string.Length();
  std::wstring result(string.Data(), srclength);
  return result;
}



