#.rst:
# FindLibdlmclient
# -------
# FindLibdlmclient.cmake is created based on FindLibinput.cmake
#
# Try to find libdlmclient on a Unix system.
#
# This will define the following variables:
#
# ``Libdlmclient_FOUND``
#     True if (the requested version of) libdlmclient is available
# ``Libdlmclient_VERSION``
#     The version of libdlmclient
# ``Libdlmclient_LIBRARIES``
#     This can be passed to target_link_libraries() instead of the ``Libdlmclient::Libdlmclient``
#     target
# ``Libdlmclient_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``Libdlmclient_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``Libdlmclient_FOUND`` is TRUE, it will also define the following imported target:
#
# ``Libdlmclient::Libdlmclient``
#     The libdlmclient library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.

#=============================================================================
# Copyright 2014 Alex Merry <alex.merry@kde.org>
# Copyright 2014 Martin Gräßlin <mgraesslin@kde.org>
# Copyright 2022 Naoto Yamaguchi <naoto.yamaguchi@aisin.co.jp>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by FindLibdlmclient.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use FindLibdlmclient.cmake")
endif()

if(NOT WIN32)
    # Use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig QUIET)
    pkg_check_modules(PKG_Libdlmclient QUIET libdlmclient)

    set(Libdlmclient_DEFINITIONS ${PKG_Libdlmclient_CFLAGS_OTHER})
    set(Libdlmclient_VERSION ${PKG_Libdlmclient_VERSION})

    find_path(Libdlmclient_INCLUDE_DIR
        NAMES
            dlmclient.h
        HINTS
            ${PKG_Libdlmclient_INCLUDE_DIRS}
    )
    find_library(Libdlmclient_LIBRARY
        NAMES
            dlmclient
        HINTS
            ${PKG_Libdlmclient_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Libdlmclient
        FOUND_VAR
            Libdlmclient_FOUND
        REQUIRED_VARS
            Libdlmclient_LIBRARY
            Libdlmclient_INCLUDE_DIR
        VERSION_VAR
            Libdlmclient_VERSION
    )

    if(Libdlmclient_FOUND AND NOT TARGET Libdlmclient::Libdlmclient)
        add_library(Libdlmclient::Libdlmclient UNKNOWN IMPORTED)
        set_target_properties(Libdlmclient::Libdlmclient PROPERTIES
            IMPORTED_LOCATION "${Libdlmclient_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${Libdlmclient_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${Libdlmclient_INCLUDE_DIR}"
        )
    endif()

    mark_as_advanced(Libdlmclient_LIBRARY Libdlmclient_INCLUDE_DIR)

    # compatibility variables
    set(Libdlmclient_LIBRARIES ${Libdlmclient_LIBRARY})
    set(Libdlmclient_INCLUDE_DIRS ${Libdlmclient_INCLUDE_DIR})
    set(Libdlmclient_VERSION_STRING ${Libdlmclient_VERSION})

else()
    message(STATUS "FindLibdlmclient.cmake cannot find libdlmclient on Windows systems.")
    set(Libdlmclient_FOUND FALSE)
endif()

include(FeatureSummary)
set_package_properties(Libdlmclient PROPERTIES
    URL "https://git.automotivelinux.org/src/drm-lease-manager/"
    DESCRIPTION "The DRM Lease Manager uses the DRM Lease feature, introduced in the Linux kernel version 4.15, to partition display controller output resources between multiple processes."
)
