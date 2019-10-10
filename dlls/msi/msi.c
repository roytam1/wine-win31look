/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002,2003 Mike McCormack for CodeWeavers
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

#define NONAMELESSUNION

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winnls.h"
#include "shlwapi.h"
#include "wine/debug.h"
#include "msi.h"
#include "msiquery.h"
#include "msipriv.h"
#include "objidl.h"
#include "wincrypt.h"
#include "objbase.h"

WINE_DEFAULT_DEBUG_CHANNEL(msi);

/*
 * The MSVC headers define the MSIDBOPEN_* macros cast to LPCTSTR,
 *  which is a problem because LPCTSTR isn't defined when compiling wine.
 * To work around this problem, we need to define LPCTSTR as LPCWSTR here,
 *  and make sure to only use it in W functions.
 */
#define LPCTSTR LPCWSTR

const WCHAR szInstaller[] = {
'S','o','f','t','w','a','r','e','\\',
'M','i','c','r','o','s','o','f','t','\\',
'W','i','n','d','o','w','s','\\',
'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
'I','n','s','t','a','l','l','e','r',0 };

const WCHAR szFeatures[] = {
'F','e','a','t','u','r','e','s',0 };
const WCHAR szComponents[] = {
'C','o','m','p','o','n','e','n','t','s',0 };

/*
 *  .MSI  file format
 *
 *  A .msi file is a structured storage file.
 *  It should contain a number of streams.
 */

BOOL unsquash_guid(LPCWSTR in, LPWSTR out)
{
    DWORD i,n=0;

    out[n++]='{';
    for(i=0; i<8; i++)
        out[n++] = in[7-i];
    out[n++]='-';
    for(i=0; i<4; i++)
        out[n++] = in[11-i];
    out[n++]='-';
    for(i=0; i<4; i++)
        out[n++] = in[15-i];
    out[n++]='-';
    for(i=0; i<2; i++)
    {
        out[n++] = in[17+i*2];
        out[n++] = in[16+i*2];
    }
    out[n++]='-';
    for( ; i<8; i++)
    {
        out[n++] = in[17+i*2];
        out[n++] = in[16+i*2];
    }
    out[n++]='}';
    out[n]=0;
    return TRUE;
}

BOOL squash_guid(LPCWSTR in, LPWSTR out)
{
    DWORD i,n=0;

    if(in[n++] != '{')
        return FALSE;
    for(i=0; i<8; i++)
        out[7-i] = in[n++];
    if(in[n++] != '-')
        return FALSE;
    for(i=0; i<4; i++)
        out[11-i] = in[n++];
    if(in[n++] != '-')
        return FALSE;
    for(i=0; i<4; i++)
        out[15-i] = in[n++];
    if(in[n++] != '-')
        return FALSE;
    for(i=0; i<2; i++)
    {
        out[17+i*2] = in[n++];
        out[16+i*2] = in[n++];
    }
    if(in[n++] != '-')
        return FALSE;
    for( ; i<8; i++)
    {
        out[17+i*2] = in[n++];
        out[16+i*2] = in[n++];
    }
    out[32]=0;
    if(in[n++] != '}')
        return FALSE;
    if(in[n])
        return FALSE;
    return TRUE;
}

VOID MSI_CloseDatabase( VOID *arg )
{
    MSIDATABASE *db = (MSIDATABASE *) arg;

    free_cached_tables( db );
    IStorage_Release( db->storage );
}

UINT WINAPI MsiOpenDatabaseA(
               LPCSTR szDBPath, LPCSTR szPersist, MSIHANDLE *phDB)
{
    HRESULT r = ERROR_FUNCTION_FAILED;
    LPWSTR szwDBPath = NULL, szwPersist = NULL;
    UINT len;

    TRACE("%s %s %p\n", debugstr_a(szDBPath), debugstr_a(szPersist), phDB);

    if( szDBPath )
    {
        len = MultiByteToWideChar( CP_ACP, 0, szDBPath, -1, NULL, 0 );
        szwDBPath = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        if( !szwDBPath )
            goto end;
        MultiByteToWideChar( CP_ACP, 0, szDBPath, -1, szwDBPath, len );
    }

    if( HIWORD(szPersist) )
    {
        len = MultiByteToWideChar( CP_ACP, 0, szPersist, -1, NULL, 0 );
        szwPersist = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        if( !szwPersist )
            goto end;
        MultiByteToWideChar( CP_ACP, 0, szPersist, -1, szwPersist, len );
    }
    else
        szwPersist = (LPWSTR) szPersist;

    r = MsiOpenDatabaseW( szwDBPath, szwPersist, phDB );

end:
    if( szwPersist )
        HeapFree( GetProcessHeap(), 0, szwPersist );
    if( szwDBPath )
        HeapFree( GetProcessHeap(), 0, szwDBPath );

    return r;
}

