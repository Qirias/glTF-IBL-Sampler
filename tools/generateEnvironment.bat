@echo
 off
setlocal

:: Needed to show cmgen output correctly (no ruk ruk ruk).
chcp 65001 > nul

set IMAGE_FILE=%1
if "%1"=="" goto usage 
if "%3"=="" goto no_input_size 

:: Check if the input image exists
if exist "%IMAGE_FILE%" goto begin

:no_input_file
echo   ERROR: Input file not found: "%IMAGE_FILE%"
goto usage

:no_input_size
echo   ERROR: Specular size or skybox size not defined.
goto usage

:usage
echo.
echo Usage:
echo   generateEnvironment.bat inputFile specularSize skyboxSize
echo.
echo     inputFile:       An equirectangular image file in .png or .hdr format.
echo     specularSize:    Size for the specular radiance cubemap. Should be a power of 2 (e.g., 256).
echo     skyboxSize:      Size for the skybox cubemap. Should be a power of 2 (e.g., 512).
echo.
goto :eof

:error
echo ERROR: Generating environment failed.
goto :eof

::*****************************************************************
:begin
if not exist maps mkdir maps || goto error
if not exist maps\processed mkdir maps\processed || goto error
if not exist maps\processed\images mkdir maps\processed\images || goto error
if not exist maps\tmp mkdir maps\tmp || goto error

del /Q maps\tmp > nul || goto error

set CMD_FILE_DIR=%~dp0
set TOOLS_DIR=%CMD_FILE_DIR%.\
set CLI=%CMD_FILE_DIR%\cli.exe

set "CLI_DIR=%CD%"
set "KTX_DIR=%CLI_DIR%\thirdparty\KTX-Software-Executables"
set "CONVERTER_DIR=%CMD_FILE_DIR%"

set "IMAGE_NAME_ONLY=%~n1"
set "IMAGE_EXTENSION=%~x1"
set "IMAGE_PATH=%CMD_FILE_DIR%%IMAGE_FILE%"
set KTX2_FILES=diffuse specular skybox

set /A SPECULARSIZE=%2
set /A SKYBOXSIZE=%3

set TMP_INPUT=maps\tmp\tmp%IMAGE_EXTENSION%
copy "%IMAGE_PATH%" "%TMP_INPUT%" > nul || goto error

echo Generating specular cubemap...
"%CLI%" -inputPath "%TMP_INPUT%" -outCubeMap maps\processed\images\%IMAGE_NAME_ONLY%_specular.ktx2 -distribution GGX -sampleCount 1024 -targetFormat B10G11R11_UFLOAT_PACK32 -cubeMapResolution %SPECULARSIZE% || goto error

echo Generating diffuse cubemap...
"%CLI%" -inputPath "%TMP_INPUT%" -outCubeMap maps\processed\images\%IMAGE_NAME_ONLY%_diffuse.ktx2 -distribution Lambertian -sampleCount 1024 -targetFormat B10G11R11_UFLOAT_PACK32 -cubeMapResolution %SPECULARSIZE% || goto error

:: If skybox size is different from specular size, generate skybox cubemap
if %SPECULARSIZE% neq %SKYBOXSIZE% (
    echo Generating skybox cubemap...
    "%CLI%" -inputPath "%TMP_INPUT%" -outCubeMap maps\processed\images\%IMAGE_NAME_ONLY%_skybox.ktx2 -sampleCount 1024 -targetFormat B10G11R11_UFLOAT_PACK32 -cubeMapResolution %SKYBOXSIZE% || goto error
) else (
    :: If they are the same, use the specular map as the skybox
    copy maps\processed\images\%IMAGE_NAME_ONLY%_specular.ktx2 maps\processed\images\%IMAGE_NAME_ONLY%_skybox.ktx2 > nul || goto error
)

echo Generating BRDF LUT...
"%CLI%" -inputPath "%TMP_INPUT%" -outLUT maps\processed\images\%IMAGE_NAME_ONLY%_brdfLut.png -sampleCount 1024 || goto error

del /Q maps\tmp > nul
rmdir maps\tmp > nul

setlocal EnableDelayedExpansion

set /A PARSELINE=1

:: Parse the coefficients
FOR /F "tokens=1,2,3 delims=," %%i in (sh9.txt) do (
    set "ic!PARSELINE!1=%%i"
    set "ic!PARSELINE!2=%%j"
    set "ic!PARSELINE!3=%%k"
    set /A PARSELINE+=1
)

echo Converting KTX2 files to KTX1...
for /r "maps\processed\images" %%f in (*.ktx2) do (
    echo Converting %%f to KTX1...
    "!CONVERTER_DIR!\ktx2_to_ktx1_converter.exe" "%%f" "%%~dpnf.ktx" || goto error
)

echo Generating glTF environment file...
(
echo {
echo     "asset": {
echo         "version": "2.0"
echo     },
echo     "images": [
echo        {
echo            "name": "%IMAGE_NAME_ONLY%_specular.ktx",
echo            "uri": "images/%IMAGE_NAME_ONLY%_specular.ktx",
echo            "mimeType": "image/ktx"
echo        },
echo        {
echo            "name": "%IMAGE_NAME_ONLY%_skybox.ktx",
echo            "uri": "images/%IMAGE_NAME_ONLY%_skybox.ktx",
echo            "mimeType": "image/ktx"
echo        }
echo     ],
echo     "scenes": [
echo         {
echo             "name": "scene",
echo             "extensions": {
echo                 "EXT_lights_image_based": {
echo                     "light": 0
echo                 }
echo             }
echo         }
echo     ],
echo     "scene": 0,
echo     "extensions": {
echo         "EXT_lights_image_based": {
echo             "lights": [
echo                 {
echo                     "intensity": 1,
echo                     "irradianceCoefficients": [
echo                         [
echo                             %ic11%, %ic12%, %ic13%
echo                         ],
echo                         [
echo                             %ic21%, %ic22%, %ic23%
echo                         ],
echo                         [
echo                             %ic31%, %ic32%, %ic33%
echo                         ],
echo                         [
echo                             %ic41%, %ic42%, %ic43%
echo                         ],
echo                         [
echo                             %ic51%, %ic52%, %ic53%
echo                         ],
echo                         [
echo                             %ic61%, %ic62%, %ic63%
echo                         ],
echo                         [
echo                             %ic71%, %ic72%, %ic73%
echo                         ],
echo                         [
echo                             %ic81%, %ic82%, %ic83%
echo                         ],
echo                         [
echo                             %ic91%, %ic92%, %ic93%
echo                         ]
echo                     ],
echo                     "name": "imageBasedLight",
echo                     "extras": {
echo                         "skymapImage": 1,
echo                         "skymapImageLodLevel": 0,
echo                         "specularCubeImage": 0
echo                     }
echo                 }
echo             ]
echo         }
echo     },
echo     "extensionsUsed": [
echo         "EXT_lights_image_based"
echo     ]
echo }
) > maps\processed\Environment.gltf

echo.
echo DONE.

endlocal
goto end

:error
echo An error occurred during the process.

:end
exit

echo KTX2 extraction and conversion completed.
goto :eof