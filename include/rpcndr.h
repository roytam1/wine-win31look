/*
 * Copyright (C) 2000 Francois Gouget
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

#ifndef __RPCNDR_H_VERSION__
/* FIXME: What version?   Perhaps something is better than nothing, however incorrect */
#define __RPCNDR_H_VERSION__ ( 399 )
#endif

#ifndef __WINE_RPCNDR_H
#define __WINE_RPCNDR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <basetsd.h>

/* stupid #if can't handle casts... this __stupidity 
   is just a workaround for that limitation */

#define __NDR_CHAR_REP_MASK  0x000fL
#define __NDR_INT_REP_MASK   0x00f0L
#define __NDR_FLOAT_REP_MASK 0xff00L

#define __NDR_IEEE_FLOAT     0x0000L
#define __NDR_VAX_FLOAT      0x0100L
#define __NDR_IBM_FLOAT      0x0300L

#define __NDR_ASCII_CHAR     0x0000L
#define __NDR_EBCDIC_CHAR    0x0001L

#define __NDR_LITTLE_ENDIAN  0x0010L
#define __NDR_BIG_ENDIAN     0x0000L

/* Mac's are special */
#if defined(__RPC_MAC__)
  #define __NDR_LOCAL_DATA_REPRESENTATION \
    (__NDR_IEEE_FLOAT | __NDR_ASCII_CHAR | __NDR_BIG_ENDIAN)
#else
  #define __NDR_LOCAL_DATA_REPRESENTATION \
    (__NDR_IEEE_FLOAT | __NDR_ASCII_CHAR | __NDR_LITTLE_ENDIAN)
#endif

#define __NDR_LOCAL_ENDIAN \
  (__NDR_LOCAL_DATA_REPRESENTATION & __NDR_INT_REP_MASK)

/* for convenience, define NDR_LOCAL_IS_BIG_ENDIAN iff it is */
#if __NDR_LOCAL_ENDIAN == __NDR_BIG_ENDIAN
  #define NDR_LOCAL_IS_BIG_ENDIAN
#elif __NDR_LOCAL_ENDIAN == __NDR_LITTLE_ENDIAN
  #undef NDR_LOCAL_IS_BIG_ENDIAN
#else
  #error alien NDR_LOCAL_ENDIAN - Greg botched the defines again, please report
#endif

/* finally, do the casts like Microsoft */

#define NDR_CHAR_REP_MASK             ((unsigned long) __NDR_CHAR_REP_MASK)
#define NDR_INT_REP_MASK              ((unsigned long) __NDR_INT_REP_MASK)
#define NDR_FLOAT_REP_MASK            ((unsigned long) __NDR_FLOAT_REP_MASK)
#define NDR_IEEE_FLOAT                ((unsigned long) __NDR_IEEE_FLOAT)
#define NDR_VAX_FLOAT                 ((unsigned long) __NDR_VAX_FLOAT)
#define NDR_IBM_FLOAT                 ((unsigned long) __NDR_IBM_FLOAT)
#define NDR_ASCII_CHAR                ((unsigned long) __NDR_ASCII_CHAR)
#define NDR_EBCDIC_CHAR               ((unsigned long) __NDR_EBCDIC_CHAR)
#define NDR_LITTLE_ENDIAN             ((unsigned long) __NDR_LITTLE_ENDIAN)
#define NDR_BIG_ENDIAN                ((unsigned long) __NDR_BIG_ENDIAN)
#define NDR_LOCAL_DATA_REPRESENTATION ((unsigned long) __NDR_LOCAL_DATA_REPRESENTATION)
#define NDR_LOCAL_ENDIAN              ((unsigned long) __NDR_LOCAL_ENDIAN)


#define TARGET_IS_NT50_OR_LATER 1
#define TARGET_IS_NT40_OR_LATER 1
#define TARGET_IS_NT351_OR_WIN95_OR_LATER 1

#define small char
typedef unsigned char byte;
#define hyper __int64
#define MIDL_uhyper unsigned __int64
/* 'boolean' tend to conflict, let's call it _wine_boolean */
typedef unsigned char _wine_boolean;
/* typedef _wine_boolean boolean; */

#define __RPC_CALLEE WINAPI
#define RPC_VAR_ENTRY WINAPIV
#define NDR_SHAREABLE static

