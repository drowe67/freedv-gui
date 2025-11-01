

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


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


#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        EXTERN_C __declspec(selectany) const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif // !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, LIBID_OmniRig,0x4FE359C5,0xA58F,0x459D,0xBE,0x95,0xCA,0x55,0x9F,0xB4,0xF2,0x70);


MIDL_DEFINE_GUID(IID, IID_IOmniRigX,0x501A2858,0x3331,0x467A,0x83,0x7A,0x98,0x9F,0xDE,0xDA,0xCC,0x7D);


MIDL_DEFINE_GUID(IID, DIID_IOmniRigXEvents,0x2219175F,0xE561,0x47E7,0xAD,0x17,0x73,0xC4,0xD8,0x89,0x1A,0xA1);


MIDL_DEFINE_GUID(IID, IID_IRigX,0xD30A7E51,0x5862,0x45B7,0xBF,0xFA,0x64,0x15,0x91,0x7D,0xA0,0xCF);


MIDL_DEFINE_GUID(IID, IID_IPortBits,0x3DEE2CC8,0x1EA3,0x46E7,0xB8,0xB4,0x3E,0x73,0x21,0xF2,0x44,0x6A);


MIDL_DEFINE_GUID(CLSID, CLSID_OmniRigX,0x0839E8C6,0xED30,0x4950,0x80,0x87,0x96,0x6F,0x97,0x0F,0x0C,0xAE);


MIDL_DEFINE_GUID(CLSID, CLSID_RigX,0x78AECFA2,0x3F52,0x4E39,0x98,0xD3,0x16,0x46,0xC0,0x0A,0x62,0x34);


MIDL_DEFINE_GUID(CLSID, CLSID_PortBits,0xB786DE29,0x3B3D,0x4C66,0xB7,0xC4,0x54,0x7F,0x9A,0x77,0xA2,0x1D);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



