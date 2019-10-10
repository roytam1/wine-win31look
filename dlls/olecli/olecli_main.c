/*
 *	OLECLI library
 *
 *	Copyright 1995	Martin von Loewis
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

/*	At the moment, these are only empty stubs.
 */

#include "config.h"

#include <stdarg.h>

#include "windef.h"
#include "wine/windef16.h"
#include "winbase.h"
#include "wingdi.h"
#include "wownt32.h"
#include "objbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

typedef enum
{
    OLE_OK,
    OLE_WAIT_FOR_RELEASE,
    OLE_BUSY,
    OLE_ERROR_PROTECT_ONLY,
    OLE_ERROR_MEMORY,
    OLE_ERROR_STREAM,
    OLE_ERROR_STATIC,
    OLE_ERROR_BLANK,
    OLE_ERROR_DRAW,
    OLE_ERROR_METAFILE,
    OLE_ERROR_ABORT,
    OLE_ERROR_CLIPBOARD,
    OLE_ERROR_FORMAT,
    OLE_ERROR_OBJECT,
    OLE_ERROR_OPTION,
    OLE_ERROR_PROTOCOL,
    OLE_ERROR_ADDRESS,
    OLE_ERROR_NOT_EQUAL,
    OLE_ERROR_HANDLE,
    OLE_ERROR_GENERIC,
    OLE_ERROR_CLASS,
    OLE_ERROR_SYNTAX,
    OLE_ERROR_DATATYPE,
    OLE_ERROR_PALETTE,
    OLE_ERROR_NOT_LINK,
    OLE_ERROR_NOT_EMPTY,
    OLE_ERROR_SIZE,
    OLE_ERROR_DRIVE,
    OLE_ERROR_NETWORK,
    OLE_ERROR_NAME,
    OLE_ERROR_TEMPLATE,
    OLE_ERROR_NEW,
    OLE_ERROR_EDIT,
    OLE_ERROR_OPEN,
    OLE_ERROR_NOT_OPEN,
    OLE_ERROR_LAUNCH,
    OLE_ERROR_COMM,
    OLE_ERROR_TERMINATE,
    OLE_ERROR_COMMAND,
    OLE_ERROR_SHOW,
    OLE_ERROR_DOVERB,
    OLE_ERROR_ADVISE_NATIVE,
    OLE_ERROR_ADVISE_PICT,
    OLE_ERROR_ADVISE_RENAME,
    OLE_ERROR_POKE_NATIVE,
    OLE_ERROR_REQUEST_NATIVE,
    OLE_ERROR_REQUEST_PICT,
    OLE_ERROR_SERVER_BLOCKED,
    OLE_ERROR_REGISTRATION,
    OLE_ERROR_ALREADY_REGISTERED,
    OLE_ERROR_TASK,
    OLE_ERROR_OUTOFDATE,
    OLE_ERROR_CANT_UPDATE_CLIENT,
    OLE_ERROR_UPDATE,
    OLE_ERROR_SETDATA_FORMAT,
    OLE_ERROR_STATIC_FROM_OTHER_OS,
    OLE_WARN_DELETE_DATA = 1000
} OLESTATUS;

typedef enum
{
    olerender_none,
    olerender_draw,
    olerender_format
} OLEOPT_RENDER;

typedef enum
{
    oleupdate_always,
    oleupdate_onsave,
    oleupdate_oncall,
    oleupdate_onclose
} OLEOPT_UPDATE;

typedef enum {
    OLE_NONE,     /* none */
    OLE_DELETE,   /* delete object */
    OLE_LNKPASTE, /* link paste */
    OLE_EMBPASTE, /* paste(and update) */
    OLE_SHOW,
    OLE_RUN,
    OLE_ACTIVATE,
    OLE_UPDATE,
    OLE_CLOSE,
    OLE_RECONNECT,
    OLE_SETUPDATEOPTIONS,
    OLE_SERVERRUNLAUNCH, /* unlaunch (terminate?) server */
    OLE_LOADFROMSTREAM,  /* (auto reconnect) */
    OLE_SETDATA,         /* OleSetData */
    OLE_REQUESTDATA,     /* OleRequestData */
    OLE_OTHER,
    OLE_CREATE,
    OLE_CREATEFROMTEMPLATE,
    OLE_CREATELINKFROMFILE,
    OLE_COPYFROMLNK,
    OLE_CREATREFROMFILE,
    OLE_CREATEINVISIBLE
} OLE_RELEASE_METHOD;

