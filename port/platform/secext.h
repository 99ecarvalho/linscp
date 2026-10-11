// Stand-in for Win32's <secext.h> (security name formats).
#ifndef LINSCP_SECEXT_H
#define LINSCP_SECEXT_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"

typedef enum EXTENDED_NAME_FORMAT
{
  NameUnknown = 0,
  NameFullyQualifiedDN = 1,
  NameSamCompatible = 2,
  NameDisplay = 3,
  NameUniqueId = 6,
  NameCanonical = 7,
  NameUserPrincipal = 8
} EXTENDED_NAME_FORMAT;
typedef uint8_t BOOLEAN;

#ifdef __cplusplus
extern "C" {
#endif
// NameSamCompatible is DOMAIN\user; on a machine outside a domain Windows
// gives the computer name as the domain, so this gives host\user. Size is
// in characters, terminator included, and on success becomes the length.
// Other formats fail with ERROR_NONE_MAPPED.
BOOLEAN GetUserNameEx(EXTENDED_NAME_FORMAT Format, LPWSTR Name, ULONG * Size);
// The narrow form, NameSamCompatible only, as the wide one.
BOOLEAN GetUserNameExA(EXTENDED_NAME_FORMAT Format, LPSTR Name, ULONG * Size);
#ifdef __cplusplus
}
// PULONG is unsigned long on Windows (32 bits), which core passes; 64 bits
// on LP64. See GetFileSize in win32_on_posix.h.
inline BOOLEAN GetUserNameEx(EXTENDED_NAME_FORMAT Format, LPWSTR Name, unsigned long * Size)
{
  ULONG S = (Size != nullptr) ? static_cast<ULONG>(*Size) : 0;
  const BOOLEAN R = GetUserNameEx(Format, Name, (Size != nullptr) ? &S : nullptr);
  if (Size != nullptr)
  {
    *Size = S;
  }
  return R;
}
#endif
#endif
