cmake_minimum_required(VERSION 3.20)
project(VIC25 VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vic25_core INTERFACE)
target_include_directories(vic25_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vic25_core INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_executable(vic25_demo src/main.cpp)
target_link_libraries(vic25_demo PRIVATE vic25_core)

include(CTest)
if (BUILD_TESTING)
    add_executable(vic25_core_tests tests/test_core.cpp)
    target_link_libraries(vic25_core_tests PRIVATE vic25_core)
    add_test(NAME vic25_core_tests COMMAND vic25_core_tests)
endif()

# POP-I1~I4: independent social/demographic simulation (existing S0 core preserved).
add_executable(vic25_phase_b_demo src/phase_b_demo.cpp)
target_link_libraries(vic25_phase_b_demo PRIVATE vic25_core)
add_executable(vic25_phase_b_scaling bench/phase_b_scaling.cpp)
target_link_libraries(vic25_phase_b_scaling PRIVATE vic25_core)

if (BUILD_TESTING)
    add_executable(vic25_phase_b_tests tests/test_phase_b.cpp)
    target_link_libraries(vic25_phase_b_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_b_tests COMMAND vic25_phase_b_tests)

    add_executable(vic25_phase_b_features_tests tests/test_phase_b_features.cpp)
    target_link_libraries(vic25_phase_b_features_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_b_features_tests COMMAND vic25_phase_b_features_tests)

    add_executable(vic25_phase_b_longrun_tests tests/test_phase_b_longrun.cpp)
    target_link_libraries(vic25_phase_b_longrun_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_b_longrun_tests COMMAND vic25_phase_b_longrun_tests)
    set_tests_properties(vic25_phase_b_longrun_tests PROPERTIES TIMEOUT 180)
endif()

# Phase C vertical slice: original Phase B POP registry + S0 finance/inventory
# with cross-domain staging, contract tests and a 1,200-month integration loop.
if (BUILD_TESTING)
    add_executable(vic25_phase_c_tests tests/test_phase_c.cpp)
    target_link_libraries(vic25_phase_c_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_c_tests COMMAND vic25_phase_c_tests)

    add_executable(vic25_phase_c_longrun tests/test_phase_c_longrun.cpp)
    target_link_libraries(vic25_phase_c_longrun PRIVATE vic25_core)
    add_test(NAME vic25_phase_c_longrun COMMAND vic25_phase_c_longrun)
    set_tests_properties(vic25_phase_c_longrun PROPERTIES TIMEOUT 180)
endif()

# Observed large-slice performance; not automatically a 100-year stress pass.
add_executable(vic25_phase_c_scaling bench/phase_c_scaling.cpp)
target_link_libraries(vic25_phase_c_scaling PRIVATE vic25_core)

# Bounded 1,200-month multi-domain scenario with economic, political, military
# shocks. Its PASS does not replace the full D4-T46 suite with demographic risks.
if (BUILD_TESTING)
    add_executable(vic25_phase_c_combined_longrun tests/test_phase_c_combined_longrun.cpp)
    target_link_libraries(vic25_phase_c_combined_longrun PRIVATE vic25_core)
    add_test(NAME vic25_phase_c_combined_longrun COMMAND vic25_phase_c_combined_longrun)
    set_tests_properties(vic25_phase_c_combined_longrun PROPERTIES TIMEOUT 180)
endif()

# D4-T46: 1,200 months x2 with endogenous population dynamics and simultaneous
# economic, political, military, healthcare and financial crisis events.
if (BUILD_TESTING)
    add_executable(vic25_phase_c_population_crisis tests/test_phase_c_population_crisis.cpp)
    target_link_libraries(vic25_phase_c_population_crisis PRIVATE vic25_core)
    add_test(NAME vic25_phase_c_population_crisis COMMAND vic25_phase_c_population_crisis)
    set_tests_properties(vic25_phase_c_population_crisis PROPERTIES TIMEOUT 240)
endif()

# C-D4-T48 is a contract/source baseline gate, not the final complete-game release.
# The audit fails on edited canonical Git blobs, missing Phase B/P code, or
# incomplete D4 test IDs. The C++ D4-T48 itself is an aggregate of 47 real cases.
if (BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    target_compile_definitions(vic25_phase_c_tests PRIVATE VIC25_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}\")
    add_test(
        NAME vic25_phase_c_contract_audit
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/verify_phase_c_baseline.py"
                --root "${CMAKE_SOURCE_DIR}"
    )
endif()


# Phase D: bounded construction/industry/research integration on the *same*
# Phase C atomic World, with explicit unfinished D-V acceptance boundaries.
if (BUILD_TESTING)
    add_executable(vic25_phase_d_tests tests/test_phase_d.cpp)
    target_link_libraries(vic25_phase_d_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_tests COMMAND vic25_phase_d_tests)
    add_executable(vic25_phase_d_snapshot_tests tests/test_phase_d_snapshot.cpp)
    target_link_libraries(vic25_phase_d_snapshot_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_snapshot_tests COMMAND vic25_phase_d_snapshot_tests)
    add_executable(vic25_phase_d_longrun_tests tests/test_phase_d_longrun.cpp)
    target_link_libraries(vic25_phase_d_longrun_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_longrun_tests COMMAND vic25_phase_d_longrun_tests)
    add_executable(vic25_phase_d_shared_reservations_tests tests/test_phase_d_shared_reservations.cpp)
    target_link_libraries(vic25_phase_d_shared_reservations_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_shared_reservations_tests COMMAND vic25_phase_d_shared_reservations_tests)
    add_executable(vic25_phase_d_construction_tests tests/test_phase_d_construction.cpp)
    target_link_libraries(vic25_phase_d_construction_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_construction_tests COMMAND vic25_phase_d_construction_tests)
    add_executable(vic25_phase_d_industry_tests tests/test_phase_d_industry.cpp)
    target_link_libraries(vic25_phase_d_industry_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_industry_tests COMMAND vic25_phase_d_industry_tests)
    add_executable(vic25_phase_d_technology_tests tests/test_phase_d_technology.cpp)
    target_link_libraries(vic25_phase_d_technology_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_technology_tests COMMAND vic25_phase_d_technology_tests)
    add_executable(vic25_phase_d_cross_domain_tests tests/test_phase_d_cross_domain.cpp)
    target_link_libraries(vic25_phase_d_cross_domain_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_cross_domain_tests COMMAND vic25_phase_d_cross_domain_tests)
    add_executable(vic25_phase_d_population_scale bench/phase_d_population_scale.cpp)
    target_link_libraries(vic25_phase_d_population_scale PRIVATE vic25_core)
    add_test(NAME vic25_phase_d_sparse_pop_100k COMMAND vic25_phase_d_population_scale 100000 2)
    set_tests_properties(vic25_phase_d_sparse_pop_100k PROPERTIES TIMEOUT 240)
    set_tests_properties(vic25_phase_d_longrun_tests PROPERTIES TIMEOUT 180)
endif()


# Phase E real bounded C/D/E integration — EV-Txx tests report explicit BLOCKED
# for GPU renderer, historic geometry and advanced voting cases not yet coded.
if (BUILD_TESTING)
    add_executable(vic25_phase_e_tests tests/test_phase_e.cpp)
    target_link_libraries(vic25_phase_e_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_e_tests COMMAND vic25_phase_e_tests)
    set_tests_properties(vic25_phase_e_tests PROPERTIES TIMEOUT 240)
endif()

# Phase G/H bounded integration on the existing Phase C/D/E staged world.
# Distinct from the unimplemented G 62 / H 176 full contract acceptance tests.
if (BUILD_TESTING)
    add_executable(vic25_phase_gh_tests tests/test_phase_gh.cpp)
    target_link_libraries(vic25_phase_gh_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_gh_tests COMMAND vic25_phase_gh_tests)
    set_tests_properties(vic25_phase_gh_tests PROPERTIES TIMEOUT 120)
endif()
