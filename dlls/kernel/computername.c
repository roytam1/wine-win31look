/*
 * Win32 kernel functions
 *
 * Copyright 1995 Martin von Loewis and Cameron Heide
 * Copyright 1999 Peter Ganten
 * Copyright 2002 Martin Wilck
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winnls.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/unicode.h"
#include "wine/exception.h"
#include "excpt.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(computername);

/* Registry key and value names */
static const WCHAR ComputerW[] = {'M','a','c','h','i','n','e','\\',
                                  'S','y','s','t','e','m','\\',
                                  'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                  'C','o','n','t','r','o','l','\\',
                                  'C','o','m','p','u','t','e','r','N','a','m','e',0};
static const WCHAR ActiveComputerNameW[] =   {'A','c','t','i','v','e','C','o','m','p','u','t','e','r','N','a','m','e',0};
static const WCHAR ComputerNameW[] = {'C','o','m','p','u','t','e','r','N','a','m','e',0};

static const char default_ComputerName[] = "WINE";

#define IS_OPTION_TRUE(ch) ((ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1')

/* filter for page-fault exceptions */
static WINE_EXCEPTION_FILTER(page_fault)
{
    if (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

/*********************************************************************** 
 *                    dns_gethostbyname (INTERNAL)
 *
 *  From hostname(1):
 *  "The FQDN is the name gethostbyname(2) returns for the host name returned by gethostname(2)."
 *
 *  Wine can use this technique only if the thread-safe gethostbyname_r is available.
 */
#ifdef  HAVE_LINUX_GETHOSTBYNAME_R_6
static BOOL dns_gethostbyname ( char *name, int *size )
{
    struct hostent* host = NULL;
    char *extrabuf;
    int ebufsize = 1024;
    struct hostent hostentry;
    int locerr = ENOBUFS, res = ENOMEM;

    extrabuf = HeapAlloc( GetProcessHeap(), 0, ebufsize ) ;

    while( extrabuf ) 
    {
        res = gethostbyname_r ( name, &hostentry, extrabuf, ebufsize, &host, &locerr );
        if( res != ERANGE ) break;
        ebufsize *= 2;
        extrabuf = HeapReAlloc( GetProcessHeap(), 0, extrabuf, ebufsize ) ;
    }
    
    if ( res )
        WARN ("Error in gethostbyname_r %d (%d)\n", res, locerr);
    else
    {
        size_t len = strlen ( host->h_name );
        if ( len < *size )
        {
            strcpy ( name, host->h_name );
            *size = len;
        }
        else
        {
            memcpy ( name, host->h_name, *size );
            name[*size] = 0;
            SetLastError ( ERROR_MORE_DATA );
            res = 1;
        }
    }

    HeapFree( GetProcessHeap(), 0, extrabuf );
    return !res;
}
#else
#  define dns_gethostbyname(name,size) 0
#endif

/*********************************************************************** 
 *                     dns_fqdn (INTERNAL)
 */
static BOOL dns_fqdn ( char *name, int *size )
{
    if ( gethostname ( name, *size + 1 ) ) 
    {
        switch( errno )
        {
        case ENAMETOOLONG:
            SetLastError ( ERROR_MORE_DATA );
        default:
            SetLastError ( ERROR_INVALID_PARAMETER );
        }
        return FALSE;
    }

    if ( !dns_gethostbyname ( name, size ) )
        *size = strlen ( name );

    return TRUE;
}

/*********************************************************************** 
 *                     dns_hostname (INTERNAL)
 */
static BOOL dns_hostname ( char *name, int *size )
{
    char *c;
    if ( ! dns_fqdn ( name, size ) ) return FALSE;
    c = strchr ( name, '.' );
    if (c)
    {
        *c = 0;
        *size = (c - name);
    }
    return TRUE;
}

/*********************************************************************** 
 *                     dns_domainname (INTERNAL)
 */
static BOOL dns_domainname ( char *name, int *size )
{
    char *c;
    if ( ! dns_fqdn ( name, size ) ) return FALSE;
    c = strchr ( name, '.' );
    if (c)
    {
        c += 1;
        *size -= (c - name);
        memmove ( name, c, *size + 1 );
    }
    return TRUE;
}

/*********************************************************************** 
 *                      _init_attr    (INTERNAL)
 */
inline static void _init_attr ( OBJECT_ATTRIBUTES *attr, UNICODE_STRING *name )
{
    attr->Length = sizeof (OBJECT_ATTRIBUTES);
    attr->RootDirectory = 0;
    attr->ObjectName = name;
    attr->Attributes = 0;
    attr->SecurityDescriptor = NULL;
    attr->SecurityQualityOfService = NULL;
}

/***********************************************************************
 *           get_use_dns_option
 */
static BOOL get_use_dns_option(void)
{
    static const WCHAR NetworkW[] = {'M','a','c','h','i','n','e','\\',
                                  'S','o','f','t','w','a','r','e','\\',
                                  'W','i','n','e','\\','W','i','n','e','\\',
                                  'C','o','n','f','i','g','\\','N','e','t','w','o','r','k',0};
    static const WCHAR UseDNSW[] = {'U','s','e','D','n','s','C','o','m','p','u','t','e','r','N','a','m','e',0};

    char tmp[80];
    HKEY hkey;
    DWORD dummy;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    BOOL ret = TRUE;

    _init_attr( &attr, &nameW );
    RtlInitUnicodeString( &nameW, NetworkW );

    if (!NtOpenKey( &hkey, KEY_ALL_ACCESS, &attr ))
    {
        RtlInitUnicodeString( &nameW, UseDNSW );
        if (!NtQueryValueKey( hkey, &nameW, KeyValuePartialInformation, tmp, sizeof(tmp), &dummy ))
        {
            WCHAR *str = (WCHAR *)((KEY_VALUE_PARTIAL_INFORMATION *)tmp)->Data;
            ret = IS_OPTION_TRUE( str[0] );
        }
        NtClose( hkey );
    }
    return ret;
}


/*********************************************************************** 
 *                      COMPUTERNAME_Init    (INTERNAL)
 */
void COMPUTERNAME_Init (void)
{
    HKEY hkey = INVALID_HANDLE_VALUE, hsubkey = INVALID_HANDLE_VALUE;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    char buf[offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data ) + (MAX_COMPUTERNAME_LENGTH + 1) * sizeof( WCHAR )];
    DWORD len = sizeof( buf );
    LPWSTR computer_name = (LPWSTR) (buf + offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data ));
    NTSTATUS st = STATUS_INTERNAL_ERROR;

    TRACE("(void)\n");
    _init_attr ( &attr, &nameW );
    
    RtlInitUnicodeString( &nameW, ComputerW );
    if ( ( st = NtCreateKey( &hkey, KEY_ALL_ACCESS, &attr, 0, NULL, 0, NULL ) ) != STATUS_SUCCESS )
        goto out;
    
    attr.RootDirectory = hkey;
    RtlInitUnicodeString( &nameW, ComputerNameW );
    if ( (st = NtCreateKey( &hsubkey, KEY_ALL_ACCESS, &attr, 0, NULL, 0, NULL ) ) != STATUS_SUCCESS )
        goto out;
    
    st = NtQueryValueKey( hsubkey, &nameW, KeyValuePartialInformation, buf, len, &len );

    if ( st == STATUS_OBJECT_NAME_NOT_FOUND || ( st == STATUS_SUCCESS && get_use_dns_option()))
    {
        char hbuf[256];
        int hlen = sizeof (hbuf);
        char *dot;
        TRACE( "retrieving Unix host name\n" );
        if ( gethostname ( hbuf, hlen ) )
        {
            strcpy ( hbuf, default_ComputerName );
            WARN( "gethostname() error: %d, using host name %s\n", errno, hbuf );
        }
        hbuf[MAX_COMPUTERNAME_LENGTH] = 0;
        dot = strchr ( hbuf, '.' );
        if ( dot ) *dot = 0;
        hlen = strlen ( hbuf );
        len = MultiByteToWideChar( CP_ACP, 0, hbuf, hlen + 1, computer_name, MAX_COMPUTERNAME_LENGTH + 1 )
            * sizeof( WCHAR );
        if ( NtSetValueKey( hsubkey, &nameW, 0, REG_SZ, computer_name, len ) != STATUS_SUCCESS )
            WARN ( "failed to set ComputerName\n" );
    }
    else if ( st == STATUS_SUCCESS)
    {
        len = (len - offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data ));
        TRACE( "found in registry\n" );
    }
    else goto out;
    
    NtClose( hsubkey );
    TRACE(" ComputerName: %s (%lu)\n", debugstr_w ( computer_name ), len / sizeof(WCHAR));

    RtlInitUnicodeString( &nameW, ActiveComputerNameW );
    if ( ( st = NtCreateKey( &hsubkey, KEY_ALL_ACCESS, &attr, 0, NULL, REG_OPTION_VOLATILE, NULL ) )
         != STATUS_SUCCESS )
        goto out;
    
    RtlInitUnicodeString( &nameW, ComputerNameW );
    st = NtSetValueKey( hsubkey, &nameW, 0, REG_SZ, computer_name, len );

