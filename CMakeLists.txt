cmake_minimum_required(VERSION 3.24)

project(MikuLibrary VERSION 1.0.2 LANGUAGES CXX)
# Windows, macOS and Linux. See docs/development.md for what differs between them.
if (WIN32)
    enable_language(RC)
elseif (APPLE)
    enable_language(OBJCXX)
endif()
# The single source of the version shown in About, the user agent and packages.
add_compile_definitions(MIKU_LIBRARY_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Sql Concurrent Qml Quick)
# Windows talks to the shell directly; elsewhere the tray and file pickers come from Widgets.
if (NOT WIN32)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
endif()
# Libraries only Windows has.
if (WIN32)
    set(MIKU_WIN32_NET iphlpapi)
else()
    set(MIKU_WIN32_NET)
endif()
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
    LibtorrentRasterbar::torrent-rasterbar ${MIKU_WIN32_NET})

# What QML talks to: controllers and models, without the window itself.
add_library(MikuApp STATIC
    src/app/AppController.cpp
    src/app/BrowseControllers.cpp
    src/app/DetailController.cpp
    src/app/DialogHub.cpp
    src/app/DownloadsController.cpp
    src/app/Format.cpp
    src/app/ImageProviders.cpp
    src/app/LanguageController.cpp
    src/app/MapListModel.cpp
    src/app/MpvVideoItem.cpp
    src/app/PlayerController.cpp
    src/app/SettingsController.cpp
    src/app/SitesController.cpp
    src/app/StartupFlow.cpp
    src/app/ThemeController.cpp
    src/app/WindowChrome.cpp
)
target_link_libraries(MikuApp PUBLIC MikuCore Qt6::Qml Qt6::Quick)
if (WIN32)
    target_sources(MikuApp PRIVATE src/app/NativeDialogs.cpp src/app/TrayIcon.cpp)
    target_link_libraries(MikuApp PUBLIC dwmapi d3d11)
else()
    # The header is named here so AUTOMOC finds it without a TrayIcon.cpp beside it.
    target_sources(MikuApp PRIVATE src/app/NativeDialogs_qt.cpp src/app/TrayIcon_qt.cpp src/app/TrayIcon.h)
    target_link_libraries(MikuApp PUBLIC Qt6::Widgets)
endif()
if (APPLE)
    target_sources(MikuApp PRIVATE src/app/WindowChrome_mac.mm)
    target_link_libraries(MikuApp PUBLIC "-framework AppKit")
endif()
# Interface translations, keyed by the Simplified Chinese text (see docs/development.md).
qt_add_resources(MikuApp "translations" PREFIX "/i18n" BASE i18n FILES i18n/zh-TW.json i18n/en.json i18n/ja.json)
target_include_directories(MikuApp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/include)
# The player loads libmpv at run time. Windows: libmpv-2.dll beside the program; it is too
# large for the repository, see docs/development.md for where to get it. macOS and Linux
# use the system's libmpv (Homebrew, the distribution's package) unless one is bundled.
if (WIN32)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/libmpv-2.dll")
        file(COPY third_party/mpv/libmpv-2.dll DESTINATION ${CMAKE_BINARY_DIR})
    else()
        message(WARNING "third_party/mpv/libmpv-2.dll is missing: the built-in player will be unavailable.")
    endif()
