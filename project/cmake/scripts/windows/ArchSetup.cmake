# -------- Architecture settings ---------

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(ARCH win32)
  set(SDK_TARGET_ARCH x86)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH x64)
  set(SDK_TARGET_ARCH x64)
endif()


# -------- Paths (mainly for find_package) ---------

set(PLATFORM_DIR platform/win32)

# Precompiled headers fail with per target output directory. (needs CMake 3.1)
set(PRECOMPILEDHEADER_DIR ${PROJECT_BINARY_DIR}/${CORE_BUILD_CONFIG}/objs)

set(CMAKE_SYSTEM_NAME Windows)
list(APPEND CMAKE_SYSTEM_PREFIX_PATH ${PROJECT_SOURCE_DIR}/../../lib/win32)
list(APPEND CMAKE_SYSTEM_PREFIX_PATH ${PROJECT_SOURCE_DIR}/../../lib/win32/ffmpeg)
list(APPEND CMAKE_SYSTEM_LIBRARY_PATH ${PROJECT_SOURCE_DIR}/../../lib/win32/ffmpeg/bin)
list(APPEND CMAKE_SYSTEM_PREFIX_PATH ${PROJECT_SOURCE_DIR}/../BuildDependencies/${ARCH})
list(APPEND CMAKE_SYSTEM_PREFIX_PATH ${PROJECT_SOURCE_DIR}/../BuildDependencies)

if(${ARCH} STREQUAL win32)
  set(PYTHON_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/../BuildDependencies/include/python)
else()
  set(PYTHON_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/../BuildDependencies/${ARCH}/include/python)
endif()


# -------- Compiler options ---------

add_options(CXX ALL_BUILDS "/wd\"4996\"")
set(ARCH_DEFINES -D_WINDOWS -DTARGET_WINDOWS -D__SSE__ -D__SSE2__)
set(SYSTEM_DEFINES -DNOMINMAX -DHAS_DX -D__STDC_CONSTANT_MACROS
                   -DTAGLIB_STATIC -DNPT_CONFIG_ENABLE_LOGGING
                   -DPLT_HTTP_DEFAULT_USER_AGENT="UPnP/1.0 DLNADOC/1.50 Kodi"
                   -DPLT_HTTP_DEFAULT_SERVER="UPnP/1.0 DLNADOC/1.50 Kodi"
                   -DHAS_DS_PLAYER
                   $<$<CONFIG:Debug>:-DD3D_DEBUG_INFO>)

if(${ARCH} STREQUAL win32)
  list(APPEND SYSTEM_DEFINES $<$<CONFIG:Debug>:-D_ITERATOR_DEBUG_LEVEL=0>)
endif()

# Make sure /FS is set for Visual Studio in order to prevent simultaneous access to pdb files.
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set(CMAKE_CXX_FLAGS "/MP /FS ${CMAKE_CXX_FLAGS}")
endif()

# Google Test needs to use shared version of runtime libraries
set(gtest_force_shared_crt ON CACHE STRING "" FORCE)


# -------- Linker options ---------

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE /SAFESEH:NO")

# For #pragma comment(lib X)
# TODO: It would certainly be better to handle these libraries via CMake modules.
if(${ARCH} STREQUAL win32)
  link_directories(${PROJECT_SOURCE_DIR}/../../lib/win32/ffmpeg/bin
                   ${PROJECT_SOURCE_DIR}/../BuildDependencies/lib)
else()
  link_directories(${PROJECT_SOURCE_DIR}/../../lib/win32/ffmpeg/bin
                   ${PROJECT_SOURCE_DIR}/../BuildDependencies/${ARCH}/lib)
endif()

# Additional libraries
list(APPEND DEPLIBS d3d11.lib DInput8.lib DSound.lib winmm.lib Mpr.lib Iphlpapi.lib WS2_32.lib
                    PowrProf.lib setupapi.lib dwmapi.lib dxguid.lib DelayImp.lib comctl32.lib)

# NODEFAULTLIB option
set(_nodefaultlibs_RELEASE libcmt)
set(_nodefaultlibs_DEBUG libcmt msvcrt)
foreach(_lib ${_nodefaultlibs_RELEASE})
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /NODEFAULTLIB:\"${_lib}\"")
endforeach()
foreach(_lib ${_nodefaultlibs_DEBUG})
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:\"${_lib}\"")
endforeach()

# DELAYLOAD option
set(_delayloadlibs zlib.dll libmysql.dll libxslt.dll dnssd.dll dwmapi.dll ssh.dll sqlite3.dll
                   avcodec-57.dll avfilter-6.dll avformat-57.dll avutil-55.dll
                   postproc-54.dll swresample-2.dll swscale-4.dll d3dcompiler_47.dll)
foreach(_lib ${_delayloadlibs})
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DELAYLOAD:\"${_lib}\"")
endforeach()

# Make the Release version create a PDB
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
# Minimize the size or the resulting DLLs
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF")


# -------- Visual Studio options ---------

if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  # Generate a batch file that opens Visual Studio with the necessary env variables set.
  file(WRITE ${CMAKE_BINARY_DIR}/kodi-sln.bat
             "@echo off\n"
             "set KODI_HOME=%~dp0\n"
             "set PATH=%~dp0\\system\n"
             "start %~dp0\\${PROJECT_NAME}.sln")
endif()
