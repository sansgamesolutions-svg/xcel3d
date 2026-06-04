# Locate gdal
# This module defines
# OSG_LIBRARY
# OSG_FOUND, if false, do not try to link to gdal 
# OSG_INCLUDE_DIR, where to find the headers
#
# $OSG_DIR is an environment variable that would
# correspond to the ./configure --prefix=$OSG_DIR
#
# Created by Robert Osfield. 

SET (OSG_DIR ${VENDOR_PATH}/OpenSceneGraph)

MESSAGE(STATUS "OSG root set to ${OSG_DIR}")

FIND_PATH(OSG_INCLUDE_DIR osg/Node
    ${OSG_DIR}/include
    NO_DEFAULT_PATH
)

SET(LIB_PREFIX osg)
SET(LIB_SOVERSION 145)
FIND_PATH(OSG_INCLUDE_DIR osg/Node)

MACRO(FIND_OSG_LIBRARY MYLIBRARY MYLIBRARYNAME)

    FIND_LIBRARY("${MYLIBRARY}_DEBUG"
        NAMES "${MYLIBRARYNAME}${CMAKE_DEBUG_POSTFIX}"
        PATHS
        ${OSG_DIR}/lib/${CMAKE_SYSTEM_NAME}
        NO_DEFAULT_PATH
    )

    FIND_LIBRARY("${MYLIBRARY}_DEBUG"
        NAMES "${MYLIBRARYNAME}${CMAKE_DEBUG_POSTFIX}"
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;OSG_ROOT]/lib
        /usr/freeware/lib64
    )
    
    FIND_LIBRARY(${MYLIBRARY}
        NAMES "${MYLIBRARYNAME}${CMAKE_RELEASE_POSTFIX}"
        PATHS
        ${OSG_DIR}/lib/${CMAKE_SYSTEM_NAME}
        NO_DEFAULT_PATH
    )

    FIND_LIBRARY(${MYLIBRARY}
        NAMES "${MYLIBRARYNAME}${CMAKE_RELEASE_POSTFIX}"
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;OSG_ROOT]/lib
        /usr/freeware/lib64
    )

    SET( "${MYLIBRARYNAME}_SHARED" "${LIB_PREFIX}${LIB_SOVERSION}-${MYLIBRARYNAME}${CMAKE_RELEASE_POSTFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}" )
    SET( "${MYLIBRARYNAME}_SHARED_DEBUG" "${LIB_PREFIX}${LIB_SOVERSION}-${MYLIBRARYNAME}${CMAKE_DEBUG_POSTFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

    IF( NOT ${MYLIBRARY}_DEBUG)
        IF(MYLIBRARY)
            SET(${MYLIBRARY}_DEBUG ${MYLIBRARY})
         ENDIF(MYLIBRARY)
    ENDIF( NOT ${MYLIBRARY}_DEBUG)
           
ENDMACRO(FIND_OSG_LIBRARY LIBRARY LIBRARYNAME)

SET( "OSG_SHARED_PATH" ${OSG_DIR}/bin/win )
SET( "OSG_SHARED_PATH_DEBUG" ${OSG_DIR}/bin/wind)

FIND_OSG_LIBRARY(OSG_LIBRARY osg)
FIND_OSG_LIBRARY(OSGGA_LIBRARY osgGA)
FIND_OSG_LIBRARY(OSGUTIL_LIBRARY osgUtil)
FIND_OSG_LIBRARY(OSGDB_LIBRARY osgDB)
FIND_OSG_LIBRARY(OSGTEXT_LIBRARY osgText)
FIND_OSG_LIBRARY(OSGWIDGET_LIBRARY osgWidget)
FIND_OSG_LIBRARY(OSGTERRAIN_LIBRARY osgTerrain)
FIND_OSG_LIBRARY(OSGFX_LIBRARY osgFX)
FIND_OSG_LIBRARY(OSGVIEWER_LIBRARY osgViewer)
FIND_OSG_LIBRARY(OSGVOLUME_LIBRARY osgVolume)
FIND_OSG_LIBRARY(OPENTHREADS_LIBRARY OpenThreads)

SET(OSG_FOUND "NO")
IF(OSG_LIBRARY AND OSG_INCLUDE_DIR)
    SET(OSG_FOUND "YES")
ENDIF(OSG_LIBRARY AND OSG_INCLUDE_DIR)
