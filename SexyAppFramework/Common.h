#ifndef __SEXYAPPFRAMEWORK_COMMON_H__
#define __SEXYAPPFRAMEWORK_COMMON_H__

#pragma warning(disable:4786)
#pragma warning(disable:4503)

#undef _WIN32_WINNT
#undef WIN32_LEAN_AND_MEAN

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#undef _UNICODE
#undef UNICODE

#include <string>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <climits>

#ifdef _WIN32
#define NOMINMAX 1
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#else

#include <wctype.h>
#include <string.h>
#include <stdint.h>
#define _stricmp strcasecmp
#define _cdecl
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;
typedef uint32_t UINT;
typedef int64_t __int64;
typedef int INT;
typedef long LONG;
typedef unsigned long ULONG;
typedef LONG WINBOOL;
typedef void* HANDLE;
typedef WORD* LPWORD;
typedef DWORD* LPDWORD;
typedef char CHAR;
typedef CHAR* LPSTR;
typedef const CHAR* LPCSTR;
typedef wchar_t WCHAR;
typedef WCHAR TCHAR;
typedef WCHAR* LPWSTR;
typedef TCHAR* LPTSTR;
typedef const WCHAR* LPCWSTR;
typedef const TCHAR* LPCTSTR;
typedef HANDLE* LPHANDLE;
typedef HANDLE HWND;

typedef struct tagRECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT, *PRECT, *NPRECT, *LPRECT;

typedef struct _GUID {
	unsigned long  Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char  Data4[8];
} GUID;

#endif

#include "misc/ModVal.h"

// fallback if NOMINMAX fails (somehow?)
#undef min
#undef max

// Define unreachable()
#ifdef MSVC
#define unreachable std::unreachable
#else
#define unreachable __builtin_unreachable
#endif

#define _USE_WIDE_STRING
// Removed wide string support
typedef std::wstring			SexyString;
#define __S(x)				L ##x
inline int sexywtoi(const wchar_t* str)
{
    wchar_t* end;
    long val = wcstol(str, &end, 10);
    if (end == str || *end != L'\0') {
        // 非法输入，可返回 0 或抛异常
        return 0;
    }
    if (val > INT_MAX || val < INT_MIN) {
        // 溢出处理
        return 0;
    }
    return static_cast<int>(val);
}

#ifdef _WIN32
#define wstrcasecmp _wcsicmp
#else
#define wstrcasecmp wcscasecmp
#endif

#define sexystrncmp			wcsncmp
#define sexystrcmp			wcscmp
#define sexystricmp			wstrcasecmp
#define sexysscanf			swscanf
#define sexyatoi			sexywtoi
#define sexystrcpy			wcscpy
#define sexystrncpy			wcsncpy
#define sexystrlen			wcslen
#define sexyisdigit			iswdigit
#define sexyisalnum			iswalnum
#define sexystrchr			wcschr

#define SexyStringToStringFast(x)	WStringToString(x)
#define SexyStringToWStringFast(x)	(x)
#define StringToSexyStringFast(x)	StringToWString(x)
#define WStringToSexyStringFast(x)	(x)

#define LONG_BIGE_TO_NATIVE(l) (((l >> 24) & 0xFF) | ((l >> 8) & 0xFF00) | ((l << 8) & 0xFF0000) | ((l << 24) & 0xFF000000))
#define WORD_BIGE_TO_NATIVE(w) (((w >> 8) & 0xFF) | ((w << 8) & 0xFF00))
#define LONG_LITTLEE_TO_NATIVE(l) (l)
#define WORD_LITTLEE_TO_NATIVE(w) (w)

#define LENGTH(anyarray) (sizeof(anyarray) / sizeof(anyarray[0]))

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef int64_t int64;

typedef std::map<std::string, std::string>		DefinesMap;
typedef std::map<std::wstring, std::wstring>	WStringWStringMap;
typedef SexyString::value_type					SexyChar;
#define HAS_SEXYCHAR

