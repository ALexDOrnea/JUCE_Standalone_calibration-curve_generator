# CMake toolchain: cross-compile for Windows x64 with clang-cl, using the CRT/SDK splatted by xwin.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(XWIN_DIR "/xwin" CACHE PATH "Output directory of 'xwin splat'")
set(triple x86_64-pc-windows-msvc)

set(CMAKE_C_COMPILER   clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_C_COMPILER_TARGET   ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})
set(CMAKE_LINKER   lld-link)
set(CMAKE_AR       llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT       llvm-mt)

# xwin ships only release CRT libs: static release runtime everywhere, compiler checks included
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)

set(includes
  "${XWIN_DIR}/crt/include"
  "${XWIN_DIR}/sdk/include/ucrt"
  "${XWIN_DIR}/sdk/include/um"
  "${XWIN_DIR}/sdk/include/shared"
  "${XWIN_DIR}/sdk/include/winrt")
set(compile_flags "")
set(rc_flags "")
foreach(dir IN LISTS includes)
  string(APPEND compile_flags " /imsvc \"${dir}\"")
  string(APPEND rc_flags " -I \"${dir}\"")
endforeach()

set(CMAKE_C_FLAGS_INIT   "${compile_flags}")
set(CMAKE_CXX_FLAGS_INIT "${compile_flags} /EHsc")
set(CMAKE_RC_FLAGS_INIT  "${rc_flags}")

# /tmp/xwin-case: case-variant symlinks (e.g. Dwrite.lib -> dwrite.lib) created by build-windows.sh
set(link_flags
  "/libpath:\"${XWIN_DIR}/crt/lib/x86_64\" /libpath:\"${XWIN_DIR}/sdk/lib/um/x86_64\" /libpath:\"${XWIN_DIR}/sdk/lib/ucrt/x86_64\" /libpath:/tmp/xwin-case")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${link_flags}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${link_flags}")

set(CMAKE_FIND_ROOT_PATH "${XWIN_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
