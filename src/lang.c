/*
 * FRANK OS — Localization / Language Support
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lang.h"
#include "settings.h"

/* English string table */
static const char *str_en[] = {
    /* Common buttons */
    [STR_OK]             = "OK",
    [STR_CANCEL]         = "Cancel",
    [STR_CLOSE]          = "Close",
    [STR_YES]            = "Yes",
    [STR_NO]             = "No",

    /* Start menu */
    [STR_START]          = "Start",
    [STR_PROGRAMS]       = "Programs",
    [STR_SETTINGS]       = "Settings",
    [STR_FIRMWARE]       = "Firmware",
    [STR_RUN_DOTS]       = "Run...",
    [STR_REBOOT]         = "Reboot",

    /* Programs submenu */
    [STR_NAVIGATOR]      = "Navigator",
    [STR_TERMINAL]       = "Terminal",

    /* Settings submenu */
    [STR_CONTROL_PANEL]  = "Control Panel",
    [STR_NETWORK]        = "Network",
    [STR_LANGUAGE]       = "Language",

    /* System menu */
    [STR_RESTORE]        = "Restore",
    [STR_MOVE]           = "Move",
    [STR_MAXIMIZE]       = "Maximize",
    [STR_MINIMIZE]       = "Minimize",
    [STR_ENTER_FULLSCREEN] = "Enter Fullscreen",
    [STR_EXIT_FULLSCREEN]  = "Exit Fullscreen",

    /* Taskbar */
    [STR_VOLUME]         = "Volume",
    [STR_MUTE]           = "Mute",
    [STR_DISCONNECT]     = "Disconnect",

    /* Control Panel items */
    [STR_DESKTOP]        = "Desktop",
    [STR_SYSTEM]         = "System",
    [STR_MOUSE]          = "Mouse",
    [STR_FREQUENCIES]    = "Frequencies",

    /* Desktop Properties */
    [STR_BG_COLOR]       = "Background color:",
    [STR_PREVIEW]        = "Preview:",
    [STR_WINDOW_THEME]   = "Window theme:",
    [STR_DESKTOP_PROPS]  = "Desktop Properties",

    /* System Properties */
    [STR_SYSTEM_PROPS]   = "System Properties",

    /* Mouse Properties */
    [STR_DBLCLICK_SPEED] = "Double-click speed:",
    [STR_SLOW]           = "Slow",
    [STR_FAST]           = "Fast",
    [STR_TEST_AREA]      = "Test area:",
    [STR_MOUSE_PROPS]    = "Mouse Properties",

    /* Frequencies */
    [STR_CPU_FREQ]       = "CPU Frequency:",
    [STR_PSRAM_FREQ]     = "PSRAM Frequency:",
    [STR_REBOOT_NOTICE]  = "Changes take effect\nafter reboot.",

    /* Language */
    [STR_LANGUAGE_PROPS]   = "Language",
    [STR_SELECT_LANGUAGE]  = "Select language:",

    /* File dialog */
    [STR_LOOK_IN]        = "Look in:",
    [STR_FILE_NAME]      = "File name:",
    [STR_OPEN]           = "Open",
    [STR_SAVE]           = "Save",
    [STR_CONFIRM_SAVE_AS] = "Confirm Save As",

    /* Find/Replace */
    [STR_FIND]           = "Find",
    [STR_REPLACE]        = "Replace",
    [STR_FIND_NEXT]      = "Find Next",
    [STR_REPLACE_ALL]    = "Replace All",
    [STR_FIND_WHAT]      = "Find what:",
    [STR_REPLACE_WITH]   = "Replace with:",
    [STR_MATCH_CASE]     = "Match case",

    /* Run dialog */
    [STR_RUN]            = "Run",
    [STR_RUN_DESC]       = "Type the name of a program,\nfolder, or document, and\nFRANK OS will open it.",
    [STR_ERR_NO_SDCARD]  = "No SD card mounted.\nPlease insert an SD card\nand try again.",
    [STR_ERR_FILE_NOT_FOUND] = "Cannot find '%s'.\n\nMake sure the name is correct,\nthen try again.",

    /* Network */
    [STR_NET_CONN_FAILED]  = "Connection failed.\n\nCheck the password\nand try again.",
    [STR_WIFI_PASSWORD]    = "WiFi Password",
    [STR_ENTER_PASSWORD]   = "Enter password:",
    [STR_CONNECTING_TO]    = "Connecting to %s...",
    [STR_CONNECTING]       = "Connecting...",
    [STR_CONNECTED_TO]     = "Connected to: %s (%s)",
    [STR_NO_ADAPTER]       = "No network adapter detected",
    [STR_NOT_CONNECTED]    = "Not connected",
    [STR_SCANNING]         = "Scanning...",
    [STR_NO_NETWORKS]      = "No networks found. Click Scan.",
    [STR_SCAN]             = "Scan",
    [STR_CONNECT]          = "Connect",
    [STR_HDR_NETWORK]      = "Network",
    [STR_HDR_SIGNAL]       = "Signal",
    [STR_HDR_TYPE]         = "Type",
    [STR_ERR_NO_NET_ADAPTER] = "No network adapter detected.\n\nPlease connect a network\nadapter and reboot.",

    /* File manager - toolbar */
    [STR_FM_BACK]          = "Back",
    [STR_FM_UP]            = "Up",
    [STR_FM_CUT]           = "Cut",
    [STR_FM_COPY]          = "Copy",
    [STR_FM_PASTE]         = "Paste",
    [STR_FM_DELETE]        = "Delete",

    /* File manager - context menu */
    [STR_FM_OPEN_WITH]     = "Open with",
    [STR_FM_RENAME]        = "Rename",
    [STR_FM_NEW_FOLDER]    = "New Folder",
    [STR_FM_REFRESH]       = "Refresh",

    /* File manager - menu bar */
    [STR_FILE]             = "File",
    [STR_EDIT]             = "Edit",
    [STR_VIEW]             = "View",
    [STR_HELP]             = "Help",
    [STR_FM_NEW_FOLDER_MENU] = "New Folder",
    [STR_FM_DELETE_MENU]   = "Delete     Del",
    [STR_FM_RENAME_MENU]   = "Rename      F2",
    [STR_FM_CLOSE_MENU]    = "Close",
    [STR_FM_CUT_MENU]      = "Cut    Ctrl+X",
    [STR_FM_COPY_MENU]     = "Copy   Ctrl+C",
    [STR_FM_PASTE_MENU]    = "Paste  Ctrl+V",
    [STR_FM_SELALL_MENU]   = "SelAll Ctrl+A",
    [STR_FM_LARGE_ICONS]   = "Large Icons",
    [STR_FM_SMALL_ICONS]   = "Small Icons",
    [STR_FM_LIST]          = "List",
    [STR_FM_REFRESH_MENU]  = "Refresh     F5",
    [STR_FM_ABOUT_MENU]    = "About      F1",

    /* File manager - columns + types */
    [STR_FM_NAME]          = "Name",
    [STR_FM_SIZE]          = "Size",
    [STR_FM_TYPE]          = "Type",
    [STR_FM_FOLDER]        = "Folder",
    [STR_FM_APPLICATION]   = "Application",
    [STR_FM_FILE_TYPE]     = "File",
    [STR_FM_OBJECTS]       = "object(s)",

    /* File manager - dialogs */
    [STR_CONFIRM_DELETE]   = "Confirm Delete",
    [STR_FM_DELETE_ITEMS]  = "Delete selected item(s)?",
    [STR_FM_NO_APP_REGISTERED] = "No application is registered\nfor this file type.",
    [STR_ABOUT_NAVIGATOR]  = "About Navigator",
    [STR_FM_DELETING]      = "Deleting",
    [STR_FM_COPYING]       = "Copying",
    [STR_FM_NEW_FOLDER_DLG] = "New Folder",
    [STR_FM_RENAME_DLG]    = "Rename",
    [STR_FM_ENTER_NAME]    = "Name:",
    [STR_FM_NEW_NAME]      = "New name:",
    [STR_FM_ALL_FILES]     = "All Files (*.*)",

    /* Terminal */
    [STR_FM_EXIT]          = "Exit",
    [STR_ABOUT_TERMINAL]   = "About Terminal",

    /* Desktop context menu */
    [STR_SEND_TO_DESKTOP]  = "Send to Desktop",
    [STR_DT_REMOVE]        = "Remove",
    [STR_DT_SORT_BY_NAME]  = "Sort by Name",
    [STR_DT_REFRESH]       = "Refresh",

    /* Alt+Tab */
    [STR_ALTTAB_DESKTOP]   = "Desktop",

    /* Reboot/firmware dialogs */
    [STR_LAUNCH_FIRMWARE]  = "Launch Firmware",
    [STR_LAUNCH_FW_MSG]    = "Launch \"%s\"?...\n\nScreen will turn off...\n\nTo return to FRANK OS,\nhold Space and press Reset...",
    [STR_REBOOT_CONFIRM]   = "Are you sure you want to\nreboot the system?",
    [STR_FLASHING_FW]      = "Flashing firmware...",

    /* No UF2 */
    [STR_NO_UF2]           = "(no .uf2 files)",

    /* Run dialog extras */
    [STR_BROWSE]           = "Browse...",
    [STR_OPEN_LABEL]       = "Open:",
    [STR_FILES_OF_TYPE]    = "Files of type:",

    /* Dialog buttons */
    [STR_DLG_OK]           = "OK",
    [STR_DLG_CANCEL]       = "Cancel",
    [STR_DLG_YES]          = "Yes",
    [STR_DLG_NO]           = "No",
};

