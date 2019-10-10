/* File generated automatically from tools/winapi/test.dat; do not edit! */
/* This file can be copied, modified and distributed without restriction. */

/*
 * Unit tests for data structure packing
 */

#define WINVER 0x0501
#define _WIN32_IE 0x0501
#define _WIN32_WINNT 0x0501

#define WINE_NOWINSOCK

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "wininet.h"

#include "wine/test.h"

/***********************************************************************
 * Compability macros
 */

#define DWORD_PTR UINT_PTR
#define LONG_PTR INT_PTR
#define ULONG_PTR UINT_PTR

/***********************************************************************
 * Windows API extension
 */

#if defined(_MSC_VER) && (_MSC_VER >= 1300) && defined(__cplusplus)
# define FIELD_ALIGNMENT(type, field) __alignof(((type*)0)->field)
#elif defined(__GNUC__)
# define FIELD_ALIGNMENT(type, field) __alignof__(((type*)0)->field)
#else
/* FIXME: Not sure if is possible to do without compiler extension */
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1300) && defined(__cplusplus)
# define _TYPE_ALIGNMENT(type) __alignof(type)
#elif defined(__GNUC__)
# define _TYPE_ALIGNMENT(type) __alignof__(type)
#else
/*
 * FIXME: Not sure if is possible to do without compiler extension
 *        (if type is not just a name that is, if so the normal)
 *         TYPE_ALIGNMENT can be used)
 */
#endif

#if !defined(TYPE_ALIGNMENT) && defined(_TYPE_ALIGNMENT)
# define TYPE_ALIGNMENT _TYPE_ALIGNMENT
#endif

/***********************************************************************
 * Test helper macros
 */

#ifdef FIELD_ALIGNMENT
# define TEST_FIELD_ALIGNMENT(type, field, align) \
   ok(FIELD_ALIGNMENT(type, field) == align, \
       "FIELD_ALIGNMENT(" #type ", " #field ") == %d (expected " #align ")\n", \
           FIELD_ALIGNMENT(type, field))
#else
# define TEST_FIELD_ALIGNMENT(type, field, align) do { } while (0)
#endif

#define TEST_FIELD_OFFSET(type, field, offset) \
    ok(FIELD_OFFSET(type, field) == offset, \
        "FIELD_OFFSET(" #type ", " #field ") == %ld (expected " #offset ")\n", \
             FIELD_OFFSET(type, field))

