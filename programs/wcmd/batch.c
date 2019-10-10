/*
 * WCMD - Wine-compatible command line interface - batch interface.
 *
 * Copyright (C) 1999 D A Pickles
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

#include "wcmd.h"

void WCMD_batch_command (char *line);

extern char nyi[];
extern char newline[];
extern char version_string[];
extern int echo_mode;
extern char quals[MAX_PATH], param1[MAX_PATH], param2[MAX_PATH];
extern BATCH_CONTEXT *context;
extern DWORD errorlevel;

#define MAXSTRING 1024

/****************************************************************************
 * WCMD_batch
 *
 * Open and execute a batch file.
 * On entry *command includes the complete command line beginning with the name
 * of the batch file (if a CALL command was entered the CALL has been removed).
 * *file is the name of the file, which might not exist and may not have the
 * .BAT suffix on. Called is 1 for a CALL, 0 otherwise.
 *
 * We need to handle recursion correctly, since one batch program might call another.
 * So parameters for this batch file are held in a BATCH_CONTEXT structure.
 */

void WCMD_batch (char *file, char *command, int called) {

#define WCMD_BATCH_EXT_SIZE 5

HANDLE h = INVALID_HANDLE_VALUE;
char string[MAXSTRING];
char extension[][WCMD_BATCH_EXT_SIZE] = {".bat",".cmd"};
int  i;
BATCH_CONTEXT *prev_context;

  for(i=0; (i<(sizeof(extension)/WCMD_BATCH_EXT_SIZE)) && 
           (h == INVALID_HANDLE_VALUE); i++) {
  strcpy (string, file);
  CharLower (string);
    if (strstr (string, extension[i]) == NULL) strcat (string, extension[i]);
  h = CreateFile (string, GENERIC_READ, FILE_SHARE_READ,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  }
  if (h == INVALID_HANDLE_VALUE) {
    SetLastError (ERROR_FILE_NOT_FOUND);
    WCMD_print_error ();
    return;
  }

/*
 *	Create a context structure for this batch file.
 */

  prev_context = context;
  context = (BATCH_CONTEXT *)LocalAlloc (LMEM_FIXED, sizeof (BATCH_CONTEXT));
  context -> h = h;
  context -> command = command;
  context -> shift_count = 0;
  context -> prev_context = prev_context;

/*
 * 	Work through the file line by line. Specific batch commands are processed here,
 * 	the rest are handled by the main command processor.
 */

  while (WCMD_fgets (string, sizeof(string), h)) {
    if (strlen(string) == MAXSTRING -1)
      WCMD_output("Line in Batch processing possible truncated. Using:\n%s\n",string);
    if (string[0] != ':') {                      /* Skip over labels */
      WCMD_batch_command (string);
    }
  }
  CloseHandle (h);

/*
 *	If invoked by a CALL, we return to the context of our caller. Otherwise return
 *	to the caller's caller.
 */

  LocalFree ((HANDLE)context);
  if ((prev_context != NULL) && (!called)) {
    CloseHandle (prev_context -> h);
    context = prev_context -> prev_context;
    LocalFree ((HANDLE)prev_context);
  }
  else {
    context = prev_context;
  }
}

/****************************************************************************
 * WCMD_batch_command
 *
 * Execute one line from a batch file, expanding parameters.
 */

void WCMD_batch_command (char *line) {

DWORD status;
char cmd1[MAXSTRING],cmd2[MAXSTRING];
char *p, *s, *t;
int i;

  /* Get working version of command line */
  strcpy(cmd1, line);

  /* Expand environment variables in a batch file %{0-9} first  */
  /*   Then env vars, and if any left (ie use of undefined vars,*/
  /*   replace with spaces                                      */
  /* FIXME: Winnt would replace %1%fred%1 with first parm, then */
  /*   contents of fred, then the digit 1. Would need to remove */
  /*   ExpandEnvStrings to achieve this                         */

  /* Replace use of %0...%9 */
  p = cmd1;
  while ((p = strchr(p, '%'))) {
    i = *(p+1) - '0';
    if ((i >= 0) && (i <= 9)) {
      s = strdup (p+2);
      t = WCMD_parameter (context -> command, i + context -> shift_count, NULL);
      strcpy (p, t);
      strcat (p, s);
      free (s);
    } else {
      p++;
    }
  }

  /* Now replace environment variables */
  status = ExpandEnvironmentStrings(cmd1, cmd2, sizeof(cmd2));
  if (!status) {
    WCMD_print_error ();
    return;
  }

  /* In a batch program, unknown variables are replace by nothing */
  /* so remove any remaining %var%                                */
  p = cmd2;
  while ((p = strchr(p, '%'))) {
    s = strchr(p+1, '%');
    if (!s) {
      *p=0x00;
    } else {
      t = strdup(s+1);
      strcpy(p, t);
      free(t);
    }
  }

  /* Show prompt before batch line IF echo is on */
  if (echo_mode && (line[0] != '@')) {
    WCMD_show_prompt();
    WCMD_output ("%s\n", cmd2);
  }

  WCMD_process_command (cmd2);
}

/*******************************************************************
 * WCMD_parameter - extract a parameter from a command line.
 *
 *	Returns the 'n'th space-delimited parameter on the command line (zero-based).
 *	Parameter is in static storage overwritten on the next call.
 *	Parameters in quotes (and brackets) are handled.
 *	Also returns a pointer to the location of the parameter in the command line.
 */

char *WCMD_parameter (char *s, int n, char **where) {

int i = 0;
static char param[MAX_PATH];
char *p;

  p = param;
  while (TRUE) {
    switch (*s) {
      case ' ':
	s++;
	break;
      case '"':
        if (where != NULL) *where = s;
	s++;
	while ((*s != '\0') && (*s != '"')) {
	  *p++ = *s++;
	}
        if (i == n) {
          *p = '\0';
          return param;
        }
	if (*s == '"') s++;
          param[0] = '\0';
          i++;
        p = param;
	break;
      case '(':
        if (where != NULL) *where = s;
	s++;
	while ((*s != '\0') && (*s != ')')) {
	  *p++ = *s++;
	}
        if (i == n) {
          *p = '\0';
          return param;
        }
	if (*s == ')') s++;
          param[0] = '\0';
          i++;
        p = param;
	break;
      case '\0':
        return param;
      default:
        if (where != NULL) *where = s;
	while ((*s != '\0') && (*s != ' ')) {
	  *p++ = *s++;
	}
        if (i == n) {
          *p = '\0';
          return param;
        }
          param[0] = '\0';
          i++;
        p = param;
    }
  }
}

/****************************************************************************
 * WCMD_fgets
 *
 * Get one line from a batch file. We can't use the native f* functions because
 * of the filename syntax differences between DOS and Unix. Also need to lose
 * the LF (or CRLF) from the line.
 */

char *WCMD_fgets (char *s, int n, HANDLE h) {

DWORD bytes;
BOOL status;
char *p;

  p = s;
  do {
    status = ReadFile (h, s, 1, &bytes, NULL);
    if ((status == 0) || ((bytes == 0) && (s == p))) return NULL;
    if (*s == '\n') bytes = 0;
    else if (*s != '\r') {
      s++;
      n--;
    }
    *s = '\0';
  } while ((bytes == 1) && (n > 1));
  return p;
}