typedef LONG LHCLIENTDOC;
typedef struct _OLEOBJECT *LPOLEOBJECT;
typedef struct _OLECLIENT *LPOLECLIENT;
typedef LONG OLECLIPFORMAT;/* dunno about this type, please change/add */
typedef OLEOPT_UPDATE *LPOLEOPT_UPDATE;
typedef LPCSTR LPCOLESTR16;

struct _OLESTREAM;

typedef struct _OLEOBJECTVTBL {
    void CALLBACK *(*QueryProtocol)(LPOLEOBJECT,LPCOLESTR16);
    OLESTATUS      (CALLBACK *Release)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *Show)(LPOLEOBJECT,BOOL16);
    OLESTATUS      (CALLBACK *DoVerb)(LPOLEOBJECT,UINT16,BOOL16,BOOL16);
    OLESTATUS      (CALLBACK *GetData)(LPOLEOBJECT,OLECLIPFORMAT,HANDLE16 *);
    OLESTATUS      (CALLBACK *SetData)(LPOLEOBJECT,OLECLIPFORMAT,HANDLE16);
    OLESTATUS      (CALLBACK *SetTargetDevice)(LPOLEOBJECT,HGLOBAL16);
    OLESTATUS      (CALLBACK *SetBounds)(LPOLEOBJECT,LPRECT16);
    OLESTATUS      (CALLBACK *EnumFormats)(LPOLEOBJECT,OLECLIPFORMAT);
    OLESTATUS      (CALLBACK *SetColorScheme)(LPOLEOBJECT,struct tagLOGPALETTE*);
    OLESTATUS      (CALLBACK *Delete)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *SetHostNames)(LPOLEOBJECT,LPCOLESTR16,LPCOLESTR16);
    OLESTATUS      (CALLBACK *SaveToStream)(LPOLEOBJECT,struct _OLESTREAM*);
    OLESTATUS      (CALLBACK *Clone)(LPOLEOBJECT,LPOLECLIENT,LHCLIENTDOC,LPCOLESTR16,LPOLEOBJECT *);
    OLESTATUS      (CALLBACK *CopyFromLink)(LPOLEOBJECT,LPOLECLIENT,LHCLIENTDOC,LPCOLESTR16,LPOLEOBJECT *);
    OLESTATUS      (CALLBACK *Equal)(LPOLEOBJECT,LPOLEOBJECT);
    OLESTATUS      (CALLBACK *CopyToClipBoard)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *Draw)(LPOLEOBJECT,HDC16,LPRECT16,LPRECT16,HDC16);
    OLESTATUS      (CALLBACK *Activate)(LPOLEOBJECT,UINT16,BOOL16,BOOL16,HWND16,LPRECT16);
    OLESTATUS      (CALLBACK *Execute)(LPOLEOBJECT,HGLOBAL16,UINT16);
    OLESTATUS      (CALLBACK *Close)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *Update)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *Reconnect)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *ObjectConvert)(LPOLEOBJECT,LPCOLESTR16,LPOLECLIENT,LHCLIENTDOC,LPCOLESTR16,LPOLEOBJECT*);
    OLESTATUS      (CALLBACK *GetLinkUpdateOptions)(LPOLEOBJECT,LPOLEOPT_UPDATE);
    OLESTATUS      (CALLBACK *SetLinkUpdateOptions)(LPOLEOBJECT,OLEOPT_UPDATE);
    OLESTATUS      (CALLBACK *Rename)(LPOLEOBJECT,LPCOLESTR16);
    OLESTATUS      (CALLBACK *QueryName)(LPOLEOBJECT,LPSTR,LPUINT16);
    OLESTATUS      (CALLBACK *QueryType)(LPOLEOBJECT,LPLONG);
    OLESTATUS      (CALLBACK *QueryBounds)(LPOLEOBJECT,LPRECT16);
    OLESTATUS      (CALLBACK *QuerySize)(LPOLEOBJECT,LPDWORD);
    OLESTATUS      (CALLBACK *QueryOpen)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *QueryOutOfDate)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *QueryReleaseStatus)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *QueryReleaseError)(LPOLEOBJECT);
    OLE_RELEASE_METHOD (CALLBACK *QueryReleaseMethod)(LPOLEOBJECT);
    OLESTATUS      (CALLBACK *RequestData)(LPOLEOBJECT,OLECLIPFORMAT);
    OLESTATUS      (CALLBACK *ObjectLong)(LPOLEOBJECT,UINT16,LPLONG);
} OLEOBJECTVTBL;
typedef OLEOBJECTVTBL *LPOLEOBJECTVTBL;

typedef struct _OLEOBJECT
{
    LPOLEOBJECTVTBL lpvtbl;
} OLEOBJECT;

