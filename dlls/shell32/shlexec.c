/*
 * 				Shell Library Functions
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 2002 Eric Pouech
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

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <ctype.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "wownt32.h"
#include "shellapi.h"
#include "wingdi.h"
#include "winuser.h"
#include "shlobj.h"
#include "shlwapi.h"
#include "ddeml.h"

#include "wine/winbase16.h"
#include "shell32_main.h"
#include "undocshell.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(exec);

static const WCHAR wszOpen[] = {'o','p','e','n',0};
static const WCHAR wszExe[] = {'.','e','x','e',0};
static const WCHAR wszILPtr[] = {':','%','p',0};
static const WCHAR wszShell[] = {'\\','s','h','e','l','l','\\',0};


/***********************************************************************
 *	SHELL_ArgifyW [Internal]
 *
 * this function is supposed to expand the escape sequences found in the registry
 * some diving reported that the following were used:
 * + %1, %2...  seem to report to parameter of index N in ShellExecute pmts
 *	%1 file
 *	%2 printer
 *	%3 driver
 *	%4 port
 * %I address of a global item ID (explorer switch /idlist)
 * %L seems to be %1 as long filename followed by the 8+3 variation
 * %S ???
 * %* all following parameters (see batfile)
 *
 * FIXME: use 'len'
 */
static BOOL SHELL_ArgifyW(WCHAR* out, int len, const WCHAR* fmt, const WCHAR* lpFile, LPITEMIDLIST pidl, LPCWSTR args)
{
    WCHAR   xlpFile[1024];
    BOOL    done = FALSE;
    PWSTR   res = out;
    PCWSTR  cmd;
    LPVOID  pv;

    while (*fmt)
    {
        if (*fmt == '%')
        {
            switch (*++fmt)
            {
            case '\0':
            case '%':
                *res++ = '%';
                break;

            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '0':
            case '*':
                if (args)
                {
                    if (*fmt == '*')
                    {
                        *res++ = '"';
                        while(*args)
                            *res++ = *args++;
                        *res++ = '"';
                    }
                    else
                    {
                        while(*args && !isspace(*args))
                            *res++ = *args++;

                        while(isspace(*args))
                            ++args;
                    }
                    break;
                }
                /* else fall through */
            case '1':
                if (!done || (*fmt == '1'))
                {
                    /*FIXME Is the call to SearchPathW() really needed? We already have separated out the parameter string in args. */
                    if (SearchPathW(NULL, lpFile, wszExe, sizeof(xlpFile)/sizeof(WCHAR), xlpFile, NULL))
                        cmd = xlpFile;
                    else
                        cmd = lpFile;

                    /* Add double quotation marks unless we already have them (e.g.: "%1" %* for exefile) */
                    if (res==out || res[-1]!='"')
                    {
                        *res++ = '"';
                        strcpyW(res, cmd);
                        res += strlenW(cmd);
                        *res++ = '"';
                    }
                    else
                    {
                        strcpyW(res, cmd);
                        res += strlenW(cmd);
                    }
                }
                break;

            /*
             * IE uses this alot for activating things such as windows media
             * player. This is not verified to be fully correct but it appears
             * to work just fine.
             */
            case 'l':
            case 'L':
		if (lpFile) {
		    strcpyW(res, lpFile);
		    res += strlenW(lpFile);
		}
                break;

            case 'i':
            case 'I':
		if (pidl) {
		    HGLOBAL hmem = SHAllocShared(pidl, ILGetSize(pidl), 0);
		    pv = SHLockShared(hmem, 0);
		    res += sprintfW(res, wszILPtr, pv);
		    SHUnlockShared(pv);
		}
                break;

            default: FIXME("Unknown escape sequence %%%c\n", *fmt);
            }
            fmt++;
            done = TRUE;
        }
        else
            *res++ = *fmt++;
    }

    *res = '\0';

    return done;
}

/*************************************************************************
 *	SHELL_ExecuteW [Internal]
 *
 */
static UINT SHELL_ExecuteW(const WCHAR *lpCmd, void *env, BOOL shWait,
			    LPSHELLEXECUTEINFOW psei, LPSHELLEXECUTEINFOW psei_out)
{
    STARTUPINFOW  startup;
    PROCESS_INFORMATION info;
    UINT retval = 31;
    UINT gcdret = 0;
    WCHAR curdir[MAX_PATH];

    TRACE("Execute %s from directory %s\n", debugstr_w(lpCmd), debugstr_w(psei->lpDirectory));
    /* ShellExecute specifies the command from psei->lpDirectory
     * if present. Not from the current dir as CreateProcess does */
    if( psei->lpDirectory && psei->lpDirectory[0] )
        if( ( gcdret = GetCurrentDirectoryW( MAX_PATH, curdir)))
            if( !SetCurrentDirectoryW( psei->lpDirectory))
                ERR("cannot set directory %s\n", debugstr_w(psei->lpDirectory));
    ZeroMemory(&startup,sizeof(STARTUPINFOW));
    startup.cb = sizeof(STARTUPINFOW);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = psei->nShow;
    if (CreateProcessW(NULL, (LPWSTR)lpCmd, NULL, NULL, FALSE, 0,
                       env, *psei->lpDirectory? psei->lpDirectory: NULL, &startup, &info))
    {
        /* Give 30 seconds to the app to come up, if desired. Probably only needed
           when starting app immediately before making a DDE connection. */
        if (shWait)
            if (WaitForInputIdle( info.hProcess, 30000 ) == WAIT_FAILED)
                WARN("WaitForInputIdle failed: Error %ld\n", GetLastError() );
        retval = 33;
        if (psei->fMask & SEE_MASK_NOCLOSEPROCESS)
            psei_out->hProcess = info.hProcess;
        else
            CloseHandle( info.hProcess );
        CloseHandle( info.hThread );
    }
    else if ((retval = GetLastError()) >= 32)
    {
        FIXME("Strange error set by CreateProcess: %d\n", retval);
        retval = ERROR_BAD_FORMAT;
    }

    TRACE("returning %u\n", retval);

    psei_out->hInstApp = (HINSTANCE)retval;
    if( gcdret ) 
        if( !SetCurrentDirectoryW( curdir))
            ERR("cannot return to directory %s\n", debugstr_w(curdir));

    return retval;
}


