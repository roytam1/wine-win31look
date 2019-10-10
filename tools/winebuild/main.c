/*
 * Main function
 *
 * Copyright 1993 Robert J. Amstadt
 * Copyright 1995 Martin von Loewis
 * Copyright 1995, 1996, 1997 Alexandre Julliard
 * Copyright 1997 Eric Youngdale
 * Copyright 1999 Ulrich Weigand
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

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#include "windef.h"
#include "winbase.h"
#include "build.h"

int UsePIC = 0;
int nb_debug_channels = 0;
int nb_lib_paths = 0;
int nb_errors = 0;
int display_warnings = 0;
int kill_at = 0;

/* we only support relay debugging on i386 */
#if defined(__i386__) && !defined(NO_TRACE_MSGS)
int debugging = 1;
#else
int debugging = 0;
#endif

char **debug_channels = NULL;
char **lib_path = NULL;

char *input_file_name = NULL;
const char *output_file_name = NULL;

static FILE *output_file;
static const char *current_src_dir;
static int nb_res_files;
static char **res_files;
static char *spec_file_name;

/* execution mode */
enum exec_mode_values
{
    MODE_NONE,
    MODE_DLL,
    MODE_EXE,
    MODE_DEF,
    MODE_DEBUG,
    MODE_RELAY16,
    MODE_RELAY32
};

static enum exec_mode_values exec_mode = MODE_NONE;

/* set the dll file name from the input file name */
static void set_dll_file_name( const char *name, DLLSPEC *spec )
{
    char *p;

    if (spec->file_name) return;

    if ((p = strrchr( name, '\\' ))) name = p + 1;
    if ((p = strrchr( name, '/' ))) name = p + 1;
    spec->file_name = xmalloc( strlen(name) + 5 );
    strcpy( spec->file_name, name );
    if ((p = strrchr( spec->file_name, '.' )))
    {
        if (!strcmp( p, ".spec" ) || !strcmp( p, ".def" )) *p = 0;
    }
    if (!strchr( spec->file_name, '.' )) strcat( spec->file_name, ".dll" );
}

/* set the dll subsystem */
static void set_subsystem( const char *subsystem, DLLSPEC *spec )
{
    char *major, *minor, *str = xstrdup( subsystem );

    if ((major = strchr( str, ':' ))) *major++ = 0;
    if (!strcmp( str, "native" )) spec->subsystem = IMAGE_SUBSYSTEM_NATIVE;
    else if (!strcmp( str, "windows" )) spec->subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
    else if (!strcmp( str, "console" )) spec->subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
    else fatal_error( "Invalid subsystem name '%s'\n", subsystem );
    if (major)
    {
        if ((minor = strchr( major, '.' )))
        {
            *minor++ = 0;
            spec->subsystem_minor = atoi( minor );
        }
        spec->subsystem_major = atoi( major );
    }
    free( str );
}

/* cleanup on program exit */
static void cleanup(void)
{
    if (output_file_name) unlink( output_file_name );
}


/*******************************************************************
 *         command-line option handling
 */
static const char usage_str[] =
"Usage: winebuild [OPTIONS] [FILES]\n\n"
"Options:\n"
"    -C --source-dir=DIR     Look for source files in DIR\n"
"    -d --delay-lib=LIB      Import the specified library in delayed mode\n"
"    -D SYM                  Ignored for C flags compatibility\n"
"    -e --entry=FUNC         Set the DLL entry point function (default: DllMain)\n"
"    -f FLAGS                Compiler flags (only -fPIC is supported)\n"
"    -F --filename=DLLFILE   Set the DLL filename (default: from input file name)\n"
"    -h --help               Display this help message\n"
"    -H --heap=SIZE          Set the heap size for a Win16 dll\n"
"    -i --ignore=SYM[,SYM]   Ignore specified symbols when resolving imports\n"
"    -I DIR                  Ignored for C flags compatibility\n"
"    -k --kill-at            Kill stdcall decorations in generated .def files\n"
"    -K FLAGS                Compiler flags (only -KPIC is supported)\n"
"    -l --library=LIB        Import the specified library\n"
"    -L --library-path=DIR   Look for imports libraries in DIR\n"
"    -M --main-module=MODULE Set the name of the main module for a Win16 dll\n"
"    -N --dll-name=DLLNAME   Set the DLL name (default: from input file name)\n"
"    -o --output=NAME        Set the output file name (default: stdout)\n"
"    -r --res=RSRC.RES       Load resources from RSRC.RES\n"
"       --subsystem=SUBSYS   Set the subsystem (one of native, windows, console)\n"
"       --version            Print the version and exit\n"
"    -w --warnings           Turn on warnings\n"
"\nMode options:\n"
"       --dll=FILE           Build a .c file from a .spec or .def file\n"
"       --def=FILE.SPEC      Build a .def file from a spec file\n"
"       --exe=NAME           Build a .c file for the named executable\n"
"       --debug [FILES]      Build a .c file with the debug channels declarations\n"
"       --relay16            Build the 16-bit relay assembly routines\n"
"       --relay32            Build the 32-bit relay assembly routines\n\n"
"The mode options are mutually exclusive; you must specify one and only one.\n\n";