/* Russian string table (Win1251 encoded) */
static const char *str_ru[] = {
    /* Common buttons */
    [STR_OK]             = "OK",
    [STR_CANCEL]         = "\xCE\xF2\xEC\xE5\xED\xE0",
    [STR_CLOSE]          = "\xC7\xE0\xEA\xF0\xFB\xF2\xFC",
    [STR_YES]            = "\xC4\xE0",
    [STR_NO]             = "\xCD\xE5\xF2",

    /* Start menu */
    [STR_START]          = "\xCF\xF3\xF1\xEA",
    [STR_PROGRAMS]       = "\xCF\xF0\xEE\xE3\xF0\xE0\xEC\xEC\xFB",
    [STR_SETTINGS]       = "\xCD\xE0\xF1\xF2\xF0\xEE\xE9\xEA\xE8",
    [STR_FIRMWARE]       = "\xCF\xF0\xEE\xF8\xE8\xE2\xEA\xE8",
    [STR_RUN_DOTS]       = "\xC2\xFB\xEF\xEE\xEB\xED\xE8\xF2\xFC...",
    [STR_REBOOT]         = "\xCF\xE5\xF0\xE5\xE7\xE0\xE3\xF0\xF3\xE7\xEA\xE0",

    /* Programs submenu */
    [STR_NAVIGATOR]      = "\xCD\xE0\xE2\xE8\xE3\xE0\xF2\xEE\xF0",
    [STR_TERMINAL]       = "\xD2\xE5\xF0\xEC\xE8\xED\xE0\xEB",

    /* Settings submenu */
    [STR_CONTROL_PANEL]  = "\xCF\xE0\xED\xE5\xEB\xFC \xF3\xEF\xF0\xE0\xE2\xEB\xE5\xED\xE8\xFF",
    [STR_NETWORK]        = "\xD1\xE5\xF2\xFC",
    [STR_LANGUAGE]       = "\xDF\xE7\xFB\xEA",

    /* System menu */
    [STR_RESTORE]        = "\xC2\xEE\xF1\xF1\xF2\xE0\xED\xEE\xE2\xE8\xF2\xFC",
    [STR_MOVE]           = "\xCF\xE5\xF0\xE5\xEC\xE5\xF1\xF2\xE8\xF2\xFC",
    [STR_MAXIMIZE]       = "\xD0\xE0\xE7\xE2\xE5\xF0\xED\xF3\xF2\xFC",
    [STR_MINIMIZE]       = "\xD1\xE2\xE5\xF0\xED\xF3\xF2\xFC",
    [STR_ENTER_FULLSCREEN] = "\xCF\xEE\xEB\xED\xFB\xE9 \xFD\xEA\xF0\xE0\xED",
    [STR_EXIT_FULLSCREEN]  = "\xCE\xEA\xEE\xED\xED\xFB\xE9 \xF0\xE5\xE6\xE8\xEC",

    /* Taskbar */
    [STR_VOLUME]         = "\xC3\xF0\xEE\xEC\xEA\xEE\xF1\xF2\xFC",
    [STR_MUTE]           = "\xD2\xE8\xF8\xE8\xED\xE0",
    [STR_DISCONNECT]     = "\xCE\xF2\xEA\xEB\xFE\xF7\xE8\xF2\xFC",

    /* Control Panel items */
    [STR_DESKTOP]        = "\xD0\xE0\xE1\xEE\xF7\xE8\xE9 \xF1\xF2\xEE\xEB",
    [STR_SYSTEM]         = "\xD1\xE8\xF1\xF2\xE5\xEC\xE0",
    [STR_MOUSE]          = "\xCC\xFB\xF8\xFC",
    [STR_FREQUENCIES]    = "\xD7\xE0\xF1\xF2\xEE\xF2\xFB",

    /* Desktop Properties */
    [STR_BG_COLOR]       = "\xD6\xE2\xE5\xF2 \xF4\xEE\xED\xE0:",
    [STR_PREVIEW]        = "\xCF\xF0\xE5\xE4\xEF\xF0\xEE\xF1\xEC\xEE\xF2\xF0:",
    [STR_WINDOW_THEME]   = "\xD2\xE5\xEC\xE0 \xEE\xEA\xEE\xED:",
    [STR_DESKTOP_PROPS]  = "\xD0\xE0\xE1\xEE\xF7\xE8\xE9 \xF1\xF2\xEE\xEB",

    /* System Properties */
    [STR_SYSTEM_PROPS]   = "\xD1\xE8\xF1\xF2\xE5\xEC\xE0",

    /* Mouse Properties */
    [STR_DBLCLICK_SPEED] = "\xD1\xEA\xEE\xF0\xEE\xF1\xF2\xFC \xE4\xE2\xEE\xE9\xED\xEE\xE3\xEE \xEA\xEB\xE8\xEA\xE0:",
    [STR_SLOW]           = "\xCC\xE5\xE4\xEB\xE5\xED\xED\xEE",
    [STR_FAST]           = "\xC1\xFB\xF1\xF2\xF0\xEE",
    [STR_TEST_AREA]      = "\xD2\xE5\xF1\xF2:",
    [STR_MOUSE_PROPS]    = "\xCC\xFB\xF8\xFC",

    /* Frequencies */
    [STR_CPU_FREQ]       = "\xD7\xE0\xF1\xF2\xEE\xF2\xE0 CPU:",
    [STR_PSRAM_FREQ]     = "\xD7\xE0\xF1\xF2\xEE\xF2\xE0 PSRAM:",
    [STR_REBOOT_NOTICE]  = "\xC8\xE7\xEC\xE5\xED\xE5\xED\xE8\xFF \xEF\xEE\xF1\xEB\xE5\n\xEF\xE5\xF0\xE5\xE7\xE0\xE3\xF0\xF3\xE7\xEA\xE8.",

    /* Language */
    [STR_LANGUAGE_PROPS]   = "\xDF\xE7\xFB\xEA",
    [STR_SELECT_LANGUAGE]  = "\xC2\xFB\xE1\xE5\xF0\xE8\xF2\xE5 \xFF\xE7\xFB\xEA:",

    /* File dialog */
    [STR_LOOK_IN]        = "\xCF\xE0\xEF\xEA\xE0:",
    [STR_FILE_NAME]      = "\xC8\xEC\xFF \xF4\xE0\xE9\xEB\xE0:",
    [STR_OPEN]           = "\xCE\xF2\xEA\xF0\xFB\xF2\xFC",
    [STR_SAVE]           = "\xD1\xEE\xF5\xF0\xE0\xED\xE8\xF2\xFC",
    [STR_CONFIRM_SAVE_AS] = "\xCF\xEE\xE4\xF2\xE2\xE5\xF0\xE6\xE4\xE5\xED\xE8\xE5",

    /* Find/Replace */
    [STR_FIND]           = "\xCF\xEE\xE8\xF1\xEA",
    [STR_REPLACE]        = "\xC7\xE0\xEC\xE5\xED\xE0",
    [STR_FIND_NEXT]      = "\xCD\xE0\xE9\xF2\xE8 \xE4\xE0\xEB\xE5\xE5",
    [STR_REPLACE_ALL]    = "\xC7\xE0\xEC\xE5\xED\xE8\xF2\xFC \xE2\xF1\xB8",
    [STR_FIND_WHAT]      = "\xCD\xE0\xE9\xF2\xE8:",
    [STR_REPLACE_WITH]   = "\xC7\xE0\xEC\xE5\xED\xE8\xF2\xFC \xED\xE0:",
    [STR_MATCH_CASE]     = "\xD1 \xF3\xF7\xB8\xF2\xEE\xEC \xF0\xE5\xE3\xE8\xF1\xF2\xF0\xE0",

    /* Run dialog */
    [STR_RUN]            = "\xC7\xE0\xEF\xF3\xF1\xEA",
    [STR_RUN_DESC]       = "\xC2\xE2\xE5\xE4\xE8\xF2\xE5 \xE8\xEC\xFF \xEF\xF0\xEE\xE3\xF0\xE0\xEC\xEC\xFB,\n\xEF\xE0\xEF\xEA\xE8 \xE8\xEB\xE8 \xE4\xEE\xEA\xF3\xEC\xE5\xED\xF2\xE0.",
    [STR_ERR_NO_SDCARD]  = "\xCD\xE5\xF2 SD-\xEA\xE0\xF0\xF2\xFB.\n\xC2\xF1\xF2\xE0\xE2\xFC\xF2\xE5 SD-\xEA\xE0\xF0\xF2\xF3\n\xE8 \xEF\xEE\xEF\xF0\xEE\xE1\xF3\xE9\xF2\xE5 \xF1\xED\xEE\xE2\xE0.",
    [STR_ERR_FILE_NOT_FOUND] = "\xCD\xE5 \xF3\xE4\xE0\xEB\xEE\xF1\xFC \xED\xE0\xE9\xF2\xE8 '%s'.\n\n\xCF\xF0\xEE\xE2\xE5\xF0\xFC\xF2\xE5 \xE8\xEC\xFF \xE8\n\xEF\xEE\xEF\xF0\xEE\xE1\xF3\xE9\xF2\xE5 \xF1\xED\xEE\xE2\xE0.",

    /* Network */
    [STR_NET_CONN_FAILED]  = "\xCE\xF8\xE8\xE1\xEA\xE0 \xEF\xEE\xE4\xEA\xEB\xFE\xF7\xE5\xED\xE8\xFF.\n\n\xCF\xF0\xEE\xE2\xE5\xF0\xFC\xF2\xE5 \xEF\xE0\xF0\xEE\xEB\xFC\n\xE8 \xEF\xEE\xEF\xF0\xEE\xE1\xF3\xE9\xF2\xE5 \xF1\xED\xEE\xE2\xE0.",
    [STR_WIFI_PASSWORD]    = "\xCF\xE0\xF0\xEE\xEB\xFC WiFi",
    [STR_ENTER_PASSWORD]   = "\xC2\xE2\xE5\xE4\xE8\xF2\xE5 \xEF\xE0\xF0\xEE\xEB\xFC:",
    [STR_CONNECTING_TO]    = "\xCF\xEE\xE4\xEA\xEB\xFE\xF7\xE5\xED\xE8\xE5 \xEA %s...",
    [STR_CONNECTING]       = "\xCF\xEE\xE4\xEA\xEB\xFE\xF7\xE5\xED\xE8\xE5...",
    [STR_CONNECTED_TO]     = "\xCF\xEE\xE4\xEA\xEB\xFE\xF7\xE5\xED\xEE: %s (%s)",
    [STR_NO_ADAPTER]       = "\xD1\xE5\xF2\xE5\xE2\xEE\xE9 \xE0\xE4\xE0\xEF\xF2\xE5\xF0 \xED\xE5 \xED\xE0\xE9\xE4\xE5\xED",
    [STR_NOT_CONNECTED]    = "\xCD\xE5 \xEF\xEE\xE4\xEA\xEB\xFE\xF7\xE5\xED\xEE",
    [STR_SCANNING]         = "\xCF\xEE\xE8\xF1\xEA \xF1\xE5\xF2\xE5\xE9...",
    [STR_NO_NETWORKS]      = "\xD1\xE5\xF2\xE8 \xED\xE5 \xED\xE0\xE9\xE4\xE5\xED\xFB.",
    [STR_SCAN]             = "\xCF\xEE\xE8\xF1\xEA",
    [STR_CONNECT]          = "\xCF\xEE\xE4\xEA\xEB\xFE\xF7\xE8\xF2\xFC",
    [STR_HDR_NETWORK]      = "\xD1\xE5\xF2\xFC",
    [STR_HDR_SIGNAL]       = "\xD1\xE8\xE3\xED\xE0\xEB",
    [STR_HDR_TYPE]         = "\xD2\xE8\xEF",
    [STR_ERR_NO_NET_ADAPTER] = "\xD1\xE5\xF2\xE5\xE2\xEE\xE9 \xE0\xE4\xE0\xEF\xF2\xE5\xF0 \xED\xE5 \xED\xE0\xE9\xE4\xE5\xED.\n\n\xC2\xF1\xF2\xE0\xE2\xFC\xF2\xE5 \xF1\xE5\xF2\xE5\xE2\xEE\xE9 \xE0\xE4\xE0\xEF\xF2\xE5\xF0\n\xE8 \xEF\xE5\xF0\xE5\xE7\xE0\xE3\xF0\xF3\xE7\xE8\xF2\xE5.",

    /* File manager - toolbar */
    [STR_FM_BACK]          = "\xCD\xE0\xE7\xE0\xE4",
    [STR_FM_UP]            = "\xC2\xE2\xE5\xF0\xF5",
    [STR_FM_CUT]           = "\xC2\xFB\xF0\xE5\xE7\xE0\xF2\xFC",
    [STR_FM_COPY]          = "\xCA\xEE\xEF\xE8\xF0\xEE\xE2\xE0\xF2\xFC",
    [STR_FM_PASTE]         = "\xC2\xF1\xF2\xE0\xE2\xE8\xF2\xFC",
    [STR_FM_DELETE]        = "\xD3\xE4\xE0\xEB\xE8\xF2\xFC",

    /* File manager - context menu */
    [STR_FM_OPEN_WITH]     = "\xCE\xF2\xEA\xF0\xFB\xF2\xFC \xE2",
    [STR_FM_RENAME]        = "\xCF\xE5\xF0\xE5\xE8\xEC\xE5\xED\xEE\xE2\xE0\xF2\xFC",
    [STR_FM_NEW_FOLDER]    = "\xCD\xEE\xE2\xE0\xFF \xEF\xE0\xEF\xEA\xE0",
    [STR_FM_REFRESH]       = "\xCE\xE1\xED\xEE\xE2\xE8\xF2\xFC",

    /* File manager - menu bar */
    [STR_FILE]             = "\xD4\xE0\xE9\xEB",
    [STR_EDIT]             = "\xCF\xF0\xE0\xE2\xEA\xE0",
    [STR_VIEW]             = "\xC2\xE8\xE4",
    [STR_HELP]             = "\xD1\xEF\xF0\xE0\xE2\xEA\xE0",
    [STR_FM_NEW_FOLDER_MENU] = "\xCD\xEE\xE2\xE0\xFF \xEF\xE0\xEF\xEA\xE0",
    [STR_FM_DELETE_MENU]   = "\xD3\xE4\xE0\xEB\xE8\xF2\xFC Del",
    [STR_FM_RENAME_MENU]   = "\xCF\xE5\xF0\xE5\xE8\xEC\xE5\xED\xEE\xE2\xE0\xF2\xFC F2",
    [STR_FM_CLOSE_MENU]    = "\xC7\xE0\xEA\xF0\xFB\xF2\xFC",
    [STR_FM_CUT_MENU]      = "\xC2\xFB\xF0\xE5\xE7\xE0\xF2\xFC Ctrl+X",
    [STR_FM_COPY_MENU]     = "\xCA\xEE\xEF\xE8\xF0\xEE\xE2\xE0\xF2\xFC Ctrl+C",
    [STR_FM_PASTE_MENU]    = "\xC2\xF1\xF2\xE0\xE2\xE8\xF2\xFC Ctrl+V",
    [STR_FM_SELALL_MENU]   = "\xC2\xFB\xE4\xE5\xEB\xE8\xF2\xFC Ctrl+A",
    [STR_FM_LARGE_ICONS]   = "\xCA\xF0\xF3\xEF\xED\xFB\xE5 \xE7\xED\xE0\xF7\xEA\xE8",
    [STR_FM_SMALL_ICONS]   = "\xCC\xE5\xEB\xEA\xE8\xE5 \xE7\xED\xE0\xF7\xEA\xE8",
    [STR_FM_LIST]          = "\xD1\xEF\xE8\xF1\xEE\xEA",
    [STR_FM_REFRESH_MENU]  = "\xCE\xE1\xED\xEE\xE2\xE8\xF2\xFC F5",
    [STR_FM_ABOUT_MENU]    = "\xCE \xEF\xF0\xEE\xE3\xF0\xE0\xEC\xEC\xE5  F1",

    /* File manager - columns + types */
    [STR_FM_NAME]          = "\xC8\xEC\xFF",
    [STR_FM_SIZE]          = "\xD0\xE0\xE7\xEC\xE5\xF0",
    [STR_FM_TYPE]          = "\xD2\xE8\xEF",
    [STR_FM_FOLDER]        = "\xCF\xE0\xEF\xEA\xE0",
    [STR_FM_APPLICATION]   = "\xCF\xF0\xE8\xEB\xEE\xE6\xE5\xED\xE8\xE5",
    [STR_FM_FILE_TYPE]     = "\xD4\xE0\xE9\xEB",
    [STR_FM_OBJECTS]       = "\xEE\xE1\xFA\xE5\xEA\xF2\xEE\xE2",

    /* File manager - dialogs */
    [STR_CONFIRM_DELETE]   = "\xCF\xEE\xE4\xF2\xE2\xE5\xF0\xE6\xE4\xE5\xED\xE8\xE5",
    [STR_FM_DELETE_ITEMS]  = "\xD3\xE4\xE0\xEB\xE8\xF2\xFC \xE2\xFB\xE1\xF0\xE0\xED\xED\xFB\xE5?",
    [STR_FM_NO_APP_REGISTERED] = "\xCD\xE5\xF2 \xEF\xF0\xE8\xEB\xEE\xE6\xE5\xED\xE8\xFF\n\xE4\xEB\xFF \xFD\xF2\xEE\xE3\xEE \xF2\xE8\xEF\xE0 \xF4\xE0\xE9\xEB\xEE\xE2.",
    [STR_ABOUT_NAVIGATOR]  = "\xCE \xCD\xE0\xE2\xE8\xE3\xE0\xF2\xEE\xF0\xE5",
    [STR_FM_DELETING]      = "\xD3\xE4\xE0\xEB\xE5\xED\xE8\xE5",
    [STR_FM_COPYING]       = "\xCA\xEE\xEF\xE8\xF0\xEE\xE2\xE0\xED\xE8\xE5",
    [STR_FM_NEW_FOLDER_DLG] = "\xCD\xEE\xE2\xE0\xFF \xEF\xE0\xEF\xEA\xE0",
    [STR_FM_RENAME_DLG]    = "\xCF\xE5\xF0\xE5\xE8\xEC\xE5\xED\xEE\xE2\xE0\xF2\xFC",
    [STR_FM_ENTER_NAME]    = "\xC8\xEC\xFF:",
    [STR_FM_NEW_NAME]      = "\xCD\xEE\xE2\xEE\xE5 \xE8\xEC\xFF:",
    [STR_FM_ALL_FILES]     = "\xC2\xF1\xE5 \xF4\xE0\xE9\xEB\xFB (*.*)",

    /* Terminal */
    [STR_FM_EXIT]          = "\xC2\xFB\xF5\xEE\xE4",
    [STR_ABOUT_TERMINAL]   = "\xCE \xD2\xE5\xF0\xEC\xE8\xED\xE0\xEB\xE5",

    /* Desktop context menu */
    [STR_SEND_TO_DESKTOP]  = "\xCD\xE0 \xF0\xE0\xE1\xEE\xF7\xE8\xE9 \xF1\xF2\xEE\xEB",
    [STR_DT_REMOVE]        = "\xD3\xE4\xE0\xEB\xE8\xF2\xFC",
    [STR_DT_SORT_BY_NAME]  = "\xD1\xEE\xF0\xF2\xE8\xF0\xEE\xE2\xE0\xF2\xFC",
    [STR_DT_REFRESH]       = "\xCE\xE1\xED\xEE\xE2\xE8\xF2\xFC",

    /* Alt+Tab */
    [STR_ALTTAB_DESKTOP]   = "\xD0\xE0\xE1\xEE\xF7\xE8\xE9 \xF1\xF2\xEE\xEB",

    /* Reboot/firmware dialogs */
    [STR_LAUNCH_FIRMWARE]  = "\xC7\xE0\xEF\xF3\xF1\xEA \xEF\xF0\xEE\xF8\xE8\xE2\xEA\xE8",
    [STR_LAUNCH_FW_MSG]    = "\xC7\xE0\xEF\xF3\xF1\xF2\xE8\xF2\xFC \"%s\"?...\n\n\xDD\xEA\xF0\xE0\xED \xEE\xF2\xEA\xEB\xFE\xF7\xE8\xF2\xF1\xFF...\n\n\xC4\xEB\xFF \xE2\xEE\xE7\xE2\xF0\xE0\xF2\xE0 \xE2 FRANK OS,\n\xF3\xE4\xE5\xF0\xE6\xE8\xE2\xE0\xE9\xF2\xE5 Space\n\xE8 \xED\xE0\xE6\xEC\xE8\xF2\xE5 Reset...",
    [STR_REBOOT_CONFIRM]   = "\xC2\xFB \xF3\xE2\xE5\xF0\xE5\xED\xFB, \xF7\xF2\xEE \xF5\xEE\xF2\xE8\xF2\xE5\n\xEF\xE5\xF0\xE5\xE7\xE0\xE3\xF0\xF3\xE7\xE8\xF2\xFC \xF1\xE8\xF1\xF2\xE5\xEC\xF3?",
    [STR_FLASHING_FW]      = "\xC7\xE0\xEF\xE8\xF1\xFC \xEF\xF0\xEE\xF8\xE8\xE2\xEA\xE8...",

    /* No UF2 */
    [STR_NO_UF2]           = "(\xED\xE5\xF2 \xF4\xE0\xE9\xEB\xEE\xE2 .uf2)",

    /* Run dialog extras */
    [STR_BROWSE]           = "\xCE\xE1\xE7\xEE\xF0...",
    [STR_OPEN_LABEL]       = "\xCE\xF2\xEA\xF0\xFB\xF2\xFC:",
    [STR_FILES_OF_TYPE]    = "\xD2\xE8\xEF \xF4\xE0\xE9\xEB\xEE\xE2:",

    /* Dialog buttons */
    [STR_DLG_OK]           = "OK",
    [STR_DLG_CANCEL]       = "\xCE\xF2\xEC\xE5\xED\xE0",
    [STR_DLG_YES]          = "\xC4\xE0",
    [STR_DLG_NO]           = "\xCD\xE5\xF2",
};

uint8_t lang_get(void) {
    return settings_get()->language;
}

void lang_set(uint8_t lang) {
    settings_get()->language = lang;
}

const char *L(int str_id) {
    if (settings_get()->language == LANG_RU)
        return str_ru[str_id];
    return str_en[str_id];
}
