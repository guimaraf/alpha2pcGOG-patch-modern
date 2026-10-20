@echo off
setlocal
cd /d "%~dp0"

echo ========================================================
echo Empacotando alpha2gogModernPatch.zip
echo ========================================================

set "ZIPNAME=alpha2gogModernPatch.zip"

if exist "%ZIPNAME%" (
    echo Removendo pacote anterior...
    del "%ZIPNAME%"
)

echo Verificando arquivos necessarios...
set "MISSING=0"
for %%F in (ddraw.dll DDrawCompat.ini dinput.dll sfa2_patch.ini LEIAME.txt) do (
    if not exist "%%F" (
        echo [ERRO] Arquivo ausente: %%F
        set "MISSING=1"
    )
)

if "%MISSING%"=="1" (
    echo.
    echo Falha: Arquivos obrigatorios estao ausentes!
    pause
    exit /b 1
)

echo Criando %ZIPNAME%...
powershell -NoProfile -Command "Compress-Archive -Path ddraw.dll, DDrawCompat.ini, dinput.dll, sfa2_patch.ini, LEIAME.txt -DestinationPath '%ZIPNAME%' -Force"

if exist "%ZIPNAME%" (
    echo [SUCESSO] Pacote gerado com sucesso: %ZIPNAME%
    echo Conteudo:
    powershell -NoProfile -Command "tar -tf '%ZIPNAME%'"
) else (
    echo [ERRO] Falha ao gerar o arquivo ZIP.
)

echo.