#define MIDL_ascii_strlen(s) strlen(s)
#define MIDL_ascii_strcpy(d,s) strcpy(d,s)
#define MIDL_memset(d,v,n) memset(d,v,n)
#define midl_user_free MIDL_user_free
#define midl_user_allocate MIDL_user_allocate

#define NdrFcShort(s) (unsigned char)(s & 0xff), (unsigned char)(s >> 8)
#define NdrFcLong(s)  (unsigned char)(s & 0xff), (unsigned char)((s & 0x0000ff00) >> 8), \
  (unsigned char)((s & 0x00ff0000) >> 16), (unsigned char)(s >> 24)

typedef struct
{
  void *pad[2];
  void *userContext;
} *NDR_SCONTEXT;

#define NDRSContextValue(hContext) (&(hContext)->userContext)
#define cbNDRContext 20

typedef void (__RPC_USER *NDR_RUNDOWN)(void *context);
typedef void (__RPC_USER *NDR_NOTIFY_ROUTINE)(void);
typedef void (__RPC_USER *NDR_NOTIFY2_ROUTINE)(_wine_boolean flag);

#define DECLSPEC_UUID(x)
#define MIDL_INTERFACE(x)   struct

struct _MIDL_STUB_MESSAGE;
struct _MIDL_STUB_DESC;
struct _FULL_PTR_XLAT_TABLES;

typedef void (__RPC_USER *EXPR_EVAL)(struct _MIDL_STUB_MESSAGE *);
typedef const unsigned char *PFORMAT_STRING;

typedef struct
{
  long Dimension;
  unsigned long *BufferConformanceMark;
  unsigned long *BufferVarianceMark;
  unsigned long *MaxCountArray;
  unsigned long *OffsetArray;
  unsigned long *ActualCountArray;
} ARRAY_INFO, *PARRAY_INFO;

typedef struct _NDR_PIPE_DESC *PNDR_PIPE_DESC;
typedef struct _NDR_PIPE_MESSAGE *PNDR_PIPE_MESSAGE;
typedef struct _NDR_ASYNC_MESSAGE *PNDR_ASYNC_MESSAGE;
typedef struct _NDR_CORRELATION_INFO *PNDR_CORRELATION_INFO;

#include <pshpack4.h>
typedef struct _MIDL_STUB_MESSAGE
{
  PRPC_MESSAGE RpcMsg;
  unsigned char *Buffer;
  unsigned char *BufferStart;
  unsigned char *BufferEnd;
  unsigned char *BufferMark;
  unsigned long BufferLength;
  unsigned long MemorySize;
  unsigned char *Memory;
  int IsClient;
  int ReuseBuffer;
  unsigned char *AllocAllNodesMemory;
  unsigned char *AllocAllNodesMemoryEnd;
  int IgnoreEmbeddedPointers;
  unsigned char *PointerBufferMark;
  unsigned char fBufferValid;
  unsigned char Unused;
  ULONG_PTR MaxCount;
  unsigned long Offset;
  unsigned long ActualCount;
  void * (__RPC_API *pfnAllocate)(size_t);
  void (__RPC_API *pfnFree)(void *);
  unsigned char *StackTop;
  unsigned char *pPresentedType;
  unsigned char *pTransmitType;
  handle_t SavedHandle;
  const struct _MIDL_STUB_DESC *StubDesc;
  struct _FULL_PTR_XLAT_TABLES *FullPtrXlatTables;
  unsigned long FullPtrRefId;
  unsigned long ulUnused1;
  int fInDontFree:1;
  int fDontCallFreeInst:1;
  int fInOnlyParam:1;
  int fHasReturn:1;
  int fHasExtensions:1;
  int fHasNewCorrDesc:1;
  int fUnused:10;
  unsigned long dwDestContext;
  void *pvDestContext;
  NDR_SCONTEXT *SavedContextHandles;
  long ParamNumber;
  struct IRpcChannelBuffer *pRpcChannelBuffer;
  PARRAY_INFO pArrayInfo;
  unsigned long *SizePtrCountArray;
  unsigned long *SizePtrOffsetArray;
  unsigned long *SizePtrLengthArray;
  void *pArgQueue;
  unsigned long dwStubPhase;
  PNDR_PIPE_DESC pPipeDesc;
  PNDR_ASYNC_MESSAGE pAsyncMsg;
  PNDR_CORRELATION_INFO pCorrInfo;
  unsigned char *pCorrMemory;
  void *pMemoryList;
  ULONG_PTR w2kReserved[5];
} MIDL_STUB_MESSAGE, *PMIDL_STUB_MESSAGE;
#include <poppack.h>