out:
    NtClose( hsubkey );
    NtClose( hkey );

    if ( st == STATUS_SUCCESS )
        TRACE( "success\n" );
    else
    {
        WARN( "status trying to set ComputerName: %lx\n", st );
        SetLastError ( RtlNtStatusToDosError ( st ) );
    }
}


/***********************************************************************
 *              GetComputerNameW         (KERNEL32.@)
 */
BOOL WINAPI GetComputerNameW(LPWSTR name,LPDWORD size)
{
    UNICODE_STRING nameW;
    OBJECT_ATTRIBUTES attr;
    HKEY hkey = INVALID_HANDLE_VALUE, hsubkey = INVALID_HANDLE_VALUE;
    char buf[offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data ) + (MAX_COMPUTERNAME_LENGTH + 1) * sizeof( WCHAR )];
    DWORD len = sizeof( buf );
    LPWSTR theName = (LPWSTR) (buf + offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data ));
    NTSTATUS st = STATUS_INVALID_PARAMETER;
    
    TRACE ("%p %p\n", name, size);

    _init_attr ( &attr, &nameW );
    RtlInitUnicodeString( &nameW, ComputerW );
    if ( ( st = NtOpenKey( &hkey, KEY_ALL_ACCESS, &attr ) ) != STATUS_SUCCESS )
        goto out;
         
    attr.RootDirectory = hkey;
    RtlInitUnicodeString( &nameW, ActiveComputerNameW );
    if ( ( st = NtOpenKey( &hsubkey, KEY_ALL_ACCESS, &attr ) ) != STATUS_SUCCESS )
        goto out;
    
    RtlInitUnicodeString( &nameW, ComputerNameW );
    if ( ( st = NtQueryValueKey( hsubkey, &nameW, KeyValuePartialInformation, buf, len, &len ) )
         != STATUS_SUCCESS )
        goto out;

    len = (len -offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data )) / sizeof (WCHAR) - 1;
    TRACE ("ComputerName is %s (length %lu)\n", debugstr_w ( theName ), len);

    __TRY
    {
        if ( *size < len )
        {
            memcpy ( name, theName, *size * sizeof (WCHAR) );
            name[*size] = 0;
            *size = len;
            st = STATUS_MORE_ENTRIES;
        }
        else
        {
            memcpy ( name, theName, len * sizeof (WCHAR) );
            name[len] = 0;
            *size = len;
            st = STATUS_SUCCESS;
        }
    }
    __EXCEPT(page_fault)
    {
        st = STATUS_INVALID_PARAMETER;
    }
    __ENDTRY

