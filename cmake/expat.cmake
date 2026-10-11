# Build expat from the pinned submodule rather than linking a system copy.
#
# The system has libexpat, but the mingw sysroot does not, and upstream vendors
# expat precisely so WinSCP can build it. Compiling the vendored source keeps
# both targets on the same version and adds no external dependency.
set(LINSCP_EXPAT_DIR "${LINSCP_UPSTREAM}/libs/expat")

if(NOT EXISTS "${LINSCP_EXPAT_DIR}/lib/xmlparse.c")
  message(FATAL_ERROR "vendored expat missing at ${LINSCP_EXPAT_DIR}")
endif()

add_library(linscp-expat STATIC
  "${LINSCP_EXPAT_DIR}/lib/xmlparse.c"
  "${LINSCP_EXPAT_DIR}/lib/xmlrole.c"
  "${LINSCP_EXPAT_DIR}/lib/xmltok.c"
)

# On Windows expat sources entropy through RtlGenRandom, which lives here.
if(WIN32)
  target_sources(linscp-expat PRIVATE "${LINSCP_EXPAT_DIR}/lib/loadlibrary.c")
endif()
target_include_directories(linscp-expat PUBLIC
  "${LINSCP_EXPAT_DIR}" "${LINSCP_EXPAT_DIR}/lib")
target_compile_definitions(linscp-expat PUBLIC
  XML_STATIC HAVE_EXPAT_CONFIG_H)
# Upstream's own source, not ours: do not fail the build on its warnings.
target_compile_options(linscp-expat PRIVATE -w)
