/*
 * Implementation of the unicows dll
 *
 * Copyright (C) 2003 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winnls.h"
#include "wingdi.h"
#include "winuser.h"
#include "winver.h"
#include "winspool.h"
#include "commdlg.h"
#include "wincrypt.h"
#include "ddeml.h"
#include "wincon.h"
#include "oledlg.h"
#include "ras.h"
#include "mmsystem.h"
#include "shlobj.h"
#include "shellapi.h"
#include "vfw.h"
#include "winnetwk.h"

/* Create a dummy reference to every function we need so that
 * the linker will import them.
 */
const void *dummy_references[] =
{
    AddAtomW,
    AddFontResourceW,
    AddJobW,
    AddPrinterDriverW,
    AddPrinterW,
    AppendMenuW,
    BeginUpdateResourceA,
    BeginUpdateResourceW,
    BroadcastSystemMessageW,
    BuildCommDCBAndTimeoutsW,
    BuildCommDCBW,
    CallMsgFilterW,
    CallNamedPipeW,
    CallWindowProcA,
    CallWindowProcW,
    ChangeDisplaySettingsExW,
    ChangeDisplaySettingsW,
    ChangeMenuW,
    CharLowerBuffW,
    CharLowerW,
    CharNextW,
    CharPrevW,
    CharToOemBuffW,
    CharToOemW,
    CharUpperBuffW,
    CharUpperW,
    ChooseColorW,
    ChooseFontW,
    CommConfigDialogW,
    CompareStringW,
    CopyAcceleratorTableW,
    CopyFileExW ,
    CopyFileW,
    CopyMetaFileW,
    CreateAcceleratorTableW,
    CreateColorSpaceW,
    CreateDCW,
    CreateDialogIndirectParamW,
    CreateDialogParamW,
    CreateDirectoryExW,
    CreateDirectoryW,
    CreateEnhMetaFileW,
    CreateEventW,
    CreateFileMappingW,
    CreateFileW,
    CreateFontIndirectW,
    CreateFontW,
    CreateICW,
    CreateMDIWindowW,
    CreateMailslotW,
    CreateMetaFileW,
    CreateMutexW,
    CreateNamedPipeW,
    CreateProcessW,
    CreateScalableFontResourceW,
    CreateSemaphoreW,
    CreateWaitableTimerW,
    CreateWindowExW,
    CryptAcquireContextW,
    CryptEnumProviderTypesW,
    CryptEnumProvidersW,
    CryptGetDefaultProviderW,
    CryptSetProviderExW,
    CryptSetProviderW,
    CryptSignHashW,
    CryptVerifySignatureW,
    DdeConnect,
    DdeConnectList,
    DdeCreateStringHandleW,
    DdeInitializeW,
    DdeQueryConvInfo,
    DdeQueryStringW,
    DefDlgProcW,
    DefFrameProcW,
    DefMDIChildProcW,
    DefWindowProcW,
    DeleteFileW,
    DeviceCapabilitiesW,
    DialogBoxIndirectParamW,
    DialogBoxParamW,
    DispatchMessageW,
    DlgDirListComboBoxW,
    DlgDirListW,
    DlgDirSelectComboBoxExW,
    DlgDirSelectExW,
    DocumentPropertiesW,
    DragQueryFileW,
    DrawStateW,
    DrawTextExW,
    DrawTextW,
    EnableWindow,
    EndUpdateResourceA,
    EndUpdateResourceW,
    EnumClipboardFormats,
    EnumDateFormatsW,
    EnumDisplayDevicesW,
    EnumDisplaySettingsExW,
    EnumDisplaySettingsW,
    EnumFontFamiliesExW,
    EnumFontFamiliesW,
    EnumFontsW,
    EnumPrinterDriversW,
    EnumPrintersW,
    EnumPropsA,
    EnumPropsExA,
    EnumPropsExW,
    EnumPropsW,
    EnumSystemCodePagesW,
    EnumSystemLocalesW,
    EnumTimeFormatsW,
    ExpandEnvironmentStringsW,
    ExtTextOutW,
    ExtractIconExW,
    ExtractIconW,
    FatalAppExitW,
    FillConsoleOutputCharacterW,
    FindAtomW,
    FindExecutableW,
    FindFirstChangeNotificationW,
    FindFirstFileW,
    FindNextFileW,
    FindResourceExW,
    FindResourceW,
    FindTextW,
    FindWindowExW,
    FindWindowW,
    FormatMessageW,
    FreeEnvironmentStringsW,
    GetAtomNameW,
    GetCPInfo,
    GetCPInfoExW,
    GetCalendarInfoW,
    GetCharABCWidthsFloatW,
    GetCharABCWidthsW,
    GetCharWidth32W,
    GetCharWidthFloatW,
    GetCharWidthW,
    GetCharacterPlacementW,
    GetClassInfoExW,
    GetClassInfoW,
    GetClassLongW,
    GetClassNameW,
    GetClipboardData,
    GetClipboardFormatNameW,
    GetComputerNameW,
    GetConsoleTitleW,
    GetCurrencyFormatW,
    GetCurrentDirectoryW,
    GetDateFormatW,
    GetDefaultCommConfigW,
    GetDiskFreeSpaceExW ,
    GetDiskFreeSpaceW,
    GetDlgItemTextW,
    GetDriveTypeW,
    GetEnhMetaFileDescriptionW,
    GetEnhMetaFileW,
    GetEnvironmentStringsW,
    GetEnvironmentVariableW,
    GetFileAttributesExW,
    GetFileAttributesW,
    GetFileTitleW,
    GetFileVersionInfoSizeW,
    GetFileVersionInfoW,
    GetFullPathNameW,
    GetGlyphOutlineW,
    GetKerningPairsW,
    GetKeyNameTextW,
    GetKeyboardLayoutNameW,
    GetLocaleInfoW,
    GetLogicalDriveStringsW,
    GetLongPathNameW ,
    GetMenuItemInfoW,
    GetMenuStringW,
    GetMessageW,
    GetMetaFileW,
    GetModuleFileNameW,
    GetModuleHandleW,
    GetMonitorInfoW,
    GetNamedPipeHandleStateW,
    GetNumberFormatW,
    GetObjectW,
    GetOpenFileNamePreviewW,
    GetOpenFileNameW,
    GetOutlineTextMetricsW,
    GetPrinterDataW,
    GetPrinterDriverDirectoryW,
    GetPrinterDriverW,
    GetPrinterW,
    GetPrivateProfileIntW,
    GetPrivateProfileSectionNamesW,
    GetPrivateProfileSectionW,
    GetPrivateProfileStringW,
    GetPrivateProfileStructW,
    GetProcAddress,
    GetProfileIntW,
    GetProfileSectionW,
    GetProfileStringW,
    GetPropA,
    GetPropW,
    GetSaveFileNamePreviewW,
    GetSaveFileNameW,
    GetShortPathNameW,
    GetStartupInfoW,
    GetStringTypeExW,
    GetStringTypeW,
    GetSystemDirectoryW,
    GetSystemWindowsDirectoryW,
    GetTabbedTextExtentW,
    GetTempFileNameW,
    GetTempPathW,
    GetTextExtentExPointW,
    GetTextExtentPoint32W,
    GetTextExtentPointW,
    GetTextFaceW,
    GetTextMetricsW,
    GetTimeFormatW,
    GetUserNameW,
    GetVersionExW,
    GetVolumeInformationW,
    GetWindowLongA,
    GetWindowLongW,
    GetWindowModuleFileNameW,
    GetWindowTextLengthW,
    GetWindowTextW,
    GetWindowsDirectoryW,
    GlobalAddAtomW,
    GlobalFindAtomW,
    GlobalGetAtomNameW,
    GrayStringW,
    InsertMenuItemW,
    InsertMenuW,
    IsBadStringPtrW,
    IsCharAlphaNumericW,
    IsCharAlphaW,
    IsCharLowerW,
    IsCharUpperW,
    IsClipboardFormatAvailable,
    IsDialogMessageW,
    IsTextUnicode,
    IsValidCodePage,
    IsWindowUnicode,
    LCMapStringW,
    LoadAcceleratorsW,
    LoadBitmapW,
    LoadCursorFromFileW,
    LoadCursorW,
    LoadIconW,
    LoadImageW,
    LoadKeyboardLayoutW,
    LoadLibraryExW,
    LoadLibraryW,
    LoadMenuIndirectW,
    LoadMenuW,
    LoadStringW,
    MCIWndCreateW,
    MapVirtualKeyExW,
    MapVirtualKeyW,
    MessageBoxExW,
    MessageBoxIndirectW,
    MessageBoxW,
    ModifyMenuW,
    MoveFileW,
    MultiByteToWideChar,
    MultinetGetConnectionPerformanceW,
    OemToCharBuffW,
    OemToCharW,
    OleUIAddVerbMenuW,
    OleUIBusyW,
    OleUIChangeIconW,
    OleUIChangeSourceW,
    OleUIConvertW,
    OleUIEditLinksW,
    OleUIInsertObjectW,
    OleUIObjectPropertiesW,
    OleUIPasteSpecialW,
    OleUIPromptUserW,
    OleUIUpdateLinksW,
    OpenEventW,
    OpenFileMappingW,
    OpenMutexW,
    OpenPrinterW,
    OpenSemaphoreW,
    OpenWaitableTimerW,
    OutputDebugStringW,
    PageSetupDlgW,
    PeekConsoleInputW,
    PeekMessageW,
    PlaySoundW,
    PolyTextOutW,
    PostMessageW,
    PostThreadMessageW,
    PrintDlgW,
    QueryDosDeviceW,
    RasDeleteEntryW,
    RasEnumConnectionsW,
    RasEnumDevicesW,
    RasEnumEntriesW,
    RasSetEntryPropertiesW,
    ReadConsoleInputW,
    ReadConsoleOutputCharacterW,
    ReadConsoleOutputW,
    ReadConsoleW,
    RegConnectRegistryW,
    RegCreateKeyExW,
    RegCreateKeyW,
    RegDeleteKeyW,
    RegDeleteValueW,
    RegEnumKeyExW,
    RegEnumKeyW,
    RegEnumValueW,
    RegLoadKeyW,
    RegOpenKeyExW,
    RegOpenKeyW,
    RegQueryInfoKeyW,
    RegQueryMultipleValuesW,
    RegQueryValueExW,
    RegQueryValueW,
    RegReplaceKeyW,
    RegSaveKeyW,
    RegSetValueExW,
    RegSetValueW,
    RegUnLoadKeyW,
    RegisterClassExW,
    RegisterClassW,
    RegisterClipboardFormatW,
    RegisterWindowMessageW,
    RemoveDirectoryW,
    RemoveFontResourceW,
    RemovePropA,
    RemovePropW,
    ReplaceTextW,
    ResetDCW,
    ResetPrinterW,
    SHBrowseForFolderW,
    SHChangeNotify ,
    SHFileOperationW,
    SHGetFileInfoW,
    SHGetPathFromIDListW,
    ScrollConsoleScreenBufferW,
    SearchPathW,
    SendDlgItemMessageW,
    SendMessageCallbackW,
    SendMessageTimeoutW,
    SendMessageW,
    SendNotifyMessageW,
    SetCalendarInfoW,
    SetClassLongW,
    SetComputerNameW,
    SetConsoleTitleW,
    SetCurrentDirectoryW,
    SetDefaultCommConfigW,
    SetDlgItemTextW,
    SetEnvironmentVariableW,
    SetFileAttributesW,
    SetJobW,
    SetLocaleInfoW,
    SetMenuItemInfoW,
    SetPrinterDataW,
    SetPrinterW,
    SetPropA,
    SetPropW,
    SetVolumeLabelW,
    SetWindowLongA,
    SetWindowLongW,
    SetWindowTextW,
    SetWindowsHookExW,
    SetWindowsHookW,
    ShellAboutW,
    ShellExecuteExW ,
    ShellExecuteW ,
    Shell_NotifyIconW,
    StartDocPrinterW,
    StartDocW,
    SystemParametersInfoW,
    TabbedTextOutW,
    TextOutW,
    TranslateAcceleratorW,
    UnregisterClassW,
    UpdateResourceA,
    UpdateResourceW,
    VerFindFileW,
    VerInstallFileW,
    VerLanguageNameW,
    VerQueryValueW,
    VkKeyScanExW,
    VkKeyScanW,
    WNetAddConnection2W,
    WNetAddConnection3W,
    WNetAddConnectionW,
    WNetCancelConnection2W,
    WNetCancelConnectionW,
    WNetConnectionDialog1W,
    WNetDisconnectDialog1W,
    WNetEnumResourceW,
    WNetGetConnectionW,
    WNetGetLastErrorW,
    WNetGetNetworkInformationW,
    WNetGetProviderNameW,
    WNetGetResourceInformationW,
    WNetGetResourceParentW,
    WNetGetUniversalNameW ,
    WNetGetUserW,
    WNetOpenEnumW,
    WNetUseConnectionW,
    WaitNamedPipeW ,
    WideCharToMultiByte,
    WinHelpW,
    WriteConsoleInputW,
    WriteConsoleOutputCharacterW,
    WriteConsoleOutputW,
    WriteConsoleW,
    WritePrivateProfileSectionW,
    WritePrivateProfileStringW,
    WritePrivateProfileStructW,
    WriteProfileSectionW,
    WriteProfileStringW,
    auxGetDevCapsW,
    capCreateCaptureWindowW,
    capGetDriverDescriptionW,
    joyGetDevCapsW,
    lstrcatW,
    lstrcmpW,
    lstrcmpiW,
    lstrcpyW,
    lstrcpynW,
    lstrlenW,
    mciGetDeviceIDW,
    mciGetErrorStringW,
    mciSendCommandW,
    mciSendStringW,
    midiInGetDevCapsW,
    midiInGetErrorTextW,
    midiOutGetDevCapsW,
    midiOutGetErrorTextW,
    mixerGetControlDetailsW,
    mixerGetDevCapsW,
    mixerGetLineControlsW,
    mixerGetLineInfoW,
    mmioInstallIOProcW,
    mmioOpenW,
    mmioRenameW,
    mmioStringToFOURCCW,
    sndPlaySoundW,
    waveInGetDevCapsW,
    waveInGetErrorTextW,
    waveOutGetDevCapsW,
    waveOutGetErrorTextW,
    wsprintfW,
    wvsprintfW
};