enum long_options_values
{
    LONG_OPT_DLL = 1,
    LONG_OPT_DEF,
    LONG_OPT_EXE,
    LONG_OPT_DEBUG,
    LONG_OPT_RELAY16,
    LONG_OPT_RELAY32,
    LONG_OPT_SUBSYSTEM,
    LONG_OPT_VERSION
};

static const char short_options[] = "C:D:F:H:I:K:L:M:N:d:e:f:hi:kl:m:o:r:w";

static const struct option long_options[] =
{
    { "dll",      1, 0, LONG_OPT_DLL },
    { "def",      1, 0, LONG_OPT_DEF },
    { "exe",      1, 0, LONG_OPT_EXE },
    { "debug",    0, 0, LONG_OPT_DEBUG },
    { "relay16",  0, 0, LONG_OPT_RELAY16 },
    { "relay32",  0, 0, LONG_OPT_RELAY32 },
    { "subsystem",1, 0, LONG_OPT_SUBSYSTEM },
    { "version",  0, 0, LONG_OPT_VERSION },
    /* aliases for short options */
    { "source-dir",    1, 0, 'C' },
    { "delay-lib",     1, 0, 'd' },
    { "entry",         1, 0, 'e' },
    { "filename",      1, 0, 'F' },
    { "help",          0, 0, 'h' },
    { "heap",          1, 0, 'H' },
    { "ignore",        1, 0, 'i' },
    { "kill-at",       0, 0, 'k' },
    { "library",       1, 0, 'l' },
    { "library-path",  1, 0, 'L' },
    { "main-module",   1, 0, 'M' },
    { "dll-name",      1, 0, 'N' },
    { "output",        1, 0, 'o' },
    { "res",           1, 0, 'r' },
    { "warnings",      0, 0, 'w' },
    { NULL,            0, 0, 0 }
};

static void usage( int exit_code )
{
    fprintf( stderr, "%s", usage_str );
    exit( exit_code );
}

static void set_exec_mode( enum exec_mode_values mode )
{
    if (exec_mode != MODE_NONE) usage(1);
    exec_mode = mode;
}

/* parse options from the argv array and remove all the recognized ones */
static char **parse_options( int argc, char **argv, DLLSPEC *spec )
{
    char *p;
    int optc;

    while ((optc = getopt_long( argc, argv, short_options, long_options, NULL )) != -1)
    {
        switch(optc)
        {
        case 'C':
            current_src_dir = optarg;
            break;
        case 'D':
            /* ignored */
            break;
        case 'F':
            spec->file_name = xstrdup( optarg );
            break;
        case 'H':
            if (!isdigit(optarg[0]))
                fatal_error( "Expected number argument with -H option instead of '%s'\n", optarg );
            spec->heap_size = atoi(optarg);
            if (spec->heap_size > 65535)
                fatal_error( "Invalid heap size %d, maximum is 65535\n", spec->heap_size );
            break;
        case 'I':
            /* ignored */
            break;
        case 'K':
            /* ignored, because cc generates correct code. */
            break;
        case 'L':
            lib_path = xrealloc( lib_path, (nb_lib_paths+1) * sizeof(*lib_path) );
            lib_path[nb_lib_paths++] = xstrdup( optarg );
            break;
        case 'M':
            spec->owner_name = xstrdup( optarg );
            spec->type = SPEC_WIN16;
            break;
        case 'N':
            spec->dll_name = xstrdup( optarg );
            break;
        case 'd':
            add_import_dll( optarg, 1 );
            break;
        case 'e':
            spec->init_func = xstrdup( optarg );
            if ((p = strchr( spec->init_func, '@' ))) *p = 0;  /* kill stdcall decoration */
            break;
        case 'f':
            if (!strcmp( optarg, "PIC") || !strcmp( optarg, "pic")) UsePIC = 1;
            /* ignore all other flags */
            break;
        case 'h':
            usage(0);
            break;
        case 'i':
            {
                char *str = xstrdup( optarg );
                char *token = strtok( str, "," );
                while (token)
                {
                    add_ignore_symbol( token );
                    token = strtok( NULL, "," );
                }
                free( str );
            }
            break;
        case 'k':
            kill_at = 1;
            break;
        case 'l':
            add_import_dll( optarg, 0 );
            break;
        case 'o':
            if (unlink( optarg ) == -1 && errno != ENOENT)
                fatal_error( "Unable to create output file '%s'\n", optarg );
            if (!(output_file = fopen( optarg, "w" )))
                fatal_error( "Unable to create output file '%s'\n", optarg );
            output_file_name = xstrdup(optarg);
            atexit( cleanup );  /* make sure we remove the output file on exit */
            break;
        case 'r':
            res_files = xrealloc( res_files, (nb_res_files+1) * sizeof(*res_files) );
            res_files[nb_res_files++] = xstrdup( optarg );
            break;
        case 'w':
            display_warnings = 1;
            break;
        case LONG_OPT_DLL:
            set_exec_mode( MODE_DLL );
            spec_file_name = xstrdup( optarg );
            set_dll_file_name( optarg, spec );
            break;
        case LONG_OPT_DEF:
            set_exec_mode( MODE_DEF );
            spec_file_name = xstrdup( optarg );
            set_dll_file_name( optarg, spec );
            break;
        case LONG_OPT_EXE:
            set_exec_mode( MODE_EXE );
            if ((p = strrchr( optarg, '/' ))) p++;
            else p = optarg;
            spec->file_name = xmalloc( strlen(p) + 5 );
            strcpy( spec->file_name, p );
            if (!strchr( spec->file_name, '.' )) strcat( spec->file_name, ".exe" );
            if (!spec->subsystem) spec->subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
            break;
        case LONG_OPT_DEBUG:
            set_exec_mode( MODE_DEBUG );
            break;
        case LONG_OPT_RELAY16:
            set_exec_mode( MODE_RELAY16 );
            break;
        case LONG_OPT_RELAY32:
            set_exec_mode( MODE_RELAY32 );
            break;
        case LONG_OPT_SUBSYSTEM:
            set_subsystem( optarg, spec );
            break;
        case LONG_OPT_VERSION:
            printf( "winebuild version " PACKAGE_VERSION "\n" );
            exit(0);
        case '?':
            usage(1);
            break;
        }
    }
    return &argv[optind];
}


