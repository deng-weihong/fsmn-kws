add_executable(test_frontend test_frontend.cpp)
target_link_libraries(test_frontend PRIVATE fsmn_kws::fsmn_kws)
target_compile_options(test_frontend PRIVATE -Wall -Wextra -Werror)
add_test(NAME frontend_contract COMMAND test_frontend)

add_executable(test_engine test_engine.cpp)
target_link_libraries(test_engine PRIVATE fsmn_kws::fsmn_kws)
target_include_directories(test_engine PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(test_engine PRIVATE -Wall -Wextra -Werror)
add_test(NAME engine_contract COMMAND test_engine)

if(FSMN_KWS_BUILD_EXAMPLES)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME cli_contract COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py
            --wav $<TARGET_FILE:kws_wav>
            --stream $<TARGET_FILE:kws_stream>
            --frontend $<TARGET_FILE:frontend_wav>)
    else()
        message(STATUS "Python 3 unavailable: skipping CLI integration checks")
    endif()
endif()
