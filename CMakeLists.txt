cmake_minimum_required(VERSION 3.24)

project(AnimeNexusCpp VERSION 0.6.0 LANGUAGES CXX)
# The single source of the version shown in About, the user agent and packages.
add_compile_definitions(ANIME_NEXUS_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network Sql Concurrent)
find_package(LibtorrentRasterbar CONFIG REQUIRED)
qt_standard_project_setup()

# Probe the actual /showIncludes bytes: some MSVC installations ignore VSLANG
# when the English language pack is absent. A wrong prefix silently loses deps.
if (MSVC)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;VSLANG=1033")
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    file(WRITE "${CMAKE_BINARY_DIR}/NexusIncludeProbe.h" "// dependency probe\n")
    file(WRITE "${CMAKE_BINARY_DIR}/NexusIncludeProbe.cpp" "#include \"NexusIncludeProbe.h\"\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env VSLANG=1033
        "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /c
        "${CMAKE_BINARY_DIR}/NexusIncludeProbe.cpp"
        "/Fo${CMAKE_BINARY_DIR}/NexusIncludeProbe.obj"
        OUTPUT_VARIABLE _include_output ERROR_VARIABLE _include_error ENCODING UTF8)
    string(REPLACE "\n" ";" _include_lines "${_include_output}\n${_include_error}")
    foreach(_line IN LISTS _include_lines)
        if (_line MATCHES "^(.*):[ \t]+[A-Za-z]:[/\\\\].*NexusIncludeProbe[.]h")
            string(STRIP "${CMAKE_MATCH_1}:" CMAKE_CL_SHOWINCLUDES_PREFIX)
        endif()
    endforeach()
endif()

add_executable(OptimizationSmoke tests/optimization_smoke.cpp src/core/Catalog.cpp src/core/Models.cpp src/core/Grouping.cpp src/core/TitleParser.cpp
    src/net/HttpClient.cpp src/net/BangumiClient.cpp src/net/FeedParser.cpp src/net/Providers.cpp src/net/SearchAggregator.cpp)