/***********************************************************************
 *           SHELL_BuildEnvW	[Internal]
 *
 * Build the environment for the new process, adding the specified
 * path to the PATH variable. Returned pointer must be freed by caller.
 */
static void *SHELL_BuildEnvW( const WCHAR *path )
{
    static const WCHAR wPath[] = {'P','A','T','H','=',0};
    WCHAR *strings, *new_env;
    WCHAR *p, *p2;
    int total = strlenW(path) + 1;
    BOOL got_path = FALSE;

    if (!(strings = GetEnvironmentStringsW())) return NULL;
    p = strings;
    while (*p)
    {
        int len = strlenW(p) + 1;
        if (!strncmpiW( p, wPath, 5 )) got_path = TRUE;
        total += len;
        p += len;
    }
    if (!got_path) total += 5;  /* we need to create PATH */
    total++;  /* terminating null */

    if (!(new_env = HeapAlloc( GetProcessHeap(), 0, total * sizeof(WCHAR) )))
    {
        FreeEnvironmentStringsW( strings );
        return NULL;
    }
    p = strings;
    p2 = new_env;
    while (*p)
    {
        int len = strlenW(p) + 1;
        memcpy( p2, p, len * sizeof(WCHAR) );
        if (!strncmpiW( p, wPath, 5 ))
        {
            p2[len - 1] = ';';
            strcpyW( p2 + len, path );
            p2 += strlenW(path) + 1;
        }
        p += len;
        p2 += len;
    }
    if (!got_path)
    {
        strcpyW( p2, wPath );
        strcatW( p2, path );
        p2 += strlenW(p2) + 1;
    }
    *p2 = 0;
    FreeEnvironmentStringsW( strings );
    return new_env;
}


/***********************************************************************
 *           SHELL_TryAppPathW	[Internal]
 *
 * Helper function for SHELL_FindExecutable
 * @param lpResult - pointer to a buffer of size MAX_PATH
 * On entry: szName is a filename (probably without path separators).
 * On exit: if szName found in "App Path", place full path in lpResult, and return true
 */
static BOOL SHELL_TryAppPathW( LPCWSTR szName, LPWSTR lpResult, void**env)
{
    static const WCHAR wszKeyAppPaths[] = {'S','o','f','t','w','a','r','e','\\','M','i','c','r','o','s','o','f','t','\\','W','i','n','d','o','w','s',
	'\\','C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\','A','p','p',' ','P','a','t','h','s','\\',0};
    static const WCHAR wPath[] = {'P','a','t','h',0};
    HKEY hkApp = 0;
    WCHAR buffer[1024];
    LONG len;
    LONG res;
    BOOL found = FALSE;

    if (env) *env = NULL;
    strcpyW(buffer, wszKeyAppPaths);
    strcatW(buffer, szName);
    res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, buffer, 0, KEY_READ, &hkApp);
    if (res) goto end;

    len = MAX_PATH*sizeof(WCHAR);
    res = RegQueryValueW(hkApp, NULL, lpResult, &len);
    if (res) goto end;
    found = TRUE;

    if (env)
    {
        DWORD count = sizeof(buffer);
        if (!RegQueryValueExW(hkApp, wPath, NULL, NULL, (LPBYTE)buffer, &count) && buffer[0])
            *env = SHELL_BuildEnvW( buffer );
    }

end:
    if (hkApp) RegCloseKey(hkApp);
    return found;
}

static UINT SHELL_FindExecutableByOperation(LPCWSTR lpPath, LPCWSTR lpFile, LPCWSTR lpOperation, LPWSTR key, LPWSTR filetype, LPWSTR command, LONG commandlen)
{
    static const WCHAR wCommand[] = {'\\','c','o','m','m','a','n','d',0};

    /* Looking for ...buffer\shell\<verb>\command */
    strcatW(filetype, wszShell);
    strcatW(filetype, lpOperation);
    strcatW(filetype, wCommand);

    if (RegQueryValueW(HKEY_CLASSES_ROOT, filetype, command,
                       &commandlen) == ERROR_SUCCESS)
    {
	commandlen /= sizeof(WCHAR);
        if (key) strcpyW(key, filetype);
#if 0
        LPWSTR tmp;
        WCHAR param[256];
	LONG paramlen = sizeof(param);
        static const WCHAR wSpace[] = {' ',0};

        /* FIXME: it seems all Windows version don't behave the same here.
         * the doc states that this ddeexec information can be found after
         * the exec names.
         * on Win98, it doesn't appear, but I think it does on Win2k
         */
	/* Get the parameters needed by the application
	   from the associated ddeexec key */
	tmp = strstrW(filetype, wCommand);
	tmp[0] = '\0';
	strcatW(filetype, wDdeexec);
	if (RegQueryValueW(HKEY_CLASSES_ROOT, filetype, param,
				     &paramlen) == ERROR_SUCCESS)
	{
	    paramlen /= sizeof(WCHAR);
            strcatW(command, wSpace);
            strcatW(command, param);
            commandlen += paramlen;
	}
#endif

	command[commandlen] = '\0';

	return 33; /* FIXME see SHELL_FindExecutable() */
    }

    return 31;	/* default - 'No association was found' */
}

