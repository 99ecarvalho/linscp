# WinSCP's PuTTY against stock PuTTY 0.85

P1-10's reconciliation: every file in `3rd-party/winscp/source/putty` that
differs from stock PuTTY 0.85 (`3rd-party/putty`), what each change is for,
and what it means for absorbing a new PuTTY release and for the Linux build.
Reviewed 2026-10-12 against WinSCP `aaae9dcb` and PuTTY 0.85; re-reviewed
2026-10-15 at WinSCP 6.7.1 (`e19a9a3e`): the only changes are four of the
bugs below, fixed upstream from linscp's reports (#72, #74, #76, #77).

ctest `putty_deltas` holds this review in place: `port/putty/deltas.txt` lists
each delta's line counts and a hash of its diff, and the test fails when
either tree changes any of them. A WinSCP bump or a PuTTY release that does
is reviewed here first, then the list is regenerated
(`port/tools/putty_deltas.py --write`).

## Shape

Of WinSCP's 226 PuTTY files, 154 are identical to stock, 65 are modified
(1,659 lines added, 281 removed) and 7 are WinSCP's own. The deltas
fall into a few themes:

- **Per-session callback sets.** Stock PuTTY runs one session per process:
  its callback queue, packet free queue, handle-ready list and handle-wait
  tree are globals. WinSCP runs several sessions on several threads, so all
  four move into a `struct callback_set` reached from the seat, log context
  or plug, and every API that queued to a global takes it
  (`callback.c`, `ssh/common_p.c`, `windows/handle-*.c`, `putty.h`,
  `proxy/*.c`, `ssh/*.c`).
- **One lock for what stays global.** `putty_section`, a recursive critical
  section (`WINSCP_PUTTY_SECTION_ENTER/LEAVE`), guards the socket tree, the
  RNG and mpint's shared Montgomery scratch.
- **Hooks for core.** `puttyexp.h` exports accessors (ciphers, compressors,
  SSH version, host-key and MAC lists, port-forward seat) and
  `winscp_query` (`WINSCP_QUERY_TIMER/REMMAXPKT/MAIN_CHANNEL`). Seat methods
  gain parameters (host-key fingerprints, weak-crypto details);
  `sk_new` gains connect timeout, send buffer and source address; new
  `Conf` keys carry them (`conf.h`).
- **Host-key policy moves to core.** PuTTY's own cache check is forced to
  "not cached" (`ssh/common_p.c:953`); `check_stored_host_key` becomes
  `retrieve_host_key`, and core's `TSecureShell` decides.
- **Features removed.** SSH-1, X11 forwarding, PuTTY's log files, session
  lists and jump lists, host CA storage, pre-connect commands, line
  discipline, terminal unicode tables, control-character stripping
  (`stripctrl`), the proxy antispoof marker.
- **Toolchain.** C++Builder and Visual Studio accommodation: C++ keywords
  renamed, pointer comparisons fixed, `HAVE_*` hard-coded in `defs.h`
  instead of CMake's `cmake.h`, `WINSCP_VS` guards for a reduced `ssh.h`,
  objconv-friendly constants in AES.
- **Fixes over stock.** `proxy/proxy.c` (uninitialised `pn`/`proxy_addr`
  freed on early returns), `windows/handle-socket.c` (stderr handle leak),
  `windows/gss.c` (`DeleteSecurityContext` on a null context),
  `ssh/userauth2-client.c` (banner loop on disconnect). Check each against a
  new PuTTY release before rebasing: upstream PuTTY may fix them its own way.

## Files

`+a -r` are diff lines against the stock file (for an added file, its size,
and in brackets its delta against the stock file it was copied from).
Categories: **hook** (integration core needs), **off** (stock code compiled
out), **build** (toolchain, no behaviour change), **fix**, **beh** (other
behaviour change).

