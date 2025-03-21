﻿// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "bitmap.h"
#include "toolbar.h"
#include "mainwnd.h"
//#include "usermenu.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"

#include "nanosvg\nanosvg.h"
#include "nanosvg\nanosvgrast.h"
#include "svg.h"

struct CButtonData
{
    unsigned int ImageIndex : 16;   // zero base index
    unsigned int Shell32ResID : 8;  // 0: ikona z image listu; 1..254: resID ikony z shell32.dll; 255: pouze alokovat volny prostor
    unsigned int ToolTipResID : 16; // resID se stringem pro tooltip
    unsigned int ID : 16;           // univerzalni Command
    unsigned int LeftID : 16;       // Command pro levy panel
    unsigned int RightID : 16;      // Command pro pravy panel
    unsigned int DropDown : 1;      // bude mit drop down?
    unsigned int WholeDropDown : 1; // bude mit whole drop down?
    unsigned int Check : 1;         // jedna se o checkbox?
    DWORD* Enabler;                 // ridici promenna pro enablovani tlacitka
    DWORD* LeftEnabler;             // ridici promenna pro enablovani tlacitka
    DWORD* RightEnabler;            // ridici promenna pro enablovani tlacitka
    const char* SVGName;            // NULL pokud tlacitko nema SVG reprezentaci
    unsigned int BottomLeftID : 16;       // Command pro dolu levy panel
    unsigned int BottomRightID : 16;      // Command pro dolu pravy panel
    DWORD* BottomLeftEnabler;             // ridici promenna pro enablovani tlacitka
    DWORD* BottomRightEnabler;            // ridici promenna pro enablovani tlacitka
};

//****************************************************************************
//
// TBButtonEnum
//
// Unikatni indexy do pole ToolBarButton -  slouzi k adresaci tohoto pole.
// Do tohoto pole lze pouze pridavat na konec.
//

#define TBBE_CONNECT_NET 0
#define TBBE_DISCONNECT_NET 1
#define TBBE_CREATE_DIR 2
#define TBBE_FIND_FILE 3
#define TBBE_VIEW_MODE 4 // drive brief
//#define TBBE_DETAILED            5  // vyrazeno
#define TBBE_SORT_NAME 6
#define TBBE_SORT_EXT 7
#define TBBE_SORT_SIZE 8
#define TBBE_SORT_DATE 9
//#define TBBE_SORT_ATTR          10  // vyrazeno
#define TBBE_PARENT_DIR 11
#define TBBE_ROOT_DIR 12
#define TBBE_FILTER 13
#define TBBE_BACK 14
#define TBBE_FORWARD 15
#define TBBE_REFRESH 16
#define TBBE_SWAP_PANELS 17
#define TBBE_PROPERTIES 18
#define TBBE_USER_MENU_DROP 19
#define TBBE_CMD 20
#define TBBE_COPY 21
#define TBBE_MOVE 22
#define TBBE_DELETE 23
#define TBBE_COMPRESS 24
#define TBBE_UNCOMPRESS 25
#define TBBE_QUICK_RENAME 26
#define TBBE_CHANGE_CASE 27
#define TBBE_VIEW 28
#define TBBE_EDIT 29
#define TBBE_CLIPBOARD_CUT 30
#define TBBE_CLIPBOARD_COPY 31
#define TBBE_CLIPBOARD_PASTE 32
#define TBBE_CHANGE_ATTR 33
#define TBBE_COMPARE_DIR 34
#define TBBE_DRIVE_INFO 35
#define TBBE_CHANGE_DRIVE_L 36
#define TBBE_SELECT 37
#define TBBE_UNSELECT 38
#define TBBE_INVERT_SEL 39
#define TBBE_SELECT_ALL 40
#define TBBE_PACK 41
#define TBBE_UNPACK 42
#define TBBE_OPEN_ACTIVE 43
#define TBBE_OPEN_DESKTOP 44
#define TBBE_OPEN_MYCOMP 45
#define TBBE_OPEN_CONTROL 46
#define TBBE_OPEN_PRINTERS 47
#define TBBE_OPEN_NETWORK 48
#define TBBE_OPEN_RECYCLE 49
#define TBBE_OPEN_FONTS 50
#define TBBE_CHANGE_DRIVE_R 51
#define TBBE_HELP_CONTENTS 52
#define TBBE_HELP_CONTEXT 53
#define TBBE_PERMISSIONS 54
#define TBBE_CONVERT 55
#define TBBE_UNSELECT_ALL 56
#define TBBE_MENU 57 // vstup do menu
#define TBBE_ALTVIEW 58
#define TBBE_EXIT 59
#define TBBE_OCCUPIEDSPACE 60
#define TBBE_EDITNEW 61
#define TBBE_CHANGEDIR 62
#define TBBE_HOTPATHS 63
#define TBBE_CONTEXTMENU 64
#define TBBE_VIEWWITH 65
#define TBBE_EDITWITH 66
#define TBBE_CALCDIRSIZES 67
#define TBBE_FILEHISTORY 68
#define TBBE_DIRHISTORY 69
#define TBBE_HOTPATHSDROP 70
#define TBBE_USER_MENU 71
#define TBBE_EMAIL 72
#define TBBE_ZOOM_PANEL 73
#define TBBE_SHARES 74
#define TBBE_FULLSCREEN 75
#define TBBE_PREV_SELECTED 76
#define TBBE_NEXT_SELECTED 77
#define TBBE_RESELECT 78
#define TBBE_PASTESHORTCUT 79
#define TBBE_FOCUSSHORTCUT 80
#define TBBE_SAVESELECTION 81
#define TBBE_LOADSELECTION 82
#define TBBE_NEW 83
#define TBBE_SEL_BY_EXT 84
#define TBBE_UNSEL_BY_EXT 85
#define TBBE_SEL_BY_NAME 86
#define TBBE_UNSEL_BY_NAME 87
#define TBBE_OPEN_FOLDER 88
#define TBBE_CONFIGRATION 89
#define TBBE_DIRMENU 90
#define TBBE_OPEN_IN_OTHER_ACT 91
#define TBBE_OPEN_IN_OTHER 92
#define TBBE_AS_OTHER_PANEL 93
#define TBBE_HIDE_SELECTED 94
#define TBBE_HIDE_UNSELECTED 95
#define TBBE_SHOW_ALL 96
#define TBBE_OPEN_MYDOC 97
#define TBBE_SMART_COLUMN_MODE 98
#define TBBE_CHANGE_DRIVE_BL 99         //  added for bottom panels
#define TBBE_CHANGE_DRIVE_BR 100        //  added for bottom panels

#define TBBE_TERMINATOR 101 // terminator

#define NIB1(x) x
#define NIB2(x) x,
#define NIB3(x) x