/*************************************************************************
 *	SHELL_FindExecutable [Internal]
 *
 * Utility for code sharing between FindExecutable and ShellExecute
 * in:
 *      lpFile the name of a file
 *      lpOperation the operation on it (open)
 * out:
 *      lpResult a buffer, big enough :-(, to store the command to do the
 *              operation on the file
 *      key a buffer, big enough, to get the key name to do actually the
 *              command (it'll be used afterwards for more information
 *              on the operation)
 */
UINT SHELL_FindExecutable(LPCWSTR lpPath, LPCWSTR lpFile, LPCWSTR lpOperation,
                                 LPWSTR lpResult, int resultLen, LPWSTR key, void **env, LPITEMIDLIST pidl, LPCWSTR args)
{
    static const WCHAR wWindows[] = {'w','i','n','d','o','w','s',0};
    static const WCHAR wPrograms[] = {'p','r','o','g','r','a','m','s',0};
    static const WCHAR wExtensions[] = {'e','x','e',' ','p','i','f',' ','b','a','t',' ','c','m','d',' ','c','o','m',0};
    WCHAR *extension = NULL; /* pointer to file extension */
    WCHAR wtmpext[5];        /* local copy to mung as we please */
    WCHAR filetype[256];     /* registry name for this filetype */
    LONG  filetypelen = sizeof(filetype); /* length of above */
    WCHAR command[256];      /* command from registry */
    WCHAR wBuffer[256];      /* Used to GetProfileString */
    UINT  retval = 31;       /* default - 'No association was found' */
    WCHAR *tok;              /* token pointer */
    WCHAR xlpFile[256] = {0}; /* result of SearchPath */

    TRACE("%s\n", (lpFile != NULL) ? debugstr_w(lpFile) : "-");

    lpResult[0] = '\0'; /* Start off with an empty return string */
    if (key) *key = '\0';

    /* trap NULL parameters on entry */
    if ((lpFile == NULL) || (lpResult == NULL) || (lpOperation == NULL))
    {
        WARN("(lpFile=%s,lpResult=%s,lpOperation=%s): NULL parameter\n",
             debugstr_w(lpFile), debugstr_w(lpOperation), debugstr_w(lpResult));
        return 2; /* File not found. Close enough, I guess. */
    }

    if (SHELL_TryAppPathW( lpFile, lpResult, env ))
    {
        TRACE("found %s via App Paths\n", debugstr_w(lpResult));
        return 33;
    }

    if (SearchPathW(lpPath, lpFile, wszExe, sizeof(xlpFile)/sizeof(WCHAR), xlpFile, NULL))
    {
        TRACE("SearchPathW returned non-zero\n");
        lpFile = xlpFile;
        /* Hey, isn't this value ignored?  Why make this call?  Shouldn't we return here?  --dank*/
    }

    /* First thing we need is the file's extension */
    extension = strrchrW(xlpFile, '.'); /* Assume last "." is the one; */
                                       /* File->Run in progman uses */
                                       /* .\FILE.EXE :( */
    TRACE("xlpFile=%s,extension=%s\n", debugstr_w(xlpFile), debugstr_w(extension));

    if ((extension == NULL) || (extension == &xlpFile[strlenW(xlpFile)]))
    {
        WARN("Returning 31 - No association\n");
        return 31; /* no association */
    }

    /* Make local copy & lowercase it for reg & 'programs=' lookup */
    lstrcpynW(wtmpext, extension, 5);
    CharLowerW(wtmpext);
    TRACE("%s file\n", debugstr_w(wtmpext));

    /* Three places to check: */
    /* 1. win.ini, [windows], programs (NB no leading '.') */
    /* 2. Registry, HKEY_CLASS_ROOT\<filetype>\shell\open\command */
    /* 3. win.ini, [extensions], extension (NB no leading '.' */
    /* All I know of the order is that registry is checked before */
    /* extensions; however, it'd make sense to check the programs */
    /* section first, so that's what happens here. */

    /* See if it's a program - if GetProfileString fails, we skip this
     * section. Actually, if GetProfileString fails, we've probably
     * got a lot more to worry about than running a program... */
    if (GetProfileStringW(wWindows, wPrograms, wExtensions, wBuffer, sizeof(wBuffer)/sizeof(WCHAR)) > 0)
    {
        CharLowerW(wBuffer);
        tok = wBuffer;
        while (*tok)
        {
            WCHAR *p = tok;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p)
            {
                *p++ = 0;
                while (*p == ' ' || *p == '\t') p++;
            }

            if (strcmpW(tok, &wtmpext[1]) == 0) /* have to skip the leading "." */
            {
                strcpyW(lpResult, xlpFile);
                /* Need to perhaps check that the file has a path
                 * attached */
                TRACE("found %s\n", debugstr_w(lpResult));
                return 33;

		/* Greater than 32 to indicate success FIXME According to the
		 * docs, I should be returning a handle for the
		 * executable. Does this mean I'm supposed to open the
		 * executable file or something? More RTFM, I guess... */
            }
            tok = p;
        }
    }

    /* Check registry */
    if (RegQueryValueW(HKEY_CLASSES_ROOT, wtmpext, filetype, 
			&filetypelen) == ERROR_SUCCESS)
    {
	filetypelen /= sizeof(WCHAR);
	filetype[filetypelen] = '\0';
	TRACE("File type: %s\n", debugstr_w(filetype));
    }

    if (*filetype)
    {
	if (lpOperation)
	{
	    /* pass the operation string to SHELL_FindExecutableByOperation() */
	    filetype[filetypelen] = '\0';
	    retval = SHELL_FindExecutableByOperation(lpPath, lpFile, lpOperation, key, filetype, command, sizeof(command));
	}
	else
	{
	    WCHAR operation[MAX_PATH];
	    HKEY hkey;

	    /* Looking for ...buffer\shell\<operation>\command */
	    strcatW(filetype, wszShell);

	    /* enumerate the operation subkeys in the registry and search for one with an associated command */
	    if (RegOpenKeyW(HKEY_CLASSES_ROOT, filetype, &hkey) == ERROR_SUCCESS)
	    {
		int idx = 0;
		for(;; ++idx)
		{
		    if (RegEnumKeyW(hkey, idx, operation, MAX_PATH) != ERROR_SUCCESS)
			break;

		    filetype[filetypelen] = '\0';
		    retval = SHELL_FindExecutableByOperation(lpPath, lpFile, operation, key, filetype, command, sizeof(command));

		    if (retval > 32)
			break;
	    }
		RegCloseKey(hkey);
	    }
	}

	if (retval > 32)
	{
	    SHELL_ArgifyW(lpResult, resultLen, command, xlpFile, pidl, args);

	    /* Remove double quotation marks and command line arguments */
	    if (*lpResult == '"')
	    {
		WCHAR *p = lpResult;
		while (*(p + 1) != '"')
		{
		    *p = *(p + 1);
		    p++;
		}
		*p = '\0';
	    }
	}
    }
    else /* Check win.ini */
    {
	static const WCHAR wExtensions[] = {'e','x','t','e','n','s','i','o','n','s',0};
	static const WCHAR wEmpty[] = {0};

	/* Toss the leading dot */
	extension++;
	if (GetProfileStringW(wExtensions, extension, wEmpty, command, sizeof(command)/sizeof(WCHAR)) > 0)
        {
            if (strlenW(command) != 0)
            {
                strcpyW(lpResult, command);
                tok = strchrW(lpResult, '^'); /* should be ^.extension? */
                if (tok != NULL)
                {
                    tok[0] = '\0';
                    strcatW(lpResult, xlpFile); /* what if no dir in xlpFile? */
                    tok = strchrW(command, '^'); /* see above */
                    if ((tok != NULL) && (strlenW(tok)>5))
                    {
                        strcatW(lpResult, &tok[5]);
                    }
                }
                retval = 33; /* FIXME - see above */
            }
        }
    }

    TRACE("returning %s\n", debugstr_w(lpResult));
    return retval;
}

