cmake_minimum_required(VERSION 3.22)

project(RealtimeFeedbackEngine VERSION 0.1.0 LANGUAGES CXX)

set(ASIO_SDK_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ASIO-SDK_2.3.4/ASIOSDK"
    CACHE PATH "Path to the official Steinberg ASIO SDK")

if (WIN32)
    if (NOT EXISTS "${ASIO_SDK_PATH}/common/iasiodrv.h")
        message(FATAL_ERROR
            "Official Steinberg ASIO SDK not found. Expected: ${ASIO_SDK_PATH}/common/iasiodrv.h")
    endif()

    message(STATUS "Using official Steinberg ASIO SDK: ${ASIO_SDK_PATH}")
endif()

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(RealtimeFeedbackEngine
    PRODUCT_NAME "RealtimeFeedbackEngine"
    COMPANY_NAME "RealtimeFeedbackEngine"
    VERSION "0.1.0"
)

juce_generate_juce_header(RealtimeFeedbackEngine)

target_sources(RealtimeFeedbackEngine PRIVATE
    Source/main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/AudioEngine.h
    Source/AudioEngine.cpp
    Source/Processors/IProcessor.h
    Source/Processors/PassthroughProcessor.h
    Source/Processors/PassthroughProcessor.cpp
    Source/Config/AudioConfig.h
)

target_compile_features(RealtimeFeedbackEngine PRIVATE cxx_std_20)

target_compile_definitions(RealtimeFeedbackEngine PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if (WIN32)
    target_compile_definitions(RealtimeFeedbackEngine PRIVATE
        JUCE_ASIO=1
    )

    target_include_directories(RealtimeFeedbackEngine PRIVATE
        "${ASIO_SDK_PATH}/common"
    )
endif()

target_link_libraries(RealtimeFeedbackEngine PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
)
