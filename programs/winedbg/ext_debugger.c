/*
 * Convenience functions to handle use of external debugger.
 *
 * Copyright 1999 Kevin Holbrook
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DBG_BUFF_SIZE  12

#define DBG_EXTERNAL_DEFAULT   "gdb"
#define DBG_LOCATION_DEFAULT   "/usr/local/bin/wine"
#define DBG_SLEEPTIME_DEFAULT  120



/* DEBUG_ExternalDebugger
 *
 * This function invokes an external debugger on the current
 * wine process. The form of the command executed is:
 *  <debugger image> <wine image> <attach process id>
 *
 * The debugger command is normally invoked by a newly created xterm.
 *
 * The current calling process is temporarily put to sleep
 * so that the invoked debugger has time to come up and attach.
 *
 * The following environment variables may be used:
 *
 *   Name                Use                                      Default
 *  -------------------------------------------------------------------------------------
 *   WINE_DBG_EXTERNAL   debugger command to invoke               ("gdb")
 *   WINE_DBG_LOCATION   fully qualified location of wine image   ("/usr/local/bin/wine")
 *   WINE_DBG_NO_XTERM   if set do not invoke xterm with command  (not set)
 *   WINE_DBG_SLEEPTIME  number of seconds to make process sleep  (120)
 *
 *
 * Usage:
 *
 *   #include "wine/debug.h"
 *
 *   DEBUG_ExternalDebugger();
 *
 *
 * Environment Example:
 *
 *   export WINE_DBG_EXTERNAL="ddd"
 *   export WINE_DBG_NO_XTERM=1
 *   export WINE_DBG_SLEEPTIME=60
 *
 */

void DEBUG_ExternalDebugger(void)
{
  pid_t attach_pid;
  pid_t child_pid;
  int   dbg_sleep_secs = DBG_SLEEPTIME_DEFAULT;
  char *dbg_sleeptime;


  dbg_sleeptime = getenv("WINE_DBG_SLEEPTIME");

  /* convert sleep time string to integer seconds */
  if (dbg_sleeptime)
  {
    dbg_sleep_secs = atoi(dbg_sleeptime);

    /* check for conversion error */
    if (dbg_sleep_secs == 0)
      dbg_sleep_secs = DBG_SLEEPTIME_DEFAULT;
  }

  /* get the current process id */
  attach_pid = getpid();

  /* create new process */
  child_pid = fork();

  /* check if we are the child process */
  if (child_pid == 0)
  {
    int  status;
    const char *dbg_external;
    const char *dbg_wine_location;
    const char *dbg_no_xterm;
    char pid_string[DBG_BUFF_SIZE];


    /* check settings in environment for debugger to use */
    dbg_external      = getenv("WINE_DBG_EXTERNAL");
    dbg_wine_location = getenv("WINE_DBG_LOCATION");
    dbg_no_xterm      = getenv("WINE_DBG_NO_XTERM");

    /* if not set in environment, use default */
    if (!dbg_external)
      dbg_external = "gdb";

    /* if not set in environment, use default */
    if (!dbg_wine_location)
      if (!(dbg_wine_location = getenv("WINELOADER")))
        dbg_wine_location = "miscemu/wine";

    /* check for empty string in WINE_DBG_NO_XTERM */
    if (dbg_no_xterm && (strlen(dbg_no_xterm) < 1))
      dbg_no_xterm = NULL;

    /* clear the buffer */
    memset(pid_string, 0, DBG_BUFF_SIZE);

    /* make pid into string */
    snprintf(pid_string, sizeof(pid_string), "%ld", (long) attach_pid);

    /* now exec the debugger to get it's own clean memory space */
    if (dbg_no_xterm)
      status = execlp(dbg_external, dbg_external, dbg_wine_location, pid_string, NULL);
    else
      status = execlp("xterm", "xterm", "-e", dbg_external, dbg_wine_location, pid_string, NULL);

    if (status == -1)
    {
      if (dbg_no_xterm)
        fprintf(stderr, "DEBUG_ExternalDebugger failed to execute \"%s %s %s\" (%s)\n",
                dbg_external, dbg_wine_location, pid_string, strerror(errno));
      else
        fprintf(stderr, "DEBUG_ExternalDebugger failed to execute \"xterm -e %s %s %s\" (%s)\n",
                dbg_external, dbg_wine_location, pid_string, strerror(errno));
    }

  }
  else if (child_pid != -1)
  {
    /* make the parent/caller sleep so the child/debugger can catch it */
    sleep(dbg_sleep_secs);
  }
  else
    fprintf(stderr, "DEBUG_ExternalDebugger failed.\n");

}
