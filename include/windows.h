/*
 * Copyright (C) the Wine project
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

#ifndef __WINE_WINDOWS_H
#define __WINE_WINDOWS_H

#ifdef __WINESRC__
#error Wine should not include windows.h internally
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 800) && !defined(__cplusplus)
/* TYPE_ALIGNMENT generates this - move it outside the warning push/pop scope. */
# pragma warning(disable:4116)
#endif

#if defined(RC_INVOKED) && !defined(NOWINRES)
#include <winresrc.h>
#else /* RC_INVOKED && !NOWINRES */

/* All the basic includes */
#include <excpt.h>
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include <winnls.h>
#include <wincon.h>
#include <winver.h>
#include <winreg.h>
#include <winnetwk.h>

/* Not so essential ones */
#ifndef WIN32_LEAN_AND_MEAN

#include <cderr.h>
#include <dde.h>
#include <ddeml.h>
#include <dlgs.h>
#include <lzexpand.h>
#include <mmsystem.h>
#include <nb30.h>
#include <rpc.h>
#include <shellapi.h>
/* #include <winperf.h> */

#ifndef WINE_NOWINSOCK
#include <winsock2.h>
#endif /* WINE_NOWINSOCK */

#ifndef NOCRYPT
#include <wincrypt.h>
/* #include <winefs.h> */
/* #include <winscard.h> */
#endif /* !NOCRYPT */

#ifndef NOGDI
#include <winspool.h>
#ifdef INC_OLE1
/* #include <ole.h> */
#else
#include <ole2.h>
#endif
#include <commdlg.h>
#endif /* !NOGDI */

#endif /* !WIN32_LEAN_AND_MEAN */

/* #include <stralign.h> */

#ifdef INC_OLE2
#include <ole2.h>
#endif /* INC_OLE2 */

#ifndef NOSERVICE
#include <winsvc.h>
#endif /* !NOSERVICE */

#ifndef NOMCX
#include <mcx.h>
#endif /* !NOMCX */

#ifndef NOIMM
#include <imm.h>
#endif /* !NOIMM */

#endif  /* RC_INVOKED && !NOWINRES */
#endif  /* __WINE_WINDOWS_H */
