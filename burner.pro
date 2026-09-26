#-------------------------------------------------
# Burner - CD/DVD Burning Application
# Qt Project File
#-------------------------------------------------

QT       += core gui widgets

TARGET = burner
TEMPLATE = app

# C++20 support
CONFIG += c++20
QMAKE_CXXFLAGS += -std=c++20

# Warn on deprecated Qt features
DEFINES += QT_DEPRECATED_WARNINGS

# The release version lives in the VERSION file
BURNER_VERSION = $$cat($$PWD/VERSION)
DEFINES += BURNER_VERSION=\\\"$$BURNER_VERSION\\\"

#-------------------------------------------------
# Source files
#-------------------------------------------------

SOURCES += \
    src/main.cpp \
    # Core library
    src/core/libburnia/BurniaInit.cpp \
    src/core/libburnia/DriveHandle.cpp \
    src/core/libburnia/IsoImageHandle.cpp \
    src/core/DriveManager.cpp \
    src/core/ImageBuilder.cpp \
    src/core/BurnEngine.cpp \
    src/core/AudioEncoder.cpp \
    src/core/AudioOutputEncoder.cpp \
    src/core/BurnLog.cpp \
    src/core/CdReader.cpp \
    src/core/CdRipper.cpp \
    src/core/DiscCloner.cpp \
    src/core/FileNamingPattern.cpp \
    src/core/MetadataProvider.cpp \
    src/core/VcdBuilder.cpp \
    # Engine library
    src/engine/QtBurnEngine.cpp \
    src/engine/QtRipEngine.cpp \
    src/engine/DriveMonitor.cpp \
    src/engine/BurnWorker.cpp \
    src/engine/RipWorker.cpp \
    # Models library
    src/models/DriveListModel.cpp \
    src/models/FileTreeModel.cpp \
    src/models/TrackListModel.cpp \
    src/models/CdTrackModel.cpp \
    # UI library
    src/ui/MainWindow.cpp \
    src/ui/Dialogs.cpp \
    src/ui/widgets/TitleBar.cpp \
    src/ui/widgets/WindowChrome.cpp \
    src/ui/widgets/DriveSelector.cpp \
    src/ui/widgets/CapacityBar.cpp \
    src/ui/widgets/FileDropArea.cpp \
    src/ui/pages/DataDiscPage.cpp \
    src/ui/pages/AudioCdPage.cpp \
    src/ui/pages/VideoCdPage.cpp \
    src/ui/pages/IsoWriterPage.cpp \
    src/ui/pages/DiscClonerPage.cpp \
    src/ui/pages/CdRipperPage.cpp \
    src/ui/dialogs/BurnOptionsDialog.cpp \
    src/ui/dialogs/BurnProgressDialog.cpp \
    src/ui/dialogs/RipProgressDialog.cpp

#-------------------------------------------------
# Header files
#-------------------------------------------------