typedef struct _GENERIC_BINDING_ROUTINE_PAIR GENERIC_BINDING_ROUTINE_PAIR, *PGENERIC_BINDING_ROUTINE_PAIR;

typedef struct __GENERIC_BINDING_INFO GENERIC_BINDING_INFO, *PGENERIC_BINDING_INFO;

typedef void (__RPC_USER *XMIT_HELPER_ROUTINE)(PMIDL_STUB_MESSAGE);

typedef struct _XMIT_ROUTINE_QUINTUPLE
{
  XMIT_HELPER_ROUTINE pfnTranslateToXmit;
  XMIT_HELPER_ROUTINE pfnTranslateFromXmit;
  XMIT_HELPER_ROUTINE pfnFreeXmit;
  XMIT_HELPER_ROUTINE pfnFreeInst;
} XMIT_ROUTINE_QUINTUPLE, *PXMIT_ROUTINE_QUINTUPLE;

typedef unsigned long (__RPC_USER *USER_MARSHAL_SIZING_ROUTINE)(unsigned long *, unsigned long, void *);
typedef unsigned char * (__RPC_USER *USER_MARSHAL_MARSHALLING_ROUTINE)(unsigned long *, unsigned char *, void *);
typedef unsigned char * (__RPC_USER *USER_MARSHAL_UNMARSHALLING_ROUTINE)(unsigned long *, unsigned char *, void *);
typedef void (__RPC_USER *USER_MARSHAL_FREEING_ROUTINE)(unsigned long *, void *);

typedef struct _USER_MARSHAL_ROUTINE_QUADRUPLE
{
  USER_MARSHAL_SIZING_ROUTINE pfnBufferSize;
  USER_MARSHAL_MARSHALLING_ROUTINE pfnMarshall;
  USER_MARSHAL_UNMARSHALLING_ROUTINE pfnUnmarshall;
  USER_MARSHAL_FREEING_ROUTINE pfnFree;
} USER_MARSHAL_ROUTINE_QUADRUPLE;

typedef struct _MALLOC_FREE_STRUCT
{
  void * (__RPC_USER *pfnAllocate)(size_t);
  void   (__RPC_USER *pfnFree)(void *);
} MALLOC_FREE_STRUCT;

typedef struct _COMM_FAULT_OFFSETS
{
  short CommOffset;
  short FaultOffset;
} COMM_FAULT_OFFSETS;

typedef struct _MIDL_STUB_DESC
{
  void *RpcInterfaceInformation;
  void * (__RPC_API *pfnAllocate)(size_t);
  void (__RPC_API *pfnFree)(void *);
  union {
    handle_t *pAutoHandle;
    handle_t *pPrimitiveHandle;
    PGENERIC_BINDING_INFO pGenericBindingInfo;
  } IMPLICIT_HANDLE_INFO;
  const NDR_RUNDOWN *apfnNdrRundownRoutines;
  const GENERIC_BINDING_ROUTINE_PAIR *aGenericBindingRoutinePairs;
  const EXPR_EVAL *apfnExprEval;
  const XMIT_ROUTINE_QUINTUPLE *aXmitQuintuple;
  const unsigned char *pFormatTypes;
  int fCheckBounds;
  unsigned long Version;
  MALLOC_FREE_STRUCT *pMallocFreeStruct;
  long MIDLVersion;
  const COMM_FAULT_OFFSETS *CommFaultOffsets;
  const USER_MARSHAL_ROUTINE_QUADRUPLE *aUserMarshalQuadruple;
  const NDR_NOTIFY_ROUTINE *NotifyRoutineTable;
  ULONG_PTR mFlags;
  ULONG_PTR Reserved3;
  ULONG_PTR Reserved4;
  ULONG_PTR Reserved5;
} MIDL_STUB_DESC;
typedef const MIDL_STUB_DESC *PMIDL_STUB_DESC;

typedef struct _MIDL_FORMAT_STRING
{
  short Pad;
#if defined(__GNUC__)
  unsigned char Format[0];
#else
  unsigned char Format[1];
#endif
} MIDL_FORMAT_STRING;

