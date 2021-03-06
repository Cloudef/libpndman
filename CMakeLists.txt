CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(libpndman)
SET(LIBPNDMAN_NAME "libpndman")
SET(LIBPNDMAN_DESCRIPTION "Package managment library for PND files")
SET(LIBPNDMAN_MAJOR 1)
SET(LIBPNDMAN_MINOR 0)
SET(LIBPNDMAN_BUGFIX 0)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${libpndman_SOURCE_DIR}/CMake/modules)

# General build options
OPTION(LIBPNDMAN_BUILD_STATIC "Build as static library" OFF)
OPTION(LIBPNDMAN_BUILD_TESTS "Build tests" OFF)
OPTION(LIBPNDMAN_BUILD_MILKYHELPER "Build milkyhelper" ON)
OPTION(LIBPNDMAN_NO_SYSTEM_LIBS "Force build dependencies from git" OFF)

IF (WIN32)
   SET(LIBPNDMAN_BUILD_STATIC ON FORCE)
   SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
   SET(CMAKE_EXE_LINKER_FLAGS "-static-libgcc")
   ADD_DEFINITIONS(-DCURL_STATICLIB)
ENDIF ()

FIND_PACKAGE(CURL REQUIRED)
LIST(APPEND LIBPNDMAN_INCL ${CURL_INCLUDE_DIR})
LIST(APPEND LIBPNDMAN_LINK ${CURL_LIBRARIES})

FIND_PACKAGE(OpenSSL REQUIRED)
LIST(APPEND LIBPNDMAN_INCL ${OpenSSL_INCLUDE_DIR})
LIST(APPEND LIBPNDMAN_LINK ${OPENSSL_LIBRARIES})

FIND_PACKAGE(EXPAT REQUIRED)
LIST(APPEND LIBPNDMAN_INCL ${EXPAT_INCLUDE_DIR})
LIST(APPEND LIBPNDMAN_LINK ${EXPAT_LIBRARIES})

FIND_PACKAGE(BZip2 REQUIRED)
LIST(APPEND LIBPNDMAN_INCL ${BZIP_INCLUDE_DIR})
LIST(APPEND LIBPNDMAN_LINK ${BZIP2_LIBRARIES})

FIND_PACKAGE(BZip2 REQUIRED)
LIST(APPEND LIBPNDMAN_INCL ${BZIP_INCLUDE_DIR})
LIST(APPEND LIBPNDMAN_LINK ${BZIP2_LIBRARIES})

FIND_PACKAGE(ZLIB REQUIRED)
LIST(APPEND LIBPNDMAN_INCL ${ZLIB_INCLUDE_DIR})
LIST(APPEND LIBPNDMAN_LINK ${ZLIB_LIBRARIES})

# Stuff we can build from git
IF (NOT LIBPNDMAN_NO_SYSTEM_LIBS)
    FIND_PACKAGE(Jansson)
ENDIF ()

IF (JANSSON_FOUND)
    LIST(APPEND LIBPNDMAN_INCL ${JANSSON_INCLUDE_DIRS})
    LIST(APPEND LIBPNDMAN_LINK ${JANSSON_LIBRARIES})
ENDIF ()

# Build depencies, if no system libraries found
ADD_SUBDIRECTORY(deps)

IF (WIN32)
   LIST(APPEND LIBPNDMAN_LINK "ws2_32")
ENDIF ()

# Export libpndman library dependencies
SET(LIBPNDMAN_LIBRARIES ${LIBPNDMAN_LINK} CACHE STRING "Dependencies of libpndman")

IF (PANDORA)
    ADD_DEFINITIONS(-DPANDORA)
ENDIF ()

# Warnings
IF (MSVC)
   IF (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
      STRING(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
   ELSE ()
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
   ENDIF ()
ELSEIF (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
   SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -Wextra -Wno-variadic-macros -Wno-long-long")
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-variadic-macros -Wno-long-long")
ENDIF ()

IF (UNIX AND CMAKE_COMPILER_IS_GNUCC)
   SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
   IF (${CMAKE_VERSION} VERSION_LESS 2.8.9)
      ADD_DEFINITIONS(-fPIC)
   ENDIF ()
ENDIF ()

# Build libpndman
ADD_SUBDIRECTORY(lib)

# Build tests
IF (LIBPNDMAN_BUILD_TESTS)
   MESSAGE("Building libpndman with tests")
   ADD_SUBDIRECTORY(test)
ENDIF ()

# Build milkyhelper
IF (LIBPNDMAN_BUILD_MILKYHELPER)
   MESSAGE("Building libpndman with milkyhelper")
   ADD_SUBDIRECTORY(helper)
   INSTALL(PROGRAMS 3rdparty/milkshake-repo-keygen DESTINATION bin)
ENDIF ()

# Install
INSTALL(FILES include/pndman.h DESTINATION include)
