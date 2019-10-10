/*
 * an application for displaying Win32 console
 *      registry and init functions
 *
 * Copyright 2001 Eric Pouech
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winecon_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wineconsole);

static const WCHAR wszConsole[]           = {'C','o','n','s','o','l','e',0};
static const WCHAR wszCursorSize[]        = {'C','u','r','s','o','r','S','i','z','e',0};
static const WCHAR wszCursorVisible[]     = {'C','u','r','s','o','r','V','i','s','i','b','l','e',0};
static const WCHAR wszEditionMode[]       = {'E','d','i','t','i','o','n','M','o','d','e',0};
static const WCHAR wszExitOnDie[]         = {'E','x','i','t','O','n','D','i','e',0};
static const WCHAR wszFaceName[]          = {'F','a','c','e','N','a','m','e',0};
static const WCHAR wszFontSize[]          = {'F','o','n','t','S','i','z','e',0};
static const WCHAR wszFontWeight[]        = {'F','o','n','t','W','e','i','g','h','t',0};
static const WCHAR wszHistoryBufferSize[] = {'H','i','s','t','o','r','y','B','u','f','f','e','r','S','i','z','e',0};
static const WCHAR wszHistoryNoDup[]      = {'H','i','s','t','o','r','y','N','o','D','u','p',0};
static const WCHAR wszMenuMask[]          = {'M','e','n','u','M','a','s','k',0};
static const WCHAR wszQuickEdit[]         = {'Q','u','i','c','k','E','d','i','t',0};
static const WCHAR wszScreenBufferSize[]  = {'S','c','r','e','e','n','B','u','f','f','e','r','S','i','z','e',0};
static const WCHAR wszScreenColors[]      = {'S','c','r','e','e','n','C','o','l','o','r','s',0};
static const WCHAR wszWindowSize[]        = {'W','i','n','d','o','w','S','i','z','e',0};

void WINECON_DumpConfig(const char* pfx, const struct config_data* cfg)
{
    WINE_TRACE("%s cell=(%u,%u) cursor=(%d,%d) attr=%02lx font=%s/%lu hist=%lu/%d flags=%c%c msk=%08lx sb=(%u,%u) win=(%u,%u)x(%u,%u) edit=%u registry=%s\n",
               pfx, cfg->cell_width, cfg->cell_height, cfg->cursor_size, cfg->cursor_visible, cfg->def_attr,
               wine_dbgstr_w(cfg->face_name), cfg->font_weight, cfg->history_size, cfg->history_nodup ? 1 : 2,
               cfg->quick_edit ? 'Q' : 'q', cfg->exit_on_die ? 'X' : 'x',
               cfg->menu_mask, cfg->sb_width, cfg->sb_height, cfg->win_pos.X, cfg->win_pos.Y, cfg->win_width, cfg->win_height,
               cfg->edition_mode,
               wine_dbgstr_w(cfg->registry));
}

/******************************************************************
 *		WINECON_CreateKeyName
 *
 * Get a proper key name from an appname (mainly convert '\\' to '_')
 */
static LPWSTR   WINECON_CreateKeyName(LPCWSTR kn)
{
    LPWSTR      ret = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(kn) + 1) * sizeof(WCHAR));
    LPWSTR      ptr = ret;

    if (!ptr) WINECON_Fatal("OOM");

    do
    {
        *ptr++ = *kn == '\\' ? '_' : *kn;
    } while (*kn++ != 0);
    return ret;
}

/******************************************************************
 *		WINECON_RegLoadHelper
 *
 * Read the basic configuration from any console key or subkey
 */
static void WINECON_RegLoadHelper(HKEY hConKey, struct config_data* cfg)
{
    DWORD 	type;
    DWORD 	count;
    DWORD       val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszCursorSize, 0, &type, (char*)&val, &count))
        cfg->cursor_size = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszCursorVisible, 0, &type, (char*)&val, &count))
        cfg->cursor_visible = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszEditionMode, 0, &type, (char*)&val, &count))
        cfg->edition_mode = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszExitOnDie, 0, &type, (char*)&val, &count))
        cfg->exit_on_die = val;

    count = sizeof(cfg->face_name);
    RegQueryValueEx(hConKey, wszFaceName, 0, &type, (char*)&cfg->face_name, &count);

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszFontSize, 0, &type, (char*)&val, &count))
    {
        cfg->cell_height = HIWORD(val);
        cfg->cell_width  = LOWORD(val);
    }

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszFontWeight, 0, &type, (char*)&val, &count))
        cfg->font_weight = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszHistoryBufferSize, 0, &type, (char*)&val, &count))
        cfg->history_size = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszHistoryNoDup, 0, &type, (char*)&val, &count))
        cfg->history_nodup = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszMenuMask, 0, &type, (char*)&val, &count))
        cfg->menu_mask = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszQuickEdit, 0, &type, (char*)&val, &count))
        cfg->quick_edit = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszScreenBufferSize, 0, &type, (char*)&val, &count))
    {
        cfg->sb_height = HIWORD(val);
        cfg->sb_width  = LOWORD(val);
    }

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszScreenColors, 0, &type, (char*)&val, &count))
        cfg->def_attr = val;

    count = sizeof(val);
    if (!RegQueryValueEx(hConKey, wszWindowSize, 0, &type, (char*)&val, &count))
    {
        cfg->win_height = HIWORD(val);
        cfg->win_width  = LOWORD(val);
    }

    /* win_pos isn't read from registry */
}

