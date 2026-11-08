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
# The C++20 header-heavy native world exceeds MSVC's COFF section limit
# in several unit-test translation units. Apply /bigobj uniformly through
# the interface target instead of discovering each test failure separately.
if(MSVC)
    target_compile_options(vic25_core INTERFACE /bigobj)
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
    find_package(Threads REQUIRED)
    add_executable(vic25_phase_e_tests tests/test_phase_e.cpp)
    target_link_libraries(vic25_phase_e_tests PRIVATE vic25_core Threads::Threads)
    add_test(NAME vic25_phase_e_tests COMMAND vic25_phase_e_tests)
    set_tests_properties(vic25_phase_e_tests PROPERTIES TIMEOUT 240)
endif()

# Optional OpenGL 3.3 raster globe. Keep headless C++ core independent.
option(VIC25_ENABLE_GL "Enable real GL raster globe test" OFF)
if (VIC25_ENABLE_GL)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    add_executable(vic25_phase_e_gpu_tests tests/test_phase_e_gpu.cpp)
    target_link_libraries(vic25_phase_e_gpu_tests PRIVATE vic25_core glfw GLEW::GLEW OpenGL::GL)
    if (BUILD_TESTING)
        if (UNIX AND NOT APPLE)
            find_program(VIC25_XVFB_EXECUTABLE xvfb-run REQUIRED)
            add_test(NAME vic25_phase_e_gpu_tests COMMAND "${VIC25_XVFB_EXECUTABLE}" -a $<TARGET_FILE:vic25_phase_e_gpu_tests>)
            set_tests_properties(vic25_phase_e_gpu_tests PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
        else()
            add_test(NAME vic25_phase_e_gpu_tests COMMAND vic25_phase_e_gpu_tests)
        endif()
    endif()
endif()

# EV-T59: actual headless synthetic spatial/POP stress with peak RSS and
# P50/P95/P99 month timings. Synthetic geography != completed world GIS.
if (BUILD_TESTING)
    add_executable(vic25_phase_e_scaling bench/phase_e_scaling.cpp)
    target_link_libraries(vic25_phase_e_scaling PRIVATE vic25_core Threads::Threads)
    if (WIN32)
        target_link_libraries(vic25_phase_e_scaling PRIVATE Psapi)
    endif()
    add_test(NAME vic25_phase_e_scale_100k COMMAND vic25_phase_e_scaling 10000 100000 4)
    set_tests_properties(vic25_phase_e_scale_100k PROPERTIES TIMEOUT 450)
    if (UNIX AND NOT APPLE)
        add_test(NAME vic25_phase_e_scale_1m COMMAND vic25_phase_e_scaling 100000 1000000 3)
        set_tests_properties(vic25_phase_e_scale_1m PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)
    endif()
endif()

# Strict EV-T60 final acceptance (intentionally not a success-oriented CTest).
if (BUILD_TESTING)
    add_custom_target(vic25_phase_e_release_gate
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_phase_e_gate.py" --phase-e-test $<TARGET_FILE:vic25_phase_e_tests>
        DEPENDS vic25_phase_e_tests)
endif()

# Phase G/H bounded integration into the existing C/D/E staged World.
# Does not represent the complete G(62) or H(176) acceptance engine.
if (BUILD_TESTING)
    add_executable(vic25_phase_gh_tests tests/test_phase_gh.cpp)
    target_link_libraries(vic25_phase_gh_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_gh_tests COMMAND vic25_phase_gh_tests)
    set_tests_properties(vic25_phase_gh_tests PROPERTIES TIMEOUT 120)
endif()

# Phase G extended real-ledger contracts / network / FX / insurance / collateral.
if (BUILD_TESTING)
    add_executable(vic25_phase_gh_commerce_tests tests/test_phase_gh_commerce.cpp)
    target_link_libraries(vic25_phase_gh_commerce_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_gh_commerce_tests COMMAND vic25_phase_gh_commerce_tests)
    set_tests_properties(vic25_phase_gh_commerce_tests PROPERTIES TIMEOUT 120)
endif()

# Phase H: contract-verified multilateral treaties and actual C/E/G integration.
if (BUILD_TESTING)
    add_executable(vic25_phase_gh_diplomacy_tests tests/test_phase_gh_diplomacy.cpp)
    target_link_libraries(vic25_phase_gh_diplomacy_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_gh_diplomacy_tests COMMAND vic25_phase_gh_diplomacy_tests)
    set_tests_properties(vic25_phase_gh_diplomacy_tests PROPERTIES TIMEOUT 120)
endif()

# Full C/D/E/G/H snapshot codec and 100-year staged long-run.
if (BUILD_TESTING)
    add_executable(vic25_phase_gh_snapshot_tests tests/test_phase_gh_snapshot.cpp)
    target_link_libraries(vic25_phase_gh_snapshot_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_gh_snapshot_tests COMMAND vic25_phase_gh_snapshot_tests)
    add_executable(vic25_phase_gh_longrun_tests tests/test_phase_gh_longrun.cpp)
    target_link_libraries(vic25_phase_gh_longrun_tests PRIVATE vic25_core)
    add_test(NAME vic25_phase_gh_longrun_tests COMMAND vic25_phase_gh_longrun_tests)
    set_tests_properties(vic25_phase_gh_longrun_tests PROPERTIES TIMEOUT 300)
endif()

# Optional real G/H 30k-province synthetic scaling benchmark. Separate from
# historical GIS accuracy, GPU framerate and Phase E 1m-POP scaling tests.
option(VIC25_GH_STRESS "Build GH synthetic scaling benchmark" OFF)
if (VIC25_GH_STRESS)
    add_executable(vic25_phase_gh_scaling bench/phase_gh_scaling.cpp)
    target_link_libraries(vic25_phase_gh_scaling PRIVATE vic25_core)
endif()


# G-D0(12) + G-T1..5(50) + H-D0(20) + H-D1..7(84) + H-V(72):
# 238 distinct acceptance IDs, not a blanket PASS.
# Exit0 is an individual real C++ oracle; 77 means BLOCKED (CTest SKIPPED).
if (BUILD_TESTING)
    add_executable(vic25_phase_gh_acceptance tests/test_phase_gh_238.cpp)
    target_link_libraries(vic25_phase_gh_acceptance PRIVATE vic25_core)
    foreach(prefix IN ITEMS GD0 HD0)
        if(prefix STREQUAL "GD0")
            set(max_case 12)
        else()
            set(max_case 20)
        endif()
        foreach(i RANGE 1 ${max_case})
            if(i LESS 10)
                set(n "0${i}")
            else()
                set(n "${i}")
            endif()
            set(case_id "${prefix}-T${n}")
            add_test(NAME "vic25_gh_accept_${case_id}"
                     COMMAND vic25_phase_gh_acceptance "${case_id}")
            set_tests_properties("vic25_gh_accept_${case_id}"
                PROPERTIES SKIP_RETURN_CODE 77 LABELS "phase-gh-238")
        endforeach()
    endforeach()
    foreach(group RANGE 1 5)
        foreach(i RANGE 1 10)
            if(i LESS 10)
                set(n "0${i}")
            else()
                set(n "${i}")
            endif()
            set(case_id "GT${group}-T${n}")
            add_test(NAME "vic25_gh_accept_${case_id}"
                     COMMAND vic25_phase_gh_acceptance "${case_id}")
            set_tests_properties("vic25_gh_accept_${case_id}"
                PROPERTIES SKIP_RETURN_CODE 77 LABELS "phase-gh-238")
        endforeach()
    endforeach()
    foreach(group RANGE 1 7)
        foreach(i RANGE 1 12)
            if(i LESS 10)
                set(n "0${i}")
            else()
                set(n "${i}")
            endif()
            set(case_id "HD${group}-T${n}")
            add_test(NAME "vic25_gh_accept_${case_id}"
                     COMMAND vic25_phase_gh_acceptance "${case_id}")
            set_tests_properties("vic25_gh_accept_${case_id}"
                PROPERTIES SKIP_RETURN_CODE 77 LABELS "phase-gh-238")
        endforeach()
    endforeach()
    foreach(i RANGE 1 72)
        if(i LESS 10)
            set(n "0${i}")
        else()
            set(n "${i}")
        endif()
        set(case_id "HV-T${n}")
        add_test(NAME "vic25_gh_accept_${case_id}"
                 COMMAND vic25_phase_gh_acceptance "${case_id}")
        set_tests_properties("vic25_gh_accept_${case_id}"
            PROPERTIES SKIP_RETURN_CODE 77 LABELS "phase-gh-238")
    endforeach()
endif()

# EV-T10 historical geometry auditing runs OFFLINE. Shapely/PROJ are
# build-time QA dependencies only and never loaded into the C++ game runtime.
option(VIC25_ENABLE_EV10_GIS "Enable Python historical border source tests" OFF)
if (BUILD_TESTING)
    add_executable(vic25_phase_e_compact_bench bench/phase_e_30k.cpp)
    target_link_libraries(vic25_phase_e_compact_bench PRIVATE vic25_core Threads::Threads)
    foreach(COUNT IN ITEMS 10000 20000 30000)
        add_test(NAME vic25_ev10_compact_${COUNT} COMMAND vic25_phase_e_compact_bench ${COUNT})
        set_tests_properties(vic25_ev10_compact_${COUNT} PROPERTIES TIMEOUT 240)
    endforeach()
    if (VIC25_ENABLE_EV10_GIS)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME vic25_ev10_history_overlay COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_phase_e_ev10_overlay.py")
        set_tests_properties(vic25_ev10_history_overlay PROPERTIES TIMEOUT 240)
    endif()
endif()

# Daily tick v2 transitional C/D/E/G/H real-ledger adapter and migration.
# Explicitly separates daily commits from once-per-calendar-month settlement.
if (BUILD_TESTING)
    add_executable(vic25_daily_time_tests tests/test_daily_time.cpp)
    target_link_libraries(vic25_daily_time_tests PRIVATE vic25_core)
    add_test(NAME vic25_daily_time_tests COMMAND vic25_daily_time_tests)
    set_tests_properties(vic25_daily_time_tests PROPERTIES TIMEOUT 240)
endif()

# Offline legacy save -> daily v2 checkpoint CLI. Requires explicit source format
# and seed; cannot silently manufacture sub-month historical events.
add_executable(vic25_daily_migrate src/migrate_daily_save.cpp)
target_link_libraries(vic25_daily_migrate PRIVATE vic25_core)

# Native daily Domain B/C/D/G/H integration: real POP, production, work,
# exact-day transit and legally effective day-dated treaties.
if (BUILD_TESTING)
    add_executable(vic25_daily_native_tests tests/test_daily_native.cpp)
    target_link_libraries(vic25_daily_native_tests PRIVATE vic25_core Threads::Threads)
    add_test(NAME vic25_daily_native_tests COMMAND vic25_daily_native_tests)
    set_tests_properties(vic25_daily_native_tests PROPERTIES TIMEOUT 240)
endif()

# Synthetic 30k-Province / 100k-POP day benchmark is Linux-only,
# not a proof of full low-end GPU frametime or autonomous daily demographics.
if (BUILD_TESTING AND UNIX AND NOT APPLE)
    add_executable(vic25_daily_30k bench/daily_30k.cpp)
    target_link_libraries(vic25_daily_30k PRIVATE vic25_core)
    add_test(NAME vic25_daily_30k COMMAND vic25_daily_30k)
    set_tests_properties(vic25_daily_30k PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
endif()

# P0 deterministic daily grand-strategy slice: real C/D/E/G/H ledgers.
if(BUILD_TESTING)
    add_executable(vic25_p0_daily_tests tests/test_p0_daily.cpp)
    target_link_libraries(vic25_p0_daily_tests PRIVATE vic25_core)
    add_test(NAME vic25_p0_daily_tests COMMAND vic25_p0_daily_tests)
endif()

if(BUILD_TESTING)
    add_executable(vic25_p0_daily_longrun tests/test_p0_daily_longrun.cpp)
    target_link_libraries(vic25_p0_daily_longrun PRIVATE vic25_core)
    add_test(NAME vic25_p0_daily_longrun COMMAND vic25_p0_daily_longrun)
endif()

# P4 party-system and institution-specific election schedule regressions.
if(BUILD_TESTING)
    add_executable(vic25_p4_governance_tests tests/test_p4_governance.cpp)
    target_link_libraries(vic25_p4_governance_tests PRIVATE vic25_core)
    add_test(NAME vic25_p4_governance_tests COMMAND vic25_p4_governance_tests)
endif()

# P2 legislative rules, statutory amendments and legal elected-office terms.
# Focused test executable is available; no CI/tests are run by this change.
if(BUILD_TESTING)
    add_executable(vic25_p2_governance_tests tests/test_p2_governance.cpp)
    target_link_libraries(vic25_p2_governance_tests PRIVATE vic25_core)
    add_test(NAME vic25_p2_governance_tests COMMAND vic25_p2_governance_tests)
endif()

# P2 remaining political-system integration: actual bicameral votes,
# interest-group amendment bargaining, named-person elected mandates and
# law-constrained economy/service/conscription/trade actions.
if(BUILD_TESTING)
    add_executable(vic25_p2_advanced_tests tests/test_p2_advanced.cpp)
    target_link_libraries(vic25_p2_advanced_tests PRIVATE vic25_core)
    add_test(NAME vic25_p2_advanced_tests COMMAND vic25_p2_advanced_tests)
endif()

# P3 and P5 opt-in daily politics: POP attitudes, institutional interest-group
# clout, movements, actual one-day strikes, unrest and army-backed civil cases.
if(BUILD_TESTING)
    add_executable(vic25_p3_p5_politics_tests tests/test_p3_p5_politics.cpp)
    target_link_libraries(vic25_p3_p5_politics_tests PRIVATE vic25_core)
    add_test(NAME vic25_p3_p5_politics_tests COMMAND vic25_p3_p5_politics_tests)
endif()

# P5-G regional guerrilla journal and C-backed irregular conflict adapter.
# Scenarios cover real military/population/stock, temporary market access,
# low-intensity skirmishes, conventional fronts and save/replay.
if(BUILD_TESTING)
    add_executable(vic25_p5_guerrilla_tests tests/test_p5_guerrilla.cpp)
    target_link_libraries(vic25_p5_guerrilla_tests PRIVATE vic25_core)
    add_test(NAME vic25_p5_guerrilla_tests COMMAND vic25_p5_guerrilla_tests)
endif()


# Victoria 3 1.15-style shared content runtime: events, decisions, scripted
# journal outcomes, country formation and lossless native daily save/rollback.
if(BUILD_TESTING)
    add_executable(vic25_content_tests tests/test_content.cpp)
    target_link_libraries(vic25_content_tests PRIVATE vic25_core)
    add_test(NAME vic25_content_tests COMMAND vic25_content_tests)
    set_tests_properties(vic25_content_tests PROPERTIES TIMEOUT 180)
endif()

# Bounded JSON content rulesets: no runtime scripting engine or shadow ledgers.
if(BUILD_TESTING)
    add_executable(vic25_content_json_tests tests/test_content_json.cpp)
    target_link_libraries(vic25_content_json_tests PRIVATE vic25_core)
    target_compile_definitions(vic25_content_json_tests PRIVATE VIC25_SOURCE_ROOT=\"${CMAKE_SOURCE_DIR}\")
    add_test(NAME vic25_content_json_tests COMMAND vic25_content_json_tests)
    set_tests_properties(vic25_content_json_tests PROPERTIES TIMEOUT 120)
endif()
