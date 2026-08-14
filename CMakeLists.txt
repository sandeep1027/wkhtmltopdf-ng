cmake_minimum_required(VERSION 3.16)
project(wkhtmltopdf-ng VERSION 0.13.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 6.2 builds work for local testing. Page-range printToPdf needs 6.8+.
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Widgets
    WebEngineCore
    WebEngineWidgets
    PrintSupport
    Network
)

add_library(wkhtmltopdf_core STATIC
    src/converter/HtmlToPdfConverter.cpp
    src/converter/HtmlToImageConverter.cpp
    src/converter/PageLayout.cpp
    src/converter/HeaderFooter.cpp
    src/converter/TocGenerator.cpp
    src/converter/PdfPostProcessor.cpp
    src/converter/Settings.cpp
    src/utils/ArgParser.cpp
    src/utils/FileUtils.cpp
    src/utils/JsBridge.cpp
)
set_target_properties(wkhtmltopdf_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(wkhtmltopdf_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(wkhtmltopdf_core PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
    Qt6::PrintSupport
    Qt6::Network
)

add_executable(wkhtmltopdf-ng src/main.cpp)
target_link_libraries(wkhtmltopdf-ng PRIVATE wkhtmltopdf_core)

add_executable(wkhtmltoimage-ng src/image/main.cpp)
target_link_libraries(wkhtmltoimage-ng PRIVATE wkhtmltopdf_core)

# C API shared library
add_library(wkhtmltox SHARED
    src/capi/wkhtmltox.h
    src/capi/shared.cpp
    src/capi/global_settings.cpp
    src/capi/object_settings.cpp
    src/capi/converter.cpp
    src/capi/image_converter.cpp
    src/capi/callbacks.cpp
)
target_include_directories(wkhtmltox PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(wkhtmltox PRIVATE WKHTMLTOX_BUILD)
target_link_libraries(wkhtmltox PRIVATE wkhtmltopdf_core)
set_target_properties(wkhtmltox PROPERTIES
    SOVERSION 0
    VERSION 0.13.1
    OUTPUT_NAME wkhtmltox
    POSITION_INDEPENDENT_CODE ON
)

# Install targets
install(TARGETS wkhtmltopdf-ng RUNTIME DESTINATION bin)
install(TARGETS wkhtmltoimage-ng RUNTIME DESTINATION bin)
install(TARGETS wkhtmltox
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES src/capi/wkhtmltox.h DESTINATION include/wkhtmltox)

# Enable testing
enable_testing()
add_subdirectory(tests)