UINT WINAPI MsiOpenDatabaseW(
              LPCWSTR szDBPath, LPCWSTR szPersist, MSIHANDLE *phDB)
{
    IStorage *stg = NULL;
    HRESULT r;
    MSIHANDLE handle;
    MSIDATABASE *db;
    UINT ret;
    LPWSTR szMode;

    TRACE("%s %s %p\n",debugstr_w(szDBPath),debugstr_w(szPersist), phDB);

    if( !phDB )
        return ERROR_INVALID_PARAMETER;

    szMode = (LPWSTR) szPersist;
    if( HIWORD( szPersist ) )
    {
        /* UINT len = lstrlenW( szPerist ) + 1; */
        FIXME("don't support persist files yet\b");
        return ERROR_INVALID_PARAMETER;
        /* szMode = HeapAlloc( GetProcessHeap(), 0, len * sizeof (DWORD) ); */
    }
    else if( szPersist == MSIDBOPEN_READONLY )
    {
        r = StgOpenStorage( szDBPath, NULL,
              STGM_DIRECT|STGM_READ|STGM_SHARE_DENY_WRITE, NULL, 0, &stg);
    }
    else if( szPersist == MSIDBOPEN_CREATE )
    {
        r = StgCreateDocfile( szDBPath, 
              STGM_DIRECT|STGM_READWRITE|STGM_SHARE_EXCLUSIVE, 0, &stg);
        if( r == ERROR_SUCCESS )
            r = init_string_table( stg );
    }
    else if( szPersist == MSIDBOPEN_TRANSACT )
    {
        r = StgOpenStorage( szDBPath, NULL,
              STGM_DIRECT|STGM_READWRITE|STGM_SHARE_EXCLUSIVE, NULL, 0, &stg);
    }
    else
    {
        ERR("unknown flag %p\n",szPersist);
        return ERROR_INVALID_PARAMETER;
    }

    if( FAILED( r ) )
    {
        FIXME("open failed r = %08lx!\n",r);
        return ERROR_FUNCTION_FAILED;
    }

    handle = alloc_msihandle( MSIHANDLETYPE_DATABASE, sizeof (MSIDATABASE),
                              MSI_CloseDatabase, (void**) &db );
    if( !handle )
    {
        FIXME("Failed to allocate a handle\n");
        ret = ERROR_FUNCTION_FAILED;
        goto end;
    }

    db->storage = stg;
    db->mode = szMode;
    /* db->strings = NULL;
    db->first_table = NULL;
    db->last_table = NULL; */

    ret = load_string_table( db );
    if( ret != ERROR_SUCCESS )
        goto end;

    *phDB = handle;

    IStorage_AddRef( stg );
end:
    if( stg )
        IStorage_Release( stg );

    return ret;
}

UINT WINAPI MsiOpenProductA(LPCSTR szProduct, MSIHANDLE *phProduct)
{
    UINT len, ret;
    LPWSTR szwProd = NULL;

    TRACE("%s %p\n",debugstr_a(szProduct), phProduct);

    if( szProduct )
    {
        len = MultiByteToWideChar( CP_ACP, 0, szProduct, -1, NULL, 0 );
        szwProd = HeapAlloc( GetProcessHeap(), 0, len * sizeof (WCHAR) );
        if( szwProd )
            MultiByteToWideChar( CP_ACP, 0, szProduct, -1, szwProd, len );
    }

    ret = MsiOpenProductW( szwProd, phProduct );

    if( szwProd )
        HeapFree( GetProcessHeap(), 0, szwProd );

    return ret;
}

