/*
 * Declarations for OLE2
 *
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

#ifndef __WINE_OLE2_H
#define __WINE_OLE2_H

#include <winerror.h>
#include <objbase.h>
#include <oleauto.h>
#include <oleidl.h>

struct tagMSG;

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#define E_DRAW                  VIEW_E_DRAW
#define DATA_E_FORMATETC        DV_E_FORMATETC

#define OLEIVERB_PRIMARY            (0L)
#define OLEIVERB_SHOW               (-1L)
#define OLEIVERB_OPEN               (-2L)
#define OLEIVERB_HIDE               (-3L)
#define OLEIVERB_UIACTIVATE         (-4L)
#define OLEIVERB_INPLACEACTIVATE    (-5L)
#define OLEIVERB_DISCARDUNDOSTATE   (-6L)
#define OLEIVERB_PROPERTIES         (-7L)

/*
 * API declarations
 */
HRESULT     WINAPI RegisterDragDrop(HWND,LPDROPTARGET);
HRESULT     WINAPI RevokeDragDrop(HWND);
HRESULT     WINAPI DoDragDrop(LPDATAOBJECT,LPDROPSOURCE,DWORD,DWORD*);
HRESULT  WINAPI OleLoadFromStream(IStream *pStm,REFIID iidInterface,void** ppvObj);
HRESULT  WINAPI OleSaveToStream(IPersistStream *pPStm,IStream *pStm);
HOLEMENU WINAPI OleCreateMenuDescriptor(HMENU hmenuCombined,LPOLEMENUGROUPWIDTHS lpMenuWidths);
HRESULT   WINAPI OleDestroyMenuDescriptor(HOLEMENU hmenuDescriptor);
HRESULT  WINAPI OleSetMenuDescriptor(HOLEMENU hmenuDescriptor,HWND hwndFrame,HWND hwndActiveObject,LPOLEINPLACEFRAME lpFrame,LPOLEINPLACEACTIVEOBJECT lpActiveObject);

HRESULT WINAPI ReadClassStg(IStorage *pstg,CLSID *pclsid);
HRESULT WINAPI WriteClassStm(IStream *pStm,REFCLSID rclsid);
HRESULT WINAPI ReadClassStm(IStream *pStm,CLSID *pclsid);


HRESULT     WINAPI OleSave(LPPERSISTSTORAGE pPS, LPSTORAGE pStg, BOOL fSameAsLoad);
HRESULT     WINAPI OleRegGetUserType(REFCLSID clsid,
				     DWORD dwFormOfType,
				     LPOLESTR* pszUserType);
HRESULT     WINAPI OleRegGetMiscStatus (REFCLSID clsid, DWORD dwAspect, DWORD* pdwStatus);
HRESULT     WINAPI OleRegEnumFormatEtc (REFCLSID clsid,
					DWORD    dwDirection,
					LPENUMFORMATETC* ppenumFormatetc);
HRESULT     WINAPI CreateStreamOnHGlobal (HGLOBAL hGlobal, BOOL fDeleteOnRelease, LPSTREAM* ppstm);
HRESULT     WINAPI GetHGlobalFromStream(LPSTREAM pstm, HGLOBAL* phglobal);
HRESULT     WINAPI OleRegEnumVerbs (REFCLSID clsid, LPENUMOLEVERB* ppenum);
BOOL        WINAPI OleIsRunning(LPOLEOBJECT pObject);
HRESULT     WINAPI OleCreateLinkFromData(LPDATAOBJECT pSrcDataObj, REFIID riid,
                DWORD renderopt, LPFORMATETC pFormatEtc,
                LPOLECLIENTSITE pClientSite, LPSTORAGE pStg,
                LPVOID* ppvObj);
HRESULT     WINAPI OleSetContainedObject(LPUNKNOWN pUnknown, BOOL fContained);
HRESULT     WINAPI OleQueryLinkFromData(IDataObject* pSrcDataObject);
HRESULT     WINAPI OleQueryCreateFromData(LPDATAOBJECT pSrcDataObject);
HRESULT     WINAPI OleRun(LPUNKNOWN pUnknown);
VOID        WINAPI ReleaseStgMedium(LPSTGMEDIUM);
HRESULT     WINAPI OleGetClipboard(IDataObject** ppDataObj);
HRESULT     WINAPI OleIsCurrentClipboard(LPDATAOBJECT);
HRESULT     WINAPI OleSetClipboard(LPDATAOBJECT);
HRESULT     WINAPI OleCreateStaticFromData(LPDATAOBJECT pSrcDataObj, REFIID iid,
                DWORD renderopt, LPFORMATETC pFormatEtc, LPOLECLIENTSITE pClientSite,
                LPSTORAGE pStg, LPVOID* ppvObj);