namespace Sexy
{

const ulong SEXY_RAND_MAX = 0x7FFFFFFF;

extern bool			gDebug;
//extern HINSTANCE	gHInstance;

#define printf(...) Sexy::PrintF(__VA_ARGS__)
void				PrintF(const char *text, ...);

int					Rand();
int					Rand(int range);
float				Rand(float range);
void				SRand(ulong theSeed);
extern std::string	vformat(const char* fmt, va_list argPtr);
extern std::wstring	vformat(const wchar_t* fmt, va_list argPtr);
extern std::string	StrFormat(const char* fmt ...);
extern std::wstring	StrFormat(const wchar_t* fmt ...);
//bool				CheckFor98Mill();
//bool				CheckForVista();
std::string			GetAppDataFolder();
void				SetAppDataFolder(const std::string& thePath);
std::string			URLEncode(const std::string& theString);
std::string			StringToUpper(const std::string& theString);
std::wstring		StringToUpper(const std::wstring& theString);
std::string			StringToLower(const std::string& theString);
std::wstring		StringToLower(const std::wstring& theString);
std::wstring		StringToWString(const std::string &theString);
std::string			WStringToString(const std::wstring &theString);
SexyString			StringToSexyString(const std::string& theString);
SexyString			WStringToSexyString(const std::wstring& theString);
std::string			SexyStringToString(const SexyString& theString);
std::wstring		SexyStringToWString(const SexyString& theString);
std::string			Upper(const std::string& theData);
std::wstring		Upper(const std::wstring& theData);
std::string			Lower(const std::string& theData);
std::wstring		Lower(const std::wstring& theData);
std::string			Trim(const std::string& theString);
std::wstring		Trim(const std::wstring& theString);
bool				StringToInt(const std::string theString, int* theIntVal);
bool				StringToDouble(const std::string theString, double* theDoubleVal);
bool				StringToInt(const std::wstring theString, int* theIntVal);
bool				StringToDouble(const std::wstring theString, double* theDoubleVal);
int					StrFindNoCase(const char *theStr, const char *theFind);
bool				StrPrefixNoCase(const char *theStr, const char *thePrefix, int maxLength = 10000000);
SexyString			CommaSeperate(int theValue);
std::string			Evaluate(const std::string& theString, const DefinesMap& theDefinesMap);
std::string			XMLDecodeString(const std::string& theString);
std::string			XMLEncodeString(const std::string& theString);
std::wstring		XMLDecodeString(const std::wstring& theString);
std::wstring		XMLEncodeString(const std::wstring& theString);

bool				Deltree(const std::string& thePath);
bool				FileExists(const std::string& theFileName);
void				MkDir(const std::string& theDir);
std::string			GetFileName(const std::string& thePath, bool noExtension = false);
std::string			GetFileDir(const std::string& thePath, bool withSlash = false);
std::string			RemoveTrailingSlash(const std::string& theDirectory);
std::string			AddTrailingSlash(const std::string& theDirectory, bool backSlash = false);
time_t				GetFileDate(const std::string& theFileName);
std::string			GetCurDir();
std::string			GetFullPath(const std::string& theRelPath);
std::string			GetPathFrom(const std::string& theRelPath, const std::string& theDir);
bool				AllowAllAccess(const std::string& theFileName);
std::wstring		UTF8StringToWString(const std::string theString);

// Read memory and then move the pointer
void				SMemR(void*& _Src, void* _Dst, size_t _Size);
void				SMemRStr(void*& _Src, std::string& theString);
// Write memory and then move the pointer
void				SMemW(void*& _Dst, const void* _Src, size_t _Size);
void				SMemWStr(void*& _Dst, const std::string& theString);

inline void			inlineUpper(std::string &theData)
{
    //std::transform(theData.begin(), theData.end(), theData.begin(), toupper);

	int aStrLen = (int) theData.length();
	for (int i = 0; i < aStrLen; i++)
	{
		theData[i] = toupper(theData[i]);
	}
}

inline void			inlineUpper(std::wstring &theData)
{
    //std::transform(theData.begin(), theData.end(), theData.begin(), toupper);

	int aStrLen = (int) theData.length();
	for (int i = 0; i < aStrLen; i++)
	{
		theData[i] = towupper(theData[i]);
	}
}

inline void			inlineLower(std::string &theData)
{
    std::transform(theData.begin(), theData.end(), theData.begin(), tolower);
}

inline void			inlineLower(std::wstring &theData)
{
    std::transform(theData.begin(), theData.end(), theData.begin(), tolower);
}

inline void			inlineLTrim(std::string &theData, const std::string& theChars = " \t\r\n")
{
    theData.erase(0, theData.find_first_not_of(theChars));
}

inline void			inlineLTrim(std::wstring &theData, const std::wstring& theChars = L" \t\r\n")
{
    theData.erase(0, theData.find_first_not_of(theChars));
}


inline void			inlineRTrim(std::string &theData, const std::string& theChars = " \t\r\n")
{
    theData.resize(theData.find_last_not_of(theChars) + 1);
}

inline void			inlineTrim(std::string &theData, const std::string& theChars = " \t\r\n")
{
	inlineRTrim(theData, theChars);
	inlineLTrim(theData, theChars);
}

struct StringLessNoCase { 
	bool operator()(const std::wstring &s1, const std::wstring &s2) const { return wstrcasecmp(s1.c_str(),s2.c_str())<0; } 
	bool operator()(const std::string &s1, const std::string &s2) const { return strcasecmp(s1.c_str(),s2.c_str())<0; }
};

}

#endif //__SEXYAPPFRAMEWORK_COMMON_H__
