THEMENAME = sailfish-default

TARGET = screentapshot-lipstick

QT += dbus gui-private
CONFIG += sailfishapp wayland-scanner link_pkgconfig
PKGCONFIG += \
    mlite5 \
    wayland-client \
# PKGCONFIG end

WAYLANDCLIENTSOURCES += protocol/lipstick-recorder.xml

LIBS += -lsailfishapp

DEFINES += APP_VERSION=\\\"$$VERSION\\\"

images.files = images
images.path = /usr/share/$$TARGET

INSTALLS += images

SOURCES += \
    src/screenshot.cpp \
    src/viewhelper.cpp \
    src/main.cpp

HEADERS += \
    src/viewhelper.h \
    src/screenshot.h

OTHER_FILES += \
    protocol/lipstick-recorder.xml \
    rpm/screentapshot-lipstick.spec \
    screentapshot-lipstick.desktop \
    qml/overlay.qml \
    qml/settings.qml \
    qml/CoverPage.qml \
    qml/MainPage.qml \
    qml/AboutPage.qml \
    translations/*.ts

privileges.files = privileges/screentapshot-lipstick
privileges.path = /usr/share/mapplauncherd/privileges.d
INSTALLS += privileges

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG += sailfishapp_i18n

TRANSLATIONS += \
    translations/screentapshot-lipstick.ts \
    translations/screentapshot-lipstick-es.ts \
    translations/screentapshot-lipstick-fi.ts \
    translations/screentapshot-lipstick-pl.ts \
    translations/screentapshot-lipstick-pt_BR.ts \
    translations/screentapshot-lipstick-pt.ts \
    translations/screentapshot-lipstick-ru.ts \
    translations/screentapshot-lipstick-sv.ts \
    translations/screentapshot-lipstick-zh_CN.ts

appicon.profiles = \
    86x86 \
    108x108 \
    128x128 \
    256x256

appicon.scales.86x86 = 1.0
appicon.scales.108x108 = 1.25
appicon.scales.128x128 = 1.49
appicon.scales.256x256 = 2.97

for(profile, appicon.profiles) {
    system(mkdir -p $${OUT_PWD}/$${profile})

    appicon.commands += /usr/bin/sailfish_svg2png \
        -z $$eval(appicon.scales.$${profile}) \
         $${_PRO_FILE_PWD_}/appicon \
         $${profile}/apps/ &&

    appicon.files += $${profile}
}
appicon.commands += true
appicon.path = /usr/share/icons/hicolor/

INSTALLS += appicon

CONFIG += sailfish-svg2png