UINT WINAPI MsiOpenProductW(LPCWSTR szProduct, MSIHANDLE *phProduct)
{
    const WCHAR szKey[] = {
        'S','o','f','t','w','a','r','e','\\',
        'M','i','c','r','o','s','o','f','t','\\',
        'W','i','n','d','o','w','s','\\',
        'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
        'U','n','i','n','s','t','a','l','l',0 };
    const WCHAR szLocalPackage[] = {
        'L','o','c','a','l','P','a','c','k','a','g','e', 0
    };
    LPWSTR path = NULL;
    UINT r;
    HKEY hKeyProduct = NULL, hKeyUninstall = NULL;
    DWORD count, type;

    TRACE("%s %p\n",debugstr_w(szProduct), phProduct);

    r = RegOpenKeyW( HKEY_LOCAL_MACHINE, szKey, &hKeyUninstall );
    if( r != ERROR_SUCCESS )
        return r;

    r = RegOpenKeyW( hKeyUninstall, szProduct, &hKeyProduct );
    if( r != ERROR_SUCCESS )
        goto end;

    /* find the size of the path */
    type = count = 0;
    r = RegQueryValueExW( hKeyProduct, szLocalPackage,
                          NULL, &type, NULL, &count );
    if( r != ERROR_SUCCESS )
        goto end;

    /* now alloc and fetch the path of the database to open */
    path = HeapAlloc( GetProcessHeap(), 0, count );
    if( !path )
        goto end;

    r = RegQueryValueExW( hKeyProduct, szLocalPackage,
                          NULL, &type, (LPBYTE) path, &count );
    if( r != ERROR_SUCCESS )
        goto end;

    r = MsiOpenPackageW( path, phProduct );

end:
    if( path )
        HeapFree( GetProcessHeap(), 0, path );
    if( hKeyProduct )
        RegCloseKey( hKeyProduct );
    RegCloseKey( hKeyUninstall );

    return r;
}