CButtonData ToolBarButtons[TBBE_TERMINATOR] =
    {
        //                    ImageIndex, Shell32ResID, ToolTipResID, ID, LeftID, RightID, DropDown, WD, Chk, Enabler, LeftEnabler, RightEnabler, SVGName, BottomLeftID, BottomRightID, BottomLeftEnabler, BottomRightEnabler,
        /*TBBE_CONNECT_NET*/ {NIB1(IDX_TB_CONNECTNET), 0, IDS_TBTT_CONNECTNET, CM_CONNECTNET, 0, 0, 0, 0, 0, NULL, NULL, NULL, "ConnectNetworkDrive", 0, 0, NULL, NULL},
        /*TBBE_DISCONNECT_NET*/ {IDX_TB_DISCONNECTNET, 0, IDS_TBTT_DISCONNECTNET, CM_DISCONNECTNET, 0, 0, 0, 0, 0, NULL, NULL, NULL, "Disconnect", 0, 0, NULL, NULL},
        /*TBBE_CREATE_DIR*/ {IDX_TB_CREATEDIR, 0, IDS_TBTT_CREATEDIR, CM_CREATEDIR, 0, 0, 0, 0, 0, &EnablerCreateDir, NULL, NULL, "CreateDirectory", 0, 0, NULL, NULL},
        /*TBBE_FIND_FILE*/ {IDX_TB_FINDFILE, 0, IDS_TBTT_FINDFILE, CM_FINDFILE, 0, 0, 0, 0, 0, NULL, NULL, NULL, "FindFilesAndDirectories", 0, 0, NULL, NULL},
        /*TBBE_VIEW_MODE*/ {IDX_TB_VIEW_MODE, 0, IDS_TBTT_VIEW_MODE, CM_ACTIVEVIEWMODE, CM_LEFTVIEWMODE, CM_RIGHTVIEWMODE, 0, 1, 0, NULL, NULL, NULL, "Views", CM_BOTTOMLEFTVIEWMODE, CM_BOTTOMRIGHTVIEWMODE, NULL, NULL},
        /*TBBE_DETAILED*/ {0xFFFF, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_SORT_NAME*/ {IDX_TB_SORTBYNAME, 0, IDS_TBTT_SORTBYNAME, CM_ACTIVENAME, CM_LEFTNAME, CM_RIGHTNAME, 0, 0, 1, NULL, NULL, NULL, "SortByName", CM_BOTTOMLEFTNAME, CM_BOTTOMRIGHTNAME, NULL, NULL},
        /*TBBE_SORT_EXT*/ {IDX_TB_SORTBYEXT, 0, IDS_TBTT_SORTBYEXT, CM_ACTIVEEXT, CM_LEFTEXT, CM_RIGHTEXT, 0, 0, 1, NULL, NULL, NULL, "SortByExtension", CM_BOTTOMLEFTEXT, CM_BOTTOMRIGHTEXT, NULL, NULL},
        /*TBBE_SORT_SIZE*/ {IDX_TB_SORTBYSIZE, 0, IDS_TBTT_SORTBYSIZE, CM_ACTIVESIZE, CM_LEFTSIZE, CM_RIGHTSIZE, 0, 0, 1, NULL, NULL, NULL, "SortBySize", CM_BOTTOMLEFTSIZE, CM_BOTTOMRIGHTSIZE, NULL, NULL},
        /*TBBE_SORT_DATE*/ {IDX_TB_SORTBYDATE, 0, IDS_TBTT_SORTBYDATE, CM_ACTIVETIME, CM_LEFTTIME, CM_RIGHTTIME, 0, 0, 1, NULL, NULL, NULL, "SortByDate", CM_BOTTOMLEFTTIME, CM_BOTTOMRIGHTTIME, NULL, NULL},
        /*TBBE_SORT_ATTR*/ {0xFFFF, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_PARENT_DIR*/ {IDX_TB_PARENTDIR, 0, IDS_TBTT_PARENTDIR, CM_ACTIVEPARENTDIR, CM_LPARENTDIR, CM_RPARENTDIR, 0, 0, 0, &EnablerUpDir, &EnablerLeftUpDir, &EnablerRightUpDir, "ParentDirectory", CM_BLPARENTDIR, CM_BRPARENTDIR, &EnablerBottomLeftUpDir, &EnablerBottomRightUpDir},
        /*TBBE_ROOT_DIR*/ {IDX_TB_ROOTDIR, 0, IDS_TBTT_ROOTDIR, CM_ACTIVEROOTDIR, CM_LROOTDIR, CM_RROOTDIR, 0, 0, 0, &EnablerRootDir, &EnablerLeftRootDir, &EnablerRightRootDir, "RootDirectory", CM_BLROOTDIR, CM_BRROOTDIR, &EnablerBottomLeftRootDir, &EnablerBottomRightRootDir},
        /*TBBE_FILTER*/ {IDX_TB_FILTER, 0, IDS_TBTT_FILTER, CM_CHANGEFILTER, CM_LCHANGEFILTER, CM_RCHANGEFILTER, 0, 0, 0, NULL, NULL, NULL, "Filter", CM_BLCHANGEFILTER, CM_BRCHANGEFILTER, NULL, NULL},
        /*TBBE_BACK*/ {IDX_TB_BACK, 0, IDS_TBTT_BACK, CM_ACTIVEBACK, CM_LBACK, CM_RBACK, 1, 0, 0, &EnablerBackward, &EnablerLeftBackward, &EnablerRightBackward, "Back", CM_BLBACK, CM_BRBACK, &EnablerBottomLeftBackward, &EnablerBottomRightBackward},
        /*TBBE_FORWARD*/ {IDX_TB_FORWARD, 0, IDS_TBTT_FORWARD, CM_ACTIVEFORWARD, CM_LFORWARD, CM_RFORWARD, 1, 0, 0, &EnablerForward, &EnablerLeftForward, &EnablerRightForward, "Forward", CM_BLFORWARD, CM_BRFORWARD, &EnablerBottomLeftForward, &EnablerBottomRightForward},
        /*TBBE_REFRESH*/ {IDX_TB_REFRESH, 0, IDS_TBTT_REFRESH, CM_ACTIVEREFRESH, CM_LEFTREFRESH, CM_RIGHTREFRESH, 0, 0, 0, NULL, NULL, NULL, "Refresh", CM_BOTTOMLEFTREFRESH, CM_BOTTOMRIGHTREFRESH, NULL, NULL},
        /*TBBE_SWAP_PANELS*/ {IDX_TB_SWAPPANELS, 0, IDS_TBTT_SWAPPANELS, CM_SWAPPANELS, 0, 0, 0, 0, 0, NULL, NULL, NULL, "SwapPanels", 0, 0, NULL, NULL},
        /*TBBE_PROPERTIES*/ {IDX_TB_PROPERTIES, 0, IDS_TBTT_PROPERTIES, CM_PROPERTIES, 0, 0, 0, 0, 0, &EnablerShowProperties, NULL, NULL, "Properties", 0, 0, NULL, NULL},
        /*TBBE_USER_MENU_DROP*/ {IDX_TB_USERMENU, 0, IDS_TBTT_USERMENU, CM_USERMENUDROP, 0, 0, 0, 1, 0, &EnablerOnDisk, NULL, NULL, "UserMenu", 0, 0, NULL, NULL},
        /*TBBE_CMD*/ {IDX_TB_COMMANDSHELL, 0, IDS_TBTT_COMMANDSHELL, CM_DOSSHELL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "CommandShell", 0, 0, NULL, NULL},
        /*TBBE_COPY*/ {IDX_TB_COPY, 0, IDS_TBTT_COPY, CM_COPYFILES, 0, 0, 0, 0, 0, &EnablerFilesCopy, NULL, NULL, "Copy", 0, 0, NULL, NULL},
        /*TBBE_MOVE*/ {IDX_TB_MOVE, 0, IDS_TBTT_MOVE, CM_MOVEFILES, 0, 0, 0, 0, 0, &EnablerFilesMove, NULL, NULL, "Move", 0, 0, NULL, NULL},
        /*TBBE_DELETE*/ {IDX_TB_DELETE, 0, IDS_TBTT_DELETE, CM_DELETEFILES, 0, 0, 0, 0, 0, &EnablerFilesDelete, NULL, NULL, "Delete", 0, 0, NULL, NULL},
        /*TBBE_COMPRESS*/ {IDX_TB_COMPRESS, 0, IDS_TBTT_COMPRESS, CM_COMPRESS, 0, 0, 0, 0, 0, &EnablerFilesOnDiskCompress, NULL, NULL, "NTFSCompress", 0, 0, NULL, NULL},
        /*TBBE_UNCOMPRESS*/ {IDX_TB_UNCOMPRESS, 0, IDS_TBTT_UNCOMPRESS, CM_UNCOMPRESS, 0, 0, 0, 0, 0, &EnablerFilesOnDiskCompress, NULL, NULL, "NTFSUncompress", 0, 0, NULL, NULL},
        /*TBBE_QUICK_RENAME*/ {IDX_TB_QUICKRENAME, 0, IDS_TBTT_QUICKRENAME, CM_RENAMEFILE, 0, 0, 0, 0, 0, &EnablerQuickRename, NULL, NULL, "QuickRename", 0, 0, NULL, NULL},
        /*TBBE_CHANGE_CASE*/ {IDX_TB_CHANGECASE, 0, IDS_TBTT_CHANGECASE, CM_CHANGECASE, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "ChangeCase", 0, 0, NULL, NULL},
        /*TBBE_VIEW*/ {IDX_TB_VIEW, 0, IDS_TBTT_VIEW, CM_VIEW, 0, 0, 1, 0, 0, &EnablerViewFile, NULL, NULL, "View", 0, 0, NULL, NULL},
        /*TBBE_EDIT*/ {IDX_TB_EDIT, 0, IDS_TBTT_EDIT, CM_EDIT, 0, 0, 1, 0, 0, &EnablerFileOnDiskOrArchive, NULL, NULL, "Edit", 0, 0, NULL, NULL},
        /*TBBE_CLIPBOARD_CUT*/ {IDX_TB_CLIPBOARDCUT, 0, IDS_TBTT_CLIPBOARDCUT, CM_CLIPCUT, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "ClipboardCut", 0, 0, NULL, NULL},
        /*TBBE_CLIPBOARD_COPY*/ {IDX_TB_CLIPBOARDCOPY, 0, IDS_TBTT_CLIPBOARDCOPY, CM_CLIPCOPY, 0, 0, 0, 0, 0, &EnablerFilesOnDiskOrArchive, NULL, NULL, "ClipboardCopy", 0, 0, NULL, NULL},
        /*TBBE_CLIPBOARD_PASTE*/ {IDX_TB_CLIPBOARDPASTE, 0, IDS_TBTT_CLIPBOARDPASTE, CM_CLIPPASTE, 0, 0, 0, 0, 0, &EnablerPaste, NULL, NULL, "ClipboardPaste", 0, 0, NULL, NULL},
        /*TBBE_CHANGE_ATTR*/ {IDX_TB_CHANGEATTR, 0, IDS_TBTT_CHANGEATTR, CM_CHANGEATTR, 0, 0, 0, 0, 0, &EnablerChangeAttrs, NULL, NULL, "ChangeAttributes", 0, 0, NULL, NULL},
        /*TBBE_COMPARE_DIR*/ {IDX_TB_COMPAREDIR, 0, IDS_TBTT_COMPAREDIR, CM_COMPAREDIRS, 0, 0, 0, 0, 0, NULL, NULL, NULL, "CompareDirectories", 0, 0, NULL, NULL},
        /*TBBE_DRIVE_INFO*/ {IDX_TB_DRIVEINFO, 0, IDS_TBTT_DRIVEINFO, CM_DRIVEINFO, 0, 0, 0, 0, 0, &EnablerDriveInfo, NULL, NULL, "DriveInformation", 0, 0, NULL, NULL},
        /*TBBE_CHANGE_DRIVE_L*/ {IDX_TB_CHANGEDRIVEL, 255, IDS_TBTT_CHANGEDRIVE, CM_LCHANGEDRIVE, CM_LCHANGEDRIVE, 0, 0, 1, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_SELECT*/ {IDX_TB_SELECT, 0, IDS_TBTT_SELECT, CM_ACTIVESELECT, 0, 0, 0, 0, 0, NULL, NULL, NULL, "Select", 0, 0, NULL, NULL},
        /*TBBE_UNSELECT*/ {IDX_TB_UNSELECT, 0, IDS_TBTT_UNSELECT, CM_ACTIVEUNSELECT, 0, 0, 0, 0, 0, &EnablerSelected, NULL, NULL, "Unselect", 0, 0, NULL, NULL},
        /*TBBE_INVERT_SEL*/ {IDX_TB_INVERTSEL, 0, IDS_TBTT_INVERTSEL, CM_ACTIVEINVERTSEL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "InvertSelection", 0, 0, NULL, NULL},
        /*TBBE_SELECT_ALL*/ {IDX_TB_SELECTALL, 0, IDS_TBTT_SELECTALL, CM_ACTIVESELECTALL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "SelectAll", 0, 0, NULL, NULL},
        /*TBBE_PACK*/ {IDX_TB_PACK, 0, IDS_TBTT_PACK, CM_PACK, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "Pack", 0, 0, NULL, NULL},
        /*TBBE_UNPACK*/ {IDX_TB_UNPACK, 0, IDS_TBTT_UNPACK, CM_UNPACK, 0, 0, 0, 0, 0, &EnablerFileOnDisk, NULL, NULL, "Unpack", 0, 0, NULL, NULL},
        /*TBBE_OPEN_ACTIVE*/ {NIB1(IDX_TB_OPENACTIVE), 5, IDS_TBTT_OPENACTIVE, CM_OPENACTUALFOLDER, 0, 0, 0, 0, 0, &EnablerOpenActiveFolder, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OPEN_DESKTOP*/ {NIB1(IDX_TB_OPENDESKTOP), 35, IDS_TBTT_OPENDESKTOP, CM_OPENDESKTOP, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OPEN_MYCOMP*/ {NIB1(IDX_TB_OPENMYCOMP), 16, IDS_TBTT_OPENMYCOMP, CM_OPENMYCOMP, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OPEN_CONTROL*/ {NIB1(IDX_TB_OPENCONTROL), 137, IDS_TBTT_OPENCONTROL, CM_OPENCONROLPANEL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OPEN_PRINTERS*/ {NIB1(IDX_TB_OPENPRINTERS), 138, IDS_TBTT_OPENPRINTERS, CM_OPENPRINTERS, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OPEN_NETWORK*/ {NIB1(IDX_TB_OPENNETWORK), 18, IDS_TBTT_OPENNETWORK, CM_OPENNETNEIGHBOR, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OPEN_RECYCLE*/ {NIB1(IDX_TB_OPENRECYCLE), 33, IDS_TBTT_OPENRECYCLE, CM_OPENRECYCLEBIN, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OPEN_FONTS*/ {NIB1(IDX_TB_OPENFONTS), 39, IDS_TBTT_OPENFONTS, CM_OPENFONTS, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_CHANGE_DRIVE_R*/ {IDX_TB_CHANGEDRIVER, 255, IDS_TBTT_CHANGEDRIVE, CM_RCHANGEDRIVE, 0, CM_RCHANGEDRIVE, 0, 1, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_HELP_CONTENTS*/ {NIB1(IDX_TB_HELP), 0, IDS_TBTT_HELP, CM_HELP_CONTENTS, 0, 0, 0, 0, 0, NULL, NULL, NULL, "HelpContents", 0, 0, NULL, NULL},
        /*TBBE_HELP_CONTEXT*/ {NIB1(IDX_TB_CONTEXTHELP), 0, IDS_TBTT_CONTEXTHELP, CM_HELP_CONTEXT, 0, 0, 0, 0, 0, NULL, NULL, NULL, "WhatIsThis", 0, 0, NULL, NULL},
        /*TBBE_PERMISSIONS*/ {IDX_TB_PERMISSIONS, 0, IDS_TBTT_PERMISSIONS, CM_SEC_PERMISSIONS, 0, 0, 0, 0, 0, &EnablerPermissions, NULL, NULL, "Security", 0, 0, NULL, NULL},
        /*TBBE_CONVERT*/ {IDX_TB_CONVERT, 0, IDS_TBTT_CONVERT, CM_CONVERTFILES, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "Convert", 0, 0, NULL, NULL},
        /*TBBE_UNSELECT_ALL*/ {IDX_TB_UNSELECTALL, 0, IDS_TBTT_UNSELECTALL, CM_ACTIVEUNSELECTALL, 0, 0, 0, 0, 0, &EnablerSelected, NULL, NULL, "UnselectAll", 0, 0, NULL, NULL},
        /*TBBE_MENU*/ {0xFFFF, 0, IDS_TBTT_MENU, CM_MENU, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_ALTVIEW*/ {0xFFFF, 0, IDS_TBTT_ALTVIEW, CM_ALTVIEW, 0, 0, 0, 0, 0, &EnablerViewFile, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_EXIT*/ {0xFFFF, 0, IDS_TBTT_EXIT, CM_EXIT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OCCUPIEDSPACE*/ {IDX_TB_OCCUPIEDSPACE, 0, IDS_TBTT_OCCUPIEDSPACE, CM_OCCUPIEDSPACE, 0, 0, 0, 0, 0, &EnablerOccupiedSpace, NULL, NULL, "CalculateOccupiedSpace", 0, 0, NULL, NULL},
        /*TBBE_EDITNEW*/ {IDX_TB_EDITNEW, 0, IDS_TBTT_EDITNEW, CM_EDITNEW, 0, 0, 0, 0, 0, &EnablerOnDisk, NULL, NULL, "EditNewFile", 0, 0, NULL, NULL},
        /*TBBE_CHANGEDIR*/ {IDX_TB_CHANGE_DIR, 0, IDS_TBTT_CHANGEDIR, CM_ACTIVE_CHANGEDIR, CM_LEFT_CHANGEDIR, CM_RIGHT_CHANGEDIR, 0, 0, 0, NULL, NULL, NULL, "ChangeDirectory", 0, 0, NULL, NULL},
        /*TBBE_HOTPATHS*/ {0xFFFF, 0, IDS_TBTT_HOTPATHS, CM_OPENHOTPATHS, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_CONTEXTMENU*/ {0xFFFF, 0, IDS_TBTT_CONTEXTMENU, CM_CONTEXTMENU, 0, 0, 0, 0, 0, &EnablerItemsContextMenu, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_VIEWWITH*/ {0xFFFF, 0, IDS_TBTT_VIEWWITH, CM_VIEW_WITH, 0, 0, 0, 0, 0, &EnablerViewFile, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_EDITWITH*/ {0xFFFF, 0, IDS_TBTT_EDITWITH, CM_EDIT_WITH, 0, 0, 0, 0, 0, &EnablerFileOnDiskOrArchive, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_CALCDIRSIZES*/ {IDX_TB_CALCDIRSIZES, 0, IDS_TBTT_CALCDIRSIZES, CM_CALCDIRSIZES, 0, 0, 0, 0, 0, &EnablerCalcDirSizes, NULL, NULL, "CalculateDirectorySizes", 0, 0, NULL, NULL},
        /*TBBE_FILEHISTORY*/ {0xFFFF, 0, IDS_TBTT_FILEHISTORY, CM_FILEHISTORY, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_DIRHISTORY*/ {0xFFFF, 0, IDS_TBTT_DIRHISTORY, CM_DIRHISTORY, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_HOTPATHSDROP*/ {IDX_TB_HOTPATHS, 0, IDS_TBTT_HOTPATHSDROP, CM_OPENHOTPATHSDROP, 0, 0, 0, 1, 0, NULL, NULL, NULL, "GoToHotPath", 0, 0, NULL, NULL},
        /*TBBE_USER_MENU*/ {IDX_TB_USERMENU, 0, IDS_TBTT_USERMENU, CM_USERMENU, 0, 0, 0, 0, 0, &EnablerOnDisk, NULL, NULL, "UserMenu", 0, 0, NULL, NULL},
        /*TBBE_EMAIL*/ {IDX_TB_EMAIL, 0, IDS_TBTT_EMAIL, CM_EMAILFILES, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "Email", 0, 0, NULL, NULL},
        /*TBBE_ZOOM_PANEL*/ {0xFFFF, 0, IDS_TBTT_ZOOMPANEL, CM_ACTIVEZOOMPANEL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "SharedDirectories", 0, 0, NULL, NULL},
        /*TBBE_SHARES*/ {IDX_TB_SHARED_DIRS, 0, IDS_TBTT_SHARES, CM_SHARES, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_FULLSCREEN*/ {0xFFFF, 0, IDS_TBTT_FULLSCREEN, CM_FULLSCREEN, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_PREV_SELECTED*/ {IDX_TB_PREV_SELECTED, 0, IDS_TBTT_PREV_SEL, CM_GOTO_PREV_SEL, 0, 0, 0, 0, 0, &EnablerSelGotoPrev, NULL, NULL, "GoToPreviousSelectedName", 0, 0, NULL, NULL},
        /*TBBE_NEXT_SELECTED*/ {IDX_TB_NEXT_SELECTED, 0, IDS_TBTT_NEXT_SEL, CM_GOTO_NEXT_SEL, 0, 0, 0, 0, 0, &EnablerSelGotoNext, NULL, NULL, "GoToNextSelectedName", 0, 0, NULL, NULL},
        /*TBBE_RESELECT*/ {IDX_TB_RESELECT, 0, IDS_TBTT_RESELECT, CM_RESELECT, 0, 0, 0, 0, 0, &EnablerSelectionStored, NULL, NULL, "RestoreSelection", 0, 0, NULL, NULL},
        /*TBBE_PASTESHORTCUT*/ {IDX_TB_PASTESHORTCUT, 0, IDS_TBTT_PASTESHORTCUT, CM_CLIPPASTELINKS, 0, 0, 0, 0, 0, &EnablerPasteLinksOnDisk, NULL, NULL, "PasteShortcut", 0, 0, NULL, NULL},
        /*TBBE_FOCUSSHORTCUT*/ {IDX_TB_FOCUSSHORTCUT, 0, IDS_TBTT_FOCUSSHORTCUT, CM_AFOCUSSHORTCUT, 0, 0, 0, 0, 0, &EnablerFileOrDirLinkOnDisk, NULL, NULL, "GoToShortcutTarget", 0, 0, NULL, NULL},
        /*TBBE_SAVESELECTION*/ {IDX_TB_SAVESELECTION, 0, IDS_TBTT_SAVESELECTION, CM_STORESEL, 0, 0, 0, 0, 0, &EnablerSelected, NULL, NULL, "SaveSelection", 0, 0, NULL, NULL},
        /*TBBE_LOADSELECTION*/ {IDX_TB_LOADSELECTION, 0, IDS_TBTT_LOADSELECTION, CM_RESTORESEL, 0, 0, 0, 0, 0, &EnablerGlobalSelStored, NULL, NULL, "LoadSelection", 0, 0, NULL, NULL},
        /*TBBE_NEW*/ {IDX_TB_NEW, 0, IDS_TBTT_NEW, CM_NEWDROP, 0, 0, 0, 1, 0, &EnablerOnDisk, NULL, NULL, "New", 0, 0, NULL, NULL},
        /*TBBE_SEL_BY_EXT*/ {IDX_TB_SEL_BY_EXT, 0, IDS_TBTT_SEL_BY_EXT, CM_SELECTBYFOCUSEDEXT, 0, 0, 0, 0, 0, &EnablerFileDir, NULL, NULL, "SelectFilesWithSameExtension", 0, 0, NULL, NULL},
        /*TBBE_UNSEL_BY_EXT*/ {IDX_TB_UNSEL_BY_EXT, 0, IDS_TBTT_UNSEL_BY_EXT, CM_UNSELECTBYFOCUSEDEXT, 0, 0, 0, 0, 0, &EnablerFileDirANDSelected, NULL, NULL, "UnselectFilesWithSameExtension", 0, 0, NULL, NULL},
        /*TBBE_SEL_BY_NAME*/ {IDX_TB_SEL_BY_NAME, 0, IDS_TBTT_SEL_BY_NAME, CM_SELECTBYFOCUSEDNAME, 0, 0, 0, 0, 0, &EnablerFileDir, NULL, NULL, "SelectFilesWithSameName", 0, 0, NULL, NULL},
        /*TBBE_UNSEL_BY_NAME*/ {IDX_TB_UNSEL_BY_NAME, 0, IDS_TBTT_UNSEL_BY_NAME, CM_UNSELECTBYFOCUSEDNAME, 0, 0, 0, 0, 0, &EnablerFileDirANDSelected, NULL, NULL, "UnselectFilesWithSameName", 0, 0, NULL, NULL},
        /*TBBE_OPEN_FOLDER*/ {NIB1(IDX_TB_OPEN_FOLDER), 0, IDS_TBTT_OPEN_FOLDER, CM_OPEN_FOLDER_DROP, 0, 0, 0, 1, 0, NULL, NULL, NULL, "OpenFolder", 0, 0, NULL, NULL},
        /*TBBE_CONFIGRATION*/ {IDX_TB_CONFIGURARTION, 0, IDS_TBTT_CONFIGURATION, CM_CONFIGURATION, 0, 0, 0, 0, 0, NULL, NULL, NULL, "Configuration", 0, 0, NULL, NULL},
        /*TBBE_DIRMENU*/ {0xFFFF, 0, IDS_TBTT_DIRMENU, CM_DIRMENU, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_OPEN_IN_OTHER_ACT*/ {IDX_TB_OPEN_IN_OTHER_ACT, 0, IDS_TBTT_OPENINOTHER_A, CM_OPEN_IN_OTHER_PANEL_ACT, 0, 0, 0, 0, 0, NULL, NULL, NULL, "FocusNameInOtherPanel", 0, 0, NULL, NULL},
        /*TBBE_OPEN_IN_OTHER*/ {IDX_TB_OPEN_IN_OTHER, 0, IDS_TBTT_OPENINOTHER, CM_OPEN_IN_OTHER_PANEL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "OpenNameInOtherPanel", 0, 0, NULL, NULL},
        /*TBBE_AS_OTHER_PANEL*/ {IDX_TB_AS_OTHER_PANEL, 0, IDS_TBTT_ASOTHERPANEL, CM_ACTIVE_AS_OTHER, CM_LEFT_AS_OTHER, CM_RIGHT_AS_OTHER, 0, 0, 0, NULL, NULL, NULL, "GoToPathFromOtherPanel", 0, 0, NULL, NULL},
        /*TBBE_HIDE_SELECTED*/ {IDX_TB_HIDE_SELECTED, 0, IDS_TBTT_HIDE_SELECTED, CM_HIDE_SELECTED_NAMES, 0, 0, 0, 0, 0, &EnablerSelected, NULL, NULL, "HideSelectedNames", 0, 0, NULL, NULL},
        /*TBBE_HIDE_UNSELECTED*/ {IDX_TB_HIDE_UNSELECTED, 0, IDS_TBTT_HIDE_UNSELECTED, CM_HIDE_UNSELECTED_NAMES, 0, 0, 0, 0, 0, &EnablerUnselected, NULL, NULL, "HideUnselectedNames", 0, 0, NULL, NULL},
        /*TBBE_SHOW_ALL*/ {IDX_TB_SHOW_ALL, 0, IDS_TBTT_SHOW_ALL, CM_SHOW_ALL_NAME, 0, 0, 0, 0, 0, &EnablerHiddenNames, NULL, NULL, "ShowHiddenNames", 0, 0, NULL, NULL},
        /*TBBE_OPEN_MYDOC*/ {NIB1(IDX_TB_OPENMYDOC), 21, IDS_TBTT_OPENMYDOC, CM_OPENPERSONAL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL},
        /*TBBE_SMART_COLUMN_MODE*/ {IDX_TB_SMART_COLUMN_MODE, 0, IDS_TBTT_SMARTMODE, CM_ACTIVE_SMARTMODE, CM_LEFT_SMARTMODE, CM_RIGHT_SMARTMODE, 0, 0, 0, NULL, NULL, NULL, "SmartColumnMode", CM_BOTTOMLEFT_SMARTMODE, CM_BOTTOMRIGHT_SMARTMODE, NULL, NULL},

            //  added for bottom panels
        /*TBBE_CHANGE_DRIVE_BL*/ {IDX_TB_CHANGEDRIVEL, 255, IDS_TBTT_CHANGEDRIVE, CM_BLCHANGEDRIVE, 0, 0, 0, 1, 0, NULL, NULL, NULL, NULL, CM_BLCHANGEDRIVE, 0, NULL, NULL},
        /*TBBE_CHANGE_DRIVE_BR*/ {IDX_TB_CHANGEDRIVER, 255, IDS_TBTT_CHANGEDRIVE, CM_BRCHANGEDRIVE, 0, 0, 0, 1, 0, NULL, NULL, NULL, NULL, 0, CM_BRCHANGEDRIVE, NULL, NULL},
};

//
// TopToolbar
//
// Vyjadruje vsechna mozna tlacitka, ktera muze obsahovat TopToolbar.
// Poradi udava poradi tlacitek v konfiguracnim dialogu toolbary a muze
// byt libovolne meneno.
//

DWORD TopToolBarButtons[] =
    {
        TBBE_BACK,
        TBBE_FORWARD,
        TBBE_HOTPATHSDROP,
        TBBE_PARENT_DIR,
        TBBE_ROOT_DIR,

        NIB2(TBBE_CONNECT_NET)
            TBBE_DISCONNECT_NET,
        TBBE_CREATE_DIR,
        TBBE_FIND_FILE,
        TBBE_CHANGEDIR,
        TBBE_SHARES,

        TBBE_VIEW_MODE,
        TBBE_SORT_NAME,
        TBBE_SORT_EXT,
        TBBE_SORT_SIZE,
        TBBE_SORT_DATE,

        TBBE_FILTER,
        TBBE_REFRESH,
        TBBE_SWAP_PANELS,
        TBBE_SMART_COLUMN_MODE,

        TBBE_USER_MENU_DROP,
        TBBE_CMD,

        TBBE_COPY,
        TBBE_MOVE,
        TBBE_DELETE,
        TBBE_EMAIL,

        TBBE_COMPRESS,
        TBBE_UNCOMPRESS,
        TBBE_QUICK_RENAME,
        TBBE_CHANGE_CASE,
        TBBE_CONVERT,
        TBBE_VIEW,
        TBBE_EDIT,
        TBBE_EDITNEW,
        TBBE_NEW,

        TBBE_CLIPBOARD_CUT,
        TBBE_CLIPBOARD_COPY,
        TBBE_CLIPBOARD_PASTE,
        TBBE_PASTESHORTCUT,
        TBBE_CHANGE_ATTR,
        TBBE_COMPARE_DIR,
        TBBE_DRIVE_INFO,
        TBBE_CALCDIRSIZES,
        TBBE_OCCUPIEDSPACE,
        TBBE_PERMISSIONS,
        TBBE_PROPERTIES,
        TBBE_FOCUSSHORTCUT,

        TBBE_OPEN_IN_OTHER_ACT,
        TBBE_OPEN_IN_OTHER,
        TBBE_AS_OTHER_PANEL,

        TBBE_SELECT,
        TBBE_UNSELECT,
        TBBE_INVERT_SEL,
        TBBE_SELECT_ALL,
        TBBE_UNSELECT_ALL,
        TBBE_RESELECT,
        TBBE_SEL_BY_EXT,
        TBBE_UNSEL_BY_EXT,
        TBBE_SEL_BY_NAME,
        TBBE_UNSEL_BY_NAME,
        TBBE_PREV_SELECTED,
        TBBE_NEXT_SELECTED,
        TBBE_SAVESELECTION,
        TBBE_LOADSELECTION,
        TBBE_HIDE_SELECTED,
        TBBE_HIDE_UNSELECTED,
        TBBE_SHOW_ALL,

        TBBE_PACK,
        TBBE_UNPACK,

        NIB2(TBBE_OPEN_FOLDER)
            NIB2(TBBE_OPEN_ACTIVE)
                NIB2(TBBE_OPEN_DESKTOP)
                    NIB2(TBBE_OPEN_MYCOMP)
                        NIB2(TBBE_OPEN_CONTROL)
                            NIB2(TBBE_OPEN_PRINTERS)
                                NIB2(TBBE_OPEN_NETWORK)
                                    NIB2(TBBE_OPEN_RECYCLE)
                                        NIB2(TBBE_OPEN_FONTS)
                                            NIB2(TBBE_OPEN_MYDOC)

                                                TBBE_CONFIGRATION,

        NIB2(TBBE_HELP_CONTENTS)
            NIB2(TBBE_HELP_CONTEXT)

                TBBE_TERMINATOR // terminator - musi zde byt !
};

DWORD LeftToolBarButtons[] =
    {
        TBBE_CHANGE_DRIVE_L,
        TBBE_PARENT_DIR,
        TBBE_ROOT_DIR,
        TBBE_CHANGEDIR,
        TBBE_AS_OTHER_PANEL,
        TBBE_BACK,
        TBBE_FORWARD,
        TBBE_VIEW_MODE,
        TBBE_SORT_NAME,
        TBBE_SORT_EXT,
        TBBE_SORT_SIZE,
        TBBE_SORT_DATE,
        TBBE_FILTER,
        TBBE_REFRESH,
        TBBE_SMART_COLUMN_MODE,

        TBBE_TERMINATOR // terminator - musi zde byt !
};

DWORD RightToolBarButtons[] =
    {
        TBBE_CHANGE_DRIVE_R,
        TBBE_PARENT_DIR,
        TBBE_ROOT_DIR,
        TBBE_CHANGEDIR,
        TBBE_AS_OTHER_PANEL,
        TBBE_BACK,
        TBBE_FORWARD,
        TBBE_VIEW_MODE,
        TBBE_SORT_NAME,
        TBBE_SORT_EXT,
        TBBE_SORT_SIZE,
        TBBE_SORT_DATE,
        TBBE_FILTER,
        TBBE_REFRESH,
        TBBE_SMART_COLUMN_MODE,

        TBBE_TERMINATOR // terminator - musi zde byt !
};

DWORD BottomLeftToolBarButtons[] =
    {
        TBBE_CHANGE_DRIVE_BL,
        TBBE_PARENT_DIR,
        TBBE_ROOT_DIR,
        TBBE_CHANGEDIR,
        TBBE_AS_OTHER_PANEL,
        TBBE_BACK,
        TBBE_FORWARD,
        TBBE_VIEW_MODE,
        TBBE_SORT_NAME,
        TBBE_SORT_EXT,
        TBBE_SORT_SIZE,
        TBBE_SORT_DATE,
        TBBE_FILTER,
        TBBE_REFRESH,
        TBBE_SMART_COLUMN_MODE,

        TBBE_TERMINATOR // terminator - musi zde byt !
};

DWORD BottomRightToolBarButtons[] =
    {
        TBBE_CHANGE_DRIVE_BR,
        TBBE_PARENT_DIR,
        TBBE_ROOT_DIR,
        TBBE_CHANGEDIR,
        TBBE_AS_OTHER_PANEL,
        TBBE_BACK,
        TBBE_FORWARD,
        TBBE_VIEW_MODE,
        TBBE_SORT_NAME,
        TBBE_SORT_EXT,
        TBBE_SORT_SIZE,
        TBBE_SORT_DATE,
        TBBE_FILTER,
        TBBE_REFRESH,
        TBBE_SMART_COLUMN_MODE,

        TBBE_TERMINATOR // terminator - musi zde byt !
};

void GetSVGIconsMainToolbar(CSVGIcon** svgIcons, int* svgIconsCount)
{
    static CSVGIcon SVGIcons[TBBE_TERMINATOR];
    for (auto i = 0; i < TBBE_TERMINATOR; i++)
    {
        SVGIcons[i].ImageIndex = ToolBarButtons[i].ImageIndex;
        SVGIcons[i].SVGName = ToolBarButtons[i].SVGName;
    }
    *svgIcons = SVGIcons;
    *svgIconsCount = TBBE_TERMINATOR;
}

//****************************************************************************
//
// CreateGrayscaleAndMaskBitmaps
//
// Vytvori novou bitmapu o hloubce 24 bitu, nakopiruje do ni zdrojovou
// bitmapu a prevede ji na stupne sedi. Zaroven pripravi druhou bitmapu
// s maskou dle transparentni barvy.
//

BOOL CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                   HBITMAP& hGrayscale, HBITMAP& hMask)
{
    CALL_STACK_MESSAGE1("CreateGrayscaleAndMaskBitmaps(, , ,)");
    BOOL ret = FALSE;
    VOID* lpvBits = NULL;
    VOID* lpvBitsMask = NULL;
    hGrayscale = NULL;
    hMask = NULL;
    HDC hDC = HANDLES(GetDC(NULL));

    // vytahnu rozmery bitmapy
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biBitCount = 0; // nechceme paletu

    if (!GetDIBits(hDC,
                   hSource,
                   0, 0,
                   NULL,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    if (bi.bmiHeader.biSizeImage == 0)
    {
        TRACE_E("bi.bmiHeader.biSizeImage = 0");
        goto exitus;
    }

    // pozadovana barevna hloubka je 24 bitu
    bi.bmiHeader.biSizeImage = ((((bi.bmiHeader.biWidth * 24) + 31) & ~31) >> 3) * bi.bmiHeader.biHeight;
    // naalokuju potrebny prostor
    lpvBits = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBits == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }
    lpvBitsMask = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBitsMask == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }

    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;

    // vytahnu vlastni data
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBits,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // vytahnu vlastni data pro mask
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBitsMask,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // prevedu na grayscale
    BYTE* rgb;
    BYTE* rgbMask;
    rgb = (BYTE*)lpvBits;
    rgbMask = (BYTE*)lpvBitsMask;
    int i;
    for (i = 0; i < bi.bmiHeader.biWidth * bi.bmiHeader.biHeight; i++)
    {
        BYTE r = rgb[2];
        BYTE g = rgb[1];
        BYTE b = rgb[0];
        if (transparent != RGB(r, g, b))
        {
            BYTE brightness = GetGrayscaleFromRGB(r, g, b);
            rgb[0] = rgb[1] = rgb[2] = brightness;
            rgbMask[0] = rgbMask[1] = rgbMask[2] = (BYTE)0;
        }
        else
            rgbMask[0] = rgbMask[1] = rgbMask[2] = (BYTE)255;
        rgb += 3;
        rgbMask += 3;
    }

    // vytvorim novou bitmapu nad grayscale datama
    hGrayscale = HANDLES(CreateDIBitmap(hDC,
                                        &bi.bmiHeader,
                                        (LONG)CBM_INIT,
                                        lpvBits,
                                        &bi,
                                        DIB_RGB_COLORS));
    if (hGrayscale == NULL)
    {
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    // vytvorim novou bitmapu nad mask datama
    hMask = HANDLES(CreateDIBitmap(hDC,
                                   &bi.bmiHeader,
                                   (LONG)CBM_INIT,
                                   lpvBitsMask,
                                   &bi,
                                   DIB_RGB_COLORS));
    if (hMask == NULL)
    {
        HANDLES(DeleteObject(hGrayscale));
        hGrayscale = NULL;
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    ret = TRUE;
exitus:
    if (lpvBits != NULL)
        free(lpvBits);
    if (lpvBitsMask != NULL)
        free(lpvBitsMask);
    if (hDC != NULL)
        HANDLES(ReleaseDC(NULL, hDC));
    return ret;
}

// JRYFIXME - docasne pro prechod na SVG
BOOL CreateGrayscaleAndMaskBitmaps_tmp(HBITMAP hSource, COLORREF transparent, COLORREF bkColorForAlpha,
                                       HBITMAP& hGrayscale, HBITMAP& hMask)
{
    CALL_STACK_MESSAGE1("CreateGrayscaleAndMaskBitmaps(, , ,)");
    BOOL ret = FALSE;
    VOID* lpvBits = NULL;
    VOID* lpvBitsMask = NULL;
    hGrayscale = NULL;
    hMask = NULL;
    HDC hDC = HANDLES(GetDC(NULL));

    // vytahnu rozmery bitmapy
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biBitCount = 0; // nechceme paletu

    if (!GetDIBits(hDC,
                   hSource,
                   0, 0,
                   NULL,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    if (bi.bmiHeader.biSizeImage == 0)
    {
        TRACE_E("bi.bmiHeader.biSizeImage = 0");
        goto exitus;
    }

    // pozadovana barevna hloubka je 24 bitu
    bi.bmiHeader.biSizeImage = ((((bi.bmiHeader.biWidth * 24) + 31) & ~31) >> 3) * bi.bmiHeader.biHeight;
    // naalokuju potrebny prostor
    lpvBits = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBits == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }
    lpvBitsMask = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBitsMask == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }

    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;

    // vytahnu vlastni data
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBits,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // vytahnu vlastni data pro mask
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBitsMask,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // prevedu na grayscale
    BYTE* rgb;
    BYTE* rgbMask;
    rgb = (BYTE*)lpvBits;
    rgbMask = (BYTE*)lpvBitsMask;
    int i;
    for (i = 0; i < bi.bmiHeader.biWidth * bi.bmiHeader.biHeight; i++)
    {
        BYTE r = rgb[2];
        BYTE g = rgb[1];
        BYTE b = rgb[0];
        if (transparent != RGB(r, g, b) && bkColorForAlpha != RGB(r, g, b))
        {
            BYTE brightness = GetGrayscaleFromRGB(r, g, b);
            float alpha = 0.5;
            float alphablended = (float)GetRValue(bkColorForAlpha) * alpha + (float)brightness * (1 - alpha);
            rgb[0] = rgb[1] = rgb[2] = (BYTE)alphablended;
            rgbMask[0] = rgbMask[1] = rgbMask[2] = (BYTE)0;
        }
        else
            rgbMask[0] = rgbMask[1] = rgbMask[2] = (BYTE)255;
        rgb += 3;
        rgbMask += 3;
    }

    // vytvorim novou bitmapu nad grayscale datama
    hGrayscale = HANDLES(CreateDIBitmap(hDC,
                                        &bi.bmiHeader,
                                        (LONG)CBM_INIT,
                                        lpvBits,
                                        &bi,
                                        DIB_RGB_COLORS));
    if (hGrayscale == NULL)
    {
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    // vytvorim novou bitmapu nad mask datama
    hMask = HANDLES(CreateDIBitmap(hDC,
                                   &bi.bmiHeader,
                                   (LONG)CBM_INIT,
                                   lpvBitsMask,
                                   &bi,
                                   DIB_RGB_COLORS));
    if (hMask == NULL)
    {
        HANDLES(DeleteObject(hGrayscale));
        hGrayscale = NULL;
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    ret = TRUE;
exitus:
    if (lpvBits != NULL)
        free(lpvBits);
    if (lpvBitsMask != NULL)
        free(lpvBitsMask);
    if (hDC != NULL)
        HANDLES(ReleaseDC(NULL, hDC));
    return ret;
}

void RenderSVGImages(HDC hDC, int iconSize, COLORREF bkColor, const CSVGIcon* svgIcons, int svgIconsCount)
{
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    // JRYFIXME: docasne cteme ze souboru, prejit na spolecne uloziste s toolbars
    for (int i = 0; i < svgIconsCount; i++)
        if (svgIcons[i].SVGName != NULL)
            RenderSVGImage(rast, hDC, svgIcons[i].ImageIndex * iconSize, 0, svgIcons[i].SVGName, iconSize, bkColor, TRUE);

    nsvgDeleteRasterizer(rast);
}

//****************************************************************************
//
// CreateToolbarBitmaps
//
// Vytahne z resID bitmapu, nakopiruje ji do nove bitmapy, ktera je
// barevne kompatibilni s obrazovkou. Potom k teto bitmape pripoji
// ikonky z shell32.dll. Cti transparentni barvu.
// bkColorForAlpha udava barvu, ktera bude prosvitat pod pruhlednou casti ikon (WinXP)
//

BOOL CreateToolbarBitmaps(HINSTANCE hInstance, int resID, COLORREF transparent, COLORREF bkColorForAlpha,
                          HBITMAP& hMaskBitmap, HBITMAP& hGrayBitmap, HBITMAP& hColorBitmap, BOOL appendIcons,
                          const CSVGIcon* svgIcons, int svgIconsCount)
{
    CALL_STACK_MESSAGE5("CreateToolbarBitmaps(%p, %d, %x, %x, , , )", hInstance, resID, transparent, bkColorForAlpha);
    BOOL ret = FALSE;
    HDC hDC = NULL;
    HBITMAP hOldSrcBitmap = NULL;
    HBITMAP hOldTgtBitmap = NULL;
    HDC hTgtMemDC = NULL;
    HDC hSrcMemDC = NULL;
    hMaskBitmap = NULL;
    hGrayBitmap = NULL;
    hColorBitmap = NULL;

    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16); // small icon size
    int iconCount = 0;

    // Windows XP a novejsi pouzivaji transparentni ikony; protoze je pomoci masky
    // zobrazime do teto docasne bitmapy a zajistime, aby pod pruhlednou casti byla
    // sediva barva z toolbary a ne nase fialova maskovaci
    HBITMAP hTmpBitmap = NULL;
    HDC hTmpMemDC = NULL;
    HBITMAP hOldTmpBitmap = NULL;

    // nactu zdrojovou bitmapu
    HBITMAP hSource;
    if (resID == IDB_TOOLBAR_256) // dirty hack, chtelo by to detekci podle typu resourcu (RCDATA), pripadne podle PNG signatury
        hSource = LoadPNGBitmap(hInstance, MAKEINTRESOURCE(resID), 0);
    else
        hSource = HANDLES(LoadBitmap(hInstance, MAKEINTRESOURCE(resID)));
    if (hSource == NULL)
    {
        TRACE_E("LoadBitmap failed on redID " << resID);
        goto exitus;
    }

    hDC = HANDLES(GetDC(NULL));
    // vytahnu rozmery bitmapy
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biBitCount = 0; // nechceme paletu
    if (!GetDIBits(hDC,
                   hSource,
                   0, 0,
                   NULL,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    int tbbe_BMPCOUNT;
    int i;
    tbbe_BMPCOUNT = 0;
    if (appendIcons)
    {
        for (i = 0; i < TBBE_TERMINATOR; i++)
            if (ToolBarButtons[i].Shell32ResID != 0)
                tbbe_BMPCOUNT++;
    }

    // pripravim novou bitmapu, do ktere se vejde hSource a ikonky z DLLka
    // prodlouzim delku o ikonky z DLL
    iconCount = bi.bmiHeader.biWidth / 16 + tbbe_BMPCOUNT;

    //  hColorBitmap = HANDLES(CreateBitmap(width, height, bh.bV4Planes, bh.bV4BitCount, NULL));
    //protoze je CreateBitmap() vhodne pouze pro vytvareni B&W bitmap (viz MSDN)
    //prechazime od sal 2.5b7 na rychlou CreateCompatibleBitmap()
    hColorBitmap = HANDLES(CreateCompatibleBitmap(hDC, iconSize * iconCount, iconSize));

    hTgtMemDC = HANDLES(CreateCompatibleDC(NULL));
    hSrcMemDC = HANDLES(CreateCompatibleDC(NULL));
    hOldTgtBitmap = (HBITMAP)SelectObject(hTgtMemDC, hColorBitmap);
    hOldSrcBitmap = (HBITMAP)SelectObject(hSrcMemDC, hSource);

    // pro prenaseni icon (vcetne transparentnich)
    hTmpBitmap = HANDLES(CreateBitmap(iconSize, iconSize, 1, 1, NULL));
    hTmpMemDC = HANDLES(CreateCompatibleDC(NULL));
    hOldTmpBitmap = (HBITMAP)SelectObject(hTmpMemDC, hTmpBitmap);

    // prenesu do nove bitmapy bitmapu puvodni
    if (!StretchBlt(hTgtMemDC, 0, 0, (iconCount - tbbe_BMPCOUNT) * iconSize, iconSize,
                    hSrcMemDC, 0, 0, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight,
                    SRCCOPY))
    {
        TRACE_E("BitBlt failed");
        goto exitus;
    }

    // zahodime zdrojovou bitmapu
    SelectObject(hSrcMemDC, hOldSrcBitmap);
    hOldSrcBitmap = NULL;

    // pokud mame SVG verzi, pouzijeme ji
    if (svgIcons != NULL)
    {
        RenderSVGImages(hTgtMemDC, iconSize, bkColorForAlpha, svgIcons, svgIconsCount);
    }

    // pouzijeme pri BitBlt hTmpMemDC->hTgtMemDC
    //SetBkColor(hTgtMemDC, transparent);
    //SetTextColor(hTgtMemDC, bkColorForAlpha);

    if (appendIcons)
    {
        // podojime shell32.dll
        HICON hIcon;
        for (i = 0; i < TBBE_TERMINATOR; i++)
        {
            BYTE resID2 = ToolBarButtons[i].Shell32ResID;
            if (resID2 == 0 || resID2 == 255)
                continue;

            if ((resID2 == 21 && (resID2 = 112) != 0 || // Documents
                 resID2 == 33 && (resID2 = 54) != 0 ||  // Recycle Bin
                 resID2 == 138 && (resID2 = 26) != 0 || // Printers
                 resID2 == 39 && (resID2 = 77) != 0 ||  // Fonts
                 resID2 == 18 && (resID2 = 152) != 0))  // Network
            {
                hIcon = SalLoadIcon(ImageResDLL, resID2, iconSize);
            }
            else
            {
                // Documents jsou od WinXP jinde
                if (resID2 == 21)
                    resID2 = 235;

                hIcon = SalLoadIcon(Shell32DLL, resID2, iconSize);
            }
            if (hIcon == NULL)
            {
                TRACE_E("LoadImage failed on resID2 " << resID2);
                continue;
            }

            // pripravime pozadi pro ikonky s alfa kanalem pod WinXP
            DrawIconEx(hTmpMemDC, 0, 0, hIcon, iconSize, iconSize, 0, 0, DI_MASK);

            // pouzijeme pri BitBlt hTmpMemDC->hTgtMemDC
            SetBkColor(hTgtMemDC, transparent);
            SetTextColor(hTgtMemDC, bkColorForAlpha);
            BitBlt(hTgtMemDC, iconSize * ToolBarButtons[i].ImageIndex, 0, iconSize, iconSize,
                   hTmpMemDC, 0, 0,
                   SRCCOPY);

            if (!DrawIconEx(hTgtMemDC, iconSize * ToolBarButtons[i].ImageIndex, 0, hIcon,
                            iconSize, iconSize, 0, 0, DI_NORMAL))
            {
                HANDLES(DestroyIcon(hIcon));
                TRACE_E("DrawIconEx failed on resID2 " << resID2);
                continue;
            }
            else
            {
                /*
        // --- sileny patch BEGIN
        // John: pod W2K mi neslapalo pro ikonku 21 (Documents) DrawIconEx s parametrem DI_NORMAL
        // z neznameho duvodu saturovalo pruhledny prostor a tim zmenilo barvu 'transparent'
        SetBkColor(hTgtMemDC, RGB(0, 0, 0));
        SetTextColor(hTgtMemDC, RGB(255, 255, 255));
        BitBlt(hTgtMemDC, ICON16_CX * ToolBarButtons[i].ImageIndex, 0, ICON16_CX, ICON16_CX,
               hTmpMemDC, 0, 0,
               SRCAND);
        SetBkColor(hTgtMemDC, RGB(255, 0, 255));
        SetTextColor(hTgtMemDC, RGB(0, 0, 0));
        BitBlt(hTgtMemDC, ICON16_CX * ToolBarButtons[i].ImageIndex, 0, ICON16_CX, ICON16_CX,
               hTmpMemDC, 0, 0,
               SRCPAINT);
        // --- sileny patch END
        */
                HANDLES(DestroyIcon(hIcon));
            }
        }
    }

    ret = TRUE;
exitus:
    if (hOldSrcBitmap != NULL)
        SelectObject(hSrcMemDC, hOldSrcBitmap);
    if (hOldTgtBitmap != NULL)
        SelectObject(hTgtMemDC, hOldTgtBitmap);
    if (hSrcMemDC != NULL)
        HANDLES(DeleteDC(hSrcMemDC));
    if (hTgtMemDC != NULL)
        HANDLES(DeleteDC(hTgtMemDC));
    if (hSource != NULL)
        HANDLES(DeleteObject(hSource));
    if (hDC != NULL)
        HANDLES(ReleaseDC(NULL, hDC));

    if (hOldTmpBitmap != NULL)
        SelectObject(hTmpMemDC, hOldTmpBitmap);
    if (hTmpBitmap != NULL)
        HANDLES(DeleteObject(hTmpBitmap));
    if (hTmpMemDC != NULL)
        HANDLES(DeleteDC(hTmpMemDC));

    if (ret)
    {
        ret = CreateGrayscaleAndMaskBitmaps_tmp(hColorBitmap, transparent, bkColorForAlpha, hGrayBitmap, hMaskBitmap);
        if (!ret)
        {
            HANDLES(DeleteObject(hColorBitmap));
            hColorBitmap = NULL;
        }
    }

    return ret;
}

//****************************************************************************
//
// PrepareToolTipText
//
// Prohleda buff na prvni vyskyt znaku '\t'. Pokud je nastavena promenna
// stripHotKey, vlozi na jeho misto terminator a vrati se. Jinak na jeho
// misto vlozi mezeru, zbytek posunu o znak vpravo a ozavorkuje.
//

void PrepareToolTipText(char* buff, BOOL stripHotKey)
{
    CALL_STACK_MESSAGE2("PrepareToolTipText(, %d)", stripHotKey);
    char* p = buff;
    while (*p != '\t' && *p != 0)
        p++;
    if (*p == '\t')
    {
        if (!stripHotKey && *(p + 1) != 0)
        {
            *p = ' ';
            p++;
            memmove(p + 1, p, lstrlen(p) + 1);
            *p = '(';
            lstrcat(p, ")");
        }
        else
            *p = 0;
    }
}

//*****************************************************************************
//
// CMainToolBar
//

CMainToolBar::CMainToolBar(HWND hNotifyWindow, CMainToolBarType type, CObjectOrigin origin)
    : CToolBar(hNotifyWindow, origin)
{
    CALL_STACK_MESSAGE_NONE
    Type = type;
}

BOOL CMainToolBar::FillTII(int tbbeIndex, TLBI_ITEM_INFO2* tii, BOOL fillName)
{
    CALL_STACK_MESSAGE3("CMainToolBar::FillTII(%d, , %d)", tbbeIndex, fillName);
    if (tbbeIndex < -1 || tbbeIndex >= TBBE_TERMINATOR)
    {
        TRACE_E("tbbeIndex = " << tbbeIndex);
        return FALSE;
    }
    if (ToolBarButtons[tbbeIndex].ImageIndex == 0xFFFF)
    {
        // stara polozka, ktera v teto verzi byla zrusena
        return FALSE;
    }

    if (tbbeIndex == -1 || tbbeIndex >= TBBE_TERMINATOR)
    {
        tii->Mask = TLBI_MASK_STYLE;
        tii->Style = TLBI_STYLE_SEPARATOR;
    }
    else
    {   
        if (Type == mtbtLeft &&
            (tbbeIndex == TBBE_CHANGE_DRIVE_R || tbbeIndex == TBBE_CHANGE_DRIVE_BL || tbbeIndex == TBBE_CHANGE_DRIVE_BR))
            tbbeIndex = TBBE_CHANGE_DRIVE_L;
        if (Type == mtbtRight &&
            (tbbeIndex == TBBE_CHANGE_DRIVE_L || tbbeIndex == TBBE_CHANGE_DRIVE_BL || tbbeIndex == TBBE_CHANGE_DRIVE_BR))
            tbbeIndex = TBBE_CHANGE_DRIVE_R;

        if (Type == mtbtBottomLeft &&
            (tbbeIndex == TBBE_CHANGE_DRIVE_R || tbbeIndex == TBBE_CHANGE_DRIVE_L || tbbeIndex == TBBE_CHANGE_DRIVE_BR))
            tbbeIndex = TBBE_CHANGE_DRIVE_BL;
        if (Type == mtbtBottomRight &&
            (tbbeIndex == TBBE_CHANGE_DRIVE_L || tbbeIndex == TBBE_CHANGE_DRIVE_R || tbbeIndex == TBBE_CHANGE_DRIVE_BL))
            tbbeIndex = TBBE_CHANGE_DRIVE_BR;
        

        tii->Mask = TLBI_MASK_STYLE | TLBI_MASK_ID |
                    TLBI_MASK_ENABLER | TLBI_MASK_CUSTOMDATA;
        tii->Style = 0;
        tii->CustomData = tbbeIndex;
        tii->Mask |= TLBI_MASK_IMAGEINDEX;
        tii->ImageIndex = ToolBarButtons[tbbeIndex].ImageIndex;

        if (ToolBarButtons[tbbeIndex].DropDown)
            tii->Style |= TLBI_STYLE_SEPARATEDROPDOWN;
        if (ToolBarButtons[tbbeIndex].WholeDropDown)
            tii->Style |= TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_DROPDOWN;
        if (ToolBarButtons[tbbeIndex].Check)
            tii->Style |= TLBI_STYLE_RADIO;
        switch (Type)
        {
        case mtbtMiddle:
        case mtbtTop:
        {
            tii->ID = ToolBarButtons[tbbeIndex].ID;
            tii->Enabler = ToolBarButtons[tbbeIndex].Enabler;
            break;
        }

        case mtbtLeft:
        {
            tii->ID = ToolBarButtons[tbbeIndex].LeftID;
            tii->Enabler = ToolBarButtons[tbbeIndex].LeftEnabler;
            break;
        }

        case mtbtRight:
        {
            tii->ID = ToolBarButtons[tbbeIndex].RightID;
            tii->Enabler = ToolBarButtons[tbbeIndex].RightEnabler;
            break;
        }
        case mtbtBottomLeft:
        {
            tii->ID = ToolBarButtons[tbbeIndex].BottomLeftID;
            tii->Enabler = ToolBarButtons[tbbeIndex].BottomLeftEnabler;
            break;
        }

        case mtbtBottomRight:
        {
            tii->ID = ToolBarButtons[tbbeIndex].BottomRightID;
            tii->Enabler = ToolBarButtons[tbbeIndex].BottomRightEnabler;
            break;
        }
        }
        if (fillName)
        {
            tii->Name = LoadStr(ToolBarButtons[tbbeIndex].ToolTipResID);
            // retezec bude orezan, proto muzeme operaci provest nad bufferem z LoadStr
            PrepareToolTipText(tii->Name, TRUE);
        }
    }
    return TRUE;
}

BOOL CMainToolBar::Load(const char* data)
{
    CALL_STACK_MESSAGE2("CMainToolBar::Load(%s)", data);
    char tmp[5000];
    lstrcpyn(tmp, data, 5000);

    RemoveAllItems();
    char* p = strtok(tmp, ",");
    while (p != NULL)
    {
        int tbbeIndex = atoi(p);
        if (tbbeIndex >= -1 && tbbeIndex < TBBE_TERMINATOR)
        {
            TLBI_ITEM_INFO2 tii;
            if (FillTII(tbbeIndex, &tii, FALSE))
            {
                if (!InsertItem2(0xFFFFFFFF, TRUE, &tii))
                    return FALSE;
            }
            else
            {
                TRACE_I("CMainToolBar::Load skipping tbbeIndex=" << tbbeIndex);
            }
            p = strtok(NULL, ",");
        }
        else
        {
            TRACE_I("CMainToolBar::Load skipping tbbeIndex=" << tbbeIndex);
            p = strtok(NULL, ",");
        }
    }
    return TRUE;
}

BOOL CMainToolBar::Save(char* data)
{
    CALL_STACK_MESSAGE1("CMainToolBar::Save()");
    TLBI_ITEM_INFO2 tii;
    int count = GetItemCount();
    int tbbeIndex;
    char* p = data;
    *p = 0;
    int i;
    for (i = 0; i < count; i++)
    {
        tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_CUSTOMDATA;
        GetItemInfo2(i, TRUE, &tii);
        if (tii.Style == TLBI_STYLE_SEPARATOR)
            tbbeIndex = -1;
        else
            tbbeIndex = tii.CustomData;
        p += sprintf(p, "%d", tbbeIndex);
        if (i < count - 1)
        {
            lstrcpy(p, ",");
            p++;
        }
    }
    return TRUE;
}

void CMainToolBar::OnGetToolTip(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CMainToolBar::OnGetToolTip(0x%IX)", lParam);
    TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
    int tbbeIndex = tt->CustomData;
    if (tbbeIndex < TBBE_TERMINATOR)
    {
        lstrcpy(tt->Buffer, LoadStr(ToolBarButtons[tbbeIndex].ToolTipResID));
        if (tbbeIndex == TBBE_CLIPBOARD_PASTE)
        {
            CFilesWindow* activePanel = MainWindow != NULL ? MainWindow->GetActivePanel() : NULL;
            BOOL activePanelIsDisk = (activePanel != NULL && activePanel->Is(ptDisk));
            if (EnablerPastePath &&
                (!activePanelIsDisk || !EnablerPasteFiles) && // PasteFiles je prioritni
                !EnablerPasteFilesToArcOrFS)                  // PasteFilesToArcOrFS je prioritni
            {
                char tail[50];
                tail[0] = 0;
                char* p = strrchr(tt->Buffer, '\t');
                if (p != NULL)
                    strcpy(tail, p);
                else
                    p = tt->Buffer + strlen(tt->Buffer);
                sprintf(p, " (%s)%s", LoadStr(IDS_PASTE_CHANGE_DIRECTORY), tail);
            }
        }
        PrepareToolTipText(tt->Buffer, FALSE);
    }
}

BOOL CMainToolBar::OnEnumButton(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CMainToolBar::OnEnumButton(0x%IX)", lParam);
    TLBI_ITEM_INFO2* tii = (TLBI_ITEM_INFO2*)lParam;
    DWORD tbbeIndex;
    switch (Type)
    {
    case mtbtMiddle:
    case mtbtTop:
        tbbeIndex = TopToolBarButtons[tii->Index];
        break;
    case mtbtLeft:
        tbbeIndex = LeftToolBarButtons[tii->Index];
        break;
    case mtbtRight:
        tbbeIndex = RightToolBarButtons[tii->Index];
        break;
    case mtbtBottomLeft:
        tbbeIndex = BottomLeftToolBarButtons[tii->Index];
        break;
    case mtbtBottomRight:
        tbbeIndex = BottomRightToolBarButtons[tii->Index];
        break;
    }
    if (tbbeIndex == TBBE_TERMINATOR)
        return FALSE; // vsechna tlacitka uz byla natlacena
    FillTII(tbbeIndex, tii, TRUE);
    return TRUE;
}

void CMainToolBar::OnReset()
{
    CALL_STACK_MESSAGE1("CMainToolBar::OnReset()");
    const char* defStr = NULL;
    switch (Type)
    {
    case mtbtTop:
        defStr = DefTopToolBar;
        break;
    case mtbtMiddle:
        defStr = DefMiddleToolBar;
        break;
    case mtbtLeft:
        defStr = DefLeftToolBar;
        break;
    case mtbtRight:
        defStr = DefRightToolBar;
        break;
    case mtbtBottomLeft:
        defStr = DefBottomLeftToolBar;
        break;
    case mtbtBottomRight:
        defStr = DefBottomRightToolBar;
        break;
    }
    if (defStr != NULL)
        Load(defStr);
}

void CMainToolBar::SetType(CMainToolBarType type)
{
    CALL_STACK_MESSAGE_NONE
    Type = type;
}

//*****************************************************************************
//
// CBottomToolBar
//

#define BOTTOMTB_TEXT_MAX 15 // maximalni delka retezce pro jednu klavesu
struct CBottomTBData
{
    DWORD Index;
    BYTE TextLen;                 // pocet zanaku v promenne 'Text'
    char Text[BOTTOMTB_TEXT_MAX]; // text bez terminatoru
};

CBottomTBData BottomTBData[btbsCount][12] =
    {
        // btbdNormal
        {
            {NIB3(TBBE_HELP_CONTENTS)}, // F1
            {TBBE_QUICK_RENAME},        // F2
            {TBBE_VIEW},                // F3
            {TBBE_EDIT},                // F4
            {TBBE_COPY},                // F5
            {TBBE_MOVE},                // F6
            {TBBE_CREATE_DIR},          // F7
            {TBBE_DELETE},              // F8
            {TBBE_USER_MENU},           // F9
            {TBBE_MENU},                // F10
            {NIB3(TBBE_CONNECT_NET)},   // F11
            {TBBE_DISCONNECT_NET},      // F12
        },
        // btbdAlt,
        {
            {TBBE_CHANGE_DRIVE_L}, // F1
            {TBBE_CHANGE_DRIVE_R}, // F2
            {TBBE_ALTVIEW},        // F3      // here can be replaced  {TBBE_CHANGE_DRIVE_BL}, // F3
            {TBBE_EXIT},           // F4      // here can be replaced {TBBE_CHANGE_DRIVE_BR}, // F4
            {TBBE_PACK},           // F5
            {TBBE_UNPACK},         // F6
            {TBBE_FIND_FILE},      // F7
            {TBBE_TERMINATOR},     // F8
            {TBBE_UNPACK},         // F9
            {TBBE_OCCUPIEDSPACE},  // F10
            {TBBE_FILEHISTORY},    // F11
            {TBBE_DIRHISTORY},     // F12
        },
        // btbdCtrl
        {
            {TBBE_DRIVE_INFO},  // F1
            {TBBE_CHANGE_ATTR}, // F2
            {TBBE_SORT_NAME},   // F3
            {TBBE_SORT_EXT},    // F4
            {TBBE_SORT_DATE},   // F5
            {TBBE_SORT_SIZE},   // F6
            {TBBE_CHANGE_CASE}, // F7
            {TBBE_TERMINATOR},  // F8
            {TBBE_REFRESH},     // F9
            {TBBE_COMPARE_DIR}, // F10
            {TBBE_ZOOM_PANEL},  // F11
            {TBBE_FILTER},      // F12
        },
        // btbdShift
        {
            {NIB3(TBBE_HELP_CONTEXT)}, // F1
            {TBBE_TERMINATOR},         // F2
            {NIB3(TBBE_OPEN_ACTIVE)},  // F3
            {TBBE_EDITNEW},            // F4
            {TBBE_TERMINATOR},         // F5
            {TBBE_TERMINATOR},         // F6
            {TBBE_CHANGEDIR},          // F7
            {TBBE_DELETE},             // F8
            {TBBE_HOTPATHS},           // F9
            {TBBE_CONTEXTMENU},        // F10
            {TBBE_TERMINATOR},         // F11
            {TBBE_TERMINATOR},         // F12
        },
        // btbdCtrlShift
        {
            {TBBE_TERMINATOR},    // F1
            {TBBE_TERMINATOR},    // F2
            {TBBE_VIEWWITH},      // F3
            {TBBE_EDITWITH},      // F4
            {TBBE_SAVESELECTION}, // F5
            {TBBE_LOADSELECTION}, // F6
            {TBBE_TERMINATOR},    // F7
            {TBBE_TERMINATOR},    // F8
            {TBBE_SHARES},        // F9
            {TBBE_CALCDIRSIZES},  // F10
            {TBBE_FULLSCREEN},    // F11
            {TBBE_TERMINATOR},    // F12
        },
        // btbsAltShift,
        {
            {TBBE_TERMINATOR}, // F1
            {TBBE_TERMINATOR}, // F2
            {TBBE_TERMINATOR}, // F3
            {TBBE_TERMINATOR}, // F4
            {TBBE_TERMINATOR}, // F5
            {TBBE_TERMINATOR}, // F6
            {TBBE_TERMINATOR}, // F7
            {TBBE_TERMINATOR}, // F8
            {TBBE_TERMINATOR}, // F9
            {TBBE_DIRMENU},    // F10
            {TBBE_TERMINATOR}, // F11
            {TBBE_TERMINATOR}, // F12
        },
        // btbsMenu,
        {
            {NIB3(TBBE_HELP_CONTENTS)}, // F1
            {TBBE_TERMINATOR},          // F2
            {TBBE_TERMINATOR},          // F3
            {TBBE_TERMINATOR},          // F4
            {TBBE_TERMINATOR},          // F5
            {TBBE_TERMINATOR},          // F6
            {TBBE_TERMINATOR},          // F7
            {TBBE_TERMINATOR},          // F8
            {TBBE_TERMINATOR},          // F9
            {TBBE_MENU},                // F10
            {TBBE_TERMINATOR},          // F11
            {TBBE_TERMINATOR},          // F12
        },
};

CBottomToolBar::CBottomToolBar(HWND hNotifyWindow, CObjectOrigin origin)
    : CToolBar(hNotifyWindow, origin)
{
    CALL_STACK_MESSAGE_NONE
    State = btbsCount;
    Padding.ButtonIconText = 1; // pritahneme text k ikonce
    Padding.IconLeft = 2;       // prostor pred ikonou
    Padding.TextRight = 2;      // prostor za textem
}

// naplni v poli BottomTBData promennou 'Text', kterou vycte z resourcu
// 'state' udava radek v poli BottomTBData a 'BottomTBData' oznacuje retezec s textama
// texty pro jednotlive klavesy jsou oddeleny znakem ';'
BOOL CBottomToolBar::InitDataResRow(CBottomTBStateEnum state, int textResID)
{
    CALL_STACK_MESSAGE2("CBottomToolBar::InitDataResRow(, %d)", textResID);
    char buff[BOTTOMTB_TEXT_MAX * 12];
    lstrcpyn(buff, LoadStr(textResID), BOTTOMTB_TEXT_MAX * 12);

    int index = 0;
    const char* begin = buff;
    const char* end;
    do
    {
        end = begin;
        while (*end != 0 && *end != ',')
            end++;
        if (index < 12)
        {
            int count = (int)(end - begin);
            if (count > BOTTOMTB_TEXT_MAX)
            {
                TRACE_E("Bottom Toolbar text state:" << state << " index:" << index << " exceeds " << BOTTOMTB_TEXT_MAX << " chars");
                count = BOTTOMTB_TEXT_MAX;
            }
            if (BottomTBData[state][index].Index == TBBE_TERMINATOR)
                count = 0;
            BottomTBData[state][index].TextLen = count;
            if (count > 0)
                memmove(BottomTBData[state][index].Text, begin, count);
            begin = end + 1;
        }
        else
        {
            TRACE_E("Bottom Toolbar text state:" << state << " index:" << index << " exceeds array range");
        }
        index++;
    } while (*end != 0);
    if (index != 12)
    {
        TRACE_E("Bottom Toolbar text state:" << state << " does not contains all 12 items.");
        return FALSE;
    }
    return TRUE;
}

CBottomTBStateEnum BottomTBStates[btbsCount] = {btbsNormal, btbsAlt, btbsCtrl, btbsShift, btbsCtrlShift, btbsAltShift, btbsMenu};

BOOL CBottomToolBar::InitDataFromResources()
{
    CALL_STACK_MESSAGE1("CBottomToolBar::InitDataFromResources()");
    int res[btbsCount] = {IDS_BTBTEXTS, IDS_BTBTEXTS_A, IDS_BTBTEXTS_C, IDS_BTBTEXTS_S, IDS_BTBTEXTS_CS, IDS_BTBTEXTS_AS, IDS_BTBTEXTS_MENU};
    int i;
    for (i = 0; i < btbsCount; i++)
    {
        if (!InitDataResRow(BottomTBStates[i], res[i]))
            return FALSE;
    }
    return TRUE;
}

void CBottomToolBar::SetFont()
{
    CToolBar::SetFont();
    SetMaxItemWidths();
}

BOOL CBottomToolBar::SetMaxItemWidths()
{
    CALL_STACK_MESSAGE1("CBottomToolBar::SetMaxItemWidths()");
    if (HWindow == NULL || Items.Count == 0)
        return TRUE;
    HFONT hOldFont = (HFONT)SelectObject(CacheBitmap->HMemDC, HFont);
    int i;
    for (i = 0; i < 12; i++)
    {
        WORD maxWidth = 0;
        int j;
        for (j = 0; j < btbsCount; j++)
        {
            const char* text = BottomTBData[j][i].Text;
            int textLen = BottomTBData[j][i].TextLen;

            RECT r;
            r.left = 0;
            r.top = 0;
            r.right = 0;
            r.bottom = 0;
            DrawText(CacheBitmap->HMemDC, text, textLen,
                     &r, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
            if (r.right > maxWidth)
                maxWidth = (WORD)r.right;
        }
        maxWidth = 3 + BOTTOMBAR_CX + 1 + maxWidth + 3;
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_WIDTH;
        tii.Width = maxWidth;
        if (!SetItemInfo2(i, TRUE, &tii))
        {
            if (hOldFont != NULL)
                SelectObject(CacheBitmap->HMemDC, hOldFont);
            return FALSE;
        }
    }
    if (hOldFont != NULL)
        SelectObject(CacheBitmap->HMemDC, hOldFont);
    return TRUE;
}

BOOL CBottomToolBar::CreateWnd(HWND hParent)
{
    CALL_STACK_MESSAGE1("CBottomToolBar::CreateWnd()");
    if (!CToolBar::CreateWnd(hParent))
        return FALSE;

    RemoveAllItems();
    SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_TEXT);
    int i;
    for (i = 0; i < 12; i++)
    {
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_IMAGEINDEX | TLBI_MASK_WIDTH;
        tii.Style = TLBI_STYLE_SHOWTEXT | TLBI_STYLE_FIXEDWIDTH | TLBI_STYLE_NOPREFIX;
        tii.ImageIndex = i;
        tii.Width = 60;
        if (!InsertItem2(i, TRUE, &tii))
            return FALSE;
    }
    SetMaxItemWidths();
    return TRUE;
}

BOOL CBottomToolBar::SetState(CBottomTBStateEnum state)
{
    CALL_STACK_MESSAGE1("CBottomToolBar::SetState()");
    if (State == state)
        return TRUE;
    if (Items.Count != 12)
    {
        TRACE_E("Items.Count != 12");
        return FALSE;
    }
    BOOL empty = state == btbsCount;
    int i;
    for (i = 0; i < 12; i++)
    {
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_TEXT | TLBI_MASK_TEXTLEN | TLBI_MASK_ID | TLBI_MASK_ENABLER |
                   TLBI_MASK_STATE | TLBI_MASK_CUSTOMDATA;
        tii.State = 0; // povolime command - bude volano UpdateItemsState(), ktere zakaze co je treba
        char emptyBuff[] = "";
        tii.Text = empty ? emptyBuff : BottomTBData[state][i].Text;
        tii.TextLen = empty ? 0 : BottomTBData[state][i].TextLen;
        int btIndex = BottomTBData[state][i].Index;
        tii.CustomData = btIndex;
        tii.ID = 0;
        tii.Enabler = NULL;
        if (!empty && btIndex != TBBE_TERMINATOR)
        {
            tii.ID = ToolBarButtons[btIndex].ID;
            tii.Enabler = ToolBarButtons[btIndex].Enabler;
        }
        if (!SetItemInfo2(i, TRUE, &tii))
            return FALSE;
    }
    State = state;

    POINT p;
    GetCursorPos(&p);
    if (WindowFromPoint(p) == HWindow)
    {
        // zajistime obnovu pripadneho tooltipu
        ScreenToClient(HWindow, &p);
        SetCurrentToolTip(NULL, 0);
        PostMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
    }

    return TRUE;
}

void CBottomToolBar::OnGetToolTip(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CBottomToolBar::OnGetToolTip(0x%IX)", lParam);
    TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
    int tbbeIndex = tt->CustomData;
    if (tbbeIndex < TBBE_TERMINATOR)
    {
        lstrcpy(tt->Buffer, LoadStr(ToolBarButtons[tbbeIndex].ToolTipResID));
        PrepareToolTipText(tt->Buffer, TRUE);
    }
}
