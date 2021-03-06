include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)

cmake_minimum_required(VERSION 3.0)
project(obelix)

# TODO: Make options for:
# - Windows 32 bit build (-w32)
# - Debug build (CMAKE_BUILD_TYPE=Debug)

# Uncomment if you want debug output:
# set(CMAKE_C_FLAGS -m32)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

# set(CMAKE_VERBOSE_MAKEFILE ON)

# The version number.
set(obelix_NAME Obelix)
set(obelix_VERSION_MAJOR 1)
set(obelix_VERSION_MINOR 0)

set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})
set(obelix_DIR ${CMAKE_INSTALL_PREFIX})
set(obelix_DATADIR ${CMAKE_INSTALL_PREFIX}/share)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Threads)
find_package(Check)

check_include_file(alloca.h HAVE_ALLOCA_H)
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(dlfcn.h HAVE_DLFCN_H)
check_include_file(langinfo.h HAVE_LANGINFO_H)
check_include_file(libintl.h HAVE_LIBINTL_H)
check_include_file(locale.h HAVE_LOCALE_H)
check_include_file(malloc.h HAVE_MALLOC_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(pthread.h HAVE_PTHREAD_H)
check_include_file(pwd.h HAVE_PWD_H)
check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(stdbool.h HAVE_STDBOOL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/utsname.h HAVE_SYS_UTSNAME_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(wchar.h HAVE_WCHAR_H)
check_include_file(wctype.h HAVE_WCTYPE_H)
check_include_file(windows.h HAVE_WINDOWS_H)
if (HAVE_WINDOWS_H AND NOT HAVE_SYS_SOCKET_H)
  check_include_file(winsock2.h HAVE_WINSOCK2_H)
  check_include_file(ws2tcpip.h HAVE_WS2TCPIP_H)
  check_include_file(io.h HAVE_IO_H)
endif (HAVE_WINDOWS_H AND NOT HAVE_SYS_SOCKET_H)

check_include_file(regex.h HAVE_REGEX_H)
if(HAVE_REGEX_H)
  set(WITH_RE 1)
else(HAVE_REGEX_H)
  find_package(Pcre2)
  if(PCRE2_FOUND)
    set(WITH_RE 1)
    if(PCRE2_FOUND)
      include_directories(${PCRE2_INCLUDE_DIR})
    endif(PCRE2_FOUND)
  endif(PCRE2_FOUND)
endif(HAVE_REGEX_H)

set(CMAKE_EXTRA_INCLUDE_FILES stdint.h)
check_type_size(intptr_t INTPTR_T)
set(CMAKE_EXTRA_INCLUDE_FILES)

if(HAVE_ALLOCA_H)
  set(ALLOCA_HDR alloca.h)
elseif(HAVE_MALLOC_H)
  set(ALLOCA_HDR malloc.h)
endif(HAVE_ALLOCA_H)
check_symbol_exists(alloca ${ALLOCA_HDR} HAVE_ALLOCA)
check_function_exists(mempcpy HAVE_MEMPCPY)
check_function_exists(putenv HAVE_PUTENV)
if(NOT HAVE_PUTENV)
  check_function_exists(_putenv HAVE__PUTENV)
endif(NOT HAVE_PUTENV)

check_symbol_exists(itoa stdio.h HAVE_ITOA)
check_symbol_exists(_itoa stdio.h HAVE__ITOA)
check_symbol_exists(dtoa stdio.h HAVE_DTOA)
check_symbol_exists(_dtoa stdio.h HAVE__DTOA)

set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
set(CMAKE_EXTRA_INCLUDE_FILES stdio.h)
check_function_exists(asprintf HAVE_ASPRINTF)
check_function_exists(vasprintf HAVE_VASPRINTF)
set(CMAKE_EXTRA_INCLUDE_FILES)
set(CMAKE_REQUIRED_DEFINITIONS)

if(HAVE_PTHREAD_H)
  set(CMAKE_REQUIRED_DEFINITIONS, -D_GNU_SOURCE)
  set(CMAKE_EXTRA_INCLUDE_FILES pthread.h)
  check_function_exists(pthread_setname_np HAVE_PTHREAD_SETNAME_NP)
  check_function_exists(pthread_yield HAVE_PTHREAD_YIELD)
  set(CMAKE_EXTRA_INCLUDE_FILES)
  set(CMAKE_REQUIRED_DEFINITIONS)
endif(HAVE_PTHREAD_H)

check_symbol_exists(kill signal.h HAVE_KILL)
check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)
check_symbol_exists(getpwnam pwd.h HAVE_GETPWNAM)
check_symbol_exists(sysconf unistd.h HAVE_SYSCONF)

if(HAVE_SYS_SOCKET_H)
  set(CMAKE_EXTRA_INCLUDE_FILES sys/socket.h)
  check_type_size(socklen_t SOCKLEN_T)
  set(CMAKE_EXTRA_INCLUDE_FILES)
  set(_SOCKET_HDR sys/socket.h)
  check_symbol_exists(gethostname unistd.h HAVE_GETHOSTNAME)
endif(HAVE_SYS_SOCKET_H)

if(HAVE_WINDOWS_H)
  check_symbol_exists(GetSystemInfo windows.h HAVE_GETSYSTEMINFO)
  check_symbol_exists(GetVersionEx windows.h HAVE_GETVERSIONEX)
  check_symbol_exists(FlushFileBuffers windows.h HAVE_FLUSHFILEBUFFERS)
  check_symbol_exists(CreateSemaphore windows.h HAVE_CREATESEMAPHORE)
  check_symbol_exists(CreateThread windows.h HAVE_CREATETHREAD)
  check_symbol_exists(GetUserName windows.h HAVE_GETUSERNAME)
  check_symbol_exists(InitializeCriticalSection windows.h HAVE_INITIALIZECRITICALSECTION)
  check_symbol_exists(InitOnceExecuteOnce windows.h HAVE_INITONCEEXECUTEONCE)
  check_symbol_exists(QueryPerformanceCounter windows.h HAVE_QUERYPERFORMANCECOUNTER)
endif(HAVE_WINDOWS_H)

if(HAVE_IO_H)
  set(CMAKE_EXTRA_INCLUDE_FILES io.h)
  check_function_exists(_findfirst HAVE__FINDFIRST)
endif(HAVE_IO_H)

if(HAVE_WINSOCK2_H)
  set(CMAKE_EXTRA_INCLUDE_FILES winsock2.h)
  check_type_size(SOCKET TYPE_SOCKET)
  set(CMAKE_EXTRA_INCLUDE_FILES)
  set(_SOCKET_HDR winsock2.h)
  check_symbol_exists(gethostname winsock2.h HAVE_GETHOSTNAME)
endif(HAVE_WINSOCK2_H)

check_symbol_exists(SO_REUSEADDR sys/socket.h HAVE_SO_REUSEADDR)
check_symbol_exists(SO_NOSIGPIPE sys/socket.h HAVE_SO_NOSIGPIPE)
check_symbol_exists(MSG_NOSIGNAL sys/socket.h HAVE_MSG_NOSIGNAL)

if(HAVE_WINDOWS_H)
check_symbol_exists(MAX_PATH windows.h HAVE_MAX_PATH)
else(HAVE_WINDOWS_H)
check_symbol_exists(MAX_PATH stdlib.h HAVE_MAX_PATH)
endif(HAVE_WINDOWS_H)
if(NOT HAVE_MAX_PATH)
  check_symbol_exists(_MAX_PATH stdlib.h HAVE__MAX_PATH)
  if(NOT HAVE__MAX_PATH)
    check_symbol_exists(PATH_MAX limits.h HAVE_PATH_MAX)
  endif(NOT HAVE__MAX_PATH)
endif(NOT HAVE_MAX_PATH)
check_symbol_exists(ECONNRESET errno.h HAVE_ECONNRESET)
check_symbol_exists(fsync unistd.h HAVE_FSYNC)
check_symbol_exists(getline unistd.h HAVE_FSYNC)

set(CMAKE_EXTRA_INCLUDE_FILES string.h)
check_function_exists(_strdup HAVE__STRDUP)
check_function_exists(_stricmp HAVE__STRICMP)
check_function_exists(_strnicmp HAVE__STRNICMP)

set(CMAKE_EXTRA_INCLUDE_FILES strings.h)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(strncasecmp HAVE_STRNCASECMP)

set(CMAKE_EXTRA_INCLUDE_FILES wchar.h)
check_function_exists(mbrtowc HAVE_MBRTOWC)
check_function_exists(wcrtomb HAVE_WCRTOMB)
check_function_exists(mbrtowc HAVE_WCSCOLL)


###############################################################################
# Check for integer types
# (The following are used in check.h. Regardless if they are used in
# the project, they will need to be checked in order to use Check).
check_type_size(intmax_t INTMAX_T)
check_type_size(uintmax_t UINTMAX_T)

check_type_size(pid_t PID_T)
if(NOT HAVE_PID_T)
  if(WIN32)
    set(pid_t "int")
  else(WIN32)
    MESSAGE(FATAL_ERROR "pid_t doesn't exist on this platform?")
  endif(WIN32)
endif(NOT HAVE_PID_T)
###############################################################################

if(CMAKE_COMPILER_IS_GNUCC)
  set(_GNU_SOURCE 1)
endif(CMAKE_COMPILER_IS_GNUCC)

find_package(Readline)
if(READLINE_FOUND)
  set(WITH_READLINE 1)
endif(READLINE_FOUND)

###############################################################################
# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/oblconfig.h.in"
  "${PROJECT_BINARY_DIR}/oblconfig.h"
)