typedef void (__RPC_API *STUB_THUNK)( PMIDL_STUB_MESSAGE );

typedef long (__RPC_API *SERVER_ROUTINE)();

typedef struct _MIDL_SERVER_INFO_
{
  PMIDL_STUB_DESC pStubDesc;
  const SERVER_ROUTINE *DispatchTable;
  PFORMAT_STRING ProcString;
  const unsigned short *FmtStringOffset;
  const STUB_THUNK *ThunkTable;
  PFORMAT_STRING LocalFormatTypes;
  PFORMAT_STRING LocalProcString;
  const unsigned short *LocalFmtStringOffset;
} MIDL_SERVER_INFO, *PMIDL_SERVER_INFO;

typedef struct _MIDL_STUBLESS_PROXY_INFO
{
  PMIDL_STUB_DESC pStubDesc;
  PFORMAT_STRING ProcFormatString;
  const unsigned short *FormatStringOffset;
  PFORMAT_STRING LocalFormatTypes;
  PFORMAT_STRING LocalProcStrings;
  const unsigned short *LocalFmtStringOffset;
} MIDL_STUBLESS_PROXY_INFO, *PMIDL_STUBLESS_PROXY_INFO;

typedef union _CLIENT_CALL_RETURN
{
  void *Pointer;
  LONG_PTR Simple;
} CLIENT_CALL_RETURN;

typedef enum {
  STUB_UNMARSHAL,
  STUB_CALL_SERVER,
  STUB_MARSHAL,
  STUB_CALL_SERVER_NO_HRESULT
} STUB_PHASE;

typedef enum {
  PROXY_CALCSIZE,
  PROXY_GETBUFFER,
  PROXY_MARSHAL,
  PROXY_SENDRECEIVE,
  PROXY_UNMARSHAL
} PROXY_PHASE;

typedef enum {
  XLAT_SERVER = 1,
  XLAT_CLIENT
} XLAT_SIDE;

typedef struct _FULL_PTR_TO_REFID_ELEMENT {
  struct _FULL_PTR_TO_REFID_ELEMENT *Next;
  void *Pointer;
  unsigned long RefId;
  unsigned char State;
} FULL_PTR_TO_REFID_ELEMENT, *PFULL_PTR_TO_REFID_ELEMENT;

/* Full pointer translation tables */
typedef struct _FULL_PTR_XLAT_TABLES {
  struct {
    void **XlatTable;
    unsigned char *StateTable;
    unsigned long NumberOfEntries;
  } RefIdToPointer;

  struct {
    PFULL_PTR_TO_REFID_ELEMENT *XlatTable;
    unsigned long NumberOfBuckets;
    unsigned long HashMask;
  } PointerToRefId;

  unsigned long           NextRefId;
  XLAT_SIDE               XlatSide;
} FULL_PTR_XLAT_TABLES,  *PFULL_PTR_XLAT_TABLES;

struct IRpcStubBuffer;

typedef unsigned long error_status_t;
typedef void  * NDR_CCONTEXT;

typedef struct _SCONTEXT_QUEUE {
  unsigned long NumberOfObjects;
  NDR_SCONTEXT *ArrayOfObjects;
} SCONTEXT_QUEUE, *PSCONTEXT_QUEUE;

RPCRTAPI RPC_BINDING_HANDLE RPC_ENTRY
  NDRCContextBinding( IN NDR_CCONTEXT CContext );

RPCRTAPI void RPC_ENTRY
  NDRCContextMarshall( IN NDR_CCONTEXT CContext, OUT void *pBuff );

RPCRTAPI void RPC_ENTRY
  NDRCContextUnmarshall( OUT NDR_CCONTEXT *pCContext, IN RPC_BINDING_HANDLE hBinding,
                         IN void *pBuff, IN unsigned long DataRepresentation );

RPCRTAPI void RPC_ENTRY
  NDRSContextMarshall( IN NDR_SCONTEXT CContext, OUT void *pBuff, IN NDR_RUNDOWN userRunDownIn );

RPCRTAPI NDR_SCONTEXT RPC_ENTRY
  NDRSContextUnmarshall( IN void *pBuff, IN unsigned long DataRepresentation );

RPCRTAPI void RPC_ENTRY
  NDRSContextMarshallEx( IN RPC_BINDING_HANDLE BindingHandle, IN NDR_SCONTEXT CContext, 
                         OUT void *pBuff, IN NDR_RUNDOWN userRunDownIn );