HRESULT     WINAPI ReadFmtUserTypeStg(LPSTORAGE pstg, CLIPFORMAT* pcf, LPOLESTR* lplpszUserType);
HRESULT     WINAPI OleLoad(LPSTORAGE pStg, REFIID riid, LPOLECLIENTSITE pClientSite, LPVOID* ppvObj);
HRESULT     WINAPI GetHGlobalFromILockBytes(LPLOCKBYTES plkbyt, HGLOBAL* phglobal);
HRESULT     WINAPI CreateILockBytesOnHGlobal(HGLOBAL hGlobal, BOOL fDeleteOnRelease, LPLOCKBYTES* pplkbyt);
HRESULT     WINAPI CreateDataAdviseHolder(LPDATAADVISEHOLDER* ppDAHolder);
HGLOBAL     WINAPI OleGetIconOfClass(REFCLSID rclsid, LPOLESTR lpszLabel, BOOL fUseTypeAsLabel);
HRESULT     WINAPI OleLockRunning(LPUNKNOWN pUnknown, BOOL fLock, BOOL fLastUnlockCloses);
HRESULT     WINAPI OleCreateFromFile(REFCLSID rclsid, LPCOLESTR lpszFileName, REFIID riid,
                DWORD renderopt, LPFORMATETC lpFormatEtc, LPOLECLIENTSITE pClientSite, LPSTORAGE pStg, LPVOID* ppvObj);
HRESULT     WINAPI OleCreateLink(LPMONIKER pmkLinkSrc, REFIID riid, DWORD renderopt, LPFORMATETC lpFormatEtc,
                LPOLECLIENTSITE pClientSite, LPSTORAGE pStg, LPVOID* ppvObj);
HRESULT     WINAPI OleCreate(REFCLSID rclsid, REFIID riid, DWORD renderopt, LPFORMATETC pFormatEtc, LPOLECLIENTSITE pClientSite,
                LPSTORAGE pStg, LPVOID* ppvObj);
HRESULT     WINAPI OleFlushClipboard(void);
HRESULT     WINAPI SetConvertStg(LPSTORAGE pStg, BOOL fConvert);
BOOL        WINAPI IsAccelerator(HACCEL hAccel, int cAccelEntries, struct tagMSG* lpMsg, WORD* lpwCmd);
HRESULT     WINAPI OleCreateLinkToFile(LPCOLESTR lpszFileName, REFIID riid, DWORD renderopt, LPFORMATETC lpFormatEtc,
                LPOLECLIENTSITE pClientSite, LPSTORAGE pStg, LPVOID* ppvObj);
HANDLE      WINAPI OleDuplicateData(HANDLE hSrc, CLIPFORMAT cfFormat, UINT uiFlags);
HRESULT     WINAPI WriteFmtUserTypeStg(LPSTORAGE pstg, CLIPFORMAT cf, LPOLESTR lpszUserType);
HRESULT     WINAPI OleTranslateAccelerator (LPOLEINPLACEFRAME lpFrame, LPOLEINPLACEFRAMEINFO lpFrameInfo, struct tagMSG* lpmsg);
HRESULT     WINAPI OleCreateFromData(LPDATAOBJECT pSrcDataObj, REFIID riid, DWORD renderopt, LPFORMATETC pFormatEtc,
                LPOLECLIENTSITE pClientSite, LPSTORAGE pStg, LPVOID* ppvObj);
HRESULT     WINAPI OleCreateDefaultHandler(REFCLSID  clsid,
					   LPUNKNOWN pUnkOuter,
					   REFIID    riid,
					   LPVOID*   ppvObj);
HRESULT     WINAPI CreateOleAdviseHolder (LPOLEADVISEHOLDER *ppOAHolder);
HRESULT     WINAPI OleInitialize(LPVOID pvReserved);
void        WINAPI OleUninitialize(void);
BOOL        WINAPI IsValidInterface(LPUNKNOWN punk);

/*
 *  OLE version conversion declarations
 */


typedef struct _OLESTREAM* LPOLESTREAM;
typedef struct _OLESTREAMVTBL {
	DWORD	(CALLBACK *Get)(LPOLESTREAM,LPSTR,DWORD);
	DWORD	(CALLBACK *Put)(LPOLESTREAM,LPSTR,DWORD);
} OLESTREAMVTBL;
typedef OLESTREAMVTBL*	LPOLESTREAMVTBL;
typedef struct _OLESTREAM {
	LPOLESTREAMVTBL	lpstbl;
} OLESTREAM;

HRESULT     WINAPI OleConvertOLESTREAMToIStorage( LPOLESTREAM lpolestream, LPSTORAGE pstg, const DVTARGETDEVICE* ptd);
HRESULT     WINAPI OleConvertIStorageToOLESTREAM( LPSTORAGE pstg, LPOLESTREAM lpolestream);

HRESULT     WINAPI OleGetAutoConvert( REFCLSID clsidOld, LPCLSID pClsidNew );
HRESULT     WINAPI OleSetAutoConvert( REFCLSID clsidOld, REFCLSID clsidNew );

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif  /* __WINE_OLE2_H */