/******************************************************************
 *		dde_cb
 *
 * callback for the DDE connection. not really usefull
 */
static HDDEDATA CALLBACK dde_cb(UINT uType, UINT uFmt, HCONV hConv,
                                HSZ hsz1, HSZ hsz2,
                                HDDEDATA hData, DWORD dwData1, DWORD dwData2)
{
    return NULL;
}

/******************************************************************
 *		dde_connect
 *
 * ShellExecute helper. Used to do an operation with a DDE connection
 *
 * Handles both the direct connection (try #1), and if it fails,
 * launching an application and trying (#2) to connect to it
 *
 */
static unsigned dde_connect(WCHAR* key, WCHAR* start, WCHAR* ddeexec,
                            const WCHAR* lpFile, void *env,
			    LPCWSTR szCommandline, LPITEMIDLIST pidl, SHELL_ExecuteW32 execfunc,
			    LPSHELLEXECUTEINFOW psei, LPSHELLEXECUTEINFOW psei_out)
{
    static const WCHAR wApplication[] = {'\\','a','p','p','l','i','c','a','t','i','o','n',0};
    static const WCHAR wTopic[] = {'\\','t','o','p','i','c',0};
    WCHAR *     endkey = key + strlenW(key);
    WCHAR       app[256], topic[256], ifexec[256], res[256];
    LONG        applen, topiclen, ifexeclen;
    WCHAR *     exec;
    DWORD       ddeInst = 0;
    DWORD       tid;
    HSZ         hszApp, hszTopic;
    HCONV       hConv;
    unsigned    ret = 31;

    strcpyW(endkey, wApplication);
    applen = sizeof(app);
    if (RegQueryValueW(HKEY_CLASSES_ROOT, key, app, &applen) != ERROR_SUCCESS)
    {
        FIXME("default app name NIY %s\n", debugstr_w(key));
        return 2;
    }

    strcpyW(endkey, wTopic);
    topiclen = sizeof(topic);
    if (RegQueryValueW(HKEY_CLASSES_ROOT, key, topic, &topiclen) != ERROR_SUCCESS)
    {
        static const WCHAR wSystem[] = {'S','y','s','t','e','m',0};
        strcpyW(topic, wSystem);
    }

    if (DdeInitializeW(&ddeInst, dde_cb, APPCMD_CLIENTONLY, 0L) != DMLERR_NO_ERROR)
    {
        return 2;
    }

    hszApp = DdeCreateStringHandleW(ddeInst, app, CP_WINANSI);
    hszTopic = DdeCreateStringHandleW(ddeInst, topic, CP_WINANSI);

    hConv = DdeConnect(ddeInst, hszApp, hszTopic, NULL);
    exec = ddeexec;
    if (!hConv)
    {
        static const WCHAR wIfexec[] = {'\\','i','f','e','x','e','c',0};
        TRACE("Launching '%s'\n", debugstr_w(start));
        ret = execfunc(start, env, TRUE, psei, psei_out);
        if (ret < 32)
        {
            TRACE("Couldn't launch\n");
            goto error;
        }
        hConv = DdeConnect(ddeInst, hszApp, hszTopic, NULL);
        if (!hConv)
        {
            TRACE("Couldn't connect. ret=%d\n", ret);
            DdeUninitialize(ddeInst);
            SetLastError(ERROR_DDE_FAIL);
            return 30; /* whatever */
        }
        strcpyW(endkey, wIfexec);
        ifexeclen = sizeof(ifexec);
        if (RegQueryValueW(HKEY_CLASSES_ROOT, key, ifexec, &ifexeclen) == ERROR_SUCCESS)
        {
            exec = ifexec;
        }
    }

    SHELL_ArgifyW(res, sizeof(res)/sizeof(WCHAR), exec, lpFile, pidl, szCommandline);
    TRACE("%s %s => %s\n", debugstr_w(exec), debugstr_w(lpFile), debugstr_w(res));

    ret = (DdeClientTransaction((LPBYTE)res, (strlenW(res) + 1) * sizeof(WCHAR), hConv, 0L, 0,
                                XTYP_EXECUTE, 10000, &tid) != DMLERR_NO_ERROR) ? 31 : 33;
    DdeDisconnect(hConv);

 error:
    DdeUninitialize(ddeInst);

    return ret;
}

