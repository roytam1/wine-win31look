/*
 * IDirect3DVolume8 implementation
 *
 * Copyright 2002-2003 Jason Edmeades
 *                     Raphael Junqueira
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

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"
#include "wine/debug.h"

#include "d3d8_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);

/* IDirect3DVolume IUnknown parts follow: */
HRESULT WINAPI IDirect3DVolume8Impl_QueryInterface(LPDIRECT3DVOLUME8 iface, REFIID riid, LPVOID* ppobj) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DVolume8)) {
        IDirect3DVolume8Impl_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p) not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3DVolume8Impl_AddRef(LPDIRECT3DVOLUME8 iface) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    TRACE("(%p) : AddRef from %ld\n", This, This->ref);
    return ++(This->ref);
}

ULONG WINAPI IDirect3DVolume8Impl_Release(LPDIRECT3DVOLUME8 iface) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    ULONG ref = --This->ref;
    TRACE("(%p) : ReleaseRef to %ld\n", This, This->ref);
    if (ref == 0) {
        HeapFree(GetProcessHeap(), 0, This->allocatedMemory);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DVolume8: */
HRESULT WINAPI IDirect3DVolume8Impl_GetDevice(LPDIRECT3DVOLUME8 iface, IDirect3DDevice8** ppDevice) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    TRACE("(%p) : returning %p\n", This, This->Device);
    *ppDevice = (LPDIRECT3DDEVICE8) This->Device;

    /* Note  Calling this method will increase the internal reference count 
       on the IDirect3DDevice8 interface. */
    IDirect3DDevice8Impl_AddRef(*ppDevice);

    return D3D_OK;
}

HRESULT WINAPI IDirect3DVolume8Impl_SetPrivateData(LPDIRECT3DVOLUME8 iface, REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    FIXME("(%p) : stub\n", This);    
    return D3D_OK;
}

HRESULT WINAPI IDirect3DVolume8Impl_GetPrivateData(LPDIRECT3DVOLUME8 iface, REFGUID  refguid, void* pData, DWORD* pSizeOfData) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    FIXME("(%p) : stub\n", This);    
    return D3D_OK;
}

HRESULT WINAPI IDirect3DVolume8Impl_FreePrivateData(LPDIRECT3DVOLUME8 iface, REFGUID refguid) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    FIXME("(%p) : stub\n", This);    
    return D3D_OK;
}

HRESULT WINAPI IDirect3DVolume8Impl_GetContainer(LPDIRECT3DVOLUME8 iface, REFIID riid, void** ppContainer) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    TRACE("(%p) : returning %p\n", This, This->Container);
    *ppContainer = This->Container;
    IUnknown_AddRef(This->Container);
    return D3D_OK;
}

HRESULT WINAPI IDirect3DVolume8Impl_GetDesc(LPDIRECT3DVOLUME8 iface, D3DVOLUME_DESC* pDesc) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    TRACE("(%p) : copying into %p\n", This, pDesc);
    memcpy(pDesc, &This->myDesc, sizeof(D3DVOLUME_DESC));
    return D3D_OK;
}

HRESULT WINAPI IDirect3DVolume8Impl_LockBox(LPDIRECT3DVOLUME8 iface, D3DLOCKED_BOX* pLockedVolume, CONST D3DBOX* pBox, DWORD Flags) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    FIXME("(%p) : pBox=%p stub\n", This, pBox);    

    /* fixme: should we really lock as such? */
    TRACE("(%p) : box=%p, output pbox=%p, allMem=%p\n", This, pBox, pLockedVolume, This->allocatedMemory);

    pLockedVolume->RowPitch   = This->bytesPerPixel * This->myDesc.Width;                        /* Bytes / row   */
    pLockedVolume->SlicePitch = This->bytesPerPixel * This->myDesc.Width * This->myDesc.Height;  /* Bytes / slice */
    if (!pBox) {
        TRACE("No box supplied - all is ok\n");
        pLockedVolume->pBits = This->allocatedMemory;
	This->lockedBox.Left   = 0;
	This->lockedBox.Top    = 0;
	This->lockedBox.Front  = 0;
	This->lockedBox.Right  = This->myDesc.Width;
	This->lockedBox.Bottom = This->myDesc.Height;
	This->lockedBox.Back   = This->myDesc.Depth;
    } else {
        TRACE("Lock Box (%p) = l %d, t %d, r %d, b %d, fr %d, ba %d\n", pBox, pBox->Left, pBox->Top, pBox->Right, pBox->Bottom, pBox->Front, pBox->Back);
        pLockedVolume->pBits = This->allocatedMemory + 
	  (pLockedVolume->SlicePitch * pBox->Front) + /* FIXME: is front < back or vica versa? */
	  (pLockedVolume->RowPitch * pBox->Top) + 
	  (pBox->Left * This->bytesPerPixel);
	This->lockedBox.Left   = pBox->Left;
	This->lockedBox.Top    = pBox->Top;
	This->lockedBox.Front  = pBox->Front;
	This->lockedBox.Right  = pBox->Right;
	This->lockedBox.Bottom = pBox->Bottom;
	This->lockedBox.Back   = pBox->Back;
    }
    
    if (Flags & (D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY)) {
      /* Don't dirtify */
    } else {
      /**
       * Dirtify on lock
       * as seen in msdn docs
       */
      IDirect3DVolume8Impl_AddDirtyBox(iface, &This->lockedBox);

      /**  Dirtify Container if needed */
      if (NULL != This->Container) {
        IDirect3DVolumeTexture8* cont = (IDirect3DVolumeTexture8*) This->Container;	
        D3DRESOURCETYPE containerType = IDirect3DBaseTexture8Impl_GetType((LPDIRECT3DBASETEXTURE8) cont);
        if (containerType == D3DRTYPE_VOLUMETEXTURE) {
	  IDirect3DBaseTexture8Impl* pTexture = (IDirect3DBaseTexture8Impl*) cont;
	  pTexture->Dirty = TRUE;
        } else {
	  FIXME("Set dirty on container type %d\n", containerType);
        }
      }
    }

    This->locked = TRUE;
    TRACE("returning memory@%p rpitch(%d) spitch(%d)\n", pLockedVolume->pBits, pLockedVolume->RowPitch, pLockedVolume->SlicePitch);
    return D3D_OK;
}

