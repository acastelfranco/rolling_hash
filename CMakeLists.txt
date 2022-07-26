cmake_minimum_required(VERSION 3.10)

# set the project name
project(rollinghash)

add_compile_options(-g -O3 -Wno-terminate)

set (SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SignatureFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DeltaFile.cpp
)

set (HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/BackupService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CompressionService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Exceptions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Signature.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SignatureFile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/HashService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FileService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Delta.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DeltaFile.h
)

add_library (rollinghash ${SOURCES} ${HEADERS})

target_include_directories(rollinghash
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(backupnrestore 
    ${CMAKE_CURRENT_SOURCE_DIR}/app/main.cpp
    ${HEADERS})

target_include_directories(backupnrestore
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(backupnrestore PRIVATE rollinghash z)

add_custom_command(
    TARGET backupnrestore
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${CMAKE_SOURCE_DIR}/starwars_a_new_hope.txt ${CMAKE_BINARY_DIR}/starwars_a_new_hope.txt
)

add_custom_command(
    TARGET backupnrestore
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${CMAKE_SOURCE_DIR}/starwars_a_new_hope_modified.txt ${CMAKE_BINARY_DIR}/starwars_a_new_hope_modified.txt
)

set (TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/catch.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test0001.cpp
)

add_executable (tests ${TESTS} ${HEADERS})

target_include_directories(tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(tests PRIVATE rollinghash z)

option (BUILD_DOC "Build documentation" ON)

find_package (Doxygen)
if (DOXYGEN_FOUND)
    set (DOXYGEN_IN  ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set (DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.out)

    configure_file (${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message ("Doxygen build started")

    add_custom_target ( doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
	    WORKING_DIRECTORY ${CMAKE_CURRENT_SRC_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
    message ("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)
