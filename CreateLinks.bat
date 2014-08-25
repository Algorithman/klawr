:: Engine source is in a git repo, Klawr source is in a different git repo, no simple way to put the Klawr checkout inside Engine checkout due to a shared directory structure,
:: so create a couple of directory junctions and a few symbolic links in the Engine source that link to the Klawr source.
@echo off
set BATCH_FILE_LOCATION=%~dp0
set ENGINE_SOURCE_LOCATION=C:\Projects\UE4\UnrealEngine\
set CodeGeneratorLinkDir=%ENGINE_SOURCE_LOCATION%Engine\Plugins\Klawr\KlawrCodeGeneratorPlugin
set CodeGeneratorBaseTargetDir=%BATCH_FILE_LOCATION%Engine\Plugins\Klawr\KlawrCodeGeneratorPlugin
set ScriptPluginCodeLinkDir=%ENGINE_SOURCE_LOCATION%Engine\Plugins\Script\ScriptPlugin\Source\ScriptPlugin
set ScriptPluginCodeTargetDir=%BATCH_FILE_LOCATION%Engine\Plugins\Script\ScriptPlugin\Source\ScriptPlugin
@echo on

mklink /J %ENGINE_SOURCE_LOCATION%Engine\Source\ThirdParty\Klawr %BATCH_FILE_LOCATION%Engine\Source\ThirdParty\Klawr
mklink /J %CodeGeneratorLinkDir% %CodeGeneratorBaseTargetDir%

mklink %ScriptPluginCodeLinkDir%\Private\KlawrObjectReferencer.cpp %ScriptPluginCodeTargetDir%\Private\KlawrObjectReferencer.cpp
mklink %ScriptPluginCodeLinkDir%\Private\KlawrObjectReferencer.h %ScriptPluginCodeTargetDir%\Private\KlawrObjectReferencer.h
mklink %ScriptPluginCodeLinkDir%\Private\KlawrObjectUtils.cpp %ScriptPluginCodeTargetDir%\Private\KlawrObjectUtils.cpp
mklink %ScriptPluginCodeLinkDir%\Private\KlawrObjectUtils.h %ScriptPluginCodeTargetDir%\Private\KlawrObjectUtils.h
mklink %ScriptPluginCodeLinkDir%\Private\KlawrScriptContext.cpp %ScriptPluginCodeTargetDir%\Private\KlawrScriptContext.cpp
mklink %ScriptPluginCodeLinkDir%\Private\KlawrScriptContext.h %ScriptPluginCodeTargetDir%\Private\KlawrScriptContext.h
pause