RPCRTAPI void RPC_ENTRY
  NDRSContextMarshall2( IN RPC_BINDING_HANDLE BindingHandle, IN NDR_SCONTEXT CContext,
                        OUT void *pBuff, IN NDR_RUNDOWN userRunDownIn, IN void * CtxGuard,
                        IN unsigned long Flags );

RPCRTAPI NDR_SCONTEXT RPC_ENTRY
  NDRSContextUnmarshallEx( IN RPC_BINDING_HANDLE BindingHandle, IN void *pBuff, 
                           IN unsigned long DataRepresentation );

RPCRTAPI NDR_SCONTEXT RPC_ENTRY
  NDRSContextUnmarshall2( IN RPC_BINDING_HANDLE BindingHandle, IN void *pBuff,
                          IN unsigned long DataRepresentation, IN void *CtxGuard,
                          IN unsigned long Flags );

RPCRTAPI void RPC_ENTRY
  RpcSsDestroyClientContext( IN void **ContextHandle );

RPCRTAPI void RPC_ENTRY
  NdrSimpleTypeMarshall( PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar );
RPCRTAPI void RPC_ENTRY
  NdrSimpleTypeUnmarshall( PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar );

/* while MS declares each prototype separately, I prefer to use macros for this kind of thing instead */
#define SIMPLE_TYPE_MARSHAL(type) \
RPCRTAPI unsigned char* RPC_ENTRY \
  Ndr##type##Marshall( PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat ); \
RPCRTAPI unsigned char* RPC_ENTRY \
  Ndr##type##Unmarshall( PMIDL_STUB_MESSAGE pStubMsg, unsigned char** ppMemory, PFORMAT_STRING pFormat, unsigned char fMustAlloc ); \
RPCRTAPI void RPC_ENTRY \
  Ndr##type##BufferSize( PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat ); \
RPCRTAPI unsigned long RPC_ENTRY \
  Ndr##type##MemorySize( PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat );

#define TYPE_MARSHAL(type) \
  SIMPLE_TYPE_MARSHAL(type) \
RPCRTAPI void RPC_ENTRY \
  Ndr##type##Free( PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat );

TYPE_MARSHAL(Pointer)
TYPE_MARSHAL(SimpleStruct)
TYPE_MARSHAL(ConformantStruct)
TYPE_MARSHAL(ConformantVaryingStruct)
TYPE_MARSHAL(ComplexStruct)
TYPE_MARSHAL(FixedArray)
TYPE_MARSHAL(ConformantArray)
TYPE_MARSHAL(ConformantVaryingArray)
TYPE_MARSHAL(VaryingArray)
TYPE_MARSHAL(ComplexArray)
TYPE_MARSHAL(EncapsulatedUnion)
TYPE_MARSHAL(NonEncapsulatedUnion)
TYPE_MARSHAL(ByteCountPointer)
TYPE_MARSHAL(XmitOrRepAs)
TYPE_MARSHAL(UserMarshal)
TYPE_MARSHAL(InterfacePointer)

SIMPLE_TYPE_MARSHAL(ConformantString)
SIMPLE_TYPE_MARSHAL(NonConformantString)

#undef TYPE_MARSHAL
#undef SIMPLE_TYPE_MARSHAL

RPCRTAPI void RPC_ENTRY
  NdrConvert2( PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat, long NumberParams );
RPCRTAPI void RPC_ENTRY
  NdrConvert( PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat );

/* Note: this should return a CLIENT_CALL_RETURN, but calling convention for
 * returning structures/unions is different between Windows and gcc on i386. */
LONG_PTR RPC_VAR_ENTRY
  NdrClientCall2( PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, ... );
LONG_PTR RPC_VAR_ENTRY
  NdrClientCall( PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, ... );

RPCRTAPI void RPC_ENTRY
  NdrServerCall2( PRPC_MESSAGE pRpcMsg );
RPCRTAPI void RPC_ENTRY
  NdrServerCall( PRPC_MESSAGE pRpcMsg );

RPCRTAPI long RPC_ENTRY
  NdrStubCall2( struct IRpcStubBuffer* pThis, struct IRpcChannelBuffer* pChannel, PRPC_MESSAGE pRpcMsg, LPDWORD pdwStubPhase );