if(CMAKE_COMPILER_IS_GNUCC)
  add_definitions(-D_GNU_SOURCE)
endif(CMAKE_COMPILER_IS_GNUCC)
if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

check_library_exists(rt timer_create "" HAVE_LIBRT)

set(SYSLIBS "m")
if(HAVE_LIBRT)
  set(SYSLIBS "rt" ${SYSLIBS})
endif(HAVE_LIBRT)
set(SYSLIBS ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} ${SYSLIBS})

include_directories("${PROJECT_SOURCE_DIR}/include")

# add the binary tree to the search path for include files
# so that we will find config.h
include_directories("${PROJECT_BINARY_DIR}")

add_subdirectory(src)
add_subdirectory(share)
#add_subdirectory(include)

# install(FILES "${PROJECT_BINARY_DIR}/config.h" DESTINATION include)
if(NOT MSVC)
  enable_testing()
  add_test(NAME tcore COMMAND tcore)
  add_test(NAME tlexer COMMAND tlexer)
  add_test(NAME tgrammar COMMAND tgrammar)
  add_test(NAME tparser COMMAND tparser)
endif(NOT MSVC)

# Extend Windows Path with some of our source directories so that panoramix can
# load its DLLs.
# if(WIN32)
#   set(ENV{PATH},$ENV{PATH};${CMAKE_CURRENT_BINARY_DIR}/src/lib;${CMAKE_CURRENT_BINARY_DIR}/src/lexer;${CMAKE_CURRENT_BINARY_DIR}/src/grammar)
# endif(WIN32)

add_custom_target(install_${PROJECT_NAME}
  $(MAKE) install
  DEPENDS ${PROJECT_NAME}
  COMMENT "Installing ${PROJECT_NAME}")
