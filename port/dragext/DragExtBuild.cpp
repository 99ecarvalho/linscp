// WinSCP's drag & drop shell extension (source/dragext/DragExt.cpp, as it
// is), built with llvm-mingw (P3-24). C++Builder's headers lack the
// IsEqualGUID DragExt.cpp defines; mingw's guiddef.h has it, inline, among
// the GUID operators it then leaves out. Borland's __FUNC__ is __func__.
#define _SYS_GUID_OPERATORS_
#include <guiddef.h>
// (Declared ahead, for the headers using it before DragExt.cpp defines it.)
int __stdcall IsEqualGUID(REFGUID rguid1, REFGUID rguid2);
#define IsEqualIID(riid1, riid2) IsEqualGUID(riid1, riid2)
#define IsEqualCLSID(rclsid1, rclsid2) IsEqualGUID(rclsid1, rclsid2)
#define __FUNC__ __func__
#include "../../3rd-party/winscp/source/dragext/DragExt.cpp"