out:
    NtClose ( hsubkey );
    NtClose ( hkey );

    if ( st == STATUS_SUCCESS )
        return TRUE;
    else
    {
        SetLastError ( RtlNtStatusToDosError ( st ) );
        WARN ( "Status %lu reading computer name from registry\n", st );
        return FALSE;
    }
}

/***********************************************************************
 *              GetComputerNameA         (KERNEL32.@)
 */
BOOL WINAPI GetComputerNameA(LPSTR name, LPDWORD size)
{
    WCHAR nameW[ MAX_COMPUTERNAME_LENGTH + 1 ];
    DWORD sizeW = MAX_COMPUTERNAME_LENGTH;
    int len;
    BOOL ret;

    if ( !GetComputerNameW (nameW, &sizeW) ) return FALSE;

    len = WideCharToMultiByte ( CP_ACP, 0, nameW, sizeW, NULL, 0, NULL, 0 );
    __TRY
    {
        if ( *size < len )
        {
            WideCharToMultiByte ( CP_ACP, 0, nameW, sizeW, name, *size, NULL, 0 );
            name[*size] = 0;
            *size = len;
            SetLastError( ERROR_MORE_DATA );
            ret = FALSE;
        }
        else 
        {
            WideCharToMultiByte ( CP_ACP, 0, nameW, sizeW, name, len, NULL, 0 );
            name[len] = 0;
            *size = len;
            ret = TRUE;
        }
    }
    __EXCEPT(page_fault)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        ret = FALSE;
    }
    __ENDTRY

    return ret;
}

