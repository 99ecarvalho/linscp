/*
 * WinSCP's PuTTY on Linux, in place of its windows/utils/security_p.c.
 *
 * That unit builds the ACLs for Pageant's named pipe and caches the user's
 * SID for them; WinSCP adds win_secur_cleanup (MPEXT) to free the cache,
 * which core calls when PuTTY is finalised. There is no pipe here, nor a SID
 * to cache, so only the hook remains.
 */
#include "putty.h"
#include "puttyexp.h"

void win_secur_cleanup(void)
{
}
