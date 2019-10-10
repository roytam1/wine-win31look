/*
 * Option parsing
 *
 * Copyright 2000 Alexandre Julliard
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
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/library.h"
#include "options.h"
#include "wine/debug.h"

struct option_descr
{
    const char *longname;
    char        shortname;
    int         has_arg;
    void      (*func)( const char *arg );
    const char *usage;
};

static void do_debugmsg( const char *arg );
static void do_help( const char *arg );
static void do_version( const char *arg );

static const struct option_descr option_table[] =
{
    { "debugmsg",     0, 1, do_debugmsg,
      "--debugmsg name  Turn debugging-messages on or off" },
    { "help",       'h', 0, do_help,
      "--help,-h        Show this help message" },
    { "version",    'v', 0, do_version,
      "--version,-v     Display the Wine version" },
    { NULL,           0, 0, NULL, NULL }  /* terminator */
};


static void do_help( const char *arg )
{
    OPTIONS_Usage();
}

static void do_version( const char *arg )
{
    MESSAGE( "%s\n", PACKAGE_STRING );
    ExitProcess(0);
}

static void do_debugmsg( const char *arg )
{
    char buffer[1024];

    if (wine_dbg_parse_options( arg ))
    {
        MESSAGE("wine: Syntax: --debugmsg [class]+xxx,...  or -debugmsg [class]-xxx,...\n");
        MESSAGE("Example: --debugmsg +all,warn-heap\n"
                "  turn on all messages except warning heap messages\n");
        MESSAGE("Available message classes: err, warn, fixme, trace\n\n");
        ExitProcess(1);
    }
    MESSAGE("Warning: the --debugmsg option is deprecated. You should use\n");
    MESSAGE("the WINEDEBUG environment variable instead, like this:\n\n");
    MESSAGE("  WINEDEBUG=%s wine ...\n\n", arg );

    /* append the argument to WINEDEBUG so that it gets inherited */
    if (GetEnvironmentVariableA( "WINEDEBUG", buffer, sizeof(buffer)-1 ) && buffer[0])
    {
        char *p = buffer + strlen(buffer);
        *p++ = ',';
        lstrcpynA( p, arg, buffer + sizeof(buffer) - p );
        SetEnvironmentVariableA( "WINEDEBUG", buffer );
    }
    else SetEnvironmentVariableA( "WINEDEBUG", arg );
}

static inline void remove_options( char *argv[], int pos, int count )
{
    while ((argv[pos] = argv[pos+count])) pos++;
}

/* parse options from the argv array and remove all the recognized ones */
static void parse_options( char *argv[] )
{
    const struct option_descr *opt;
    int i;

    for (i = 0; argv[i]; i++)
    {
        const char *equalarg = NULL;
        char *p = argv[i];
        if (*p++ != '-') continue;  /* not an option */
        if (*p && !p[1]) /* short name */
        {
            if (*p == '-') break; /* "--" option */
            for (opt = option_table; opt->longname; opt++) if (opt->shortname == *p) break;
        }
        else  /* long name */
        {
	    const char *equal = strchr  (p, '=');
            if (*p == '-') p++;
            /* check for the long name */
            for (opt = option_table; opt->longname; opt++) {
	        /* Plain --option */
                if (!strcmp( p, opt->longname )) break;

		/* --option=value */
		if (opt->has_arg &&
		    equal &&
		    strlen (opt->longname) == equal - p &&
		    !strncmp (p, opt->longname, equal - p)) {
		        equalarg = equal + 1;
		        break;
		    }
	    }
        }
        if (!opt->longname) continue;

	if (equalarg)
	{
            opt->func( equalarg );
            remove_options( argv, i, 1 );
	}
        else if (opt->has_arg && argv[i+1])
        {
            opt->func( argv[i+1] );
            remove_options( argv, i, 2 );
        }
        else
        {
            opt->func( "" );
            remove_options( argv, i, 1 );
        }
        i--;
    }
}

/***********************************************************************
 *              OPTIONS_Usage
 */
void OPTIONS_Usage(void)
{
    const struct option_descr *opt;
    MESSAGE( "%s\n\n", PACKAGE_STRING );
    MESSAGE( "Usage: wine [options] [--] program_name [arguments]\n" );
    MESSAGE("The -- has to be used if you specify arguments (of the program)\n\n");
    MESSAGE( "Options:\n" );
    for (opt = option_table; opt->longname; opt++) MESSAGE( "   %s\n", opt->usage );
    ExitProcess(0);
}

/***********************************************************************
 *              OPTIONS_ParseOptions
 */
void OPTIONS_ParseOptions( char *argv[] )
{
    int i;

    parse_options( argv + 1 );

    /* check if any option remains */
    for (i = 1; argv[i]; i++)
    {
        if (!strcmp( argv[i], "--" ))
        {
            remove_options( argv, i, 1 );
            break;
        }
        if (argv[i][0] == '-')
        {
            MESSAGE( "Unknown option '%s'\n\n", argv[i] );
            OPTIONS_Usage();
        }
    }
}