/***********************************************************************
 *              GetComputerNameExA         (KERNEL32.@)
 */
BOOL WINAPI GetComputerNameExA(COMPUTER_NAME_FORMAT type, LPSTR name, LPDWORD size)
{
    char buf[256];
    int len = sizeof (buf), ret;
    TRACE("%d, %p, %p\n", type, name, size);
    switch( type )
    {
    case ComputerNameNetBIOS:
    case ComputerNamePhysicalNetBIOS:
        return GetComputerNameA (name, size);
    case ComputerNameDnsHostname:
    case ComputerNamePhysicalDnsHostname:
        ret = dns_hostname (buf, &len);
        break;
    case ComputerNameDnsDomain:
    case ComputerNamePhysicalDnsDomain:
        ret = dns_domainname (buf, &len);
        break;
    case ComputerNameDnsFullyQualified:
    case ComputerNamePhysicalDnsFullyQualified:
        ret = dns_fqdn (buf, &len);
        break;
    default:
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if ( ret )
    {
        TRACE ("-> %s (%d)\n", debugstr_a (buf), len);
        __TRY
        {
            if ( *size < len )
            {
                memcpy( name, buf, *size );
                name[*size] = 0;
                *size = len;
                SetLastError( ERROR_MORE_DATA );
                ret = FALSE;
            }
            else
            {
                memcpy( name, buf, len );
                name[len] = 0;
                *size = len;
                ret = TRUE;
            }
        }
        __EXCEPT(page_fault)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return FALSE;
        }
        __ENDTRY
    }

    return ret;
}


/***********************************************************************
 *              GetComputerNameExW         (KERNEL32.@)
 */
BOOL WINAPI GetComputerNameExW( COMPUTER_NAME_FORMAT type, LPWSTR name, LPDWORD size )
{
    char buf[256];
    int len = sizeof (buf), ret;

    TRACE("%d, %p, %p\n", type, name, size);
    switch( type )
    {
    case ComputerNameNetBIOS:
    case ComputerNamePhysicalNetBIOS:
        return GetComputerNameW (name, size);
    case ComputerNameDnsHostname:
    case ComputerNamePhysicalDnsHostname:
        ret = dns_hostname (buf, &len);
        break;
    case ComputerNameDnsDomain:
    case ComputerNamePhysicalDnsDomain:
        ret = dns_domainname (buf, &len);
        break;
    case ComputerNameDnsFullyQualified:
    case ComputerNamePhysicalDnsFullyQualified:
        ret = dns_fqdn (buf, &len);
        break;
    default:
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if ( ret )
    {
        TRACE ("-> %s (%d)\n", debugstr_a (buf), len);
        __TRY
        {
            int lenW = MultiByteToWideChar( CP_ACP, 0, buf, len, NULL, 0 );
            if ( *size < lenW )
            {
                MultiByteToWideChar( CP_ACP, 0, buf, len, name, *size );
                name[*size] = 0;
                *size = lenW;
                SetLastError( ERROR_MORE_DATA );
                ret = FALSE;
            }
            else
            {
                MultiByteToWideChar( CP_ACP, 0, buf, len, name, lenW );
                name[lenW] = 0;
                *size = lenW;
                ret = TRUE;
            }
        }
        __EXCEPT(page_fault)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return FALSE;
        }
        __ENDTRY
    }

    return ret;
}

/******************************************************************************
 * netbios_char (INTERNAL)
 */
static WCHAR netbios_char ( WCHAR wc )
{
    static const WCHAR special[] = {'!','@','#','$','%','^','&','\'',')','(','-','_','{','}','~'};
    static const WCHAR deflt = '_';
    int i;
    
    if ( isalnumW ( wc ) ) return wc;
    for ( i = 0; i < sizeof (special) / sizeof (WCHAR); i++ )
        if ( wc == special[i] ) return wc;
    return deflt;
}

/******************************************************************************
 * SetComputerNameW [KERNEL32.@]
 *
 * PARAMS
 *    lpComputerName [I] Address of new computer name
 *
 * RETURNS STD
 */
