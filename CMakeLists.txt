cmake_minimum_required(VERSION 3.10)

# set the project name
project(rollinghash)

add_compile_options(-g -O3 -I${CMAKE_SOURCE_DIR} -Wno-terminate)

# add the executable
add_executable(rollinghash
    BackupService.h
    CompressionService.h
    Exceptions.h
    Signature.h
    SignatureFile.h
    SignatureFile.cpp
    HashService.h
    FileService.h
    Delta.h
    DeltaFile.h
    DeltaFile.cpp
    main.cpp
)

target_link_libraries(rollinghash PRIVATE z)

add_custom_command(
    TARGET rollinghash
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${CMAKE_SOURCE_DIR}/starwars_a_new_hope.txt ${CMAKE_BINARY_DIR}/starwars_a_new_hope.txt
)

add_custom_command(
    TARGET rollinghash
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${CMAKE_SOURCE_DIR}/starwars_a_new_hope_modified.txt ${CMAKE_BINARY_DIR}/starwars_a_new_hope_modified.txt
)

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
