@echo off

set G="NMake Makefiles"
set LIBLAS=D:\liblas
set OSGEO4W=C:\OSGeo4W
set BOOST=D:\boost\boost_1_44
set ORACLE_HOME=D:\instantclient_11_1

set PATH=%ORACLE_HOME%;%OSGEO4W%\apps\gdal-dev\bin;%OSGEO4W%\bin;%PATH%

cmake -G %G% ^
    -DBOOST_INCLUDEDIR=%BOOST% ^
    -DWITH_GDAL=ON ^
    -DWITH_GEOTIFF=ON ^
    -DWITH_ORACLE=ON ^
    -DTIFF_INCLUDE_DIR=%OSGEO4W%\include ^
    -DTIFF_LIBRARY=%OSGEO4W%\lib\libtiff_i.lib ^
    -DGEOTIFF_INCLUDE_DIR=%OSGEO4W%\include ^
    -DGEOTIFF_LIBRARY=%OSGEO4W%\lib\geotiff_i.lib ^
    -DGDAL_INCLUDE_DIR=%OSGEO4W%\apps\gdal-dev\include ^
    -DGDAL_LIBRARY=%OSGEO4W%\apps\gdal-dev\lib\gdal_i.lib ^
    -DORACLE_INCLUDE_DIR=%ORACLE_HOME%\sdk\include ^
    -DORACLE_OCI_LIBRARY=%ORACLE_HOME%\sdk\lib\msvc\oci.lib ^
    %LIBLAS%