#ifndef WINDEF_H
#define WINDEF_H

#include <kos.h>

/*
 * Unsigned Types
 */
typedef uint32  DWORD;
typedef uint32* LPDWORD;
typedef uint32  ULONG;
typedef uint32  UINT;
typedef uint16  WORD;
typedef uint8   BYTE;
typedef uint8*  LPBYTE;
typedef bool    BOOL;

/*
 * Signed Types
 */
typedef int64   __int64;
typedef long    LONG;
typedef short   SHORT;
typedef char    CHAR;
typedef int32   HRESULT;

/*
 * Windows Types
 */
typedef char*   LPCTSTR;
typedef char*   LPCSTR;
typedef char*   LPTSTR;
typedef char*   LPSTR;
typedef char    TCHAR;
typedef int32   HANDLE;
typedef int32   HWND;
typedef int32   HINSTANCE;
typedef void    VOID;
typedef void*   LPVOID;

#define MAX_PATH 255

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define _isnan isnan
#define wsprintf sprintf
#define lstrlen strlen
#define lstrcpyn(a,b,c) strcpy(a,b)
#define lstrcmpi stricmp
#define PathAppend(a,b) strcat(a,b)
#define ZeroMemory(a,b) memset(a, 0, b)
#define TEXT(a) a
#define _ASSERTE

#define S_OK                                   ((HRESULT)0x00000000L)
#define S_FALSE                                ((HRESULT)0x00000001L)
#define E_FAIL                                 ((HRESULT)-0x00000001L)
#define E_OUTOFMEMORY                          ((HRESULT)-0x00000002L)

#define SUCCEEDED(Status) ((HRESULT)(Status) >= 0)
#define FAILED(Status) ((HRESULT)(Status)<0)

#define RGBA_GETALPHA(rgb)      ((rgb) >> 24)
#define RGBA_GETRED(rgb)        (((rgb) >> 16) & 0xff)
#define RGBA_GETGREEN(rgb)      (((rgb) >> 8) & 0xff)
#define RGBA_GETBLUE(rgb)       ((rgb) & 0xff)
#define RGBA_MAKE(r, g, b, a)   ((DWORD) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))

#define D3DTADDRESS_WRAP    1
#define D3DTADDRESS_MIRROR  2
#define D3DTADDRESS_CLAMP   3


/*
 * Windows Funktionsprototypen
 */
int min( int a, int b );
UINT timeGetTime();

#endif