/******************************************************************
 *		WINECON_RegLoad
 *
 *
 */
void WINECON_RegLoad(const WCHAR* appname, struct config_data* cfg)
{
    HKEY        hConKey;

    WINE_TRACE("loading %s registry settings.\n", appname ? wine_dbgstr_w(appname) : "default");

    /* first set default values */
    cfg->cursor_size = 25;
    cfg->cursor_visible = 1;
    cfg->exit_on_die = 1;
    memset(cfg->face_name, 0, sizeof(cfg->face_name));
    cfg->cell_height = 12;
    cfg->cell_width  = 8;
    cfg->font_weight = 0;
    cfg->history_size = 50;
    cfg->history_nodup = 0;
    cfg->menu_mask = 0;
    cfg->quick_edit = 0;
    cfg->sb_height = 25;
    cfg->sb_width  = 80;
    cfg->def_attr = 0x000F;
    cfg->win_height = 25;
    cfg->win_width  = 80;
    cfg->win_pos.X = 0;
    cfg->win_pos.Y = 0;
    cfg->edition_mode = 0;
    cfg->registry = NULL;

    /* then read global settings */
    if (!RegOpenKey(HKEY_CURRENT_USER, wszConsole, &hConKey))
    {
        WINECON_RegLoadHelper(hConKey, cfg);
        /* if requested, load part related to console title */
        if (appname)
        {
            HKEY        hAppKey;

            cfg->registry = WINECON_CreateKeyName(appname);
            if (!RegOpenKey(hConKey, cfg->registry, &hAppKey))
            {
                WINECON_RegLoadHelper(hAppKey, cfg);
                RegCloseKey(hAppKey);
            }
        }
        RegCloseKey(hConKey);
    }
    WINECON_DumpConfig("load", cfg);
}

/******************************************************************
 *		WINECON_RegSaveHelper
 *
 *
 */
static void WINECON_RegSaveHelper(HKEY hConKey, const struct config_data* cfg)
{
    DWORD       val;

    WINECON_DumpConfig("save", cfg);

    val = cfg->cursor_size;
    RegSetValueEx(hConKey, wszCursorSize, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->cursor_visible;
    RegSetValueEx(hConKey, wszCursorVisible, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->edition_mode;
    RegSetValueEx(hConKey, wszEditionMode, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->exit_on_die;
    RegSetValueEx(hConKey, wszExitOnDie, 0, REG_DWORD, (char*)&val, sizeof(val));

    RegSetValueEx(hConKey, wszFaceName, 0, REG_SZ, (char*)&cfg->face_name, sizeof(cfg->face_name));

    val = MAKELONG(cfg->cell_width, cfg->cell_height);
    RegSetValueEx(hConKey, wszFontSize, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->font_weight;
    RegSetValueEx(hConKey, wszFontWeight, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->history_size;
    RegSetValueEx(hConKey, wszHistoryBufferSize, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->history_nodup;
    RegSetValueEx(hConKey, wszHistoryNoDup, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->menu_mask;
    RegSetValueEx(hConKey, wszMenuMask, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->quick_edit;
    RegSetValueEx(hConKey, wszQuickEdit, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = MAKELONG(cfg->sb_width, cfg->sb_height);
    RegSetValueEx(hConKey, wszScreenBufferSize, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = cfg->def_attr;
    RegSetValueEx(hConKey, wszScreenColors, 0, REG_DWORD, (char*)&val, sizeof(val));

    val = MAKELONG(cfg->win_width, cfg->win_height);
    RegSetValueEx(hConKey, wszWindowSize, 0, REG_DWORD, (char*)&val, sizeof(val));
}

/******************************************************************
 *		WINECON_RegSave
 *
 *
 */
void WINECON_RegSave(const struct config_data* cfg)
{
    HKEY        hConKey;

    WINE_TRACE("saving registry settings.\n");
    if (RegCreateKey(HKEY_CURRENT_USER, wszConsole, &hConKey))
    {
        WINE_ERR("Can't open registry for saving\n");
    }
    else
    {
        if (cfg->registry)
        {
            HKEY    hAppKey;

            if (RegCreateKey(hConKey, cfg->registry, &hAppKey))
            {
                WINE_ERR("Can't open registry for saving\n");
            }
            else
            {
                /* FIXME: maybe only save the values different from the default value ? */
                WINECON_RegSaveHelper(hAppKey, cfg);
                RegCloseKey(hAppKey);
            }
        }
        else WINECON_RegSaveHelper(hConKey, cfg);
        RegCloseKey(hConKey);
    }
}
