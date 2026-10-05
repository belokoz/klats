#pragma once
// Resource ids shared by klats.rc, dialogs.rc2 and the code. Dialog texts are set in code from
// strings.cpp, so the templates only carry the layout.

#define IDI_APP 1

#define IDD_SETTINGS 100
#define IDD_ONBOARDING 101
#define IDD_ABOUT 102

// SysLink style: the control asks its parent before drawing each piece of text (commctrl.h has it,
// the resource compiler does not see it).
#ifndef LWS_USECUSTOMTEXT
#define LWS_USECUSTOMTEXT 0x0010
#endif

// Settings
#define IDC_HOTKEYS_HEADER 1000
#define IDC_LAYOUT_LABEL 1001
#define IDC_LAYOUT_HOTKEY 1002
#define IDC_LAYOUT_RESET 1003
#define IDC_CASE_LABEL 1004
#define IDC_CASE_HOTKEY 1005
#define IDC_CASE_RESET 1006
#define IDC_HOTKEYS_NOTE 1007
#define IDC_HOTKEY_ERROR 1008
#define IDC_LAYOUT_HEADER 1010
#define IDC_PAIR_LABEL 1011
#define IDC_PAIR_TEXT 1012
#define IDC_FIRST_LAYOUT 1013
#define IDC_SECOND_LABEL 1014
#define IDC_SECOND_LAYOUT 1015
#define IDC_SWITCH_LAYOUT 1016
#define IDC_CASE_HEADER 1020
#define IDC_CASE_INVERT 1021
#define IDC_CASE_INVERT_EXAMPLE 1022
#define IDC_CASE_LOWER 1023
#define IDC_CASE_LOWER_EXAMPLE 1024
#define IDC_GENERAL_HEADER 1030
#define IDC_AUTOSTART 1031
#define IDC_AUTOSTART_NOTE 1032
#define IDC_CHECK_UPDATES 1033
#define IDC_FOOTER_REPO 1040
#define IDC_FOOTER_DIKTUY 1041

// First run
#define IDC_OB_ICON 1100
#define IDC_OB_TITLE 1101
#define IDC_OB_SUBTITLE 1102
#define IDC_OB_TRY_HEADER 1103
#define IDC_OB_TRY_TEXT 1104
#define IDC_OB_PROBE 1105
#define IDC_OB_TRY_RESULT 1106
#define IDC_OB_WHERE_HEADER 1107
#define IDC_OB_WHERE_TEXT 1108
#define IDC_OB_AUTOSTART 1110
#define IDC_OB_ADMIN_NOTE 1111

// About
#define IDC_ABOUT_ICON 1200
#define IDC_ABOUT_TITLE 1201
#define IDC_ABOUT_VERSION 1202
#define IDC_ABOUT_DESCRIPTION 1203
#define IDC_ABOUT_REPO 1204
#define IDC_ABOUT_DIKTUY 1205
#define IDC_ABOUT_LICENSE 1206
