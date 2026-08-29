cmake_minimum_required(VERSION 3.16)
project(smartpill_host_tests C)

enable_testing()

add_executable(test_domain
    test_domain.c
    ../../components/pill_domain/schedule.c
    ../../components/pill_domain/dose.c
)
target_include_directories(test_domain PRIVATE ../../components/pill_domain/include)
target_compile_features(test_domain PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_domain PRIVATE /W4 /WX)
else()
    target_compile_options(test_domain PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME domain COMMAND test_domain)

add_executable(test_dose_controller
    test_dose_controller.c
    ../../components/dose_controller/dose_controller.c
    ../../components/pill_domain/schedule.c
    ../../components/pill_domain/dose.c
)
target_include_directories(test_dose_controller PRIVATE
    ../../components/dose_controller/include
    ../../components/pill_domain/include
)
target_compile_features(test_dose_controller PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_dose_controller PRIVATE /W4 /WX)
else()
    target_compile_options(test_dose_controller PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME dose_controller COMMAND test_dose_controller)

add_executable(test_board_contract
    test_board_contract.c
    ../../components/board_support/board_contract.c
)
target_include_directories(test_board_contract PRIVATE ../../components/board_support/include)
target_compile_features(test_board_contract PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_board_contract PRIVATE /W4 /WX)
else()
    target_compile_options(test_board_contract PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME board_contract COMMAND test_board_contract)

add_executable(test_app_policy
    test_app_policy.c
    ../../components/pill_app/clock_policy.c
    ../../components/pill_app/reminder_policy.c
)
target_include_directories(test_app_policy PRIVATE ../../components/pill_app/include)
target_compile_features(test_app_policy PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_app_policy PRIVATE /W4 /WX)
else()
    target_compile_options(test_app_policy PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME app_policy COMMAND test_app_policy)

add_executable(test_display_model
    test_display_model.c
    ../../components/pill_app/display_model.c
)
target_include_directories(test_display_model PRIVATE
    ../../components/pill_app/include
    ../../components/board_support/include
)
target_compile_features(test_display_model PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_display_model PRIVATE /W4 /WX)
else()
    target_compile_options(test_display_model PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME display_model COMMAND test_display_model)

add_executable(test_display_renderer
    test_display_renderer.c
    ../../components/board_support/display_renderer.c
)
target_include_directories(test_display_renderer PRIVATE ../../components/board_support/include)
target_compile_features(test_display_renderer PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_display_renderer PRIVATE /W4 /WX)
else()
    target_compile_options(test_display_renderer PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME display_renderer COMMAND test_display_renderer)

add_executable(test_rtc_decode
    test_rtc_decode.c
    ../../components/board_support/rtc_decode.c
)
target_include_directories(test_rtc_decode PRIVATE ../../components/board_support/include)
target_compile_features(test_rtc_decode PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_rtc_decode PRIVATE /W4 /WX)
else()
    target_compile_options(test_rtc_decode PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME rtc_decode COMMAND test_rtc_decode)

add_executable(test_web_validation
    test_web_validation.c
    ../../components/pill_web/web_validation.c
    ../../components/pill_web/schedule_json.c
    ../../components/pill_domain/schedule.c
    ../../.tools/esp-idf-v5.5.4/components/json/cJSON/cJSON.c
)
target_include_directories(test_web_validation PRIVATE
    ../../components/pill_web/include
    ../../components/pill_domain/include
    ../../.tools/esp-idf-v5.5.4/components/json/cJSON
)
target_compile_features(test_web_validation PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_web_validation PRIVATE /W4 /WX)
else()
    target_compile_options(test_web_validation PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME web_validation COMMAND test_web_validation)

add_executable(test_schedule_store
    test_schedule_store.c
    ../../components/schedule_store/schedule_store.c
    ../../components/pill_domain/schedule.c
)
target_include_directories(test_schedule_store PRIVATE
    ../../components/schedule_store/include
    ../../components/pill_domain/include
)
target_compile_features(test_schedule_store PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_schedule_store PRIVATE /W4 /WX)
else()
    target_compile_options(test_schedule_store PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME schedule_store COMMAND test_schedule_store)

add_executable(test_wav_reader
    test_wav_reader.c
    ../../components/pill_audio/wav_reader.c
)
target_include_directories(test_wav_reader PRIVATE ../../components/pill_audio/include)
target_compile_features(test_wav_reader PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_wav_reader PRIVATE /W4 /WX)
else()
    target_compile_options(test_wav_reader PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME wav_reader COMMAND test_wav_reader)

add_executable(test_audio_policy
    test_audio_policy.c
    ../../components/pill_audio/player.c
)
target_include_directories(test_audio_policy PRIVATE ../../components/pill_audio/include)
target_compile_features(test_audio_policy PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_audio_policy PRIVATE /W4 /WX)
else()
    target_compile_options(test_audio_policy PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME audio_policy COMMAND test_audio_policy)

add_executable(test_ai_transport_policy
    test_ai_transport_policy.c
    ../../components/pill_ai_transport/policy.c
)
target_include_directories(test_ai_transport_policy PRIVATE
    ../../components/pill_ai_transport/include
)
target_compile_features(test_ai_transport_policy PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_ai_transport_policy PRIVATE /W4 /WX)
else()
    target_compile_options(test_ai_transport_policy PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME ai_transport_policy COMMAND test_ai_transport_policy)

add_executable(test_ai_config_validation
    test_ai_config_validation.c
    ../../components/pill_ai_transport/config_validation.c
)
target_include_directories(test_ai_config_validation PRIVATE
    ../../components/pill_ai_transport/include
)
target_compile_features(test_ai_config_validation PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_ai_config_validation PRIVATE /W4 /WX)
else()
    target_compile_options(test_ai_config_validation PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME ai_config_validation COMMAND test_ai_config_validation)

add_executable(test_ai_wire_protocol
    test_ai_wire_protocol.c
    ../../components/pill_ai_transport/wire_protocol.c
    ../../.tools/esp-idf-v5.5.4/components/json/cJSON/cJSON.c
)
target_include_directories(test_ai_wire_protocol PRIVATE
    ../../components/pill_ai_transport/include
    ../../.tools/esp-idf-v5.5.4/components/json/cJSON
)
target_compile_features(test_ai_wire_protocol PRIVATE c_std_17)
if(MSVC)
    target_compile_options(test_ai_wire_protocol PRIVATE /W4 /WX)
else()
    target_compile_options(test_ai_wire_protocol PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME ai_wire_protocol COMMAND test_ai_wire_protocol)