target_include_directories(OptimizationSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(OptimizationSmoke PRIVATE Qt6::Core Qt6::Network)
if(MSVC)
    target_compile_options(OptimizationSmoke PRIVATE /utf-8)
endif()

qt_add_executable(AnimeNexus WIN32
    src/main.cpp

    src/core/Models.cpp
    src/core/EngineSettings.cpp
    src/core/AppConfig.cpp
    src/core/Catalog.cpp
    src/core/TitleParser.cpp
    src/core/Grouping.cpp
    src/core/LocalLibrary.cpp
    src/core/Database.cpp

    src/net/HttpClient.cpp
    src/net/BangumiClient.cpp
    src/net/FeedParser.cpp
    src/net/Providers.cpp
    src/net/SearchAggregator.cpp
    src/net/ExternalDownloader.cpp src/net/DownloaderRpc.cpp
    src/net/AnimeDownloads.cpp
    src/net/TorrentEngine.cpp
    src/net/UpdateChecker.cpp

    src/ui/Theme.cpp src/ui/Motion.cpp
    src/ui/Overlays.cpp
    src/ui/Controls.cpp
    src/ui/FlowLayout.cpp
    src/ui/Widgets.cpp
    src/ui/ImageLoader.cpp
    src/ui/MainWindow.cpp
    src/ui/UpdatePanel.cpp
    src/ui/FramelessWindow.cpp
    src/ui/pages/HomePage.cpp
    src/ui/pages/SearchPage.cpp
    src/ui/pages/LibraryPage.cpp
    src/ui/pages/DownloadsPage.cpp
    src/ui/pages/SitesPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/AnimeDetailPage.cpp
)

target_include_directories(AnimeNexus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)

set(UI_ICON_FILES assets/icons/check.png assets/icons/chevron-down.png
    assets/icons/chevron-up.png assets/icons/plus.png assets/icons/minus.png assets/icons/dash.png)
qt_add_resources(AnimeNexus "ui_icons" PREFIX "/" FILES ${UI_ICON_FILES})
target_link_libraries(AnimeNexus PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    LibtorrentRasterbar::torrent-rasterbar
    iphlpapi
    dwmapi
)

if (MSVC)
    target_compile_options(AnimeNexus PRIVATE /utf-8 /W3 /permissive-)
endif()

add_executable(WindowSmoke tests/window_smoke.cpp src/ui/FramelessWindow.cpp src/ui/Theme.cpp src/ui/Motion.cpp src/ui/Overlays.cpp)
qt_add_resources(WindowSmoke "window_ui_icons" PREFIX "/" FILES ${UI_ICON_FILES})
target_include_directories(WindowSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(WindowSmoke PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets dwmapi)
if (MSVC)
    target_compile_options(WindowSmoke PRIVATE /utf-8)
endif()

add_executable(ControlsSmoke tests/controls_smoke.cpp src/ui/Controls.cpp src/ui/Theme.cpp src/ui/Motion.cpp src/ui/Overlays.cpp)
target_include_directories(ControlsSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ControlsSmoke PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
qt_add_resources(ControlsSmoke "control_ui_icons" PREFIX "/" FILES ${UI_ICON_FILES})
if (MSVC)
    target_compile_options(ControlsSmoke PRIVATE /utf-8)
endif()

add_custom_command(TARGET AnimeNexus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:AnimeNexus>/config
    COMMENT "Copying site catalog"
)

add_executable(DownloadSmoke tests/download_smoke.cpp src/net/TorrentEngine.cpp src/net/AnimeDownloads.cpp src/net/ExternalDownloader.cpp src/net/DownloaderRpc.cpp
    src/core/LocalLibrary.cpp src/core/Grouping.cpp
    src/core/AppConfig.cpp src/core/EngineSettings.cpp src/core/Models.cpp src/core/TitleParser.cpp)
target_include_directories(DownloadSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(DownloadSmoke PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent
    LibtorrentRasterbar::torrent-rasterbar iphlpapi)
if (MSVC)
    target_compile_options(DownloadSmoke PRIVATE /utf-8)
endif()

add_executable(UiAuditSmoke tests/ui_audit_smoke.cpp src/ui/Overlays.cpp src/ui/Theme.cpp src/ui/Motion.cpp
    src/ui/Controls.cpp src/ui/Widgets.cpp src/ui/FlowLayout.cpp src/ui/ImageLoader.cpp
    src/net/HttpClient.cpp src/net/BangumiClient.cpp src/core/Models.cpp)
target_include_directories(UiAuditSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(UiAuditSmoke PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
qt_add_resources(UiAuditSmoke "audit_ui_icons" PREFIX "/" FILES ${UI_ICON_FILES})
if (MSVC)
    target_compile_options(UiAuditSmoke PRIVATE /utf-8)
endif()

add_executable(DirectorySmoke tests/directory_smoke.cpp src/core/AppConfig.cpp src/core/EngineSettings.cpp)
target_include_directories(DirectorySmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(DirectorySmoke PRIVATE Qt6::Core)
if (MSVC)
    target_compile_options(DirectorySmoke PRIVATE /utf-8)
endif()

add_executable(SearchInputSmoke tests/search_input_smoke.cpp src/ui/pages/SearchPage.cpp
    src/ui/Controls.cpp src/ui/Overlays.cpp src/ui/Theme.cpp src/ui/Motion.cpp src/ui/Widgets.cpp src/ui/FlowLayout.cpp
    src/ui/ImageLoader.cpp src/net/HttpClient.cpp src/net/BangumiClient.cpp src/core/Models.cpp)
target_include_directories(SearchInputSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(SearchInputSmoke PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
qt_add_resources(SearchInputSmoke "search_ui_icons" PREFIX "/" FILES ${UI_ICON_FILES})
if (MSVC)
    target_compile_options(SearchInputSmoke PRIVATE /utf-8)
endif()

install(TARGETS AnimeNexus RUNTIME DESTINATION .)
add_executable(LocalLibrarySmoke tests/local_library_smoke.cpp src/core/LocalLibrary.cpp
    src/core/Models.cpp src/core/TitleParser.cpp src/core/Grouping.cpp)
target_include_directories(LocalLibrarySmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(LocalLibrarySmoke PRIVATE Qt6::Core)
if (MSVC)
    target_compile_options(LocalLibrarySmoke PRIVATE /utf-8)
endif()
add_executable(DetailLibrarySmoke tests/detail_library_smoke.cpp src/ui/pages/AnimeDetailPage.cpp
    src/core/LocalLibrary.cpp src/core/Models.cpp src/core/TitleParser.cpp src/core/Grouping.cpp src/core/Catalog.cpp
    src/net/HttpClient.cpp src/net/BangumiClient.cpp src/net/SearchAggregator.cpp src/net/Providers.cpp src/net/FeedParser.cpp
    src/ui/Controls.cpp src/ui/Theme.cpp src/ui/Motion.cpp src/ui/Overlays.cpp src/ui/Widgets.cpp src/ui/FlowLayout.cpp src/ui/ImageLoader.cpp)
target_include_directories(DetailLibrarySmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(DetailLibrarySmoke PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
qt_add_resources(DetailLibrarySmoke "detail_ui_icons" PREFIX "/" FILES ${UI_ICON_FILES})
if (MSVC)
    target_compile_options(DetailLibrarySmoke PRIVATE /utf-8)
endif()
add_executable(SettingsLimitsSmoke tests/settings_limits_smoke.cpp src/ui/pages/SettingsPage.cpp
    src/ui/Controls.cpp src/ui/Overlays.cpp src/ui/Theme.cpp src/ui/Motion.cpp src/ui/Widgets.cpp src/ui/FlowLayout.cpp src/ui/ImageLoader.cpp
    src/net/HttpClient.cpp src/net/BangumiClient.cpp src/net/TorrentEngine.cpp src/net/ExternalDownloader.cpp src/net/DownloaderRpc.cpp
    src/core/LocalLibrary.cpp src/core/Grouping.cpp src/core/AppConfig.cpp src/core/EngineSettings.cpp src/core/Models.cpp src/core/TitleParser.cpp)
target_include_directories(SettingsLimitsSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(SettingsLimitsSmoke PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent
    LibtorrentRasterbar::torrent-rasterbar iphlpapi)
qt_add_resources(SettingsLimitsSmoke "settings_ui_icons" PREFIX "/" FILES ${UI_ICON_FILES})
if(MSVC)
    target_compile_options(SettingsLimitsSmoke PRIVATE /utf-8)
endif()
add_executable(ExternalClientProbe tests/external_client_probe.cpp)
add_executable(RpcSmoke tests/rpc_smoke.cpp src/net/DownloaderRpc.cpp src/net/ExternalDownloader.cpp src/net/TorrentEngine.cpp
    src/core/LocalLibrary.cpp src/core/Grouping.cpp src/core/AppConfig.cpp src/core/EngineSettings.cpp src/core/Models.cpp src/core/TitleParser.cpp)
target_include_directories(RpcSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(RpcSmoke PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent LibtorrentRasterbar::torrent-rasterbar iphlpapi)
if(MSVC)
    target_compile_options(RpcSmoke PRIVATE /utf-8)
endif()
target_link_libraries(ExternalClientProbe PRIVATE Qt6::Core)
add_executable(ExternalDownloaderSmoke tests/external_downloader_smoke.cpp src/net/ExternalDownloader.cpp src/net/DownloaderRpc.cpp src/net/TorrentEngine.cpp src/net/AnimeDownloads.cpp
    src/core/LocalLibrary.cpp src/core/Grouping.cpp src/core/AppConfig.cpp src/core/EngineSettings.cpp src/core/Models.cpp src/core/TitleParser.cpp)
target_include_directories(ExternalDownloaderSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ExternalDownloaderSmoke PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent LibtorrentRasterbar::torrent-rasterbar iphlpapi)
add_dependencies(ExternalDownloaderSmoke ExternalClientProbe)
if(MSVC)
    target_compile_options(ExternalDownloaderSmoke PRIVATE /utf-8)
endif()
