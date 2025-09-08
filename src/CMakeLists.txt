find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
BISON_TARGET(booksim_config_parser config.y ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
            COMPILE_FLAGS "-y -d --file-prefix=${CMAKE_CURRENT_BINARY_DIR}/y")
FLEX_TARGET(booksim_config_lexer config.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
ADD_FLEX_BISON_DEPENDENCY(booksim_config_lexer booksim_config_parser)

set_source_files_properties(
    ${BISON_booksim_config_parser_OUTPUT_SOURCE}                  
    ${FLEX_booksim_config_lexer_OUTPUTS}
    PROPERTIES LANGUAGE C   # <--- Changed from CXX to C (C++ symbol mangling)
)

list(APPEND booksim_incs 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/allocators
    ${CMAKE_CURRENT_SOURCE_DIR}/arbiters
    ${CMAKE_CURRENT_SOURCE_DIR}/networks
    ${CMAKE_CURRENT_SOURCE_DIR}/power
    ${CMAKE_CURRENT_SOURCE_DIR}/routers
)

list(APPEND booksim_srcs
    # FLEX/BISON Generated Sources
    ${BISON_booksim_config_parser_OUTPUT_SOURCE}
    ${FLEX_booksim_config_lexer_OUTPUTS}
    # Allocators
    allocators/allocator.cpp
    allocators/islip.cpp
    allocators/loa.cpp
    allocators/maxsize.cpp
    allocators/pim.cpp
    allocators/selalloc.cpp
    allocators/separable_input_first.cpp
    allocators/separable_output_first.cpp
    allocators/separable.cpp
    allocators/wavefront.cpp
    # Arbiters
    arbiters/arbiter.cpp
    arbiters/matrix_arb.cpp
    arbiters/prio_arb.cpp
    arbiters/roundrobin_arb.cpp
    arbiters/tree_arb.cpp
    # Networks
    networks/anynet.cpp
    networks/cmesh.cpp
    networks/dragonfly.cpp
    networks/fattree.cpp
    networks/flatfly_onchip.cpp
    networks/fly.cpp
    networks/kncube.cpp
    networks/network.cpp
    networks/qtree.cpp
    networks/tree4.cpp
    # Power
    power/buffer_monitor.cpp
    power/power_module.cpp
    power/switch_monitor.cpp
    # Routers
    routers/chaos_router.cpp
    routers/event_router.cpp
    routers/iq_router.cpp
    routers/router.cpp
    # Others
    batchtrafficmanager.cpp
    booksim_config.cpp
    buffer_state.cpp
    buffer.cpp
    config_utils.cpp
    credit.cpp
    flit.cpp
    flitchannel.cpp
    injection.cpp
    # main.cpp              # main.cpp is removed from the static library and global variables are integrated in the py_interface
    misc_utils.cpp
    module.cpp
    mta_icnt_wrapper.cpp    # ADDED SOURCE CODE
    mta_trafficmanager.cpp  # ADDED SOURCE CODE
    outputset.cpp
    packet_reply_info.cpp
    py_interface.cpp        # ADDED SOURCE CODE
    random_utils.cpp
    rng_double_wrapper.cpp
    rng_wrapper.cpp
    # rng-double.c
    # rng.c
    routefunc.cpp
    stats.cpp
    traffic.cpp
    trafficmanager.cpp
    vc.cpp
)

# TARGET: Booksim Library (libbooksim2.a)
add_library(booksim2 STATIC ${booksim_srcs})
target_include_directories(booksim2 PUBLIC  ${booksim_incs})
target_include_directories(booksim2 PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(booksim2 PRIVATE -O3 -fPIC)
set_target_properties(booksim2 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# TARGET: Booksim main (booksim2main)
add_executable(booksim2main main.cpp)
target_include_directories(booksim2main PUBLIC  ${booksim_incs})
target_include_directories(booksim2main PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(booksim2main ${CMAKE_BINARY_DIR}/lib/libbooksim2.a)
target_compile_options(booksim2main PRIVATE -O3 -fPIC)
set_target_properties(booksim2main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)