#ifdef _TYPE_ALIGNMENT
#define TEST__TYPE_ALIGNMENT(type, align) \
    ok(_TYPE_ALIGNMENT(type) == align, "TYPE_ALIGNMENT(" #type ") == %d (expected " #align ")\n", _TYPE_ALIGNMENT(type))
#else
# define TEST__TYPE_ALIGNMENT(type, align) do { } while (0)
#endif

#ifdef TYPE_ALIGNMENT
#define TEST_TYPE_ALIGNMENT(type, align) \
    ok(TYPE_ALIGNMENT(type) == align, "TYPE_ALIGNMENT(" #type ") == %d (expected " #align ")\n", TYPE_ALIGNMENT(type))
#else
# define TEST_TYPE_ALIGNMENT(type, align) do { } while (0)
#endif

#define TEST_TYPE_SIZE(type, size) \
    ok(sizeof(type) == size, "sizeof(" #type ") == %d (expected " #size ")\n", sizeof(type))

/***********************************************************************
 * Test macros
 */

#define TEST_FIELD(type, field_type, field_name, field_offset, field_size, field_align) \
  TEST_TYPE_SIZE(field_type, field_size); \
  TEST_FIELD_ALIGNMENT(type, field_name, field_align); \
  TEST_FIELD_OFFSET(type, field_name, field_offset); \

#define TEST_TYPE(type, size, align) \
  TEST_TYPE_ALIGNMENT(type, align); \
  TEST_TYPE_SIZE(type, size)

#define TEST_TYPE_POINTER(type, size, align) \
    TEST__TYPE_ALIGNMENT(*(type)0, align); \
    TEST_TYPE_SIZE(*(type)0, size)

#define TEST_TYPE_SIGNED(type) \
    ok((type) -1 < 0, "(" #type ") -1 < 0\n");

#define TEST_TYPE_UNSIGNED(type) \
     ok((type) -1 > 0, "(" #type ") -1 > 0\n");

static void test_pack_GOPHER_FIND_DATAA(void)
{
    /* GOPHER_FIND_DATAA (pack 4) */
    TEST_TYPE(GOPHER_FIND_DATAA, 808, 4);
    TEST_FIELD(GOPHER_FIND_DATAA, CHAR[MAX_GOPHER_DISPLAY_TEXT + 1], DisplayString, 0, 129, 1);
    TEST_FIELD(GOPHER_FIND_DATAA, DWORD, GopherType, 132, 4, 4);
    TEST_FIELD(GOPHER_FIND_DATAA, DWORD, SizeLow, 136, 4, 4);
    TEST_FIELD(GOPHER_FIND_DATAA, DWORD, SizeHigh, 140, 4, 4);
    TEST_FIELD(GOPHER_FIND_DATAA, FILETIME, LastModificationTime, 144, 8, 4);
    TEST_FIELD(GOPHER_FIND_DATAA, CHAR[MAX_GOPHER_LOCATOR_LENGTH + 1], Locator, 152, 654, 1);
}

static void test_pack_GOPHER_FIND_DATAW(void)
{
    /* GOPHER_FIND_DATAW (pack 4) */
    TEST_TYPE(GOPHER_FIND_DATAW, 1588, 4);
    TEST_FIELD(GOPHER_FIND_DATAW, WCHAR[MAX_GOPHER_DISPLAY_TEXT + 1], DisplayString, 0, 258, 2);
    TEST_FIELD(GOPHER_FIND_DATAW, DWORD, GopherType, 260, 4, 4);
    TEST_FIELD(GOPHER_FIND_DATAW, DWORD, SizeLow, 264, 4, 4);
    TEST_FIELD(GOPHER_FIND_DATAW, DWORD, SizeHigh, 268, 4, 4);
    TEST_FIELD(GOPHER_FIND_DATAW, FILETIME, LastModificationTime, 272, 8, 4);
    TEST_FIELD(GOPHER_FIND_DATAW, WCHAR[MAX_GOPHER_LOCATOR_LENGTH + 1], Locator, 280, 1308, 2);
}

static void test_pack_GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE(void)
{
    /* GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE (pack 4) */
    TEST_TYPE(GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, 24, 4);
    TEST_FIELD(GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, INT, DegreesNorth, 0, 4, 4);
    TEST_FIELD(GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, INT, MinutesNorth, 4, 4, 4);
    TEST_FIELD(GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, INT, SecondsNorth, 8, 4, 4);
    TEST_FIELD(GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, INT, DegreesEast, 12, 4, 4);
    TEST_FIELD(GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, INT, MinutesEast, 16, 4, 4);
    TEST_FIELD(GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, INT, SecondsEast, 20, 4, 4);
}

static void test_pack_GOPHER_MOD_DATE_ATTRIBUTE_TYPE(void)
{
    /* GOPHER_MOD_DATE_ATTRIBUTE_TYPE (pack 4) */
    TEST_TYPE(GOPHER_MOD_DATE_ATTRIBUTE_TYPE, 8, 4);
    TEST_FIELD(GOPHER_MOD_DATE_ATTRIBUTE_TYPE, FILETIME, DateAndTime, 0, 8, 4);
}

static void test_pack_GOPHER_SCORE_ATTRIBUTE_TYPE(void)
{
    /* GOPHER_SCORE_ATTRIBUTE_TYPE (pack 4) */
    TEST_TYPE(GOPHER_SCORE_ATTRIBUTE_TYPE, 4, 4);
    TEST_FIELD(GOPHER_SCORE_ATTRIBUTE_TYPE, INT, Score, 0, 4, 4);
}

static void test_pack_GOPHER_SCORE_RANGE_ATTRIBUTE_TYPE(void)
{
    /* GOPHER_SCORE_RANGE_ATTRIBUTE_TYPE (pack 4) */
    TEST_TYPE(GOPHER_SCORE_RANGE_ATTRIBUTE_TYPE, 8, 4);
    TEST_FIELD(GOPHER_SCORE_RANGE_ATTRIBUTE_TYPE, INT, LowerBound, 0, 4, 4);
    TEST_FIELD(GOPHER_SCORE_RANGE_ATTRIBUTE_TYPE, INT, UpperBound, 4, 4, 4);
}

static void test_pack_GOPHER_TIMEZONE_ATTRIBUTE_TYPE(void)
{
    /* GOPHER_TIMEZONE_ATTRIBUTE_TYPE (pack 4) */
    TEST_TYPE(GOPHER_TIMEZONE_ATTRIBUTE_TYPE, 4, 4);
    TEST_FIELD(GOPHER_TIMEZONE_ATTRIBUTE_TYPE, INT, Zone, 0, 4, 4);
}

static void test_pack_GOPHER_TTL_ATTRIBUTE_TYPE(void)
{
    /* GOPHER_TTL_ATTRIBUTE_TYPE (pack 4) */
    TEST_TYPE(GOPHER_TTL_ATTRIBUTE_TYPE, 4, 4);
    TEST_FIELD(GOPHER_TTL_ATTRIBUTE_TYPE, DWORD, Ttl, 0, 4, 4);
}

static void test_pack_GOPHER_VERONICA_ATTRIBUTE_TYPE(void)
{
    /* GOPHER_VERONICA_ATTRIBUTE_TYPE (pack 4) */
    TEST_TYPE(GOPHER_VERONICA_ATTRIBUTE_TYPE, 4, 4);
    TEST_FIELD(GOPHER_VERONICA_ATTRIBUTE_TYPE, BOOL, TreeWalk, 0, 4, 4);
}

static void test_pack_HINTERNET(void)
{
    /* HINTERNET */
    TEST_TYPE(HINTERNET, 4, 4);
}

static void test_pack_HTTP_VERSION_INFO(void)
{
    /* HTTP_VERSION_INFO (pack 4) */
    TEST_TYPE(HTTP_VERSION_INFO, 8, 4);
    TEST_FIELD(HTTP_VERSION_INFO, DWORD, dwMajorVersion, 0, 4, 4);
    TEST_FIELD(HTTP_VERSION_INFO, DWORD, dwMinorVersion, 4, 4, 4);
}

static void test_pack_INTERNET_ASYNC_RESULT(void)
{
    /* INTERNET_ASYNC_RESULT (pack 4) */
    TEST_TYPE(INTERNET_ASYNC_RESULT, 8, 4);
    TEST_FIELD(INTERNET_ASYNC_RESULT, DWORD, dwResult, 0, 4, 4);
    TEST_FIELD(INTERNET_ASYNC_RESULT, DWORD, dwError, 4, 4, 4);
}

static void test_pack_INTERNET_AUTH_NOTIFY_DATA(void)
{
    /* INTERNET_AUTH_NOTIFY_DATA (pack 4) */
    TEST_TYPE(INTERNET_AUTH_NOTIFY_DATA, 16, 4);
    TEST_FIELD(INTERNET_AUTH_NOTIFY_DATA, DWORD, cbStruct, 0, 4, 4);
    TEST_FIELD(INTERNET_AUTH_NOTIFY_DATA, DWORD, dwOptions, 4, 4, 4);
    TEST_FIELD(INTERNET_AUTH_NOTIFY_DATA, PFN_AUTH_NOTIFY, pfnNotify, 8, 4, 4);
    TEST_FIELD(INTERNET_AUTH_NOTIFY_DATA, DWORD, dwContext, 12, 4, 4);
}

static void test_pack_INTERNET_BUFFERSA(void)
{
    /* INTERNET_BUFFERSA (pack 4) */
    TEST_TYPE(INTERNET_BUFFERSA, 40, 4);
    TEST_FIELD(INTERNET_BUFFERSA, DWORD, dwStructSize, 0, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, struct _INTERNET_BUFFERSA *, Next, 4, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, LPCSTR, lpcszHeader, 8, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, DWORD, dwHeadersLength, 12, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, DWORD, dwHeadersTotal, 16, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, LPVOID, lpvBuffer, 20, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, DWORD, dwBufferLength, 24, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, DWORD, dwBufferTotal, 28, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, DWORD, dwOffsetLow, 32, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSA, DWORD, dwOffsetHigh, 36, 4, 4);
}

static void test_pack_INTERNET_BUFFERSW(void)
{
    /* INTERNET_BUFFERSW (pack 4) */
    TEST_TYPE(INTERNET_BUFFERSW, 40, 4);
    TEST_FIELD(INTERNET_BUFFERSW, DWORD, dwStructSize, 0, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, struct _INTERNET_BUFFERSW *, Next, 4, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, LPCWSTR, lpcszHeader, 8, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, DWORD, dwHeadersLength, 12, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, DWORD, dwHeadersTotal, 16, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, LPVOID, lpvBuffer, 20, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, DWORD, dwBufferLength, 24, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, DWORD, dwBufferTotal, 28, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, DWORD, dwOffsetLow, 32, 4, 4);
    TEST_FIELD(INTERNET_BUFFERSW, DWORD, dwOffsetHigh, 36, 4, 4);
}

static void test_pack_INTERNET_CACHE_ENTRY_INFOA(void)
{
    /* INTERNET_CACHE_ENTRY_INFOA (pack 4) */
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOA, DWORD, dwStructSize, 0, 4, 4);
}

static void test_pack_INTERNET_CACHE_ENTRY_INFOW(void)
{
    /* INTERNET_CACHE_ENTRY_INFOW (pack 4) */
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, DWORD, dwStructSize, 0, 4, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, LPWSTR, lpszSourceUrlName, 4, 4, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, LPWSTR, lpszLocalFileName, 8, 4, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, DWORD, CacheEntryType, 12, 4, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, DWORD, dwUseCount, 16, 4, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, DWORD, dwHitRate, 20, 4, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, DWORD, dwSizeLow, 24, 4, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, DWORD, dwSizeHigh, 28, 4, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, FILETIME, LastModifiedTime, 32, 8, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, FILETIME, ExpireTime, 40, 8, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, FILETIME, LastAccessTime, 48, 8, 4);
    TEST_FIELD(INTERNET_CACHE_ENTRY_INFOW, FILETIME, LastSyncTime, 56, 8, 4);
}

static void test_pack_INTERNET_CONNECTED_INFO(void)
{
    /* INTERNET_CONNECTED_INFO (pack 4) */
    TEST_TYPE(INTERNET_CONNECTED_INFO, 8, 4);
    TEST_FIELD(INTERNET_CONNECTED_INFO, DWORD, dwConnectedState, 0, 4, 4);
    TEST_FIELD(INTERNET_CONNECTED_INFO, DWORD, dwFlags, 4, 4, 4);
}

static void test_pack_INTERNET_PORT(void)
{
    /* INTERNET_PORT */
    TEST_TYPE(INTERNET_PORT, 2, 2);
}

static void test_pack_INTERNET_STATUS_CALLBACK(void)
{
    /* INTERNET_STATUS_CALLBACK */
    TEST_TYPE(INTERNET_STATUS_CALLBACK, 4, 4);
}

static void test_pack_INTERNET_VERSION_INFO(void)
{
    /* INTERNET_VERSION_INFO (pack 4) */
    TEST_TYPE(INTERNET_VERSION_INFO, 8, 4);
    TEST_FIELD(INTERNET_VERSION_INFO, DWORD, dwMajorVersion, 0, 4, 4);
    TEST_FIELD(INTERNET_VERSION_INFO, DWORD, dwMinorVersion, 4, 4, 4);
}

static void test_pack_LPGOPHER_FIND_DATAA(void)
{
    /* LPGOPHER_FIND_DATAA */
    TEST_TYPE(LPGOPHER_FIND_DATAA, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_FIND_DATAA, 808, 4);
}

static void test_pack_LPGOPHER_FIND_DATAW(void)
{
    /* LPGOPHER_FIND_DATAW */
    TEST_TYPE(LPGOPHER_FIND_DATAW, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_FIND_DATAW, 1588, 4);
}

static void test_pack_LPGOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE(void)
{
    /* LPGOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE */
    TEST_TYPE(LPGOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE, 24, 4);
}

static void test_pack_LPGOPHER_MOD_DATE_ATTRIBUTE_TYPE(void)
{
    /* LPGOPHER_MOD_DATE_ATTRIBUTE_TYPE */
    TEST_TYPE(LPGOPHER_MOD_DATE_ATTRIBUTE_TYPE, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_MOD_DATE_ATTRIBUTE_TYPE, 8, 4);
}

static void test_pack_LPGOPHER_SCORE_ATTRIBUTE_TYPE(void)
{
    /* LPGOPHER_SCORE_ATTRIBUTE_TYPE */
    TEST_TYPE(LPGOPHER_SCORE_ATTRIBUTE_TYPE, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_SCORE_ATTRIBUTE_TYPE, 4, 4);
}

static void test_pack_LPGOPHER_SCORE_RANGE_ATTRIBUTE_TYPE(void)
{
    /* LPGOPHER_SCORE_RANGE_ATTRIBUTE_TYPE */
    TEST_TYPE(LPGOPHER_SCORE_RANGE_ATTRIBUTE_TYPE, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_SCORE_RANGE_ATTRIBUTE_TYPE, 8, 4);
}

static void test_pack_LPGOPHER_TIMEZONE_ATTRIBUTE_TYPE(void)
{
    /* LPGOPHER_TIMEZONE_ATTRIBUTE_TYPE */
    TEST_TYPE(LPGOPHER_TIMEZONE_ATTRIBUTE_TYPE, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_TIMEZONE_ATTRIBUTE_TYPE, 4, 4);
}

static void test_pack_LPGOPHER_TTL_ATTRIBUTE_TYPE(void)
{
    /* LPGOPHER_TTL_ATTRIBUTE_TYPE */
    TEST_TYPE(LPGOPHER_TTL_ATTRIBUTE_TYPE, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_TTL_ATTRIBUTE_TYPE, 4, 4);
}

static void test_pack_LPGOPHER_VERONICA_ATTRIBUTE_TYPE(void)
{
    /* LPGOPHER_VERONICA_ATTRIBUTE_TYPE */
    TEST_TYPE(LPGOPHER_VERONICA_ATTRIBUTE_TYPE, 4, 4);
    TEST_TYPE_POINTER(LPGOPHER_VERONICA_ATTRIBUTE_TYPE, 4, 4);
}

static void test_pack_LPHINTERNET(void)
{
    /* LPHINTERNET */
    TEST_TYPE(LPHINTERNET, 4, 4);
    TEST_TYPE_POINTER(LPHINTERNET, 4, 4);
}

static void test_pack_LPHTTP_VERSION_INFO(void)
{
    /* LPHTTP_VERSION_INFO */
    TEST_TYPE(LPHTTP_VERSION_INFO, 4, 4);
    TEST_TYPE_POINTER(LPHTTP_VERSION_INFO, 8, 4);
}

static void test_pack_LPINTERNET_ASYNC_RESULT(void)
{
    /* LPINTERNET_ASYNC_RESULT */
    TEST_TYPE(LPINTERNET_ASYNC_RESULT, 4, 4);
    TEST_TYPE_POINTER(LPINTERNET_ASYNC_RESULT, 8, 4);
}

static void test_pack_LPINTERNET_BUFFERSA(void)
{
    /* LPINTERNET_BUFFERSA */
    TEST_TYPE(LPINTERNET_BUFFERSA, 4, 4);
    TEST_TYPE_POINTER(LPINTERNET_BUFFERSA, 40, 4);
}

static void test_pack_LPINTERNET_BUFFERSW(void)
{
    /* LPINTERNET_BUFFERSW */
    TEST_TYPE(LPINTERNET_BUFFERSW, 4, 4);
    TEST_TYPE_POINTER(LPINTERNET_BUFFERSW, 40, 4);
}

static void test_pack_LPINTERNET_CACHE_ENTRY_INFOA(void)
{
    /* LPINTERNET_CACHE_ENTRY_INFOA */
    TEST_TYPE(LPINTERNET_CACHE_ENTRY_INFOA, 4, 4);
}

static void test_pack_LPINTERNET_CACHE_ENTRY_INFOW(void)
{
    /* LPINTERNET_CACHE_ENTRY_INFOW */
    TEST_TYPE(LPINTERNET_CACHE_ENTRY_INFOW, 4, 4);
}

static void test_pack_LPINTERNET_CONNECTED_INFO(void)
{
    /* LPINTERNET_CONNECTED_INFO */
    TEST_TYPE(LPINTERNET_CONNECTED_INFO, 4, 4);
    TEST_TYPE_POINTER(LPINTERNET_CONNECTED_INFO, 8, 4);
}

static void test_pack_LPINTERNET_PORT(void)
{
    /* LPINTERNET_PORT */
    TEST_TYPE(LPINTERNET_PORT, 4, 4);
    TEST_TYPE_POINTER(LPINTERNET_PORT, 2, 2);
}

static void test_pack_LPINTERNET_STATUS_CALLBACK(void)
{
    /* LPINTERNET_STATUS_CALLBACK */
    TEST_TYPE(LPINTERNET_STATUS_CALLBACK, 4, 4);
    TEST_TYPE_POINTER(LPINTERNET_STATUS_CALLBACK, 4, 4);
}

static void test_pack_LPINTERNET_VERSION_INFO(void)
{
    /* LPINTERNET_VERSION_INFO */
    TEST_TYPE(LPINTERNET_VERSION_INFO, 4, 4);
    TEST_TYPE_POINTER(LPINTERNET_VERSION_INFO, 8, 4);
}

static void test_pack_LPURL_COMPONENTSA(void)
{
    /* LPURL_COMPONENTSA */
    TEST_TYPE(LPURL_COMPONENTSA, 4, 4);
}

static void test_pack_LPURL_COMPONENTSW(void)
{
    /* LPURL_COMPONENTSW */
    TEST_TYPE(LPURL_COMPONENTSW, 4, 4);
}

static void test_pack_PFN_AUTH_NOTIFY(void)
{
    /* PFN_AUTH_NOTIFY */
    TEST_TYPE(PFN_AUTH_NOTIFY, 4, 4);
}

static void test_pack_PFN_DIAL_HANDLER(void)
{
    /* PFN_DIAL_HANDLER */
    TEST_TYPE(PFN_DIAL_HANDLER, 4, 4);
}

static void test_pack_URL_COMPONENTSA(void)
{
    /* URL_COMPONENTSA (pack 4) */
    TEST_FIELD(URL_COMPONENTSA, DWORD, dwStructSize, 0, 4, 4);
}

static void test_pack_URL_COMPONENTSW(void)
{
    /* URL_COMPONENTSW (pack 4) */
    TEST_FIELD(URL_COMPONENTSW, DWORD, dwStructSize, 0, 4, 4);
    TEST_FIELD(URL_COMPONENTSW, LPWSTR, lpszScheme, 4, 4, 4);
    TEST_FIELD(URL_COMPONENTSW, DWORD, dwSchemeLength, 8, 4, 4);
}

static void test_pack(void)
{
    test_pack_GOPHER_FIND_DATAA();
    test_pack_GOPHER_FIND_DATAW();
    test_pack_GOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE();
    test_pack_GOPHER_MOD_DATE_ATTRIBUTE_TYPE();
    test_pack_GOPHER_SCORE_ATTRIBUTE_TYPE();
    test_pack_GOPHER_SCORE_RANGE_ATTRIBUTE_TYPE();
    test_pack_GOPHER_TIMEZONE_ATTRIBUTE_TYPE();
    test_pack_GOPHER_TTL_ATTRIBUTE_TYPE();
    test_pack_GOPHER_VERONICA_ATTRIBUTE_TYPE();
    test_pack_HINTERNET();
    test_pack_HTTP_VERSION_INFO();
    test_pack_INTERNET_ASYNC_RESULT();
    test_pack_INTERNET_AUTH_NOTIFY_DATA();
    test_pack_INTERNET_BUFFERSA();
    test_pack_INTERNET_BUFFERSW();
    test_pack_INTERNET_CACHE_ENTRY_INFOA();
    test_pack_INTERNET_CACHE_ENTRY_INFOW();
    test_pack_INTERNET_CONNECTED_INFO();
    test_pack_INTERNET_PORT();
    test_pack_INTERNET_STATUS_CALLBACK();
    test_pack_INTERNET_VERSION_INFO();
    test_pack_LPGOPHER_FIND_DATAA();
    test_pack_LPGOPHER_FIND_DATAW();
    test_pack_LPGOPHER_GEOGRAPHICAL_LOCATION_ATTRIBUTE_TYPE();
    test_pack_LPGOPHER_MOD_DATE_ATTRIBUTE_TYPE();
    test_pack_LPGOPHER_SCORE_ATTRIBUTE_TYPE();
    test_pack_LPGOPHER_SCORE_RANGE_ATTRIBUTE_TYPE();
    test_pack_LPGOPHER_TIMEZONE_ATTRIBUTE_TYPE();
    test_pack_LPGOPHER_TTL_ATTRIBUTE_TYPE();
    test_pack_LPGOPHER_VERONICA_ATTRIBUTE_TYPE();
    test_pack_LPHINTERNET();
    test_pack_LPHTTP_VERSION_INFO();
    test_pack_LPINTERNET_ASYNC_RESULT();
    test_pack_LPINTERNET_BUFFERSA();
    test_pack_LPINTERNET_BUFFERSW();
    test_pack_LPINTERNET_CACHE_ENTRY_INFOA();
    test_pack_LPINTERNET_CACHE_ENTRY_INFOW();
    test_pack_LPINTERNET_CONNECTED_INFO();
    test_pack_LPINTERNET_PORT();
    test_pack_LPINTERNET_STATUS_CALLBACK();
    test_pack_LPINTERNET_VERSION_INFO();
    test_pack_LPURL_COMPONENTSA();
    test_pack_LPURL_COMPONENTSW();
    test_pack_PFN_AUTH_NOTIFY();
    test_pack_PFN_DIAL_HANDLER();
    test_pack_URL_COMPONENTSA();
    test_pack_URL_COMPONENTSW();
}

START_TEST(generated)
{
    test_pack();
}