BOOL WINAPI SetComputerNameW( LPCWSTR lpComputerName )
{
    UNICODE_STRING nameW;
    OBJECT_ATTRIBUTES attr;
    HKEY hkey = INVALID_HANDLE_VALUE, hsubkey = INVALID_HANDLE_VALUE;
    int plen = strlenW ( lpComputerName );
    int i;
    NTSTATUS st = STATUS_INTERNAL_ERROR;

    if (get_use_dns_option())
    {
        /* This check isn't necessary, but may help debugging problems. */
        WARN( "Disabled by Wine Configuration.\n" );
        WARN( "Set \"UseDnsComputerName\" = \"N\" in category [Network] to enable.\n" );
        SetLastError ( ERROR_ACCESS_DENIED );
        return FALSE;
    }

    TRACE( "%s\n", debugstr_w (lpComputerName) );

    /* Check parameter */
    if ( plen > MAX_COMPUTERNAME_LENGTH ) 
        goto out;

    /* This is NT behaviour. Win 95/98 would coerce characters. */
    for ( i = 0; i < plen; i++ )
    {
        WCHAR wc = lpComputerName[i];
        if ( wc != netbios_char( wc ) )
            goto out;
    }
    
    _init_attr ( &attr, &nameW );
    
    RtlInitUnicodeString (&nameW, ComputerW);
    if ( ( st = NtOpenKey( &hkey, KEY_ALL_ACCESS, &attr ) ) != STATUS_SUCCESS )
        goto out;
    attr.RootDirectory = hkey;
    RtlInitUnicodeString( &nameW, ComputerNameW );
    if ( ( st = NtOpenKey( &hsubkey, KEY_ALL_ACCESS, &attr ) ) != STATUS_SUCCESS )
        goto out;
    if ( ( st = NtSetValueKey( hsubkey, &nameW, 0, REG_SZ, lpComputerName, ( plen + 1) * sizeof(WCHAR) ) )
         != STATUS_SUCCESS )
        goto out;

out:
    NtClose( hsubkey );
    NtClose( hkey );
    
    if ( st == STATUS_SUCCESS )
    {
        TRACE( "ComputerName changed\n" );
        return TRUE;
    }

    else
    {
        SetLastError ( RtlNtStatusToDosError ( st ) );
        WARN ( "status %lu\n", st );
        return FALSE;
    }
}

/******************************************************************************
 * SetComputerNameA [KERNEL32.@]
 */
BOOL WINAPI SetComputerNameA( LPCSTR lpComputerName )
{
    BOOL ret;
    DWORD len = MultiByteToWideChar( CP_ACP, 0, lpComputerName, -1, NULL, 0 );
    LPWSTR nameW = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );

    MultiByteToWideChar( CP_ACP, 0, lpComputerName, -1, nameW, len );
    ret = SetComputerNameW( nameW );
    HeapFree( GetProcessHeap(), 0, nameW );
    return ret;
}

/******************************************************************************
 * SetComputerNameExW [KERNEL32.@]
 *
 */
BOOL WINAPI SetComputerNameExW( COMPUTER_NAME_FORMAT type, LPCWSTR lpComputerName )
{
    TRACE("%d, %s\n", type, debugstr_w (lpComputerName));
    switch( type )
    {
    case ComputerNameNetBIOS:
    case ComputerNamePhysicalNetBIOS:
        return SetComputerNameW( lpComputerName );
    default:
        SetLastError( ERROR_ACCESS_DENIED );
        return FALSE;
    }
}

/******************************************************************************
 * SetComputerNameExA [KERNEL32.@]
 *
 */
BOOL WINAPI SetComputerNameExA( COMPUTER_NAME_FORMAT type, LPCSTR lpComputerName )
{
    TRACE( "%d, %s\n", type, debugstr_a (lpComputerName) );
    switch( type )
    {
    case ComputerNameNetBIOS:
    case ComputerNamePhysicalNetBIOS:
        return SetComputerNameA( lpComputerName );
    default:
        SetLastError( ERROR_ACCESS_DENIED );
        return FALSE;
    }
}

/***********************************************************************
 *              DnsHostnameToComputerNameA         (KERNEL32.@)
 */
BOOL WINAPI DnsHostnameToComputerNameA(LPCSTR Hostname, LPSTR ComputerName,
                              LPDWORD nSize)
{
    FIXME("(%s, %s, %08lx): stub\n", debugstr_a(Hostname),
          debugstr_a(ComputerName), *nSize);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/***********************************************************************
 *              DnsHostnameToComputerNameW         (KERNEL32.@)
 */
BOOL WINAPI DnsHostnameToComputerNameW(LPCWSTR Hostname, LPWSTR ComputerName,
                              LPDWORD nSize)
{
    FIXME("(%s, %s, %08lx): stub\n", debugstr_w(Hostname),
          debugstr_w(ComputerName), *nSize);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}