/*************************************************************************
 *	execute_from_key [Internal]
 */
static UINT execute_from_key(LPWSTR key, LPCWSTR lpFile, void *env, LPCWSTR szCommandline,
			     SHELL_ExecuteW32 execfunc,
			     LPSHELLEXECUTEINFOW psei, LPSHELLEXECUTEINFOW psei_out)
{
    WCHAR cmd[1024] = {0};
    LONG cmdlen = sizeof(cmd);
    UINT retval = 31;

    /* Get the application for the registry */
    if (RegQueryValueW(HKEY_CLASSES_ROOT, key, cmd, &cmdlen) == ERROR_SUCCESS)
    {
	static const WCHAR wCommand[] = {'c','o','m','m','a','n','d',0};
	static const WCHAR wDdeexec[] = {'d','d','e','e','x','e','c',0};
        LPWSTR tmp;
        WCHAR param[256] = {0};
        LONG paramlen = sizeof(param);

        /* Get the parameters needed by the application
           from the associated ddeexec key */
        tmp = strstrW(key, wCommand);
        assert(tmp);
        strcpyW(tmp, wDdeexec);

        if (RegQueryValueW(HKEY_CLASSES_ROOT, key, param, &paramlen) == ERROR_SUCCESS)
        {
            TRACE("Got ddeexec %s => %s\n", debugstr_w(key), debugstr_w(param));
            retval = dde_connect(key, cmd, param, lpFile, env, szCommandline, psei->lpIDList, execfunc, psei, psei_out);
        }
        else
        {
            /* Is there a replace() function anywhere? */
            cmdlen /= sizeof(WCHAR);
            cmd[cmdlen] = '\0';
            SHELL_ArgifyW(param, sizeof(param)/sizeof(WCHAR), cmd, lpFile, psei->lpIDList, szCommandline);
            retval = execfunc(param, env, FALSE, psei, psei_out);
        }
    }
    else TRACE("ooch\n");

    return retval;
}

/*************************************************************************
 * FindExecutableA			[SHELL32.@]
 */
HINSTANCE WINAPI FindExecutableA(LPCSTR lpFile, LPCSTR lpDirectory, LPSTR lpResult)
{
    HINSTANCE retval;
    WCHAR *wFile = NULL, *wDirectory = NULL;
    WCHAR wResult[MAX_PATH];

    if (lpFile) __SHCloneStrAtoW(&wFile, lpFile);
    if (lpDirectory) __SHCloneStrAtoW(&wDirectory, lpDirectory);

    retval = FindExecutableW(wFile, wDirectory, wResult);
    WideCharToMultiByte(CP_ACP, 0, wResult, -1, lpResult, MAX_PATH, NULL, NULL);
    if (wFile) SHFree( wFile );
    if (wDirectory) SHFree( wDirectory );

    TRACE("returning %s\n", lpResult);
    return (HINSTANCE)retval;
}

/*************************************************************************
 * FindExecutableW			[SHELL32.@]
 */
