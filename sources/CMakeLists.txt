# Specify the minimum version of CMake 
cmake_minimum_required(VERSION 3.0)

# Specify the project name
project(KumoWorks VERSION 1.0.0)

set(MOC_HEADERS
    cloudlayerview.h
    cloudpresetcontrol.h
    colorgradientparam.h
    mainwindow.h
    mycontrol.h
    mygradientcontrol.h
    myparams.h
    mytoolbar.h
    myviewer.h
    progressview.h
    renderpopup.h
    renderworkerthread.h
    undomanager.h
	aboutpopup.h
)

set(HEADERS
    ${MOC_HEADERS}
    ArHosekSkyModel.h
    ArHosekSkyModelData_CIEXYZ.h
    ArHosekSkyModelData_RGB.h
    ArHosekSkyModelData_Spectral.h
    cloud.h
    simplexnoise.h
    myflowmanager.h
    pathutils.h
    randomnumbergenerator.h
    sky.h
    volume.h
)

set(SOURCES
    ArHosekSkyModel.cpp
    cloud.cpp
    cloudlayerview.cpp
    cloudpresetcontrol.cpp
    simplexnoise.cpp
    main.cpp
    mainwindow.cpp
    mycontrol.cpp
    mygradientcontrol.cpp
    myparams.cpp
    mytoolbar.cpp
    myviewer.cpp
    pathutils.cpp
    progressview.cpp
    renderpopup.cpp
    renderworkerthread.cpp
    sky.cpp
    undomanager.cpp
	aboutpopup.cpp
)

#-----------------------------------------------------------------------------
# Select build environment
set(BUILD_ENV_MSVC     OFF)
set(BUILD_ENV_APPLE    OFF)
if(MSVC)
    set(BUILD_ENV_MSVC ON)
    message(STATUS "Build environment is MSVC")
elseif(CMAKE_HOST_APPLE)
    set(BUILD_ENV_APPLE ON)
    message(STATUS "Build environment is APPLE")
else()
    message(ERROR "Unsupported build environment, only MSVC and APPLE are supported")
endif()
#-----------------------------------------------------------------------------

# specify qtdir
if(BUILD_ENV_MSVC)
    set(QTDIR $ENV{QTDIR} CACHE PATH "Qt installation directory")
    if(NOT EXISTS ${QTDIR})
        message("Specify QTDIR properly")
        return()
    endif()
elseif(BUILD_ENV_APPLE)
    set(QTDIR "~/Qt5.9.2/5.9.2/clang_64/" CACHE PATH "Qt installation directory")
endif()
set(CMAKE_PREFIX_PATH ${QTDIR})

if(BUILD_ENV_APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.10)
endif()

# Add Qt libraries. QtCore and QtGui libraries will be 
# added accordingly as the Widgets depends on them. 
find_package( Qt5Widgets REQUIRED )
find_package( Qt5LinguistTools REQUIRED )


# ビルドディレクトリを include ディレクトリに追加するオプションを有効にする。
set(CMAKE_INCLUDE_CURRENT_DIR ON)

qt5_wrap_cpp(SOURCES ${MOC_HEADERS})

# resource
set(QRC_FILE KumoWorks_resources.qrc)
qt5_add_resources(RESOURCES ${QRC_FILE})

# generate Qt translations and messages
set(LANGUAGES ja)
# 翻訳対象のファイルを設定する。
SET(TRANSLATION_TARGET_FILES
    ${HEADERS}
    ${SOURCES}
)

set(translation)
foreach(lang ${LANGUAGES})
    set(name "${CMAKE_SOURCE_DIR}/translations/KumoWorks_${lang}.ts")
    list(APPEND translation ${name})
    set_source_files_properties(${name} PROPERTIES
        OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/loc"
    )
endforeach()

qt5_create_translation(message ${translation} ${TRANSLATION_TARGET_FILES})

add_custom_target("translation_KumoWorks" DEPENDS ${message})
set_target_properties("translation_KumoWorks" PROPERTIES
    EXCLUDE_FROM_DEFAULT_BUILD TRUE
)

# 生成する実行ファイルを追加する
if(BUILD_ENV_MSVC)
    add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${RESOURCES})
else()
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${HEADERS} ${SOURCES} ${RESOURCES})
endif()

# link Qt library. Core and Gui will be linked accordingly
target_link_libraries(${PROJECT_NAME} Qt5::Widgets)

if(BUILD_ENV_APPLE)
# info plists
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST
        ${CMAKE_SOURCE_DIR}/../macOS/BundleInfo.plist.in)

# resources
    set_source_files_properties(${MACOSX_RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    find_program(IBTOOL ibtool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin")
    add_custom_command (TARGET ${PROJECT_NAME} PRE_BUILD COMMAND mkdir -p ${PROJECT_NAME}/Contents/Resources)
endif()

# Set a default build type to Release
if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()
endif()