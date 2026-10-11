// Forwards "asyncproxysocketlayer.h" to upstream's AsyncProxySocketLayer.h.
//
// Upstream includes this header under a spelling that differs from the file
// name only in case, which resolves on Windows' case-insensitive file systems
// and nowhere else -- including a Linux host cross-compiling for Windows.
// Checked by port/tools/check_include_case.py; see port/casefold/README.md.
#include "../../3rd-party/winscp/source/filezilla/AsyncProxySocketLayer.h"