HINSTANCE WINAPI FindExecutableW(LPCWSTR lpFile, LPCWSTR lpDirectory, LPWSTR lpResult)
{
    UINT retval = 31;    /* default - 'No association was found' */
    WCHAR old_dir[1024];

    TRACE("File %s, Dir %s\n",
          (lpFile != NULL ? debugstr_w(lpFile) : "-"), (lpDirectory != NULL ? debugstr_w(lpDirectory) : "-"));

    lpResult[0] = '\0'; /* Start off with an empty return string */

    /* trap NULL parameters on entry */
    if ((lpFile == NULL) || (lpResult == NULL))
    {
        /* FIXME - should throw a warning, perhaps! */
	return (HINSTANCE)2; /* File not found. Close enough, I guess. */
    }

    if (lpDirectory)
    {
        GetCurrentDirectoryW(sizeof(old_dir)/sizeof(WCHAR), old_dir);
        SetCurrentDirectoryW(lpDirectory);
    }

    retval = SHELL_FindExecutable(lpDirectory, lpFile, wszOpen, lpResult, MAX_PATH, NULL, NULL, NULL, NULL);

    TRACE("returning %s\n", debugstr_w(lpResult));
    if (lpDirectory)
        SetCurrentDirectoryW(old_dir);
    return (HINSTANCE)retval;
}

/*************************************************************************
 *	ShellExecuteExW32 [Internal]
 */
