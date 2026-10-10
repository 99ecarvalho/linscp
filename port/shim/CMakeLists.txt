# VCL runtime compatibility layer.
#
# Header-only for now. As types land (UnicodeString, TStrings, ...) this becomes
# a static library; keeping it an INTERFACE target until then means consumers do
# not need to change when it does.
add_library(linscp-shim INTERFACE)
target_include_directories(linscp-shim INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(linscp-shim INTERFACE cxx_std_17)
target_compile_options(linscp-shim INTERFACE -fms-extensions)
