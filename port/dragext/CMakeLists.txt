# WinSCP's drag & drop shell extension (P3-24): its copy hook tells the
# program where in Explorer remote files were dropped (the fake-file drag).
# Built from the pristine source (DragExtBuild.cpp), with the C++ runtime in
# it: Explorer loads it, with none of the program's DLLs beside it.
add_library(DragExt64 SHARED DragExtBuild.cpp)
target_compile_definitions(DragExt64 PRIVATE UNICODE _UNICODE)
target_compile_options(DragExt64 PRIVATE -Wno-microsoft-exception-spec)
target_link_options(DragExt64 PRIVATE -static)
target_link_libraries(DragExt64 PRIVATE ole32 shell32 uuid advapi32 version)
set_target_properties(DragExt64 PROPERTIES PREFIX "" OUTPUT_NAME DragExt64)
