

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0626 */
/* at Mon Jan 18 19:14:07 2038
 */
/* Compiler settings for OmniRig.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.01.0626 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __omnirig_h__
#define __omnirig_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if _CONTROL_FLOW_GUARD_XFG
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

/* Forward Declarations */ 

#ifndef __IOmniRigX_FWD_DEFINED__
#define __IOmniRigX_FWD_DEFINED__
typedef interface IOmniRigX IOmniRigX;

#endif 	/* __IOmniRigX_FWD_DEFINED__ */


#ifndef __IOmniRigXEvents_FWD_DEFINED__
#define __IOmniRigXEvents_FWD_DEFINED__
typedef interface IOmniRigXEvents IOmniRigXEvents;

#endif 	/* __IOmniRigXEvents_FWD_DEFINED__ */


#ifndef __IRigX_FWD_DEFINED__
#define __IRigX_FWD_DEFINED__
typedef interface IRigX IRigX;

#endif 	/* __IRigX_FWD_DEFINED__ */


#ifndef __IPortBits_FWD_DEFINED__
#define __IPortBits_FWD_DEFINED__
typedef interface IPortBits IPortBits;

#endif 	/* __IPortBits_FWD_DEFINED__ */


#ifndef __OmniRigX_FWD_DEFINED__
#define __OmniRigX_FWD_DEFINED__

#ifdef __cplusplus
typedef class OmniRigX OmniRigX;
#else
typedef struct OmniRigX OmniRigX;
#endif /* __cplusplus */

#endif 	/* __OmniRigX_FWD_DEFINED__ */


#ifndef __RigX_FWD_DEFINED__
#define __RigX_FWD_DEFINED__

#ifdef __cplusplus
typedef class RigX RigX;
#else
typedef struct RigX RigX;
#endif /* __cplusplus */

#endif 	/* __RigX_FWD_DEFINED__ */


#ifndef __PortBits_FWD_DEFINED__
#define __PortBits_FWD_DEFINED__

#ifdef __cplusplus
typedef class PortBits PortBits;
#else
typedef struct PortBits PortBits;
#endif /* __cplusplus */

#endif 	/* __PortBits_FWD_DEFINED__ */


#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __OmniRig_LIBRARY_DEFINED__
#define __OmniRig_LIBRARY_DEFINED__

/* library OmniRig */
/* [helpstring][version][uuid] */ 





typedef /* [public][public][public][public][public][public][public][public][public][public][public][public][public][public][public][version][uuid] */  DECLSPEC_UUID("80E6D479-0553-44E9-AA50-1878A12719FB") 
enum __MIDL___MIDL_itf_OmniRig_0000_0000_0001
    {
        PM_UNKNOWN	= 1,
        PM_FREQ	= 2,
        PM_FREQA	= 4,
        PM_FREQB	= 8,
        PM_PITCH	= 16,
        PM_RITOFFSET	= 32,
        PM_RIT0	= 64,
        PM_VFOAA	= 128,
        PM_VFOAB	= 256,
        PM_VFOBA	= 512,
        PM_VFOBB	= 1024,
        PM_VFOA	= 2048,
        PM_VFOB	= 4096,
        PM_VFOEQUAL	= 8192,
        PM_VFOSWAP	= 16384,
        PM_SPLITON	= 32768,
        PM_SPLITOFF	= 0x10000,
        PM_RITON	= 0x20000,
        PM_RITOFF	= 0x40000,
        PM_XITON	= 0x80000,
        PM_XITOFF	= 0x100000,
        PM_RX	= 0x200000,
        PM_TX	= 0x400000,
        PM_CW_U	= 0x800000,
        PM_CW_L	= 0x1000000,
        PM_SSB_U	= 0x2000000,
        PM_SSB_L	= 0x4000000,
        PM_DIG_U	= 0x8000000,
        PM_DIG_L	= 0x10000000,
        PM_AM	= 0x20000000,
        PM_FM	= 0x40000000
    } 	RigParamX;

typedef /* [public][public][version][uuid] */  DECLSPEC_UUID("47EE3D83-5CC0-4C3E-94FB-28FF8DC79C48") 
enum __MIDL___MIDL_itf_OmniRig_0000_0000_0002
    {
        ST_NOTCONFIGURED	= 0,
        ST_DISABLED	= 1,
        ST_PORTBUSY	= 2,
        ST_NOTRESPONDING	= 3,
        ST_ONLINE	= 4
    } 	RigStatusX;


