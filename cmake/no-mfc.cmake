# Guard: upstream's MFC subset (3rd-party/winscp/libs/mfc) is not free
# software and is never built, generated or included. FileZilla's MFC API
# comes from linscp's own port/mfc-compat (ADR 0017).
#
# Run at the end of configuration: fails it if any target names a source, an
# include directory or a link directory under a libs/mfc path, or if the
# transform would generate anything there. Only paths are examined, never
# file contents. The no_mfc test checks the configured tree the same way.

function(_linscp_no_mfc_targets dir out)
  get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(_sub IN LISTS _subdirs)
    _linscp_no_mfc_targets("${_sub}" _more)
    list(APPEND _targets ${_more})
  endforeach()
  set(${out} ${_targets} PARENT_SCOPE)
endfunction()

function(linscp_check_no_mfc)
  set(_bad)
  _linscp_no_mfc_targets("${CMAKE_SOURCE_DIR}" _targets)
  foreach(_t IN LISTS _targets)
    get_target_property(_type ${_t} TYPE)
    set(_props INTERFACE_INCLUDE_DIRECTORIES INTERFACE_SOURCES INTERFACE_LINK_DIRECTORIES)
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      list(APPEND _props SOURCES INCLUDE_DIRECTORIES LINK_DIRECTORIES)
    endif()
    foreach(_p IN LISTS _props)
      get_target_property(_v ${_t} ${_p})
      if(_v AND "${_v}" MATCHES "(^|[/\\\\])libs[/\\\\]mfc([/\\\\;>]|$)")
        list(APPEND _bad "${_t}.${_p}")
      endif()
    endforeach()
  endforeach()
  foreach(_f IN LISTS ARGN)
    if("${_f}" MATCHES "(^|[/\\\\])libs[/\\\\]mfc([/\\\\]|$)")
      list(APPEND _bad "generated: ${_f}")
      break()
    endif()
  endforeach()
  if(_bad)
    list(JOIN _bad "\n  " _list)
    message(FATAL_ERROR
      "Upstream's MFC subset (libs/mfc) must not be built or generated; "
      "FileZilla uses port/mfc-compat (ADR 0017). Found:\n  ${_list}")
  endif()
  message(STATUS "linscp: no target or generated file uses libs/mfc")
endfunction()