endif()
file(COPY third_party/anime4k/ DESTINATION ${CMAKE_BINARY_DIR}/shaders/anime4k)
# Motion interpolation for the Windows player (tools/assemble_vapoursynth.py builds it).
if (WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vapoursynth")
    file(COPY third_party/vapoursynth/ DESTINATION ${CMAKE_BINARY_DIR}/vapoursynth)
endif()

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
        qml/ResizeEdges.qml
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
if (WIN32)
    # Icon plus the version shown in the exe's file properties.
    configure_file(assets/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    qt_add_executable(MikuLibrary WIN32 src/main.cpp ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
elseif (APPLE)
    # A bundle: the site catalog and shaders go in Contents/Resources (AppConfig::resourcePath).
    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    qt_add_executable(MikuLibrary MACOSX_BUNDLE src/main.cpp)
    set_target_properties(MikuLibrary PROPERTIES
        OUTPUT_NAME "Miku Library"
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.magician0711.mikulibrary
        MACOSX_BUNDLE_BUNDLE_NAME "Miku Library"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/assets/macos/Info.plist.in)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/macos/AppIcon.icns")
        target_sources(MikuLibrary PRIVATE assets/macos/AppIcon.icns)
        set_source_files_properties(assets/macos/AppIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endif()
else()
    qt_add_executable(MikuLibrary src/main.cpp)
    set_target_properties(MikuLibrary PROPERTIES OUTPUT_NAME miku-library)
endif()
target_link_libraries(MikuLibrary PRIVATE MikuApp MikuQmlplugin)

if (MSVC)
    target_compile_options(MikuCore PUBLIC /utf-8 /W3 /permissive-)
endif()

if (APPLE)
    set(MIKU_RESOURCE_DIR $<TARGET_BUNDLE_CONTENT_DIR:MikuLibrary>/Resources)
else()
    set(MIKU_RESOURCE_DIR $<TARGET_FILE_DIR:MikuLibrary>)
endif()
add_custom_command(TARGET MikuLibrary POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            ${MIKU_RESOURCE_DIR}/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/anime4k
            ${MIKU_RESOURCE_DIR}/shaders/anime4k
    COMMENT "Copying site catalog and shaders"
)

add_executable(DownloadSmoke tests/download_smoke.cpp src/net/TorrentEngine.cpp src/net/AnimeDownloads.cpp src/net/ExternalDownloader.cpp src/net/DownloaderRpc.cpp
    src/core/LocalLibrary.cpp src/core/Grouping.cpp
    src/core/AppConfig.cpp src/core/EngineSettings.cpp src/core/Models.cpp src/core/TitleParser.cpp)
target_include_directories(DownloadSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(DownloadSmoke PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent
    LibtorrentRasterbar::torrent-rasterbar ${MIKU_WIN32_NET})
if (MSVC)
    target_compile_options(DownloadSmoke PRIVATE /utf-8)
endif()

add_executable(DirectorySmoke tests/directory_smoke.cpp src/core/AppConfig.cpp src/core/EngineSettings.cpp)
target_include_directories(DirectorySmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(DirectorySmoke PRIVATE Qt6::Core)
if (MSVC)
    target_compile_options(DirectorySmoke PRIVATE /utf-8)
endif()

if (WIN32)
    install(TARGETS MikuLibrary RUNTIME DESTINATION .)
elseif (APPLE)
    install(TARGETS MikuLibrary BUNDLE DESTINATION .)
else()
    # bin/miku-library, with what it ships under share/miku-library (AppConfig::resourcePath).
    include(GNUInstallDirs)
    install(TARGETS MikuLibrary RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY config DESTINATION ${CMAKE_INSTALL_DATADIR}/miku-library)
    install(DIRECTORY third_party/anime4k/ DESTINATION ${CMAKE_INSTALL_DATADIR}/miku-library/shaders/anime4k)
    install(FILES assets/linux/miku-library.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/logo.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/1024x1024/apps RENAME miku-library.png)
endif()

add_executable(DetailLibrarySmoke tests/detail_library_smoke.cpp)
target_link_libraries(DetailLibrarySmoke PRIVATE MikuApp)
add_executable(SettingsLimitsSmoke tests/settings_limits_smoke.cpp)
target_link_libraries(SettingsLimitsSmoke PRIVATE MikuApp)
add_executable(QuickUiSmoke tests/quick_ui_smoke.cpp)
# Qt Test sends clicks through the same path as real input, which pointer handlers need.
find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
target_link_libraries(QuickUiSmoke PRIVATE MikuApp MikuQmlplugin Qt6::Test)
# Records the interface frame by frame for release videos; nobody else needs it.
option(MIKU_BUILD_SHOWREEL_CAPTURE "Build the frame-by-frame interface recorder" OFF)
if (MIKU_BUILD_SHOWREEL_CAPTURE)
    add_executable(ShowreelCapture tools/showreel_capture.cpp)
    target_link_libraries(ShowreelCapture PRIVATE MikuApp MikuQmlplugin Qt6::Concurrent)
endif()
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
target_link_libraries(RpcSmoke PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent LibtorrentRasterbar::torrent-rasterbar ${MIKU_WIN32_NET})
if(MSVC)
    target_compile_options(RpcSmoke PRIVATE /utf-8)
endif()
target_link_libraries(ExternalClientProbe PRIVATE Qt6::Core)
add_executable(ExternalDownloaderSmoke tests/external_downloader_smoke.cpp src/net/ExternalDownloader.cpp src/net/DownloaderRpc.cpp src/net/TorrentEngine.cpp src/net/AnimeDownloads.cpp
    src/core/LocalLibrary.cpp src/core/Grouping.cpp src/core/AppConfig.cpp src/core/EngineSettings.cpp src/core/Models.cpp src/core/TitleParser.cpp)
target_include_directories(ExternalDownloaderSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ExternalDownloaderSmoke PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent LibtorrentRasterbar::torrent-rasterbar ${MIKU_WIN32_NET})
add_dependencies(ExternalDownloaderSmoke ExternalClientProbe)
if(MSVC)
    target_compile_options(ExternalDownloaderSmoke PRIVATE /utf-8)
endif()