UINT WINAPI MsiOpenPackageA(LPCSTR szPackage, MSIHANDLE *phPackage)
{
    FIXME("%s %p\n",debugstr_a(szPackage), phPackage);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiOpenPackageW(LPCWSTR szPackage, MSIHANDLE *phPackage)
{
    FIXME("%s %p\n",debugstr_w(szPackage), phPackage);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiOpenPackageExA(LPCSTR szPackage, DWORD dwOptions, MSIHANDLE *phPackage)
{
    FIXME("%s 0x%08lx %p\n",debugstr_a(szPackage), dwOptions, phPackage);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiOpenPackageExW(LPCWSTR szPackage, DWORD dwOptions, MSIHANDLE *phPackage)
{
    FIXME("%s 0x%08lx %p\n",debugstr_w(szPackage), dwOptions, phPackage);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiAdvertiseProductA(LPCSTR szPackagePath, LPCSTR szScriptfilePath, LPCSTR szTransforms, LANGID lgidLanguage)
{
    FIXME("%s %s %s 0x%08x\n",debugstr_a(szPackagePath), debugstr_a(szScriptfilePath), debugstr_a(szTransforms), lgidLanguage);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiAdvertiseProductW(LPCWSTR szPackagePath, LPCWSTR szScriptfilePath, LPCWSTR szTransforms, LANGID lgidLanguage)
{
    FIXME("%s %s %s 0x%08x\n",debugstr_w(szPackagePath), debugstr_w(szScriptfilePath), debugstr_w(szTransforms), lgidLanguage);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiAdvertiseProductExA(
    LPCSTR szPackagePath, LPCSTR szScriptfilePath, LPCSTR szTransforms, LANGID lgidLanguage, DWORD dwPlatform, DWORD dwOptions)
{
    FIXME("%s %s %s 0x%08x 0x%08lx 0x%08lx\n",
	debugstr_a(szPackagePath), debugstr_a(szScriptfilePath), debugstr_a(szTransforms), lgidLanguage, dwPlatform, dwOptions);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiAdvertiseProductExW(
    LPCWSTR szPackagePath, LPCWSTR szScriptfilePath, LPCWSTR szTransforms, LANGID lgidLanguage, DWORD dwPlatform, DWORD dwOptions)
{
    FIXME("%s %s %s 0x%08x 0x%08lx 0x%08lx\n",
	debugstr_w(szPackagePath), debugstr_w(szScriptfilePath), debugstr_w(szTransforms), lgidLanguage, dwPlatform, dwOptions);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiInstallProductA(LPCSTR szPackagePath, LPCSTR szCommandLine)
{
    LPWSTR szwPath = NULL, szwCommand = NULL;
    UINT r = ERROR_FUNCTION_FAILED; /* FIXME: check return code */

    TRACE("%s %s\n",debugstr_a(szPackagePath), debugstr_a(szCommandLine));

    if( szPackagePath )
    {
        UINT len = MultiByteToWideChar( CP_ACP, 0, szPackagePath, -1, NULL, 0 );
        szwPath = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        if( !szwPath )
            goto end;
        MultiByteToWideChar( CP_ACP, 0, szPackagePath, -1, szwPath, len );
    }

    if( szCommandLine )
    {
        UINT len = MultiByteToWideChar( CP_ACP, 0, szCommandLine, -1, NULL, 0 );
        szwCommand = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) );
        if( !szwCommand )
            goto end;
        MultiByteToWideChar( CP_ACP, 0, szCommandLine, -1, szwCommand, len );
    }
 
    r = MsiInstallProductW( szwPath, szwCommand );

end:
    if( szwPath )
        HeapFree( GetProcessHeap(), 0, szwPath );
    
    if( szwCommand )
        HeapFree( GetProcessHeap(), 0, szwCommand );

    return r;
}

UINT WINAPI MsiInstallProductW(LPCWSTR szPackagePath, LPCWSTR szCommandLine)
{
    FIXME("%s %s\n",debugstr_w(szPackagePath), debugstr_w(szCommandLine));

    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiConfigureProductA(
              LPCSTR szProduct, int iInstallLevel, INSTALLSTATE eInstallState)
{
    FIXME("%s %d %d\n",debugstr_a(szProduct), iInstallLevel, eInstallState);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiConfigureProductW(
              LPCWSTR szProduct, int iInstallLevel, INSTALLSTATE eInstallState)
{
    FIXME("%s %d %d\n",debugstr_w(szProduct), iInstallLevel, eInstallState);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiGetProductCodeA(LPCSTR szComponent, LPSTR szBuffer)
{
    FIXME("%s %s\n",debugstr_a(szComponent), debugstr_a(szBuffer));
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiGetProductCodeW(LPCWSTR szComponent, LPWSTR szBuffer)
{
    FIXME("%s %s\n",debugstr_w(szComponent), debugstr_w(szBuffer));
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiGetProductInfoA(LPCSTR szProduct, LPCSTR szAttribute, LPSTR szBuffer, DWORD *pcchValueBuf)
{
    FIXME("%s %s %p %p\n",debugstr_a(szProduct), debugstr_a(szAttribute), szBuffer, pcchValueBuf);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiGetProductInfoW(LPCWSTR szProduct, LPCWSTR szAttribute, LPWSTR szBuffer, DWORD *pcchValueBuf)
{
    FIXME("%s %s %p %p\n",debugstr_w(szProduct), debugstr_w(szAttribute), szBuffer, pcchValueBuf);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiDatabaseImportA(LPCSTR szFolderPath, LPCSTR szFilename)
{
    FIXME("%s %s\n",debugstr_a(szFolderPath), debugstr_a(szFilename));
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiDatabaseImportW(LPCWSTR szFolderPath, LPCWSTR szFilename)
{
    FIXME("%s %s\n",debugstr_w(szFolderPath), debugstr_w(szFilename));
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiEnableLogA(DWORD dwLogMode, LPCSTR szLogFile, BOOL fAppend)
{
    FIXME("%08lx %s %d\n", dwLogMode, debugstr_a(szLogFile), fAppend);
    return ERROR_SUCCESS;
    /* return ERROR_CALL_NOT_IMPLEMENTED; */
}

UINT WINAPI MsiEnableLogW(DWORD dwLogMode, LPCWSTR szLogFile, BOOL fAppend)
{
    FIXME("%08lx %s %d\n", dwLogMode, debugstr_w(szLogFile), fAppend);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

INSTALLSTATE WINAPI MsiQueryProductStateA(LPCSTR szProduct)
{
    FIXME("%s\n", debugstr_a(szProduct));
    return INSTALLSTATE_UNKNOWN;
}

INSTALLSTATE WINAPI MsiQueryProductStateW(LPCWSTR szProduct)
{
    FIXME("%s\n", debugstr_w(szProduct));
    return INSTALLSTATE_UNKNOWN;
}

INSTALLUILEVEL WINAPI MsiSetInternalUI(INSTALLUILEVEL dwUILevel, HWND *phWnd)
{
    FIXME("%08x %p\n", dwUILevel, phWnd);
    return dwUILevel;
}

UINT WINAPI MsiLoadStringA(DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f)
{
    FIXME("%08lx %08lx %08lx %08lx %08lx %08lx\n",a,b,c,d,e,f);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiLoadStringW(DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f)
{
    FIXME("%08lx %08lx %08lx %08lx %08lx %08lx\n",a,b,c,d,e,f);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiMessageBoxA(DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f)
{
    FIXME("%08lx %08lx %08lx %08lx %08lx %08lx\n",a,b,c,d,e,f);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiMessageBoxW(DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f)
{
    FIXME("%08lx %08lx %08lx %08lx %08lx %08lx\n",a,b,c,d,e,f);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiEnumProductsA(DWORD index, LPSTR lpguid)
{
    DWORD r;
    WCHAR szwGuid[GUID_SIZE];

    TRACE("%ld %p\n",index,lpguid);

    r = MsiEnumProductsW(index, szwGuid);
    if( r == ERROR_SUCCESS )
        WideCharToMultiByte(CP_ACP, 0, szwGuid, -1, lpguid, GUID_SIZE, NULL, NULL);

    return r;
}

UINT WINAPI MsiEnumProductsW(DWORD index, LPWSTR lpguid)
{
    HKEY hkey = 0, hkeyFeatures = 0;
    DWORD r;
    WCHAR szKeyName[33];

    TRACE("%ld %p\n",index,lpguid);

    r = RegOpenKeyW(HKEY_LOCAL_MACHINE, szInstaller, &hkey);
    if( r != ERROR_SUCCESS )
        goto end;

    r = RegOpenKeyW(hkey, szFeatures, &hkeyFeatures);
    if( r != ERROR_SUCCESS )
        goto end;

    r = RegEnumKeyW(hkeyFeatures, index, szKeyName, GUID_SIZE);

    unsquash_guid(szKeyName, lpguid);

end:

    if( hkeyFeatures )
        RegCloseKey(hkeyFeatures);
    if( hkey )
        RegCloseKey(hkey);

    return r;
}

UINT WINAPI MsiEnumFeaturesA(LPCSTR szProduct, DWORD index, 
      LPSTR szFeature, LPSTR szParent)
{
    DWORD r;
    WCHAR szwFeature[GUID_SIZE], szwParent[GUID_SIZE];
    LPWSTR szwProduct = NULL;

    TRACE("%s %ld %p %p\n",debugstr_a(szProduct),index,szFeature,szParent);

    if( szProduct )
    {
        UINT len = MultiByteToWideChar( CP_ACP, 0, szProduct, -1, NULL, 0 );
        szwProduct = HeapAlloc( GetProcessHeap(), 0, len * sizeof (WCHAR) );
        if( szwProduct )
            MultiByteToWideChar( CP_ACP, 0, szProduct, -1, szwProduct, len );
        else
            return ERROR_FUNCTION_FAILED;
    }

    r = MsiEnumFeaturesW(szwProduct, index, szwFeature, szwParent);
    if( r == ERROR_SUCCESS )
    {
        WideCharToMultiByte(CP_ACP, 0, szwFeature, -1,
                            szFeature, GUID_SIZE, NULL, NULL);
        WideCharToMultiByte(CP_ACP, 0, szwParent, -1,
                            szParent, GUID_SIZE, NULL, NULL);
    }

    if( szwProduct )
        HeapFree( GetProcessHeap(), 0, szwProduct);

    return r;
}

UINT WINAPI MsiEnumFeaturesW(LPCWSTR szProduct, DWORD index, 
      LPWSTR szFeature, LPWSTR szParent)
{
    HKEY hkey = 0, hkeyFeatures = 0, hkeyProduct = 0;
    DWORD r, sz;
    WCHAR szRegName[GUID_SIZE];

    TRACE("%s %ld %p %p\n",debugstr_w(szProduct),index,szFeature,szParent);

    if( !squash_guid(szProduct, szRegName) )
        return ERROR_INVALID_PARAMETER;

    r = RegOpenKeyW(HKEY_LOCAL_MACHINE, szInstaller, &hkey);
    if( r != ERROR_SUCCESS )
        goto end;

    r = RegOpenKeyW(hkey, szFeatures, &hkeyFeatures);
    if( r != ERROR_SUCCESS )
        goto end;

    r = RegOpenKeyW(hkeyFeatures, szRegName, &hkeyProduct);
    if( r != ERROR_SUCCESS )
        goto end;

    sz = GUID_SIZE;
    r = RegEnumValueW(hkeyProduct, index, szFeature, &sz, NULL, NULL, NULL, NULL);

end:
    if( hkeyProduct )
        RegCloseKey(hkeyProduct);
    if( hkeyFeatures )
        RegCloseKey(hkeyFeatures);
    if( hkey )
        RegCloseKey(hkey);

    return r;
}

UINT WINAPI MsiEnumComponentsA(DWORD index, LPSTR lpguid)
{
    DWORD r;
    WCHAR szwGuid[GUID_SIZE];

    TRACE("%ld %p\n",index,lpguid);

    r = MsiEnumComponentsW(index, szwGuid);
    if( r == ERROR_SUCCESS )
        WideCharToMultiByte(CP_ACP, 0, szwGuid, -1, lpguid, GUID_SIZE, NULL, NULL);

    return r;
}

UINT WINAPI MsiEnumComponentsW(DWORD index, LPWSTR lpguid)
{
    HKEY hkey = 0, hkeyComponents = 0;
    DWORD r;
    WCHAR szKeyName[33];

    TRACE("%ld %p\n",index,lpguid);

    r = RegOpenKeyW(HKEY_LOCAL_MACHINE, szInstaller, &hkey);
    if( r != ERROR_SUCCESS )
        goto end;

    r = RegOpenKeyW(hkey, szComponents, &hkeyComponents);
    if( r != ERROR_SUCCESS )
        goto end;

    r = RegEnumKeyW(hkeyComponents, index, szKeyName, GUID_SIZE);

    unsquash_guid(szKeyName, lpguid);

end:

    if( hkeyComponents )
        RegCloseKey(hkeyComponents);
    if( hkey )
        RegCloseKey(hkey);

    return r;
}

UINT WINAPI MsiEnumClientsA(LPCSTR szComponent, DWORD index, LPSTR szProduct)
{
    DWORD r;
    WCHAR szwProduct[GUID_SIZE];
    LPWSTR szwComponent = NULL;

    TRACE("%s %ld %p\n",debugstr_a(szComponent),index,szProduct);

    if( szComponent )
    {
        UINT len = MultiByteToWideChar( CP_ACP, 0, szComponent, -1, NULL, 0 );
        szwComponent = HeapAlloc( GetProcessHeap(), 0, len * sizeof (WCHAR) );
        if( szwComponent )
            MultiByteToWideChar( CP_ACP, 0, szComponent, -1, szwComponent, len );
        else
            return ERROR_FUNCTION_FAILED;
    }

    r = MsiEnumClientsW(szComponent?szwComponent:NULL, index, szwProduct);
    if( r == ERROR_SUCCESS )
    {
        WideCharToMultiByte(CP_ACP, 0, szwProduct, -1,
                            szProduct, GUID_SIZE, NULL, NULL);
    }

    if( szwComponent )
        HeapFree( GetProcessHeap(), 0, szwComponent);

    return r;
}

UINT WINAPI MsiEnumClientsW(LPCWSTR szComponent, DWORD index, LPWSTR szProduct)
{
    HKEY hkey = 0, hkeyComponents = 0, hkeyComp = 0;
    DWORD r, sz;
    WCHAR szRegName[GUID_SIZE], szValName[GUID_SIZE];

    TRACE("%s %ld %p\n",debugstr_w(szComponent),index,szProduct);

    if( !squash_guid(szComponent, szRegName) )
        return ERROR_INVALID_PARAMETER;

    r = RegOpenKeyW(HKEY_LOCAL_MACHINE, szInstaller, &hkey);
    if( r != ERROR_SUCCESS )
        goto end;

    r = RegOpenKeyW(hkey, szComponents, &hkeyComponents);
    if( r != ERROR_SUCCESS )
        goto end;

    r = RegOpenKeyW(hkeyComponents, szRegName, &hkeyComp);
    if( r != ERROR_SUCCESS )
        goto end;

    sz = GUID_SIZE;
    r = RegEnumValueW(hkeyComp, index, szValName, &sz, NULL, NULL, NULL, NULL);
    if( r != ERROR_SUCCESS )
        goto end;

    unsquash_guid(szValName, szProduct);

end:
    if( hkeyComp )
        RegCloseKey(hkeyComp);
    if( hkeyComponents )
        RegCloseKey(hkeyComponents);
    if( hkey )
        RegCloseKey(hkey);

    return r;
}

UINT WINAPI MsiEnumComponentQualifiersA(
    LPSTR szComponent, DWORD iIndex, LPSTR lpQualifierBuf, DWORD* pcchQualifierBuf, LPSTR lpApplicationDataBuf, DWORD* pcchApplicationDataBuf)
{
FIXME("%s 0x%08lx %p %p %p %p\n", debugstr_a(szComponent), iIndex, lpQualifierBuf, pcchQualifierBuf, lpApplicationDataBuf, pcchApplicationDataBuf);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiEnumComponentQualifiersW(
    LPWSTR szComponent, DWORD iIndex, LPWSTR lpQualifierBuf, DWORD* pcchQualifierBuf, LPWSTR lpApplicationDataBuf, DWORD* pcchApplicationDataBuf)
{
FIXME("%s 0x%08lx %p %p %p %p\n", debugstr_w(szComponent), iIndex, lpQualifierBuf, pcchQualifierBuf, lpApplicationDataBuf, pcchApplicationDataBuf);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiProvideAssemblyA(
    LPCSTR szAssemblyName, LPCSTR szAppContext, DWORD dwInstallMode, DWORD dwAssemblyInfo, LPSTR lpPathBuf, DWORD* pcchPathBuf) 
{
    FIXME("%s %s 0x%08lx 0x%08lx %p %p\n", 
	debugstr_a(szAssemblyName),  debugstr_a(szAppContext), dwInstallMode, dwAssemblyInfo, lpPathBuf, pcchPathBuf);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiProvideAssemblyW(
    LPCWSTR szAssemblyName, LPCWSTR szAppContext, DWORD dwInstallMode, DWORD dwAssemblyInfo, LPWSTR lpPathBuf, DWORD* pcchPathBuf) 
{
    FIXME("%s %s 0x%08lx 0x%08lx %p %p\n", 
	debugstr_w(szAssemblyName),  debugstr_w(szAppContext), dwInstallMode, dwAssemblyInfo, lpPathBuf, pcchPathBuf);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiProvideComponentFromDescriptorA( LPCSTR szDescriptor, LPSTR szPath, DWORD *pcchPath, DWORD *pcchArgs )
{
    FIXME("%s %p %p %p\n", debugstr_a(szDescriptor), szPath, pcchPath, pcchArgs );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiProvideComponentFromDescriptorW( LPCWSTR szDescriptor, LPWSTR szPath, DWORD *pcchPath, DWORD *pcchArgs )
{
    FIXME("%s %p %p %p\n", debugstr_w(szDescriptor), szPath, pcchPath, pcchArgs );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

HRESULT WINAPI MsiGetFileSignatureInformationA(
  LPCSTR szSignedObjectPath, DWORD dwFlags, PCCERT_CONTEXT* ppcCertContext, BYTE* pbHashData, DWORD* pcbHashData)
{
    FIXME("%s 0x%08lx %p %p %p\n", debugstr_a(szSignedObjectPath), dwFlags, ppcCertContext, pbHashData, pcbHashData);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

HRESULT WINAPI MsiGetFileSignatureInformationW(
  LPCWSTR szSignedObjectPath, DWORD dwFlags, PCCERT_CONTEXT* ppcCertContext, BYTE* pbHashData, DWORD* pcbHashData)
{
    FIXME("%s 0x%08lx %p %p %p\n", debugstr_w(szSignedObjectPath), dwFlags, ppcCertContext, pbHashData, pcbHashData);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiGetProductPropertyA( MSIHANDLE hProduct, LPCSTR szProperty,
                                    LPSTR szValue, DWORD *pccbValue )
{
    FIXME("%ld %s %p %p\n", hProduct, debugstr_a(szProperty), szValue, pccbValue);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiGetProductPropertyW( MSIHANDLE hProduct, LPCWSTR szProperty,
                                    LPWSTR szValue, DWORD *pccbValue )
{
    FIXME("%ld %s %p %p\n", hProduct, debugstr_w(szProperty), szValue, pccbValue);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiVerifyPackageA( LPCSTR szPackage )
{
    FIXME("%s\n", debugstr_a(szPackage) );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiVerifyPackageW( LPCWSTR szPackage )
{
    FIXME("%s\n", debugstr_w(szPackage) );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************
 *		DllMain
 *
 * @todo: maybe we can check here if MsiServer service is declared no ?
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  if (fdwReason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hinstDLL);
    /* FIXME: Initialisation */
  } else if (fdwReason == DLL_PROCESS_DETACH) {
    /* FIXME: Cleanup */
  }
  /*
  const WCHAR szMSIServerSvc[] = { 'M','S','I','S','e','r','v','e','r',0 };
  const WCHAR szNull[] = { 0 };
  if (!strcmpW(lpServiceName, szMSIServerSvc)) {
    hKey = CreateServiceW(hSCManager, 
			  szMSIServerSvc, 
			  szMSIServerSvc, 
			  SC_MANAGER_ALL_ACCESS, 
			  SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS, 
			  SERVICE_AUTO_START, 
			  SERVICE_ERROR_IGNORE,
			  szNull, 
			  NULL, 
			  NULL,
			  NULL,
			  NULL,
			  szNull);
  */
  return TRUE;
}

typedef struct {
  /* IUnknown fields */
  ICOM_VFIELD(IClassFactory);
  DWORD                       ref;
} IClassFactoryImpl;

static HRESULT WINAPI MsiCF_QueryInterface(LPCLASSFACTORY iface,REFIID riid,LPVOID *ppobj) {
  ICOM_THIS(IClassFactoryImpl,iface);
  FIXME("(%p, %s, %p): stub\n",This,debugstr_guid(riid),ppobj);
  return E_NOINTERFACE;
}

static ULONG WINAPI MsiCF_AddRef(LPCLASSFACTORY iface) {
  ICOM_THIS(IClassFactoryImpl,iface);
  return ++(This->ref);
}

static ULONG WINAPI MsiCF_Release(LPCLASSFACTORY iface) {
  ICOM_THIS(IClassFactoryImpl,iface);
  /* static class, won't be  freed */
  return --(This->ref);
}

static HRESULT WINAPI MsiCF_CreateInstance(LPCLASSFACTORY iface, LPUNKNOWN pOuter, REFIID riid, LPVOID *ppobj) {
  ICOM_THIS(IClassFactoryImpl,iface);
  FIXME ("(%p, %p, %s, %p): to implement\n", This, pOuter, debugstr_guid(riid), ppobj);
  return 0;
}

static HRESULT WINAPI MsiCF_LockServer(LPCLASSFACTORY iface,BOOL dolock) {
  ICOM_THIS(IClassFactoryImpl,iface);
  FIXME("(%p, %d): stub\n", This, dolock);
  return S_OK;
}

static ICOM_VTABLE(IClassFactory) MsiCF_Vtbl = {
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  MsiCF_QueryInterface,
  MsiCF_AddRef,
  MsiCF_Release,
  MsiCF_CreateInstance,
  MsiCF_LockServer
};

static IClassFactoryImpl Msi_CF = {&MsiCF_Vtbl, 1 };

HRESULT WINAPI MSI_DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
  FIXME("(%s, %s, %p): almost a stub.\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
  if (IsEqualCLSID (rclsid, &CLSID_IMsiServer)) {
    *ppv = (LPVOID) &Msi_CF;
    IClassFactory_AddRef((IClassFactory*)*ppv);
    return S_OK;
  } else if (IsEqualCLSID (rclsid, &CLSID_IMsiServerMessage)) {
    *ppv = (LPVOID) &Msi_CF;
    IClassFactory_AddRef((IClassFactory*)*ppv);
    return S_OK;
  } else if (IsEqualCLSID (rclsid, &CLSID_IMsiServerX1)) {
    *ppv = (LPVOID) &Msi_CF;
    IClassFactory_AddRef((IClassFactory*)*ppv);
    return S_OK;
  } else if (IsEqualCLSID (rclsid, &CLSID_IMsiServerX2)) {
    *ppv = (LPVOID) &Msi_CF;
    IClassFactory_AddRef((IClassFactory*)*ppv);
    return S_OK;
  } else if (IsEqualCLSID (rclsid, &CLSID_IMsiServerX3)) {
    *ppv = (LPVOID) &Msi_CF;
    IClassFactory_AddRef((IClassFactory*)*ppv);
    return S_OK;
  }
  WARN("(%s, %s, %p): no interface found.\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
  return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI MSI_DllGetVersion(DLLVERSIONINFO *pdvi)
{
  TRACE("%p\n",pdvi);
  
  if (pdvi->cbSize != sizeof(DLLVERSIONINFO))
    return E_INVALIDARG;
  
  pdvi->dwMajorVersion = MSI_MAJORVERSION;
  pdvi->dwMinorVersion = MSI_MINORVERSION;
  pdvi->dwBuildNumber = MSI_BUILDNUMBER;
  pdvi->dwPlatformID = 1;
  
  return S_OK;
}

BOOL WINAPI MSI_DllCanUnloadNow(void)
{
  return S_FALSE;
}