BOOL WINAPI ShellExecuteExW32 (LPSHELLEXECUTEINFOW sei, SHELL_ExecuteW32 execfunc)
{
    static const WCHAR wQuote[] = {'"',0};
    static const WCHAR wSpace[] = {' ',0};
    static const WCHAR wWww[] = {'w','w','w',0};
    static const WCHAR wFile[] = {'f','i','l','e',0};
    static const WCHAR wHttp[] = {'h','t','t','p',':','/','/',0};

    WCHAR wszApplicationName[MAX_PATH+2], wszCommandline[1024], wszDir[MAX_PATH];
    SHELLEXECUTEINFOW sei_tmp;	/* modifyable copy of SHELLEXECUTEINFO struct */
    WCHAR wfileName[MAX_PATH];
    void *env;
    WCHAR lpstrProtocol[256];
    LPCWSTR lpFile;
    UINT retval = 31;
    WCHAR wcmd[1024];
    WCHAR buffer[MAX_PATH];
    BOOL done;

    /* make a local copy of the LPSHELLEXECUTEINFO structure and work with this from now on */
    memcpy(&sei_tmp, sei, sizeof(sei_tmp));

    TRACE("mask=0x%08lx hwnd=%p verb=%s file=%s parm=%s dir=%s show=0x%08x class=%s\n",
            sei_tmp.fMask, sei_tmp.hwnd, debugstr_w(sei_tmp.lpVerb),
            debugstr_w(sei_tmp.lpFile), debugstr_w(sei_tmp.lpParameters),
            debugstr_w(sei_tmp.lpDirectory), sei_tmp.nShow,
            (sei_tmp.fMask & SEE_MASK_CLASSNAME) ? debugstr_w(sei_tmp.lpClass) : "not used");

    sei->hProcess = NULL;

    /* make copies of all path/command strings */
    if (sei_tmp.lpFile)
	strcpyW(wszApplicationName, sei_tmp.lpFile);
    else
	*wszApplicationName = '\0';

    if (sei_tmp.lpParameters)
	strcpyW(wszCommandline, sei_tmp.lpParameters);
    else
	*wszCommandline = '\0';

    if (sei_tmp.lpDirectory)
	strcpyW(wszDir, sei_tmp.lpDirectory);
    else
	*wszDir = '\0';

    /* adjust string pointers to point to the new buffers */
    sei_tmp.lpFile = wszApplicationName;
    sei_tmp.lpParameters = wszCommandline;
    sei_tmp.lpDirectory = wszDir;

    if (sei_tmp.fMask & (SEE_MASK_INVOKEIDLIST | SEE_MASK_ICON | SEE_MASK_HOTKEY |
        SEE_MASK_CONNECTNETDRV | SEE_MASK_FLAG_DDEWAIT |
        SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_NO_UI | SEE_MASK_UNICODE |
        SEE_MASK_NO_CONSOLE | SEE_MASK_ASYNCOK | SEE_MASK_HMONITOR ))
    {
        FIXME("flags ignored: 0x%08lx\n", sei_tmp.fMask);
    }

    /* process the IDList */
    if (sei_tmp.fMask & SEE_MASK_IDLIST)
    {
	IShellExecuteHookW* pSEH;

	HRESULT hr = SHBindToParent(sei_tmp.lpIDList, &IID_IShellExecuteHookW, (LPVOID*)&pSEH, NULL);

	if (SUCCEEDED(hr))
	{
	    hr = IShellExecuteHookW_Execute(pSEH, sei);

	    IShellExecuteHookW_Release(pSEH);

	    if (hr == S_OK)
		return TRUE;
	}

        wszApplicationName[0] = '"';
        SHGetPathFromIDListW(sei_tmp.lpIDList, wszApplicationName+1);
        strcatW(wszApplicationName, wQuote);
        TRACE("-- idlist=%p (%s)\n", sei_tmp.lpIDList, debugstr_w(wszApplicationName));
    }

    if (sei_tmp.fMask & (SEE_MASK_CLASSNAME | SEE_MASK_CLASSKEY))
    {
	/* launch a document by fileclass like 'WordPad.Document.1' */
        /* the Commandline contains 'c:\Path\wordpad.exe "%1"' */
        /* FIXME: szCommandline should not be of a fixed size. Fixed to 1024, MAX_PATH is way too short! */
        HCR_GetExecuteCommandW((sei_tmp.fMask & SEE_MASK_CLASSKEY) ? sei_tmp.hkeyClass : NULL,
                               (sei_tmp.fMask & SEE_MASK_CLASSNAME) ? sei_tmp.lpClass: NULL,
                               (sei_tmp.lpVerb) ? sei_tmp.lpVerb : wszOpen,
                               wszCommandline, sizeof(wszCommandline)/sizeof(WCHAR));

        /* FIXME: get the extension of lpFile, check if it fits to the lpClass */
        TRACE("SEE_MASK_CLASSNAME->'%s', doc->'%s'\n", debugstr_w(wszCommandline), debugstr_w(wszApplicationName));

        wcmd[0] = '\0';
        done = SHELL_ArgifyW(wcmd, sizeof(wcmd)/sizeof(WCHAR), wszCommandline, wszApplicationName, sei_tmp.lpIDList, NULL);
        if (!done && wszApplicationName[0])
        {
            strcatW(wcmd, wSpace);
            strcatW(wcmd, wszApplicationName);
        }
        retval = execfunc(wcmd, NULL, FALSE, &sei_tmp, sei);
        if (retval > 32)
            return TRUE;
        else
            return FALSE;
    }

    /* expand environment strings */
    if (ExpandEnvironmentStringsW(sei_tmp.lpFile, buffer, MAX_PATH))
	lstrcpyW(wszApplicationName, buffer);

    if (*sei_tmp.lpParameters)
        if (ExpandEnvironmentStringsW(sei_tmp.lpParameters, buffer, MAX_PATH))
	    lstrcpyW(wszCommandline, buffer);

    if (*sei_tmp.lpDirectory)
	if (ExpandEnvironmentStringsW(sei_tmp.lpDirectory, buffer, MAX_PATH))
	    lstrcpyW(wszDir, buffer);

    /* Else, try to execute the filename */
    TRACE("execute:'%s','%s','%s'\n", debugstr_w(wszApplicationName), debugstr_w(wszCommandline), debugstr_w(wszDir));

    strcpyW(wfileName, wszApplicationName);
    lpFile = wfileName;
    if (wszCommandline[0]) {
        strcatW(wszApplicationName, wSpace);
        strcatW(wszApplicationName, wszCommandline);
    }

    retval = execfunc(wszApplicationName, NULL, FALSE, &sei_tmp, sei);
    if (retval > 32)
        return TRUE;

    /* Else, try to find the executable */
    wcmd[0] = '\0';
    retval = SHELL_FindExecutable(sei_tmp.lpDirectory, lpFile, sei_tmp.lpVerb, wcmd, 1024, lpstrProtocol, &env, sei_tmp.lpIDList, sei_tmp.lpParameters);
    if (retval > 32)  /* Found */
    {
        WCHAR wszQuotedCmd[MAX_PATH+2];
        /* Must quote to handle case where cmd contains spaces, 
         * else security hole if malicious user creates executable file "C:\\Program"
         */
        strcpyW(wszQuotedCmd, wQuote);
        strcatW(wszQuotedCmd, wcmd);
        strcatW(wszQuotedCmd, wQuote);
        if (wszCommandline[0]) {
            strcatW(wszQuotedCmd, wSpace);
            strcatW(wszQuotedCmd, wszCommandline);
        }
        TRACE("%s/%s => %s/%s\n", debugstr_w(wszApplicationName), debugstr_w(sei_tmp.lpVerb), debugstr_w(wszQuotedCmd), debugstr_w(lpstrProtocol));
        if (*lpstrProtocol)
            retval = execute_from_key(lpstrProtocol, wszApplicationName, env, sei_tmp.lpParameters, execfunc, &sei_tmp, sei);
        else
            retval = execfunc(wszQuotedCmd, env, FALSE, &sei_tmp, sei);
        if (env) HeapFree( GetProcessHeap(), 0, env );
    }
    else if (PathIsURLW((LPWSTR)lpFile))    /* File not found, check for URL */
    {
	static const WCHAR wShell[] = {'\\','s','h','e','l','l','\\',0};
	static const WCHAR wCommand[] = {'\\','c','o','m','m','a','n','d',0};
        LPWSTR lpstrRes;
        INT iSize;

        lpstrRes = strchrW(lpFile, ':');
        if (lpstrRes)
            iSize = lpstrRes - lpFile;
        else
            iSize = strlenW(lpFile);

        TRACE("Got URL: %s\n", debugstr_w(lpFile));
        /* Looking for ...protocol\shell\lpOperation\command */
        strncpyW(lpstrProtocol, lpFile, iSize);
        lpstrProtocol[iSize] = '\0';
        strcatW(lpstrProtocol, wShell);
        strcatW(lpstrProtocol, sei_tmp.lpVerb? sei_tmp.lpVerb: wszOpen);
        strcatW(lpstrProtocol, wCommand);

        /* Remove File Protocol from lpFile */
        /* In the case file://path/file     */
        if (!strncmpiW(lpFile, wFile, iSize))
        {
            lpFile += iSize;
            while (*lpFile == ':') lpFile++;
        }
        retval = execute_from_key(lpstrProtocol, lpFile, NULL, sei_tmp.lpParameters, execfunc, &sei_tmp, sei);
    }
    /* Check if file specified is in the form www.??????.*** */
    else if (!strncmpiW(lpFile, wWww, 3))
    {
        /* if so, append lpFile http:// and call ShellExecute */
        WCHAR lpstrTmpFile[256];
        strcpyW(lpstrTmpFile, wHttp);
        strcatW(lpstrTmpFile, lpFile);
        retval = (UINT)ShellExecuteW(sei_tmp.hwnd, sei_tmp.lpVerb, lpstrTmpFile, NULL, NULL, 0);
    }

    TRACE("retval %u\n", retval);

    if (retval <= 32)
    {
        sei->hInstApp = (HINSTANCE)retval;
        return FALSE;
    }

    sei->hInstApp = (HINSTANCE)33;
    return TRUE;
}