| File | +a -r | What |
|---|---|---|
| `callback.c` | +42 -6 | hook: per-session callback set; `is_idempotent_callback_pending`. off: frontend notifications |
| `conf.h` | +26 -0 | hook: `connect_timeout`, `sndbuf`, `srcaddr`, `force_remote_cmd2`, `change_password` |
| `crypto/aes-ni.c` | +41 -4 | build: unit gated on `WINSCP_VS \|\| _WIN64`, `MM_SETR_EPI8` constants, casts. The pragma pop mismatch was winscp/winscp#72, fixed in 6.7.1; `MM_SETR_EPI8`'s `_WIN64`-only intrinsic is still linscp's `putty-aes-ni-non-win64-clang.patch` |
| `crypto/aes.h` | +17 -7 | build: vtable names as named arrays (objconv alignment) |
| `crypto/aesold.c` | added, 1,057 | hook: pre-0.71 table AES, encrypt only, for core's master password (`aesold_*`) |
| `crypto/argon2.c` | +1 -1 | beh: automatic Argon2 passes target 3x the time |
| `crypto/ecc-ssh.c` | +71 -0 | hook: `ec_cleanup` frees the static curves (not Ed448) |
| `crypto/mpint.c`, `mpint_i.h` | +16 -7, +5 -3 | hook: shared scratch guarded by `putty_section`. build: 32-bit limbs for C++Builder, `__STDC__` |
| `crypto/openssh-certs.c` | +5 -0 | off: certificate info text |
| `crypto/sha1.h`, `sha256.h` | +2 -1 each | beh: bare hash names in logs |
| `defs.h` | +24 -0 | build: hard-coded `HAVE_*`, SSH only. off: PuTTY's `#error` on `NDEBUG` |
| `logging.c` | +58 -4 | off: log files; `logwrite`/`log_packet` assert unless `WINSCP_LOG_PACKETS`. hook: `log_get_logpolicy` |
| `misc.h` | +8 -1 | off: `stripctrl_string`. build: `unreachable` under VS |
| `network.h` | +9 -1 | hook: `sk_new` timeout/sndbuf/srcaddr |
| `proxy/http_p.c` | added (identical to `http.c`) | build: renamed for the C++Builder project |
| `proxy/interactor.c` | +2 -1 | off: antispoof marker |
| `proxy/local.c`, `telnet.c` | +3 -3, +2 -2 | hook: callback sets |
| `proxy/newmain.c` | +12 -2 | off: pre-connect command |
| `proxy/proxy.c` | +41 -3 | fix: uninitialised `pn`, `proxy_addr`; `proxy_endpoint_info` before the sub-socket exists. hook: `sk_new` parameters, `get_proxy_plug_socket` |
| `proxy/proxy.h` | +6 -0 | build: hidden from C++. hook: `get_proxy_plug_socket` |
| `putty.h` | +90 -15 | hook: callback sets, seat signatures, `putty_section`, `WINSCP_BOM`, `assert` to `DoAssertC` in `_DEBUG`. beh: `nonfatal` is `NORETURN` (core throws) |
| `puttyexp.h` | added, 75 | hook: everything core imports from PuTTY |
| `puttymem.h` | +1 -1 | build: `private` renamed |
| `settings.c` | +3 -1 | off: session list |
| `sshpubk.c` | +34 -2 | hook: in-memory keys (`@<hex>`), `openssh_loadpub`. off: SSH-1 keys |
| `sshrand.c` | +25 -0 | hook: `putty_section` around the RNG. off: seed saving (core saves) |
| `ssh.h` | +53 -5 | hook: seat in packet queues, `agentf_new` callback set, writable `sshver`. build: `WINSCP_VS` guards, SHA symbols renamed `putty_*` |
| `ssh/bpp.h`, `bpp2.c` | +7 -0, +27 -4 | hook: cipher/compressor accessors. beh: cipher id in the "Initialised" event |
| `ssh/common_p.c` | added (from `common.c`: +49 -19) | hook: callback sets, seat in queues, `ppl_printf` marked. off: host-key cache |
| `ssh/connection2.c`, `.h` | +74 -1, +2 -1 | hook: `winscp_query`, `ready`. off: X11 forwarding |
| `ssh/connection2-client.c` | +7 -1 | off: X11 channel open (asserts, then refuses). hook: agent forwarding callback set |
| `ssh/agentf.c` | +4 -2 | hook: callback set |
| `ssh/gss.h` | +1 -1 | hook: `ssh_gss_setup` takes a log context |
| `ssh/kex2-client.c` | +11 -1 | hook: an accepted host certificate is reported to the seat. beh: certificate ID logged without `stripctrl`. build: no `mpunsafe` clash symbol |
| `ssh/mainchan.c` | +10 -2 | off: X11 request, specials. beh: `force_remote_cmd2`, empty `remote_cmd2` starts a shell |
| `ssh/portfwd.c` | +34 -0 | hook: `is_pfwd`, `get_pfwd_seat` |
| `ssh/ppl.h` | +6 -1 | hook: `winscp_query` slot; userauth takes loghost, change-password, seat |
| `ssh/sharing.c` | +6 -0 | off: X11 in connection sharing |
| `ssh/ssh.c` | +93 -30 | off: SSH-1, line discipline. hook: accessors, `winscp_query`, callback sets. beh: no "PuTTY" in the version string, no PPL backlog in `ssh_sendbuffer` |
| `ssh/transport2.c` | +78 -9 | hook: `winscp_query` (rekey timer driven by core), host-key/MAC lists. off: server side. beh: rekey on `now >= next_rekey` |
| `ssh/userauth2-client.c` | +68 -26 | hook: loghost, change-password mode, `WINSCP_BOM`. beh: "Agent" for "Pageant", server's methods logged, "SSH server:" prompt prefix. fix: banner loop |
| `storage.h` | +5 -0 | hook: `retrieve_host_key` |
| `stubs/null-seat.c`, `utils/tempseat.c` | +4 -2 each | hook: seat signatures |
| `utils/bufchain.c` | +4 -1 | beh: 16 KiB granule |
| `utils/conf_data.c`, `dupprintf.c` | +3 -0, +1 -1 | build |
| `utils/spr_get_err_message.c` | added (identical to `spr_get_error_message.c`) | build: shortened name |
| `utils/stripctrl.c` | +10 -0 | off: all sanitisation; only the plumbing stays |
| `utils/version.c` | +13 -0 | off: version globals. hook: `get_putty_version` |
| `version.h` | +5 -13 | build: release header |
| `windows/agent-client.c` | +30 -6 | hook: OpenSSH agent pipe, callback set. *Replaced on Linux* (`$SSH_AUTH_SOCK`) |
| `windows/gss.c` | +32 -1 | hook: log context, registry pass, `wingss_cleanup`. fix: null context. *Replaced on Linux* |
| `windows/handle-io.c`, `handle-socket.c`, `handle-wait.c` | +78 -52, +45 -8, +17 -15 | hook: callback sets, `do_select` on connect/close. fix: stderr handle leak |
| `windows/local-proxy.c` | +6 -0 | off: subprocess waiter |
| `windows/named-pipe-client.c` | +31 -4 | beh: `allow_system` (SYSTEM/Administrators-owned pipes). *Dropped on Linux* |
| `windows/network.c` | +167 -9 | hook: `sk_new` parameters, `putty_section` on the socket tree, dynamic `getsockopt`. beh: blocking connect with `SO_RCVTIMEO`, 4 MB receive buffer with `sndbuf`, IPv4 source address. off: reselect |
| `windows/platform.h` | +29 -13 | hook: `do_select(Plug*)`, callback sets, registry redirection to core's storage. off: help, GUI seat functions |
| `windows/storage.c` | +33 -2 | hook: `retrieve_host_key`. off: sessions, host CAs, jump lists |
| `windows/unicode.c` | +17 -0 | off: terminal tables, code page lists |
| `windows/utils/filename.c` | +42 -1 | hook: in-memory keys |
| `windows/utils/get_system_dir.c` | +23 -0 | hook: `win_misc_cleanup` |
| `windows/utils/registry.c` | +31 -1 | hook: redirection to core's storage. off: multi-string, delete |
| `windows/utils/security_p.c` | added (from `security.c`: +14 -0) | hook: `win_secur_cleanup`. *Replaced on Linux* |
| `windows/utils/win_strerr.c` | added (identical to `win_strerror.c`) | build: shortened name |