static LONG OLE_current_handle;

/******************************************************************************
 *		OleSavedClientDoc	[OLECLI32.45]
 */
OLESTATUS WINAPI OleSavedClientDoc(LHCLIENTDOC hDoc)
{
    FIXME("(%ld: stub\n", hDoc);
    return OLE_OK;
}

/******************************************************************************
 *		OleRegisterClientDoc	[OLECLI32.41]
 */
OLESTATUS WINAPI OleRegisterClientDoc(LPCSTR classname, LPCSTR docname,
                                        LONG reserved, LHCLIENTDOC *hRet )
{
    FIXME("(%s,%s,...): stub\n",classname,docname);
    *hRet=++OLE_current_handle;
    return OLE_OK;
}

/******************************************************************************
 *		OleRenameClientDoc	[OLECLI32.43]
 */
OLESTATUS WINAPI OleRenameClientDoc(LHCLIENTDOC hDoc, LPCSTR newName)
{
    FIXME("(%ld,%s,...): stub\n",hDoc, newName);
    return OLE_OK;
}

/******************************************************************************
 *		OleRevokeClientDoc	[OLECLI32.42]
 */
OLESTATUS WINAPI OleRevokeClientDoc(LHCLIENTDOC hServerDoc)
{
    FIXME("(%ld): stub\n",hServerDoc);
    return OLE_OK;
}

/******************************************************************************
 *           OleCreateLinkFromClip	[OLECLI32.11]
 */
OLESTATUS WINAPI OleCreateLinkFromClip(
	LPCSTR name,LPOLECLIENT olecli,LHCLIENTDOC hclientdoc,LPCSTR xname,
	LPOLEOBJECT *lpoleob,OLEOPT_RENDER render,OLECLIPFORMAT clipformat
) {
	FIXME("(%s,%p,%08lx,%s,%p,%d,%ld): stub!\n",
	      name,olecli,hclientdoc,xname,lpoleob,render,clipformat);
	return OLE_OK;
}

/******************************************************************************
 *           OleQueryLinkFromClip	[OLECLI32.9]
 */
OLESTATUS WINAPI OleQueryLinkFromClip(LPCSTR name,OLEOPT_RENDER render,OLECLIPFORMAT clipformat) {
	FIXME("(%s,%d,%ld): stub!\n",name,render,clipformat);
	return OLE_OK;
}

/******************************************************************************
 *           OleQueryCreateFromClip	[OLECLI32.10]
 */
OLESTATUS WINAPI OleQueryCreateFromClip(LPCSTR name,OLEOPT_RENDER render,OLECLIPFORMAT clipformat) {
	FIXME("(%s,%d,%ld): stub!\n",name,render,clipformat);
	return OLE_OK;
}

/******************************************************************************
 *		OleIsDcMeta	[OLECLI32.60]
 */
BOOL WINAPI OleIsDcMeta(HDC hdc)
{
    TRACE("(%p)\n",hdc);
    return GetObjectType( hdc ) == OBJ_METADC;
}

/******************************************************************************
 *		OleSetHostNames	[OLECLI32.15]
 */
OLESTATUS WINAPI OleSetHostNames(LPOLEOBJECT oleob,LPCSTR name1,LPCSTR name2) {
	FIXME("(%p,%s,%s): stub\n",oleob,name1,name2);
	return OLE_OK;
}

/******************************************************************************
 *		OleQueryType	[OLECLI32.14]
 */
OLESTATUS WINAPI OleQueryType(LPOLEOBJECT oleob,LONG*xlong) {
	FIXME("(%p,%p): stub!\n",oleob,xlong);
	if (!oleob)
		return 0x10;
	TRACE("Calling OLEOBJECT.QueryType (%p) (%p,%p)\n",
	      oleob->lpvtbl->QueryType,oleob,xlong);
	return oleob->lpvtbl->QueryType(oleob,xlong);
}

/******************************************************************************
 *		OleCreateFromClip	[OLECLI32.12]
 */
OLESTATUS WINAPI OleCreateFromClip(
	LPCSTR name,LPOLECLIENT olecli,LHCLIENTDOC hclientdoc,LPCSTR xname,
	LPOLEOBJECT *lpoleob,OLEOPT_RENDER render, OLECLIPFORMAT clipformat
) {
	FIXME("(%s,%p,%08lx,%s,%p,%d,%ld): stub!\n",
	      name,olecli,hclientdoc,xname,lpoleob,render,clipformat);
	/* clipb type, object kreieren entsprechend etc. */
	return OLE_OK;
}