/*************************************************************************
 * ShellExecuteA			[SHELL32.290]
 */
HINSTANCE WINAPI ShellExecuteA(HWND hWnd, LPCSTR lpOperation,LPCSTR lpFile,
                               LPCSTR lpParameters,LPCSTR lpDirectory, INT iShowCmd)
{
    SHELLEXECUTEINFOA sei;
    HANDLE hProcess = 0;

    TRACE("\n");
    sei.cbSize = sizeof(sei);
    sei.fMask = 0;
    sei.hwnd = hWnd;
    sei.lpVerb = lpOperation;
    sei.lpFile = lpFile;
    sei.lpParameters = lpParameters;
    sei.lpDirectory = lpDirectory;
    sei.nShow = iShowCmd;
    sei.lpIDList = 0;
    sei.lpClass = 0;
    sei.hkeyClass = 0;
    sei.dwHotKey = 0;
    sei.hProcess = hProcess;

    ShellExecuteExA (&sei);
    return sei.hInstApp;
}

/*************************************************************************
 * ShellExecuteEx				[SHELL32.291]
 *
 */
BOOL WINAPI ShellExecuteExAW (LPVOID sei)
{
    if (SHELL_OsIsUnicode())
	return ShellExecuteExW32 (sei, SHELL_ExecuteW);
    return ShellExecuteExA (sei);
}

/*************************************************************************
 * ShellExecuteExA				[SHELL32.292]
 *
 */
BOOL WINAPI ShellExecuteExA (LPSHELLEXECUTEINFOA sei)
{
    SHELLEXECUTEINFOW seiW;
    BOOL ret;
    WCHAR *wVerb = NULL, *wFile = NULL, *wParameters = NULL, *wDirectory = NULL, *wClass = NULL;

    TRACE("%p\n", sei);

    memcpy(&seiW, sei, sizeof(SHELLEXECUTEINFOW));

    if (sei->lpVerb)
	seiW.lpVerb = __SHCloneStrAtoW(&wVerb, sei->lpVerb);

    if (sei->lpFile)
        seiW.lpFile = __SHCloneStrAtoW(&wFile, sei->lpFile);

    if (sei->lpParameters)
        seiW.lpParameters = __SHCloneStrAtoW(&wParameters, sei->lpParameters);

    if (sei->lpDirectory)
        seiW.lpDirectory = __SHCloneStrAtoW(&wDirectory, sei->lpDirectory);

    if ((sei->fMask & SEE_MASK_CLASSNAME) && sei->lpClass)
        seiW.lpClass = __SHCloneStrAtoW(&wClass, sei->lpClass);
    else
        seiW.lpClass = NULL;

    ret = ShellExecuteExW32 (&seiW, SHELL_ExecuteW);

    sei->hInstApp = seiW.hInstApp;

    if (wVerb) SHFree(wVerb);
    if (wFile) SHFree(wFile);
    if (wParameters) SHFree(wParameters);
    if (wDirectory) SHFree(wDirectory);
    if (wClass) SHFree(wClass);

    return ret;
}

/*************************************************************************
 * ShellExecuteExW				[SHELL32.293]
 *
 */
BOOL WINAPI ShellExecuteExW (LPSHELLEXECUTEINFOW sei)
{
    return  ShellExecuteExW32 (sei, SHELL_ExecuteW);
}

/*************************************************************************
 * ShellExecuteW			[SHELL32.294]
 * from shellapi.h
 * WINSHELLAPI HINSTANCE APIENTRY ShellExecuteW(HWND hwnd, LPCWSTR lpOperation,
 * LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd);
 */
HINSTANCE WINAPI ShellExecuteW(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile,
                               LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd)
{
    SHELLEXECUTEINFOW sei;
    HANDLE hProcess = 0;

    TRACE("\n");
    sei.cbSize = sizeof(sei);
    sei.fMask = 0;
    sei.hwnd = hwnd;
    sei.lpVerb = lpOperation;
    sei.lpFile = lpFile;
    sei.lpParameters = lpParameters;
    sei.lpDirectory = lpDirectory;
    sei.nShow = nShowCmd;
    sei.lpIDList = 0;
    sei.lpClass = 0;
    sei.hkeyClass = 0;
    sei.dwHotKey = 0;
    sei.hProcess = hProcess;

    ShellExecuteExW32 (&sei, SHELL_ExecuteW);
    return sei.hInstApp;
}