## Findings

Checked against the build, not only read from the diff.

**For the Linux build -- verified, nothing to change:**

- `NDEBUG`: WinSCP removes PuTTY's `#error` against it, and both upstream's
  Release and linscp's default build define it. So PuTTY's `assert`s are
  off, as in WinSCP's releases -- including `connection2-client.c`'s
  `assert(false)` on a server's X11 channel open, which therefore refuses
  cleanly rather than aborting.
- Core throws C++ exceptions (`nonfatal`, `modalfatalbox`) through PuTTY's
  C frames. The C units carry unwind tables (clang's default on x86-64
  Linux; `.eh_frame` present) and Win64 always has them.
- `putty_section` must be recursive (`random_get_savedata` re-enters it):
  the emulation's critical sections are `PTHREAD_MUTEX_RECURSIVE`.
- AES-NI is built on Linux: the build defines `_WIN64`, so `aes-ni.c`'s
  gate passes, and `aes_ni_guard` checks its CPU test.
- `retrieve_host_key`, `in_memory_key_data`, the agent's
  `auth_agent_implementation` and `ssh_gss_setup(conf, logctx)` are all
  provided: the first two by WinSCP's own units compiled over the
  emulation, the rest by `port/putty/linux`.
- `stripctrl` is gone, so whatever displays server text must sanitise it.
  `linscp-cli` prints through core, as WinSCP does; the Qt shell (P2-03) must
  not echo server strings to a terminal raw.