HEADERS += \
    # Core library
    src/core/libburnia/BurniaInit.hpp \
    src/core/libburnia/DriveHandle.hpp \
    src/core/libburnia/IsoImageHandle.hpp \
    src/core/DriveManager.hpp \
    src/core/DriveInfo.hpp \
    src/core/ImageBuilder.hpp \
    src/core/BurnEngine.hpp \
    src/core/AudioEncoder.hpp \
    src/core/AudioOutputEncoder.hpp \
    src/core/FFmpegCompat.hpp \
    src/core/BurnLog.hpp \
    src/core/CdReader.hpp \
    src/core/CdRipper.hpp \
    src/core/DiscCloner.hpp \
    src/core/FileNamingPattern.hpp \
    src/core/MetadataProvider.hpp \
    src/core/VcdBuilder.hpp \
    src/core/third_party/json.hpp \
    # Engine library
    src/engine/QtBurnEngine.hpp \
    src/engine/QtRipEngine.hpp \
    src/engine/DriveMonitor.hpp \
    src/engine/BurnWorker.hpp \
    src/engine/RipWorker.hpp \
    # Models library
    src/models/DriveListModel.hpp \
    src/models/FileTreeModel.hpp \
    src/models/TrackListModel.hpp \
    src/models/CdTrackModel.hpp \
    # UI library
    src/ui/MainWindow.hpp \
    src/ui/Dialogs.hpp \
    src/ui/widgets/TitleBar.hpp \
    src/ui/widgets/WindowChrome.hpp \
    src/ui/widgets/DriveSelector.hpp \
    src/ui/widgets/CapacityBar.hpp \
    src/ui/widgets/FileDropArea.hpp \
    src/ui/pages/DataDiscPage.hpp \
    src/ui/pages/AudioCdPage.hpp \
    src/ui/pages/VideoCdPage.hpp \
    src/ui/pages/IsoWriterPage.hpp \
    src/ui/pages/DiscClonerPage.hpp \
    src/ui/pages/CdRipperPage.hpp \
    src/ui/dialogs/BurnOptionsDialog.hpp \
    src/ui/dialogs/BurnProgressDialog.hpp \
    src/ui/dialogs/RipProgressDialog.hpp

#-------------------------------------------------
# Include paths
#-------------------------------------------------

INCLUDEPATH += \
    src \
    src/core \
    src/engine \
    src/models \
    src/ui

#-------------------------------------------------
# External libraries
#-------------------------------------------------

# Libburnia libraries (libburn, libisofs, libisoburn)
CONFIG += link_pkgconfig
PKGCONFIG += libburn-1 libisofs-1 libisoburn-1

# FFmpeg libraries
PKGCONFIG += libavcodec libavformat libavutil libswresample libswscale

# MusicBrainz metadata lookup (libcurl) and disc ID calculation (OpenSSL)
PKGCONFIG += libcurl openssl
DEFINES += HAVE_LIBCURL

#-------------------------------------------------
# Platform-specific settings
#-------------------------------------------------

unix:!macx {
    # Linux-specific settings
    LIBS += -lpthread
}

macx {
    # macOS-specific settings
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
}

win32 {
    # Windows-specific settings
    # Adjust library paths as needed for Windows builds
}

#-------------------------------------------------
# Output directories
#-------------------------------------------------

# Build output directory
CONFIG(debug, debug|release) {
    DESTDIR = build/debug
    OBJECTS_DIR = build/debug/.obj
    MOC_DIR = build/debug/.moc
    RCC_DIR = build/debug/.rcc
} else {
    DESTDIR = build/release
    OBJECTS_DIR = build/release/.obj
    MOC_DIR = build/release/.moc
    RCC_DIR = build/release/.rcc
}

#-------------------------------------------------
# Translations
#-------------------------------------------------

TRANSLATIONS += \
    translations/burner_en.ts \
    translations/burner_es.ts \
    translations/burner_fr.ts \
    translations/burner_de.ts \
    translations/burner_it.ts \
    translations/burner_pt.ts \
    translations/burner_ru.ts \
    translations/burner_zh_CN.ts \
    translations/burner_zh_TW.ts \
    translations/burner_ja.ts \
    translations/burner_ko.ts \
    translations/burner_ar.ts \
    translations/burner_nl.ts \
    translations/burner_pl.ts \
    translations/burner_tr.ts

# Generate .qm files from .ts files
CONFIG += lrelease embed_translations

#-------------------------------------------------
# Resources
#-------------------------------------------------

RESOURCES += resources/resources.qrc

#-------------------------------------------------
# Install targets
#-------------------------------------------------

unix:!macx {
    target.path = /usr/local/bin
    INSTALLS += target

    desktop.path = /usr/share/applications
    desktop.files = resources/burner.desktop
    INSTALLS += desktop

    icon.path = /usr/share/icons/hicolor/256x256/apps
    icon.files = resources/icons/burner.png
    INSTALLS += icon
}
