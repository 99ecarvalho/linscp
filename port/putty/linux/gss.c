/*
 * GSSAPI library loading for WinSCP's PuTTY on Linux, in place of its
 * windows/gss.c.
 *
 * Core addresses three library slots (SecureShell.cpp, and it asserts there
 * are exactly three): 0 the Kerberos GSSAPI library, 1 SSPI, 2 a user-given
 * library. The slots and their settings keywords are kept, so stored
 * sessions mean the same thing. Slot 0 is the system's GSSAPI library -- MIT
 * Kerberos or Heimdal, whichever is installed -- loaded at run time as
 * PuTTY's unix/gss.c does; slot 1, SSPI, is Windows-only and never present;
 * slot 2 is dlopen()ed from the configured path, with WinSCP's log message
 * when that fails.
 */
#include "putty.h"
#ifndef NO_GSSAPI

#include "ssh/pgssapi.h"
#include "ssh/gss.h"
#include "ssh/gssc.h"

#include <dlfcn.h>

const int ngsslibs = 3;
const char *const gsslibnames[3] = {
    "System GSSAPI library (MIT Kerberos or Heimdal)",
    "Microsoft SSPI (Windows only)",
    "User-specified GSSAPI library",
};
const struct keyvalwhere gsslibkeywords[] = {
    { "gssapi32", 0, -1, -1 },
    { "sspi", 1, -1, -1 },
    { "custom", 2, -1, -1 },
};

static void gss_init(struct ssh_gss_library *lib, void *dlhandle, int id, const char *msg)
{
    lib->id = id;
    lib->gsslogmsg = msg;
    lib->handle = dlhandle;

#define BIND_GSS_FN(name) \
    lib->u.gssapi.name = (t_gss_##name) dlsym(dlhandle, "gss_" #name)

    BIND_GSS_FN(delete_sec_context);
    BIND_GSS_FN(display_status);
    BIND_GSS_FN(get_mic);
    BIND_GSS_FN(verify_mic);
    BIND_GSS_FN(import_name);
    BIND_GSS_FN(init_sec_context);
    BIND_GSS_FN(release_buffer);
    BIND_GSS_FN(release_cred);
    BIND_GSS_FN(release_name);
    BIND_GSS_FN(acquire_cred);
    BIND_GSS_FN(inquire_cred_by_mech);

#undef BIND_GSS_FN

    ssh_gssapi_bind_fns(lib);
}

struct ssh_gss_liblist *ssh_gss_setup(Conf *conf, LogContext *logctx)
{
    static const char *const system_libs[] = {
        "libgssapi_krb5.so.2",  /* MIT Kerberos */
        "libgssapi.so.3",       /* Heimdal */
        "libgssapi.so.2",
    };
    struct ssh_gss_liblist *list = snew(struct ssh_gss_liblist);
    void *gsslib;
    size_t i;

    list->libraries = snewn(2, struct ssh_gss_library);
    list->nlibraries = 0;

    for (i = 0; i < lenof(system_libs); i++) {
        if ((gsslib = dlopen(system_libs[i], RTLD_LAZY)) != NULL) {
            gss_init(&list->libraries[list->nlibraries++], gsslib, 0,
                     dupprintf("Using GSSAPI from %s", system_libs[i]));
            break;
        }
    }

    Filename *customlib = conf_get_filename(conf, CONF_ssh_gss_custom);
    if (customlib->utf8path && *customlib->utf8path) {
        if ((gsslib = dlopen(customlib->utf8path, RTLD_LAZY)) != NULL) {
            gss_init(&list->libraries[list->nlibraries++], gsslib, 2,
                     dupprintf("Using GSSAPI from user-specified library '%s'",
                               customlib->utf8path));
        } else if (logctx) {
            char *buf = dupprintf(
                WINSCP_BOM "Cannot load GSSAPI from user-specified library '%s': %s",
                customlib->utf8path, dlerror());
            logevent(logctx, buf);
            sfree(buf);
        }
    }

    return list;
}

void ssh_gss_cleanup(struct ssh_gss_liblist *list)
{
    int i;

    /* dlclose counts references, so closing every library here is safe
     * even with other sessions still using the same one. */
    for (i = 0; i < list->nlibraries; i++) {
        dlclose(list->libraries[i].handle);
        sfree((char *)list->libraries[i].gsslogmsg);
    }
    sfree(list->libraries);
    sfree(list);
}

#endif /* NO_GSSAPI */

/* Windows releases its never-unloaded MIT Kerberos DLLs here at shutdown;
 * dlopen's reference counting leaves nothing to do. */
void wingss_cleanup(void)
{
}