EXTERN_C const IID LIBID_OmniRig;

#ifndef __IOmniRigX_INTERFACE_DEFINED__
#define __IOmniRigX_INTERFACE_DEFINED__

/* interface IOmniRigX */
/* [object][oleautomation][dual][helpstring][version][uuid] */ 


EXTERN_C const IID IID_IOmniRigX;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("501A2858-3331-467A-837A-989FDEDACC7D")
    IOmniRigX : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_InterfaceVersion( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_SoftwareVersion( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Rig1( 
            /* [retval][out] */ IRigX **Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Rig2( 
            /* [retval][out] */ IRigX **Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_DialogVisible( 
            /* [retval][out] */ VARIANT_BOOL *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_DialogVisible( 
            /* [in] */ VARIANT_BOOL Value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IOmniRigXVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IOmniRigX * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IOmniRigX * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IOmniRigX * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IOmniRigX * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IOmniRigX * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IOmniRigX * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IOmniRigX * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IOmniRigX, get_InterfaceVersion)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_InterfaceVersion )( 
            IOmniRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IOmniRigX, get_SoftwareVersion)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_SoftwareVersion )( 
            IOmniRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IOmniRigX, get_Rig1)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Rig1 )( 
            IOmniRigX * This,
            /* [retval][out] */ IRigX **Value);
        
        DECLSPEC_XFGVIRT(IOmniRigX, get_Rig2)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Rig2 )( 
            IOmniRigX * This,
            /* [retval][out] */ IRigX **Value);
        
        DECLSPEC_XFGVIRT(IOmniRigX, get_DialogVisible)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_DialogVisible )( 
            IOmniRigX * This,
            /* [retval][out] */ VARIANT_BOOL *Value);
        
        DECLSPEC_XFGVIRT(IOmniRigX, put_DialogVisible)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_DialogVisible )( 
            IOmniRigX * This,
            /* [in] */ VARIANT_BOOL Value);
        
        END_INTERFACE
    } IOmniRigXVtbl;

    interface IOmniRigX
    {
        CONST_VTBL struct IOmniRigXVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IOmniRigX_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IOmniRigX_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IOmniRigX_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IOmniRigX_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IOmniRigX_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IOmniRigX_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IOmniRigX_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IOmniRigX_get_InterfaceVersion(This,Value)	\
    ( (This)->lpVtbl -> get_InterfaceVersion(This,Value) ) 

#define IOmniRigX_get_SoftwareVersion(This,Value)	\
    ( (This)->lpVtbl -> get_SoftwareVersion(This,Value) ) 

#define IOmniRigX_get_Rig1(This,Value)	\
    ( (This)->lpVtbl -> get_Rig1(This,Value) ) 

#define IOmniRigX_get_Rig2(This,Value)	\
    ( (This)->lpVtbl -> get_Rig2(This,Value) ) 

#define IOmniRigX_get_DialogVisible(This,Value)	\
    ( (This)->lpVtbl -> get_DialogVisible(This,Value) ) 

#define IOmniRigX_put_DialogVisible(This,Value)	\
    ( (This)->lpVtbl -> put_DialogVisible(This,Value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IOmniRigX_INTERFACE_DEFINED__ */


#ifndef __IOmniRigXEvents_DISPINTERFACE_DEFINED__
#define __IOmniRigXEvents_DISPINTERFACE_DEFINED__

/* dispinterface IOmniRigXEvents */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID DIID_IOmniRigXEvents;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("2219175F-E561-47E7-AD17-73C4D8891AA1")
    IOmniRigXEvents : public IDispatch
    {
    };
    
#else 	/* C style interface */

    typedef struct IOmniRigXEventsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IOmniRigXEvents * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IOmniRigXEvents * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IOmniRigXEvents * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IOmniRigXEvents * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IOmniRigXEvents * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IOmniRigXEvents * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IOmniRigXEvents * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        END_INTERFACE
    } IOmniRigXEventsVtbl;

    interface IOmniRigXEvents
    {
        CONST_VTBL struct IOmniRigXEventsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IOmniRigXEvents_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IOmniRigXEvents_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IOmniRigXEvents_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IOmniRigXEvents_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IOmniRigXEvents_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IOmniRigXEvents_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IOmniRigXEvents_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */


#endif 	/* __IOmniRigXEvents_DISPINTERFACE_DEFINED__ */


#ifndef __IRigX_INTERFACE_DEFINED__
#define __IRigX_INTERFACE_DEFINED__

/* interface IRigX */
/* [object][oleautomation][dual][helpstring][version][uuid] */ 


EXTERN_C const IID IID_IRigX;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D30A7E51-5862-45B7-BFFA-6415917DA0CF")
    IRigX : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_RigType( 
            /* [retval][out] */ BSTR *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_ReadableParams( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_WriteableParams( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE IsParamReadable( 
            /* [in] */ RigParamX Param,
            /* [retval][out] */ VARIANT_BOOL *Value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE IsParamWriteable( 
            /* [in] */ RigParamX Param,
            /* [retval][out] */ VARIANT_BOOL *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Status( 
            /* [retval][out] */ RigStatusX *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_StatusStr( 
            /* [retval][out] */ BSTR *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Freq( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Freq( 
            /* [in] */ long Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_FreqA( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_FreqA( 
            /* [in] */ long Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_FreqB( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_FreqB( 
            /* [in] */ long Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_RitOffset( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_RitOffset( 
            /* [in] */ long Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Pitch( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Pitch( 
            /* [in] */ long Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Vfo( 
            /* [retval][out] */ RigParamX *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Vfo( 
            /* [in] */ RigParamX Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Split( 
            /* [retval][out] */ RigParamX *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Split( 
            /* [in] */ RigParamX Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Rit( 
            /* [retval][out] */ RigParamX *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Rit( 
            /* [in] */ RigParamX Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Xit( 
            /* [retval][out] */ RigParamX *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Xit( 
            /* [in] */ RigParamX Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Tx( 
            /* [retval][out] */ RigParamX *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Tx( 
            /* [in] */ RigParamX Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Mode( 
            /* [retval][out] */ RigParamX *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Mode( 
            /* [in] */ RigParamX Value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE ClearRit( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE SetSimplexMode( 
            /* [in] */ long Freq) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE SetSplitMode( 
            /* [in] */ long RxFreq,
            /* [in] */ long TxFreq) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE FrequencyOfTone( 
            /* [in] */ long Tone,
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE SendCustomCommand( 
            /* [in] */ VARIANT Command,
            /* [in] */ long ReplyLength,
            /* [in] */ VARIANT ReplyEnd) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE GetRxFrequency( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE GetTxFrequency( 
            /* [retval][out] */ long *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_PortBits( 
            /* [retval][out] */ IPortBits **Value) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IRigXVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IRigX * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IRigX * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IRigX * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IRigX * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IRigX * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IRigX * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IRigX * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IRigX, get_RigType)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_RigType )( 
            IRigX * This,
            /* [retval][out] */ BSTR *Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_ReadableParams)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_ReadableParams )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_WriteableParams)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_WriteableParams )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, IsParamReadable)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *IsParamReadable )( 
            IRigX * This,
            /* [in] */ RigParamX Param,
            /* [retval][out] */ VARIANT_BOOL *Value);
        
        DECLSPEC_XFGVIRT(IRigX, IsParamWriteable)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *IsParamWriteable )( 
            IRigX * This,
            /* [in] */ RigParamX Param,
            /* [retval][out] */ VARIANT_BOOL *Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Status)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Status )( 
            IRigX * This,
            /* [retval][out] */ RigStatusX *Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_StatusStr)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_StatusStr )( 
            IRigX * This,
            /* [retval][out] */ BSTR *Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Freq)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Freq )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_Freq)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Freq )( 
            IRigX * This,
            /* [in] */ long Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_FreqA)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_FreqA )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_FreqA)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_FreqA )( 
            IRigX * This,
            /* [in] */ long Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_FreqB)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_FreqB )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_FreqB)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_FreqB )( 
            IRigX * This,
            /* [in] */ long Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_RitOffset)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_RitOffset )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_RitOffset)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_RitOffset )( 
            IRigX * This,
            /* [in] */ long Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Pitch)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Pitch )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_Pitch)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Pitch )( 
            IRigX * This,
            /* [in] */ long Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Vfo)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Vfo )( 
            IRigX * This,
            /* [retval][out] */ RigParamX *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_Vfo)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Vfo )( 
            IRigX * This,
            /* [in] */ RigParamX Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Split)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Split )( 
            IRigX * This,
            /* [retval][out] */ RigParamX *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_Split)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Split )( 
            IRigX * This,
            /* [in] */ RigParamX Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Rit)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Rit )( 
            IRigX * This,
            /* [retval][out] */ RigParamX *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_Rit)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Rit )( 
            IRigX * This,
            /* [in] */ RigParamX Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Xit)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Xit )( 
            IRigX * This,
            /* [retval][out] */ RigParamX *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_Xit)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Xit )( 
            IRigX * This,
            /* [in] */ RigParamX Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Tx)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Tx )( 
            IRigX * This,
            /* [retval][out] */ RigParamX *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_Tx)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Tx )( 
            IRigX * This,
            /* [in] */ RigParamX Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_Mode)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Mode )( 
            IRigX * This,
            /* [retval][out] */ RigParamX *Value);
        
        DECLSPEC_XFGVIRT(IRigX, put_Mode)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Mode )( 
            IRigX * This,
            /* [in] */ RigParamX Value);
        
        DECLSPEC_XFGVIRT(IRigX, ClearRit)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *ClearRit )( 
            IRigX * This);
        
        DECLSPEC_XFGVIRT(IRigX, SetSimplexMode)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *SetSimplexMode )( 
            IRigX * This,
            /* [in] */ long Freq);
        
        DECLSPEC_XFGVIRT(IRigX, SetSplitMode)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *SetSplitMode )( 
            IRigX * This,
            /* [in] */ long RxFreq,
            /* [in] */ long TxFreq);
        
        DECLSPEC_XFGVIRT(IRigX, FrequencyOfTone)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *FrequencyOfTone )( 
            IRigX * This,
            /* [in] */ long Tone,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, SendCustomCommand)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *SendCustomCommand )( 
            IRigX * This,
            /* [in] */ VARIANT Command,
            /* [in] */ long ReplyLength,
            /* [in] */ VARIANT ReplyEnd);
        
        DECLSPEC_XFGVIRT(IRigX, GetRxFrequency)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *GetRxFrequency )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, GetTxFrequency)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *GetTxFrequency )( 
            IRigX * This,
            /* [retval][out] */ long *Value);
        
        DECLSPEC_XFGVIRT(IRigX, get_PortBits)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_PortBits )( 
            IRigX * This,
            /* [retval][out] */ IPortBits **Value);
        
        END_INTERFACE
    } IRigXVtbl;

    interface IRigX
    {
        CONST_VTBL struct IRigXVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IRigX_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IRigX_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IRigX_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IRigX_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IRigX_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IRigX_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IRigX_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IRigX_get_RigType(This,Value)	\
    ( (This)->lpVtbl -> get_RigType(This,Value) ) 

#define IRigX_get_ReadableParams(This,Value)	\
    ( (This)->lpVtbl -> get_ReadableParams(This,Value) ) 

#define IRigX_get_WriteableParams(This,Value)	\
    ( (This)->lpVtbl -> get_WriteableParams(This,Value) ) 

#define IRigX_IsParamReadable(This,Param,Value)	\
    ( (This)->lpVtbl -> IsParamReadable(This,Param,Value) ) 

#define IRigX_IsParamWriteable(This,Param,Value)	\
    ( (This)->lpVtbl -> IsParamWriteable(This,Param,Value) ) 

#define IRigX_get_Status(This,Value)	\
    ( (This)->lpVtbl -> get_Status(This,Value) ) 

#define IRigX_get_StatusStr(This,Value)	\
    ( (This)->lpVtbl -> get_StatusStr(This,Value) ) 

#define IRigX_get_Freq(This,Value)	\
    ( (This)->lpVtbl -> get_Freq(This,Value) ) 

#define IRigX_put_Freq(This,Value)	\
    ( (This)->lpVtbl -> put_Freq(This,Value) ) 

#define IRigX_get_FreqA(This,Value)	\
    ( (This)->lpVtbl -> get_FreqA(This,Value) ) 

#define IRigX_put_FreqA(This,Value)	\
    ( (This)->lpVtbl -> put_FreqA(This,Value) ) 

#define IRigX_get_FreqB(This,Value)	\
    ( (This)->lpVtbl -> get_FreqB(This,Value) ) 

#define IRigX_put_FreqB(This,Value)	\
    ( (This)->lpVtbl -> put_FreqB(This,Value) ) 

#define IRigX_get_RitOffset(This,Value)	\
    ( (This)->lpVtbl -> get_RitOffset(This,Value) ) 

#define IRigX_put_RitOffset(This,Value)	\
    ( (This)->lpVtbl -> put_RitOffset(This,Value) ) 

#define IRigX_get_Pitch(This,Value)	\
    ( (This)->lpVtbl -> get_Pitch(This,Value) ) 

#define IRigX_put_Pitch(This,Value)	\
    ( (This)->lpVtbl -> put_Pitch(This,Value) ) 

#define IRigX_get_Vfo(This,Value)	\
    ( (This)->lpVtbl -> get_Vfo(This,Value) ) 

#define IRigX_put_Vfo(This,Value)	\
    ( (This)->lpVtbl -> put_Vfo(This,Value) ) 

#define IRigX_get_Split(This,Value)	\
    ( (This)->lpVtbl -> get_Split(This,Value) ) 

#define IRigX_put_Split(This,Value)	\
    ( (This)->lpVtbl -> put_Split(This,Value) ) 

#define IRigX_get_Rit(This,Value)	\
    ( (This)->lpVtbl -> get_Rit(This,Value) ) 

#define IRigX_put_Rit(This,Value)	\
    ( (This)->lpVtbl -> put_Rit(This,Value) ) 

#define IRigX_get_Xit(This,Value)	\
    ( (This)->lpVtbl -> get_Xit(This,Value) ) 

#define IRigX_put_Xit(This,Value)	\
    ( (This)->lpVtbl -> put_Xit(This,Value) ) 

#define IRigX_get_Tx(This,Value)	\
    ( (This)->lpVtbl -> get_Tx(This,Value) ) 

#define IRigX_put_Tx(This,Value)	\
    ( (This)->lpVtbl -> put_Tx(This,Value) ) 

#define IRigX_get_Mode(This,Value)	\
    ( (This)->lpVtbl -> get_Mode(This,Value) ) 

#define IRigX_put_Mode(This,Value)	\
    ( (This)->lpVtbl -> put_Mode(This,Value) ) 

#define IRigX_ClearRit(This)	\
    ( (This)->lpVtbl -> ClearRit(This) ) 

#define IRigX_SetSimplexMode(This,Freq)	\
    ( (This)->lpVtbl -> SetSimplexMode(This,Freq) ) 

#define IRigX_SetSplitMode(This,RxFreq,TxFreq)	\
    ( (This)->lpVtbl -> SetSplitMode(This,RxFreq,TxFreq) ) 

#define IRigX_FrequencyOfTone(This,Tone,Value)	\
    ( (This)->lpVtbl -> FrequencyOfTone(This,Tone,Value) ) 

#define IRigX_SendCustomCommand(This,Command,ReplyLength,ReplyEnd)	\
    ( (This)->lpVtbl -> SendCustomCommand(This,Command,ReplyLength,ReplyEnd) ) 

#define IRigX_GetRxFrequency(This,Value)	\
    ( (This)->lpVtbl -> GetRxFrequency(This,Value) ) 

#define IRigX_GetTxFrequency(This,Value)	\
    ( (This)->lpVtbl -> GetTxFrequency(This,Value) ) 

#define IRigX_get_PortBits(This,Value)	\
    ( (This)->lpVtbl -> get_PortBits(This,Value) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IRigX_INTERFACE_DEFINED__ */


#ifndef __IPortBits_INTERFACE_DEFINED__
#define __IPortBits_INTERFACE_DEFINED__

/* interface IPortBits */
/* [object][oleautomation][dual][version][uuid] */ 


EXTERN_C const IID IID_IPortBits;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3DEE2CC8-1EA3-46E7-B8B4-3E7321F2446A")
    IPortBits : public IDispatch
    {
    public:
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE Lock( 
            /* [retval][out] */ VARIANT_BOOL *Ok) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Rts( 
            /* [retval][out] */ VARIANT_BOOL *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Rts( 
            /* [in] */ VARIANT_BOOL Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Dtr( 
            /* [retval][out] */ VARIANT_BOOL *Value) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Dtr( 
            /* [in] */ VARIANT_BOOL Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Cts( 
            /* [retval][out] */ VARIANT_BOOL *Value) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Dsr( 
            /* [retval][out] */ VARIANT_BOOL *Value) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE Unlock( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPortBitsVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPortBits * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPortBits * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPortBits * This);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfoCount)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IPortBits * This,
            /* [out] */ UINT *pctinfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetTypeInfo)
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IPortBits * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        DECLSPEC_XFGVIRT(IDispatch, GetIDsOfNames)
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IPortBits * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        DECLSPEC_XFGVIRT(IDispatch, Invoke)
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IPortBits * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        DECLSPEC_XFGVIRT(IPortBits, Lock)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *Lock )( 
            IPortBits * This,
            /* [retval][out] */ VARIANT_BOOL *Ok);
        
        DECLSPEC_XFGVIRT(IPortBits, get_Rts)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Rts )( 
            IPortBits * This,
            /* [retval][out] */ VARIANT_BOOL *Value);
        
        DECLSPEC_XFGVIRT(IPortBits, put_Rts)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Rts )( 
            IPortBits * This,
            /* [in] */ VARIANT_BOOL Value);
        
        DECLSPEC_XFGVIRT(IPortBits, get_Dtr)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Dtr )( 
            IPortBits * This,
            /* [retval][out] */ VARIANT_BOOL *Value);
        
        DECLSPEC_XFGVIRT(IPortBits, put_Dtr)
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Dtr )( 
            IPortBits * This,
            /* [in] */ VARIANT_BOOL Value);
        
        DECLSPEC_XFGVIRT(IPortBits, get_Cts)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Cts )( 
            IPortBits * This,
            /* [retval][out] */ VARIANT_BOOL *Value);
        
        DECLSPEC_XFGVIRT(IPortBits, get_Dsr)
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Dsr )( 
            IPortBits * This,
            /* [retval][out] */ VARIANT_BOOL *Value);
        
        DECLSPEC_XFGVIRT(IPortBits, Unlock)
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *Unlock )( 
            IPortBits * This);
        
        END_INTERFACE
    } IPortBitsVtbl;

    interface IPortBits
    {
        CONST_VTBL struct IPortBitsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPortBits_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPortBits_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPortBits_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPortBits_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IPortBits_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IPortBits_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IPortBits_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IPortBits_Lock(This,Ok)	\
    ( (This)->lpVtbl -> Lock(This,Ok) ) 

#define IPortBits_get_Rts(This,Value)	\
    ( (This)->lpVtbl -> get_Rts(This,Value) ) 

#define IPortBits_put_Rts(This,Value)	\
    ( (This)->lpVtbl -> put_Rts(This,Value) ) 

#define IPortBits_get_Dtr(This,Value)	\
    ( (This)->lpVtbl -> get_Dtr(This,Value) ) 

#define IPortBits_put_Dtr(This,Value)	\
    ( (This)->lpVtbl -> put_Dtr(This,Value) ) 

#define IPortBits_get_Cts(This,Value)	\
    ( (This)->lpVtbl -> get_Cts(This,Value) ) 

#define IPortBits_get_Dsr(This,Value)	\
    ( (This)->lpVtbl -> get_Dsr(This,Value) ) 

#define IPortBits_Unlock(This)	\
    ( (This)->lpVtbl -> Unlock(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPortBits_INTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_OmniRigX;

#ifdef __cplusplus

class DECLSPEC_UUID("0839E8C6-ED30-4950-8087-966F970F0CAE")
OmniRigX;
#endif

EXTERN_C const CLSID CLSID_RigX;

#ifdef __cplusplus

class DECLSPEC_UUID("78AECFA2-3F52-4E39-98D3-1646C00A6234")
RigX;
#endif

EXTERN_C const CLSID CLSID_PortBits;

#ifdef __cplusplus

class DECLSPEC_UUID("B786DE29-3B3D-4C66-B7C4-547F9A77A21D")
PortBits;
#endif
#endif /* __OmniRig_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