/* load all specified resource files */
static void load_resources( char *argv[], DLLSPEC *spec )
{
    int i;
    char **ptr, **last;

    switch (spec->type)
    {
    case SPEC_WIN16:
        for (i = 0; i < nb_res_files; i++) load_res16_file( res_files[i], spec );
        break;

    case SPEC_WIN32:
        for (i = 0; i < nb_res_files; i++)
        {
            if (!load_res32_file( res_files[i], spec ))
                fatal_error( "%s is not a valid Win32 resource file\n", res_files[i] );
        }

        /* load any resource file found in the remaining arguments */
        for (ptr = last = argv; *ptr; ptr++)
        {
            if (!load_res32_file( *ptr, spec ))
                *last++ = *ptr; /* not a resource file, keep it in the list */
        }
        *last = NULL;
        break;
    }
}

static int parse_input_file( DLLSPEC *spec )
{
    FILE *input_file = open_input_file( NULL, spec_file_name );
    char *extension = strrchr( spec_file_name, '.' );

    if (extension && !strcmp( extension, ".def" ))
        return parse_def_file( input_file, spec );
    else
        return parse_spec_file( input_file, spec );
    close_input_file( input_file );
}


/*******************************************************************
 *         main
 */
int main(int argc, char **argv)
{
    DLLSPEC *spec = alloc_dll_spec();

    output_file = stdout;
    argv = parse_options( argc, argv, spec );

    switch(exec_mode)
    {
    case MODE_DLL:
        spec->characteristics |= IMAGE_FILE_DLL;
        load_resources( argv, spec );
        if (!parse_input_file( spec )) break;
        switch (spec->type)
        {
            case SPEC_WIN16:
                if (argv[0])
                    fatal_error( "file argument '%s' not allowed in this mode\n", argv[0] );
                BuildSpec16File( output_file, spec );
                break;
            case SPEC_WIN32:
                read_undef_symbols( argv );
                BuildSpec32File( output_file, spec );
                break;
            default: assert(0);
        }
        break;
    case MODE_EXE:
        if (spec->type == SPEC_WIN16) fatal_error( "Cannot build 16-bit exe files\n" );
        load_resources( argv, spec );
        read_undef_symbols( argv );
        BuildSpec32File( output_file, spec );
        break;
    case MODE_DEF:
        if (argv[0]) fatal_error( "file argument '%s' not allowed in this mode\n", argv[0] );
        if (spec->type == SPEC_WIN16) fatal_error( "Cannot yet build .def file for 16-bit dlls\n" );
        if (!parse_input_file( spec )) break;
        BuildDef32File( output_file, spec );
        break;
    case MODE_DEBUG:
        BuildDebugFile( output_file, current_src_dir, argv );
        break;
    case MODE_RELAY16:
        if (argv[0]) fatal_error( "file argument '%s' not allowed in this mode\n", argv[0] );
        BuildRelays16( output_file );
        break;
    case MODE_RELAY32:
        if (argv[0]) fatal_error( "file argument '%s' not allowed in this mode\n", argv[0] );
        BuildRelays32( output_file );
        break;
    default:
        usage(1);
        break;
    }
    if (nb_errors) exit(1);
    if (output_file_name)
    {
        fclose( output_file );
        output_file_name = NULL;
    }
    return 0;
}