**Upstream bugs** (in WinSCP's deltas, both platforms):

- `sshrand.c:40` -- `random_add_noise` returns with `putty_section` held
  when the RNG is inactive; every other thread then deadlocks on it.
  winscp/winscp#74, fixed in 6.7.1 (no longer carried).
- `windows/storage.c:351` -- `retrieve_host_key` compares the converted
  old-style key with its *output* buffer, uninitialised, so old-format RSA
  host keys are never migrated. winscp/winscp#75.
- `windows/network.c:1118` -- the connect timeout is set with
  `SO_RCVTIMEO`, which bounds no `connect()` on Windows or Linux:
  `-timeout=5` to an unreachable host gives up after the system's own
  limit -- 24 s for WinSCP 6.5.7 on Windows 11, 136 s for linscp on Linux,
  whose kernel retries SYN longer. A known divergence until fixed upstream
  (a non-blocking connect bounded by the timeout). Not reported yet: the fix
  changes how `sk_new` connects, so it goes with a tested patch.
- `transport2.c:2102` -- the rekey check `now >= next_rekey` is not
  wrap-safe: one early rekey per 49.7-day tick wrap, on both platforms
  (`schedule_timer` adds in 32 bits; harmless).
- `windows/utils/get_system_dir.c:32` -- `win_misc_cleanup` frees the
  cached directory without clearing the pointer (only at shutdown).
  winscp/winscp#76, fixed in 6.7.1.
- `crypto/ecc-ssh.c` -- `ec_cleanup` misses Ed448 (a leak at shutdown).
  winscp/winscp#77, fixed in 6.7.1.

**Security-relevant, by design:**

- The certificate key ID, chosen by the server, is logged without
  `stripctrl` (`kex2-client.c:927`); `logevent` folds CR/LF, other controls
  pass. Matters only if an event log is shown on a terminal raw.
- Pipe-owner checks accept SYSTEM/Administrators for the OpenSSH agent
  (Windows only; the unit is dropped on Linux).

## Absorbing a PuTTY release

1. Bump `3rd-party/putty` to the release; diff it against the previous one.
2. For each changed stock file, apply the change to WinSCP's copy -- or wait
   for WinSCP's own update -- respecting the deltas above: the four fixes
   over stock may now be fixed in PuTTY its own way; renamed files
   (`http_p.c`, `common_p.c`, `security_p.c`, `win_strerr.c`,
   `spr_get_err_message.c`) take their stock file's changes.
3. `ctest -R putty_deltas` names every delta that moved; review them here
   and regenerate `port/putty/deltas.txt`.
