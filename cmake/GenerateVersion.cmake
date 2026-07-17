# Run at build time (not just configure time - see the ALL custom target in
# CMakeLists.txt) so the commit count/dirty state is always current, even if
# nothing else changed since the last CMake configure.
#
# Expects -D SRC_DIR, OUT_FILE, VBGO_VERSION, BUILD_TYPE on the command line.

find_package(Git QUIET)

set(GIT_COMMIT_COUNT "")
set(GIT_DIRTY "")
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${SRC_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_COMMIT_COUNT_RESULT
    )
    if(NOT GIT_COMMIT_COUNT_RESULT EQUAL 0)
        set(GIT_COMMIT_COUNT "")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${SRC_DIR}
        OUTPUT_VARIABLE GIT_DIRTY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

# Release: clean "v<VERSION>", no build number - what actually ships.
# Anything else (Debug/RelWithDebInfo/unset, i.e. every local dev build):
# "v<VERSION>-dev.<commit count>", "-dirty" appended if the working tree has
# uncommitted changes. Falls back to just "-dev" (no number) if git isn't
# available or this isn't a git checkout (e.g. a source tarball).
if(BUILD_TYPE STREQUAL "Release")
    set(VERSION_STRING "v${VBGO_VERSION}")
else()
    set(VERSION_STRING "v${VBGO_VERSION}-dev")
    if(GIT_COMMIT_COUNT)
        set(VERSION_STRING "${VERSION_STRING}.${GIT_COMMIT_COUNT}")
        if(NOT GIT_DIRTY STREQUAL "")
            set(VERSION_STRING "${VERSION_STRING}-dirty")
        endif()
    endif()
endif()

set(CONTENT "#pragma once\n// Auto-generated at build time by cmake/GenerateVersion.cmake - not committed, see CMakeLists.txt.\ninline constexpr const char *kGeneratedVersionString = \"${VERSION_STRING}\";\n")

# Only touch the file if the content actually changed, so incremental builds
# don't recompile everything that includes it just because HEAD's mtime moved.
set(EXISTING "")
if(EXISTS ${OUT_FILE})
    file(READ ${OUT_FILE} EXISTING)
endif()
if(NOT EXISTING STREQUAL CONTENT)
    file(WRITE ${OUT_FILE} "${CONTENT}")
endif()
