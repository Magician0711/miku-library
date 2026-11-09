cmake_minimum_required(VERSION 3.24)

project(MikuLibrary VERSION 1.0.0 LANGUAGES CXX RC)
# The single source of the version shown in About, the user agent and packages.
add_compile_definitions(MIKU_LIBRARY_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Sql Concurrent Qml Quick)
find_package(LibtorrentRasterbar CONFIG REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

# Probe the actual /showIncludes bytes: some MSVC installations ignore VSLANG
# when the English language pack is absent. A wrong prefix silently loses deps.
if (MSVC)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;VSLANG=1033")
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    file(WRITE "${CMAKE_BINARY_DIR}/MikuIncludeProbe.h" "// dependency probe\n")
    file(WRITE "${CMAKE_BINARY_DIR}/MikuIncludeProbe.cpp" "#include \"MikuIncludeProbe.h\"\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env VSLANG=1033
        "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /c
        "${CMAKE_BINARY_DIR}/MikuIncludeProbe.cpp"
        "/Fo${CMAKE_BINARY_DIR}/MikuIncludeProbe.obj"
        OUTPUT_VARIABLE _include_output ERROR_VARIABLE _include_error ENCODING UTF8)
    string(REPLACE "\n" ";" _include_lines "${_include_output}\n${_include_error}")
    foreach(_line IN LISTS _include_lines)
        if (_line MATCHES "^(.*):[ \t]+[A-Za-z]:[/\\\\].*MikuIncludeProbe[.]h")
            string(STRIP "${CMAKE_MATCH_1}:" CMAKE_CL_SHOWINCLUDES_PREFIX)
        endif()
    endforeach()
endif()

add_executable(OptimizationSmoke tests/optimization_smoke.cpp src/core/Catalog.cpp src/core/Models.cpp src/core/Grouping.cpp src/core/TitleParser.cpp
    src/net/HttpClient.cpp src/net/BangumiClient.cpp src/net/FeedParser.cpp src/net/Providers.cpp src/net/SearchAggregator.cpp
    src/net/AppUpdate.cpp)
target_include_directories(OptimizationSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(OptimizationSmoke PRIVATE Qt6::Core Qt6::Network)
if(MSVC)
    target_compile_options(OptimizationSmoke PRIVATE /utf-8)
endif()

# Everything below the interface, shared by the app and the tests.
add_library(MikuCore STATIC
    src/core/Models.cpp
    src/core/EngineSettings.cpp
    src/core/AppConfig.cpp
    src/core/Backup.cpp
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
    src/net/AppUpdate.cpp
    src/net/ImageLoader.cpp
)
target_include_directories(MikuCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(MikuCore PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Concurrent
    LibtorrentRasterbar::torrent-rasterbar iphlpapi)

# What QML talks to: controllers and models, without the window itself.
add_library(MikuApp STATIC
    src/app/AppController.cpp
    src/app/BrowseControllers.cpp
    src/app/DetailController.cpp
    src/app/DialogHub.cpp
    src/app/DownloadsController.cpp
    src/app/Format.cpp
    src/app/ImageProviders.cpp
    src/app/MapListModel.cpp
    src/app/MpvVideoItem.cpp
    src/app/NativeDialogs.cpp
    src/app/PlayerController.cpp
    src/app/SettingsController.cpp
    src/app/SitesController.cpp
    src/app/StartupFlow.cpp
    src/app/ThemeController.cpp
    src/app/TrayIcon.cpp
    src/app/WindowChrome.cpp
)
target_link_libraries(MikuApp PUBLIC MikuCore Qt6::Qml Qt6::Quick dwmapi d3d11)
target_include_directories(MikuApp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/include)
# The player loads these from beside the program.
# libmpv is too large for the repository; see docs/development.md for where to get it.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/libmpv-2.dll")
    file(COPY third_party/mpv/libmpv-2.dll DESTINATION ${CMAKE_BINARY_DIR})
else()
    message(WARNING "third_party/mpv/libmpv-2.dll is missing: the built-in player will be unavailable.")
endif()
file(COPY third_party/anime4k/ DESTINATION ${CMAKE_BINARY_DIR}/shaders/anime4k)

qt_add_library(MikuQml STATIC)
set_source_files_properties(qml/T.qml qml/TipHub.qml qml/Shell.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(MikuQml
    URI MikuLibrary
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml_module/MikuLibrary
    QML_FILES
        qml/T.qml
        qml/PlayerPage.qml
        qml/NxSeek.qml
        qml/HeroInfo.qml
        qml/HeroPoster.qml
        qml/Glass.qml
        qml/Backdrop.qml
        qml/Scaffold.qml
        qml/Group.qml
        qml/FocusRing.qml
        qml/Shell.qml
        qml/TipHub.qml
        qml/Tip.qml
        qml/TipLayer.qml
        qml/Shadow.qml
        qml/Icon.qml
        qml/Spinner.qml
        qml/Dot.qml
        qml/Chip.qml
        qml/Divider.qml
        qml/NxButton.qml
        qml/IconButton.qml
        qml/NxSwitch.qml
        qml/NxCheck.qml
        qml/NxField.qml
        qml/NxScrollBar.qml
        qml/SmoothWheel.qml
        qml/NxSelect.qml
        qml/NxSpin.qml
        qml/Segmented.qml
        qml/NxMenu.qml
        qml/NxMenuItem.qml
        qml/EmptyState.qml
        qml/PosterCard.qml
        qml/PosterGrid.qml
        qml/ThemeFade.qml
        qml/WindowButtons.qml
        qml/Main.qml
        qml/HomePage.qml
        qml/SearchPage.qml
        qml/LibraryPage.qml
        qml/DownloadsPage.qml
        qml/SitesPage.qml
        qml/UpdatePanel.qml
        qml/Toast.qml
        qml/DownloaderPopup.qml
        qml/NxDialog.qml
        qml/DialogHost.qml
        qml/BodyText.qml
        qml/GroupDownloadDialog.qml
        qml/DetailPage.qml
        qml/SettingCard.qml
        qml/SettingRow.qml
        qml/SettingsPage.qml
        qml/StartupWindow.qml
    RESOURCES
        assets/logo.png
)
qt_add_executable(MikuLibrary WIN32 src/main.cpp assets/app.rc)
target_link_libraries(MikuLibrary PRIVATE MikuApp MikuQmlplugin)

if (MSVC)
    target_compile_options(MikuCore PUBLIC /utf-8 /W3 /permissive-)
endif()

add_custom_command(TARGET MikuLibrary POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:MikuLibrary>/config
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

add_executable(DirectorySmoke tests/directory_smoke.cpp src/core/AppConfig.cpp src/core/EngineSettings.cpp)
target_include_directories(DirectorySmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(DirectorySmoke PRIVATE Qt6::Core)
if (MSVC)
    target_compile_options(DirectorySmoke PRIVATE /utf-8)
endif()

install(TARGETS MikuLibrary RUNTIME DESTINATION .)

add_executable(DetailLibrarySmoke tests/detail_library_smoke.cpp)
target_link_libraries(DetailLibrarySmoke PRIVATE MikuApp)
add_executable(SettingsLimitsSmoke tests/settings_limits_smoke.cpp)
target_link_libraries(SettingsLimitsSmoke PRIVATE MikuApp)
add_executable(QuickUiSmoke tests/quick_ui_smoke.cpp)
target_link_libraries(QuickUiSmoke PRIVATE MikuApp MikuQmlplugin)
add_executable(LocalLibrarySmoke tests/local_library_smoke.cpp src/core/LocalLibrary.cpp
    src/core/Models.cpp src/core/TitleParser.cpp src/core/Grouping.cpp src/core/Backup.cpp)
target_include_directories(LocalLibrarySmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(LocalLibrarySmoke PRIVATE Qt6::Core)
if (MSVC)
    target_compile_options(LocalLibrarySmoke PRIVATE /utf-8)
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