RPCRTAPI long RPC_ENTRY
  NdrStubCall( struct IRpcStubBuffer* pThis, struct IRpcChannelBuffer* pChannel, PRPC_MESSAGE pRpcMsg, LPDWORD pdwStubPhase );

RPCRTAPI void* RPC_ENTRY
  NdrAllocate( PMIDL_STUB_MESSAGE pStubMsg, size_t Len );

RPCRTAPI void RPC_ENTRY
  NdrClearOutParameters( PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat, void *ArgAddr );
                            
RPCRTAPI void* RPC_ENTRY
  NdrOleAllocate( size_t Size );
RPCRTAPI void RPC_ENTRY
  NdrOleFree( void* NodeToFree );

RPCRTAPI void RPC_ENTRY
  NdrClientInitializeNew( PRPC_MESSAGE pRpcMessage, PMIDL_STUB_MESSAGE pStubMsg, 
                          PMIDL_STUB_DESC pStubDesc, unsigned int ProcNum );
RPCRTAPI unsigned char* RPC_ENTRY
  NdrServerInitializeNew( PRPC_MESSAGE pRpcMsg, PMIDL_STUB_MESSAGE pStubMsg, PMIDL_STUB_DESC pStubDesc );  
RPCRTAPI unsigned char* RPC_ENTRY
  NdrGetBuffer( MIDL_STUB_MESSAGE *stubmsg, unsigned long buflen, RPC_BINDING_HANDLE handle );
RPCRTAPI void RPC_ENTRY
  NdrFreeBuffer( MIDL_STUB_MESSAGE *pStubMsg );
RPCRTAPI unsigned char* RPC_ENTRY
  NdrSendReceive( MIDL_STUB_MESSAGE *stubmsg, unsigned char *buffer );

RPCRTAPI unsigned char * RPC_ENTRY
  NdrNsGetBuffer( PMIDL_STUB_MESSAGE pStubMsg, unsigned long BufferLength, RPC_BINDING_HANDLE Handle );
RPCRTAPI unsigned char * RPC_ENTRY
  NdrNsSendReceive( PMIDL_STUB_MESSAGE pStubMsg, unsigned char *pBufferEnd, RPC_BINDING_HANDLE *pAutoHandle );

RPCRTAPI PFULL_PTR_XLAT_TABLES RPC_ENTRY
  NdrFullPointerXlatInit( unsigned long NumberOfPointers, XLAT_SIDE XlatSide );
RPCRTAPI void RPC_ENTRY
  NdrFullPointerXlatFree( PFULL_PTR_XLAT_TABLES pXlatTables );
RPCRTAPI int RPC_ENTRY
  NdrFullPointerQueryPointer( PFULL_PTR_XLAT_TABLES pXlatTables, void *pPointer,
                              unsigned char QueryType, unsigned long *pRefId );
RPCRTAPI int RPC_ENTRY
  NdrFullPointerQueryRefId( PFULL_PTR_XLAT_TABLES pXlatTables, unsigned long RefId,
                            unsigned char QueryType, void **ppPointer );
RPCRTAPI void RPC_ENTRY
  NdrFullPointerInsertRefId( PFULL_PTR_XLAT_TABLES pXlatTables, unsigned long RefId, void *pPointer );
RPCRTAPI int RPC_ENTRY
  NdrFullPointerFree( PFULL_PTR_XLAT_TABLES pXlatTables, void *Pointer );

RPCRTAPI void RPC_ENTRY
  NdrRpcSsEnableAllocate( PMIDL_STUB_MESSAGE pMessage );
RPCRTAPI void RPC_ENTRY
  NdrRpcSsDisableAllocate( PMIDL_STUB_MESSAGE pMessage );
RPCRTAPI void RPC_ENTRY
  NdrRpcSmSetClientToOsf( PMIDL_STUB_MESSAGE pMessage );
RPCRTAPI void * RPC_ENTRY
  NdrRpcSmClientAllocate( IN size_t Size );
RPCRTAPI void RPC_ENTRY
  NdrRpcSmClientFree( IN void *NodeToFree );
RPCRTAPI void * RPC_ENTRY
  NdrRpcSsDefaultAllocate( IN size_t Size );
RPCRTAPI void RPC_ENTRY
  NdrRpcSsDefaultFree( IN void *NodeToFree );

#ifdef __cplusplus
}
#endif
#endif /*__WINE_RPCNDR_H */
