if (OPENZL_BUILD_PROTOBUF_TOOLS)
    cmake_minimum_required(VERSION 3.24)

    set(OPENZL_PROTOBUF_PROVIDER_VALUES auto system fetch)
    if(NOT OPENZL_PROTOBUF_PROVIDER IN_LIST OPENZL_PROTOBUF_PROVIDER_VALUES)
        message(FATAL_ERROR "OPENZL_PROTOBUF_PROVIDER must be one of ${OPENZL_PROTOBUF_PROVIDER_VALUES}")
    endif()

    if(OPENZL_BUILD_PROTOBUF_TRAINING AND NOT OPENZL_BUILD_CUSTOM_PARSERS)
        message(FATAL_ERROR "OPENZL_BUILD_PROTOBUF_TRAINING requires OPENZL_BUILD_CUSTOM_PARSERS")
    endif()

    if(NOT OPENZL_PROTOBUF_PROVIDER STREQUAL "fetch")
        find_package(protobuf CONFIG QUIET)
        if(OPENZL_PROTOBUF_PROVIDER STREQUAL "system"
                AND (NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::libprotoc))
            find_package(Protobuf MODULE QUIET)
        endif()
    endif()

    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_LIBPROTOC ON CACHE BOOL "" FORCE)
    set(protobuf_BUILD_LIBUPB ON CACHE BOOL "" FORCE)
    set(protobuf_BUILD_PROTOC_BINARIES ${OPENZL_BUILD_PROTOBUF_CLI} CACHE BOOL "" FORCE)

    if((NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::libprotoc)
            AND NOT OPENZL_PROTOBUF_PROVIDER STREQUAL "system")
        FetchContent_Declare(protobuf
            GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
            GIT_TAG        v32.0
        )
        FetchContent_MakeAvailable(protobuf)
    endif()
    if(NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::libprotoc)
        message(FATAL_ERROR "OpenZL protobuf tools require protobuf::libprotobuf and protobuf::libprotoc")
    endif()
    if(TARGET libprotobuf AND NOT TARGET protobuf::libprotobuf)
        target_compile_options(libprotobuf PUBLIC
            -Wno-deprecated-declarations
            -Wno-stringop-overread
            -Wno-strict-aliasing
            -Wno-attributes
        )
    endif()

    file(GLOB protobuf_serializer_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )
    list(REMOVE_ITEM protobuf_serializer_sources "${CMAKE_CURRENT_SOURCE_DIR}/cli.cpp")
    list(REMOVE_ITEM protobuf_serializer_sources "${CMAKE_CURRENT_SOURCE_DIR}/openzl_protobuf_core_ffi.cpp")
    list(REMOVE_ITEM protobuf_serializer_sources "${CMAKE_CURRENT_SOURCE_DIR}/openzl_protobuf_train_ffi.cpp")

    add_library(openzl_protobuf_serializer ${protobuf_serializer_sources})
    add_library(protobuf_serializer ALIAS openzl_protobuf_serializer)

    target_link_libraries(openzl_protobuf_serializer PUBLIC
        openzl_cpp
        protobuf::libprotobuf
        protobuf::libprotoc
    )
    apply_openzl_compile_options_to_target(openzl_protobuf_serializer)

    add_library(openzl_protobuf_core SHARED
        "${CMAKE_CURRENT_SOURCE_DIR}/openzl_protobuf_core_ffi.cpp"
    )
    set_target_properties(openzl_protobuf_core PROPERTIES
        OUTPUT_NAME openzl_protobuf_core_ffi
        POSITION_INDEPENDENT_CODE ON
    )
    target_include_directories(openzl_protobuf_core PRIVATE ${PROJECT_SOURCE_DIR})
    target_link_libraries(openzl_protobuf_core PRIVATE openzl_protobuf_serializer)
    apply_openzl_compile_options_to_target(openzl_protobuf_core)

    if(OPENZL_BUILD_PROTOBUF_TRAINING)
        add_library(openzl_protobuf_train SHARED
            "${CMAKE_CURRENT_SOURCE_DIR}/openzl_protobuf_train_ffi.cpp"
        )
        set_target_properties(openzl_protobuf_train PROPERTIES
            OUTPUT_NAME openzl_protobuf_train_ffi
            POSITION_INDEPENDENT_CODE ON
        )
        target_include_directories(openzl_protobuf_train PRIVATE ${PROJECT_SOURCE_DIR})
        target_link_libraries(openzl_protobuf_train PRIVATE
            openzl_protobuf_core
            tools_training
            custom_parsers
        )
        apply_openzl_compile_options_to_target(openzl_protobuf_train)
    endif()

    if(OPENZL_BUILD_PROTOBUF_CLI)
        file(GLOB protobuf_cli_sources CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/cli.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/schema.proto")
        add_executable(openzl_protobuf_cli ${protobuf_cli_sources})
        set_target_properties(openzl_protobuf_cli PROPERTIES OUTPUT_NAME protobuf_cli)

        if(NOT COMMAND protobuf_generate)
            if(DEFINED protobuf_SOURCE_DIR
                    AND EXISTS "${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake")
                include("${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake")
            else()
                include(FindProtobuf)
            endif()
        endif()
        protobuf_generate(TARGET openzl_protobuf_cli PROTOC_EXE protobuf::protoc)

        target_link_libraries(openzl_protobuf_cli PRIVATE
            openzl_protobuf_serializer
            arg
            tools_io
            tools_training
            custom_parsers
        )
        target_include_directories(openzl_protobuf_cli PRIVATE ${PROJECT_BINARY_DIR})

        apply_openzl_compile_options_to_target(openzl_protobuf_cli)
    endif()

    if (OPENZL_INSTALL)
        set(OPENZL_PROTOBUF_INSTALL_TARGETS openzl_protobuf_core)
        if(OPENZL_BUILD_PROTOBUF_TRAINING)
            list(APPEND OPENZL_PROTOBUF_INSTALL_TARGETS openzl_protobuf_train)
        endif()
        install(TARGETS ${OPENZL_PROTOBUF_INSTALL_TARGETS}
            RUNTIME DESTINATION ${OPENZL_INSTALL_BINDIR}
            LIBRARY DESTINATION ${OPENZL_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${OPENZL_INSTALL_LIBDIR})
    endif()

    if(OPENZL_BUILD_TESTS)
        add_library(protobuf_test_utils "${CMAKE_CURRENT_SOURCE_DIR}/tests/utils.cpp")
        target_link_libraries(protobuf_test_utils PUBLIC openzl_deps)

        add_executable(test_protobuf_descriptor_loader
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_descriptor_loader.cpp")
        target_link_libraries(test_protobuf_descriptor_loader PRIVATE
            openzl_protobuf_serializer
            GTest::gtest_main
            protobuf_test_utils
        )
        gtest_discover_tests(test_protobuf_descriptor_loader)

        add_executable(test_protobuf_dynamic_messages
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dynamic_messages.cpp")
        target_link_libraries(test_protobuf_dynamic_messages PRIVATE
            openzl_protobuf_serializer
            GTest::gtest_main
            protobuf_test_utils
        )
        gtest_discover_tests(test_protobuf_dynamic_messages)
    endif()
endif()