HRESULT WINAPI IDirect3DVolume8Impl_UnlockBox(LPDIRECT3DVOLUME8 iface) {
    ICOM_THIS(IDirect3DVolume8Impl,iface);
    if (FALSE == This->locked) {
      ERR("trying to lock unlocked volume@%p\n", This);  
      return D3DERR_INVALIDCALL;
    }
    TRACE("(%p) : unlocking volume\n", This);
    This->locked = FALSE;
    memset(&This->lockedBox, 0, sizeof(RECT));
    return D3D_OK;
}


ICOM_VTABLE(IDirect3DVolume8) Direct3DVolume8_Vtbl =
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    IDirect3DVolume8Impl_QueryInterface,
    IDirect3DVolume8Impl_AddRef,
    IDirect3DVolume8Impl_Release,
    IDirect3DVolume8Impl_GetDevice,
    IDirect3DVolume8Impl_SetPrivateData,
    IDirect3DVolume8Impl_GetPrivateData,
    IDirect3DVolume8Impl_FreePrivateData,
    IDirect3DVolume8Impl_GetContainer,
    IDirect3DVolume8Impl_GetDesc,
    IDirect3DVolume8Impl_LockBox,
    IDirect3DVolume8Impl_UnlockBox
};


HRESULT WINAPI IDirect3DVolume8Impl_CleanDirtyBox(LPDIRECT3DVOLUME8 iface) {
  ICOM_THIS(IDirect3DVolume8Impl,iface);
  This->Dirty = FALSE;
  This->lockedBox.Left   = This->myDesc.Width;
  This->lockedBox.Top    = This->myDesc.Height;
  This->lockedBox.Front  = This->myDesc.Depth;  
  This->lockedBox.Right  = 0;
  This->lockedBox.Bottom = 0;
  This->lockedBox.Back   = 0;
  return D3D_OK;
}

/**
 * Raphael:
 *   very stupid way to handle multiple dirty box but it works :)
 */
HRESULT WINAPI IDirect3DVolume8Impl_AddDirtyBox(LPDIRECT3DVOLUME8 iface, CONST D3DBOX* pDirtyBox) {
  ICOM_THIS(IDirect3DVolume8Impl,iface);
  This->Dirty = TRUE;
   if (NULL != pDirtyBox) {
    This->lockedBox.Left   = min(This->lockedBox.Left,   pDirtyBox->Left);
    This->lockedBox.Top    = min(This->lockedBox.Top,    pDirtyBox->Top);
    This->lockedBox.Front  = min(This->lockedBox.Front,  pDirtyBox->Front);
    This->lockedBox.Right  = max(This->lockedBox.Right,  pDirtyBox->Right);
    This->lockedBox.Bottom = max(This->lockedBox.Bottom, pDirtyBox->Bottom);
    This->lockedBox.Back   = max(This->lockedBox.Back,   pDirtyBox->Back);
  } else {
    This->lockedBox.Left   = 0;
    This->lockedBox.Top    = 0;
    This->lockedBox.Front  = 0;
    This->lockedBox.Right  = This->myDesc.Width;
    This->lockedBox.Bottom = This->myDesc.Height;
    This->lockedBox.Back   = This->myDesc.Depth;
  }
  return D3D_OK;
}
