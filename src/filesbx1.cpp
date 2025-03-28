﻿// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "filesbox.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "editwnd.h"
#include "mainwnd.h"
#include "stswnd.h"
#include "shellib.h"
#include "snooper.h"

const char* CFILESBOX_CLASSNAME = "SalamanderItemsBox";

//****************************************************************************
//
// CFilesBox
//

CFilesBox::CFilesBox(CFilesWindow* parent)
    : CWindow(ooStatic)
{
    BottomBar.RelayWindow = this;
    Parent = parent;
    HPrivateDC = NULL;
    HVScrollBar = NULL;
    HHScrollBar = NULL;
    ZeroMemory(&ClientRect, sizeof(ClientRect));
    ZeroMemory(&HeaderRect, sizeof(HeaderRect));
    ZeroMemory(&VScrollRect, sizeof(VScrollRect));
    ZeroMemory(&BottomBarRect, sizeof(BottomBarRect));
    ZeroMemory(&FilesRect, sizeof(FilesRect));
    ItemsCount = 0;
    TopIndex = 0;
    XOffset = 0;
    ViewMode = vmDetailed;
    HeaderLineVisible = TRUE;

    OldVertSI.cbSize = 0;
    OldHorzSI.cbSize = 0;

    ItemHeight = 10;      // dummy
    ItemWidth = 10;       // dummy
    ThumbnailWidth = 10;  // dummy
    ThumbnailHeight = 10; // dummy
    EntireItemsInColumn = 1;
    ItemsInColumn = 1;
    EntireColumnsCount = 1;

    HeaderLine.Parent = this;
    HeaderLine.Columns = &Parent->Columns;

    ResetMouseWheelAccumulator();
}

void CFilesBox::SetItemsCount(int count, int xOffset, int topIndex, BOOL disableSBUpdate)
{
    SetItemsCount2(count);
    XOffset = xOffset;
    TopIndex = topIndex;
    OldVertSI.cbSize = 0;
    OldHorzSI.cbSize = 0;
    if (!disableSBUpdate)
    {
        SetupScrollBars();
        CheckAndCorrectBoundaries();
    }
    Parent->VisibleItemsArray.InvalidateArr();
    Parent->VisibleItemsArraySurround.InvalidateArr();
}

void CFilesBox::SetItemsCount2(int count)
{
    ItemsCount = count;
    UpdateInternalData();
}

void CFilesBox::SetMode(CViewModeEnum mode, BOOL headerLine)
{
    ViewMode = mode;
    HeaderLineVisible = headerLine;
    BOOL change = ShowHideChilds();
    LayoutChilds(change);
    if (change)
        InvalidateRect(HWindow, &FilesRect, FALSE);
}

void CFilesBox::SetItemWidthHeight(int width, int height)
{
    if (height < 1)
    {
        TRACE_E("height=" << height);
        height = 1;
    }
    if (width < 1)
    {
        TRACE_E("width=" << width);
        width = 1;
    }
    if (width != ItemWidth || height != ItemHeight)
    {
        ItemWidth = width;
        ItemHeight = height;
        UpdateInternalData();
        ItemBitmap.Enlarge(ItemWidth, ItemHeight); // pro header line
                                                   //    OldVertSI.cbSize = 0;
                                                   //    OldHorzSI.cbSize = 0;
                                                   //    SetupScrollBars();
                                                   //    CheckAndCorrectBoundaries();
    }
}

void CFilesBox::UpdateInternalData()
{
    EntireItemsInColumn = (FilesRect.bottom - FilesRect.top) / ItemHeight;
    if (EntireItemsInColumn < 1)
        EntireItemsInColumn = 1;
    switch (ViewMode)
    {
    case vmDetailed:
    {
        ColumnsCount = 1;
        EntireColumnsCount = 1;
        ItemsInColumn = ItemsCount;
        break;
    }

    case vmBrief:
    {
        ColumnsCount = (ItemsCount + (EntireItemsInColumn - 1)) / EntireItemsInColumn;
        EntireColumnsCount = (FilesRect.right - FilesRect.left) / ItemWidth;
        if (EntireColumnsCount < 1)
            EntireColumnsCount = 1;
        if (EntireColumnsCount > ColumnsCount)
            EntireColumnsCount = ColumnsCount;
        ItemsInColumn = EntireItemsInColumn;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        ColumnsCount = (FilesRect.right - FilesRect.left) / ItemWidth;
        if (ColumnsCount < 1)
            ColumnsCount = 1;
        EntireColumnsCount = ColumnsCount;
        ItemsInColumn = (ItemsCount + (EntireColumnsCount - 1)) / EntireColumnsCount;
        break;
    }
    }
}

int CFilesBox::GetEntireItemsInColumn()
{
    return EntireItemsInColumn;
}

int CFilesBox::GetColumnsCount()
{
    return 1;
}

void CFilesBox::PaintAllItems(HRGN hUpdateRgn, DWORD drawFlags)
{
    int index = TopIndex;

    if (hUpdateRgn != NULL)
        SelectClipRgn(HPrivateDC, hUpdateRgn);

    RECT clipRect;
    int clipRectType = NULLREGION;
    if ((drawFlags & DRAWFLAG_IGNORE_CLIPBOX) == 0)
        clipRectType = GetClipBox(HPrivateDC, &clipRect);

    if (clipRectType == NULLREGION ||
        (clipRectType == SIMPLEREGION &&
         clipRect.left <= FilesRect.left && clipRect.top <= FilesRect.top &&
         clipRect.right >= FilesRect.right && clipRect.bottom >= FilesRect.bottom))
    {
        // pokud neni nastavena clipovaci oblast nebo je to prosty obdelnik,
        // jehoz clipovani zajistime v nasledujicim testu, nebudeme se zdrzovat
        // s testovanim viditelnosti jednotlivych polozek
        drawFlags |= DRAWFLAG_SKIP_VISTEST;
    }

    BOOL showDragBox = FALSE;
    if (Parent->DragBox && Parent->DragBoxVisible)
    {
        Parent->DrawDragBox(Parent->OldBoxPoint); // zhasneme box
        showDragBox = TRUE;
    }
    if (ImageDragging)
        ImageDragShow(FALSE);

    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed mode
        int y = FilesRect.top;
        int yMax = FilesRect.bottom;
        RECT r;
        r.left = FilesRect.left;
        r.right = FilesRect.right;

        // posunutim mezi kresleni zajistime nekresleni polozek mimo orezany obdelnik
        if (clipRectType == SIMPLEREGION || clipRectType == COMPLEXREGION)
        {
            RECT tmpRect = clipRect;
            if (tmpRect.top < FilesRect.top)
                tmpRect.top = FilesRect.top;
            if (tmpRect.bottom > FilesRect.bottom)
                tmpRect.bottom = FilesRect.bottom;

            // posuneme horni hranici
            int indexOffset = (tmpRect.top - FilesRect.top) / ItemHeight;
            index += indexOffset;
            y += indexOffset * ItemHeight;
            // posuneme spodni hranici
            yMax -= FilesRect.bottom - tmpRect.bottom;
        }

        while (y < yMax && index >= 0 && index < ItemsCount)
        {
            r.top = y;
            r.bottom = y + ItemHeight;
            Parent->DrawBriefDetailedItem(HPrivateDC, index, &r, drawFlags);
            y += ItemHeight;
            index++;
        }

        if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && !(drawFlags & DRAWFLAG_ICON_ONLY) && y < yMax)
        {
            // domazu spodek
            r.top = y;
            r.bottom = yMax;
            FillRect(HPrivateDC, &r, HNormalBkBrush);
        }
        break;
    }

    case vmBrief:
    {
        // vmBrief mode
        int x = FilesRect.left;
        int y = FilesRect.top;
        int yMax = FilesRect.bottom;
        int xMax = FilesRect.right;
        RECT r;
        r.left = FilesRect.left;
        r.right = FilesRect.left + ItemWidth;

        int firstCol = index / EntireItemsInColumn;
        //      int lastCol = firstCol + (FilesRect.right - FilesRect.left + ItemWidth - 1) / ItemWidth - 1;

        // posunutim mezi kresleni zajistime nekresleni polozek mimo orezany obdelnik
        if (clipRectType == SIMPLEREGION || clipRectType == COMPLEXREGION)
        {
            RECT tmpRect = clipRect;
            if (tmpRect.left < FilesRect.left)
                tmpRect.left = FilesRect.left;
            if (tmpRect.right > FilesRect.right)
                tmpRect.right = FilesRect.right;

            // posuneme levou hranici
            int colOffset = (tmpRect.left - FilesRect.left) / ItemWidth;
            firstCol += colOffset;
            r.left += colOffset * ItemWidth;
            // posuneme pravou hranici
            xMax -= FilesRect.right - tmpRect.right;
        }

        index = firstCol * EntireItemsInColumn;
        BOOL painted = FALSE;
        while (index < ItemsCount && r.left <= xMax)
        {
            r.top = y;
            r.bottom = y + ItemHeight;
            r.right = r.left + ItemWidth;
            Parent->DrawBriefDetailedItem(HPrivateDC, index, &r, drawFlags);
            painted = TRUE;
            y += ItemHeight;
            if (y + ItemHeight > yMax)
            {
                if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && y < yMax)
                {
                    // domazu spodek pod plnym sloupcem
                    r.top = y;
                    r.bottom = yMax;
                    FillRect(HPrivateDC, &r, HNormalBkBrush);
                }
                y = FilesRect.top;
                r.left += ItemWidth;
                firstCol++;
            }
            index++;
        }

        if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && painted && y > r.top && y < FilesRect.bottom)
        {
            // domazu prostor pod nedokreslenym poslednim sloupcem
            r.top = y;
            r.bottom = FilesRect.bottom;
            FillRect(HPrivateDC, &r, HNormalBkBrush);
            r.left += ItemWidth;
        }
        if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && r.left <= FilesRect.right)
        {
            // domazu prostor vpravo za poslednim sloupcem
            r.top = FilesRect.top;
            r.right = FilesRect.right;
            r.bottom = FilesRect.bottom;
            FillRect(HPrivateDC, &r, HNormalBkBrush);
        }
        break;
    }

    case vmThumbnails:
    case vmIcons:
    case vmTiles:
    {
        // vmIcons || vmThumbnails mode
        RECT r;

        // napocitame hranice kresleni
        int firstRow = TopIndex / ItemHeight;
        int xMax = FilesRect.right;
        int yMax = FilesRect.bottom;

        // omezime kresleni pouze na oblast zadajici si prekresleni
        if (clipRectType == SIMPLEREGION || clipRectType == COMPLEXREGION)
        {
            RECT tmpRect = clipRect;
            if (tmpRect.top < FilesRect.top)
                tmpRect.top = FilesRect.top;
            if (tmpRect.bottom > FilesRect.bottom)
                tmpRect.bottom = FilesRect.bottom;

            // posuneme horni hranici
            firstRow = (TopIndex + tmpRect.top) / ItemHeight;
            // posuneme pravou hranici
            yMax -= FilesRect.bottom - tmpRect.bottom;
        }

        // ridici promenne pro umisteni jednotlivych polozek
        int x = FilesRect.left;
        int y = FilesRect.top - TopIndex + firstRow * ItemHeight;

        // budeme kreslit zprava doleva a kdyz narazime na hranici oblasti,
        // prejdeme dolu na dalsi radek
        int index2 = firstRow * ColumnsCount;
        while (index2 < ItemsCount && y < yMax)
        {
            r.top = y;
            r.bottom = y + ItemHeight;
            r.left = x;
            r.right = x + ItemWidth;
            // nakreslim vlastni polozku
            CIconSizeEnum iconSize = (ViewMode == vmIcons) ? ICONSIZE_32 : ICONSIZE_48;
            if (ViewMode == vmTiles)
                Parent->DrawTileItem(HPrivateDC, index2, &r, drawFlags, iconSize);
            else
                Parent->DrawIconThumbnailItem(HPrivateDC, index2, &r, drawFlags, iconSize);
            // posunem se na dalsi
            x += ItemWidth;
            // pokud se jedna o posledni polozku na radku, domazu prostor za ni
            if (x + ItemWidth > xMax || index2 == ItemsCount - 1)
            {
                if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && x < xMax)
                {
                    r.left = x;
                    r.right = xMax;
                    FillRect(HPrivateDC, &r, HNormalBkBrush);
                }
                x = FilesRect.left;
                y += ItemHeight;
            }
            index2++;
        }

        if (!(drawFlags & DRAWFLAG_DIRTY_ONLY) && !(drawFlags & DRAWFLAG_ICON_ONLY) && y < yMax)
        {
            // domazu spodek
            r.left = FilesRect.left;
            r.right = FilesRect.right;
            r.top = y;
            r.bottom = FilesRect.bottom;
            FillRect(HPrivateDC, &r, HNormalBkBrush);
        }
        break;
    }
    }

    // je-li panel prazdny, zduraznime to
    if (ItemsCount == 0 && (drawFlags & DRAWFLAG_ICON_ONLY) == 0)
    {
        char textBuf[300];
        textBuf[0] = 0;
        if (!Parent->Is(ptPluginFS) || !Parent->GetPluginFS()->NotEmpty() ||
            !Parent->GetPluginFS()->GetNoItemsInPanelText(textBuf, 300))
        {
            lstrcpyn(textBuf, LoadStr(IDS_NOITEMSINPANEL), 300);
        }
        RECT textR = FilesRect;
        textR.bottom = textR.top + FontCharHeight + 4;
        BOOL focused = (Parent->FocusVisible || Parent->Parent->EditMode && Parent->Parent->GetActivePanel() == Parent);
        COLORREF newColor;
        if (focused)
        {
            FillRect(HPrivateDC, &textR, HFocusedBkBrush);
            newColor = GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED]);
        }
        else
        {
            FillRect(HPrivateDC, &textR, HNormalBkBrush);
            newColor = GetCOLORREF(CurrentColors[ITEM_FG_NORMAL]);
        }
        int oldBkMode = SetBkMode(HPrivateDC, TRANSPARENT);
        int oldTextColor = SetTextColor(HPrivateDC, newColor);
        HFONT hOldFont = (HFONT)SelectObject(HPrivateDC, Font);
        // kdyby mrkalo, muzeme volat omereni textu + ExtTextOut
        DrawText(HPrivateDC, textBuf, -1,
                 &textR, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(HPrivateDC, hOldFont);
        SetTextColor(HPrivateDC, oldTextColor);
        SetBkMode(HPrivateDC, oldBkMode);
    }

    if (ImageDragging)
        ImageDragShow(TRUE);
    if (showDragBox)
        Parent->DrawDragBox(Parent->OldBoxPoint); // zase ho nahodime

    if (hUpdateRgn != NULL)
        SelectClipRgn(HPrivateDC, NULL); // vykopneme clip region, pokud jsme ho nastavili
}

void CFilesBox::PaintItem(int index, DWORD drawFlags)
{
    RECT r;
    if (GetItemRect(index, &r))
    {
        BOOL showDragBox = FALSE;
        if (Parent->DragBox && Parent->DragBoxVisible)
        {
            Parent->DrawDragBox(Parent->OldBoxPoint); // zhasneme box
            showDragBox = TRUE;
        }
        // pokud je treba, zhasneme drag image
        BOOL showImage = FALSE;
        if (ImageDragging)
        {
            RECT r2 = r;
            POINT p;
            p.x = r2.left;
            p.y = r2.top;
            ClientToScreen(HWindow, &p);
            r2.right = p.x + r2.right - r2.left;
            r2.bottom = p.y + r2.bottom - r2.top;
            r2.left = p.x;
            r2.top = p.y;
            if (ImageDragInterfereRect(&r2))
            {
                ImageDragShow(FALSE);
                showImage = TRUE;
            }
        }
        switch (ViewMode)
        {
        case vmBrief:
        case vmDetailed:
        {
            // nebudeme testovat viditelnost - jde nam o rychlost
            Parent->DrawBriefDetailedItem(HPrivateDC, index, &r, drawFlags | DRAWFLAG_SKIP_VISTEST);
            break;
        }

        case vmIcons:
        case vmThumbnails:
        {
            // nebudeme testovat viditelnost - jde nam o rychlost
            CIconSizeEnum iconSize = (ViewMode == vmIcons) ? ICONSIZE_32 : ICONSIZE_48;
            Parent->DrawIconThumbnailItem(HPrivateDC, index, &r, drawFlags | DRAWFLAG_SKIP_VISTEST, iconSize);
            break;
        }

        case vmTiles:
        {
            // nebudeme testovat viditelnost - jde nam o rychlost
            CIconSizeEnum iconSize = ICONSIZE_48;
            Parent->DrawTileItem(HPrivateDC, index, &r, drawFlags | DRAWFLAG_SKIP_VISTEST, iconSize);
            break;
        }
        }
        if (showImage)
            ImageDragShow(TRUE);
        if (showDragBox)
            Parent->DrawDragBox(Parent->OldBoxPoint); // zase ho nahodime
    }
}

BOOL CFilesBox::GetItemRect(int index, RECT* rect)
{
    switch (ViewMode)
    {
    case vmDetailed:
    {
        rect->left = FilesRect.left;
        rect->right = FilesRect.right;
        rect->top = FilesRect.top + ItemHeight * (index - TopIndex);
        rect->bottom = rect->top + ItemHeight;
        break;
    }

    case vmBrief:
    {
        int leftColumn = TopIndex / ItemWidth;
        int col = (index - TopIndex) / EntireItemsInColumn;
        int row = (index - TopIndex) % EntireItemsInColumn;
        rect->left = FilesRect.left + col * ItemWidth;
        rect->right = rect->left + ItemWidth;
        rect->top = FilesRect.top + row * ItemHeight;
        rect->bottom = rect->top + ItemHeight;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        int row = index / ColumnsCount;
        int col = index % ColumnsCount;
        rect->left = FilesRect.left + col * ItemWidth;
        rect->right = rect->left + ItemWidth;
        rect->top = FilesRect.top + row * ItemHeight - TopIndex;
        rect->bottom = rect->top + ItemHeight;
        break;
    }
    }
    RECT dummy;
    return IntersectRect(&dummy, rect, &FilesRect) != 0;
}

void CFilesBox::EnsureItemVisible(int index, BOOL forcePaint, BOOL scroll, BOOL selFocChangeOnly)
{
    if (index < 0 || index >= ItemsCount)
    {
        TRACE_E("index=" << index);
        return;
    }

    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed
        int newTopIndex = TopIndex;
        if (index < TopIndex)
            newTopIndex = index;
        else
        {
            if (index > TopIndex + EntireItemsInColumn - 1)
            {
                if (!scroll || ItemHeight * (index - TopIndex) >= FilesRect.bottom - FilesRect.top)
                    newTopIndex = index - EntireItemsInColumn + 1;
            }
        }

        if (newTopIndex != TopIndex)
        {
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            if (ImageDragging)
                ImageDragShow(FALSE);
            ScrollWindowEx(HWindow, 0, ItemHeight * (TopIndex - newTopIndex),
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newTopIndex;
            // napocitam region nove polozky a prictu ho k regionu vzniklemu pri odrolovani
            HRGN hItemRgn = HANDLES(CreateRectRgn(FilesRect.left,
                                                  FilesRect.top + ItemHeight * (index - TopIndex),
                                                  FilesRect.right,
                                                  FilesRect.top + ItemHeight * (index - TopIndex + 1)));
            CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
            HANDLES(DeleteObject(hItemRgn));
            SetupScrollBars(UPDATE_VERT_SCROLL);
            // pri zmene bitu Selection doslo k nastaveni Diry flagu a my ho pri
            // rolovani nesmime shodit, protoze castecne viditelne polozky neprekreslime
            // kompletne a bude potreba po nasem rolovani kreslit jeste jednou
            PaintAllItems(hUpdateRgn, DRAWFLAG_KEEP_DIRTY);
            if (ImageDragging)
                ImageDragShow(TRUE);
            HANDLES(DeleteObject(hUpdateRgn));
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
        }
        else if (forcePaint)
            PaintItem(index, selFocChangeOnly ? DRAWFLAG_SELFOC_CHANGE : 0);
        break;
    }

    case vmBrief:
    {
        // vmBrief
        int leftCol = TopIndex / EntireItemsInColumn;
        int newLeftCol = leftCol;

        int col = index / EntireItemsInColumn;

        if (col < leftCol)
            newLeftCol = col;
        else
        {
            if (col >= leftCol + EntireColumnsCount)
            {
                if (!scroll || ItemWidth * (col - leftCol) >= FilesRect.right - FilesRect.left)
                {
                    newLeftCol = col - EntireColumnsCount + 1;
                    if (newLeftCol > ColumnsCount - EntireColumnsCount)
                        newLeftCol = ColumnsCount - EntireColumnsCount;
                }
            }
        }

        if (newLeftCol != leftCol)
        {
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            if (ImageDragging)
                ImageDragShow(FALSE);
            ScrollWindowEx(HWindow, ItemWidth * (leftCol - newLeftCol), 0,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newLeftCol * EntireItemsInColumn;
            // napocitam region nove polozky a prictu ho k regionu vzniklemu pri odrolovani
            HRGN hItemRgn = HANDLES(CreateRectRgn(FilesRect.left + ItemWidth * (col - newLeftCol),
                                                  FilesRect.top,
                                                  FilesRect.left + ItemWidth * (col - newLeftCol + 1),
                                                  FilesRect.bottom));
            CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
            HANDLES(DeleteObject(hItemRgn));
            SetupScrollBars(UPDATE_HORZ_SCROLL);
            // pri zmene bitu Selection doslo k nastaveni Diry flagu a my ho pri
            // rolovani nesmime shodit, protoze castecne viditelne polozky neprekreslime
            // kompletne a bude potreba po nasem rolovani kreslit jeste jednou
            PaintAllItems(hUpdateRgn, DRAWFLAG_KEEP_DIRTY);
            if (ImageDragging)
                ImageDragShow(TRUE);
            HANDLES(DeleteObject(hUpdateRgn));
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
        }
        else if (forcePaint)
            PaintItem(index, selFocChangeOnly ? DRAWFLAG_SELFOC_CHANGE : 0);
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails
        int newTopIndex = TopIndex;

        // napocitam pozici polozky
        int itemTop = FilesRect.top + (index / ColumnsCount) * ItemHeight;
        int itemBottom = itemTop + ItemHeight;

        if (itemTop < TopIndex)
        {
            if (!scroll || itemBottom <= TopIndex)
                newTopIndex = itemTop;
        }
        else
        {
            int pageHeight = FilesRect.bottom - FilesRect.top;
            if (itemBottom > TopIndex + pageHeight)
            {
                if (!scroll || itemTop >= TopIndex + pageHeight)
                    newTopIndex = itemBottom - pageHeight;
            }
        }

        if (newTopIndex != TopIndex)
        {
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            if (ImageDragging)
                ImageDragShow(FALSE);
            ScrollWindowEx(HWindow, 0, TopIndex - newTopIndex,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newTopIndex;
            // napocitam region nove polozky a prictu ho k regionu vzniklemu pri odrolovani
            int itemLeft = FilesRect.left + (index % ColumnsCount) * ItemWidth;
            HRGN hItemRgn = HANDLES(CreateRectRgn(itemLeft, itemTop - TopIndex,
                                                  itemLeft + ItemWidth, itemBottom - TopIndex));
            CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
            HANDLES(DeleteObject(hItemRgn));
            SetupScrollBars(UPDATE_VERT_SCROLL);
            // pri zmene bitu Selection doslo k nastaveni Diry flagu a my ho pri
            // rolovani nesmime shodit, protoze castecne viditelne polozky neprekreslime
            // kompletne a bude potreba po nasem rolovani kreslit jeste jednou
            PaintAllItems(hUpdateRgn, DRAWFLAG_KEEP_DIRTY);
            if (ImageDragging)
                ImageDragShow(TRUE);
            HANDLES(DeleteObject(hUpdateRgn));
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
        }
        else if (forcePaint)
            PaintItem(index, selFocChangeOnly ? DRAWFLAG_SELFOC_CHANGE : 0);
        break;
    }
    }
}

void CFilesBox::GetVisibleItems(int* firstIndex, int* count)
{
    *firstIndex = *count = 0;
    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed
        *firstIndex = TopIndex;
        *count = EntireItemsInColumn +
                 (ItemHeight * EntireItemsInColumn < FilesRect.bottom - FilesRect.top ? 1 : 0);
        break;
    }

    case vmBrief:
    {
        // vmBrief
        *firstIndex = TopIndex;
        *count = (EntireColumnsCount + (ItemWidth * EntireColumnsCount < FilesRect.right - FilesRect.left ? 1 : 0)) *
                 EntireItemsInColumn;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails
        if (ItemHeight != 0)
        {
            *firstIndex = (TopIndex / ItemHeight) * ColumnsCount;
            int last = ((TopIndex + FilesRect.bottom - FilesRect.top - 1) / ItemHeight) * ColumnsCount +
                       ColumnsCount - 1;
            if (last >= *firstIndex)
                *count = last - *firstIndex + 1;
        }
        break;
    }
    }
    if (*firstIndex < 0)
        *firstIndex = 0;
    if (*count < 0)
        *count = 0;
}

// uvazuje scroll == TRUE - i castecne viditelna polozka je oznacena jako viditelna
BOOL CFilesBox::IsItemVisible(int index, BOOL* isFullyVisible)
{
    if (isFullyVisible != NULL)
        *isFullyVisible = FALSE;
    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed
        if (index < TopIndex)
            return FALSE; // moc nahore
        else
        {
            if (index > TopIndex + EntireItemsInColumn - 1)
            {
                if (ItemHeight * (index - TopIndex) >= FilesRect.bottom - FilesRect.top)
                    return FALSE; // moc dole
            }
            else
            {
                if (isFullyVisible != NULL)
                    *isFullyVisible = TRUE;
            }
        }
        break;
    }

    case vmBrief:
    {
        // vmBrief
        int leftCol = TopIndex / EntireItemsInColumn;
        int col = index / EntireItemsInColumn;
        if (col < leftCol)
            return FALSE; // moc vlevo
        else
        {
            if (col >= leftCol + EntireColumnsCount)
            {
                if (ItemWidth * (col - leftCol) >= FilesRect.right - FilesRect.left)
                    return FALSE; // moc vpravo
            }
            else
            {
                if (isFullyVisible != NULL)
                    *isFullyVisible = TRUE;
            }
        }
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails
        int itemTop = FilesRect.top + (index / ColumnsCount) * ItemHeight;
        int itemBottom = itemTop + ItemHeight;
        if (itemBottom <= TopIndex)
            return FALSE; // polozka je cela nad viditelnou plochou
        if (itemTop >= TopIndex + FilesRect.bottom - FilesRect.top)
            return FALSE; // polozka je cela pod viditelnou plochou
        if (isFullyVisible != NULL)
            *isFullyVisible = (itemTop >= TopIndex && itemBottom <= TopIndex + FilesRect.bottom - FilesRect.top);
        break;
    }
    }
    return TRUE; // je videt
}

// uvazuje scroll == FALSE - cela polozka bude viditelna
int CFilesBox::PredictTopIndex(int index)
{
    int newTopIndex = TopIndex;
    switch (ViewMode)
    {
    case vmDetailed:
    {
        // vmDetailed
        if (index < TopIndex)
            newTopIndex = index;
        else
        {
            if (index > TopIndex + EntireItemsInColumn - 1)
                newTopIndex = index - EntireItemsInColumn + 1;
        }
        break;
    }

    case vmBrief:
    {
        // vmBrief
        int leftCol = TopIndex / EntireItemsInColumn;
        int newLeftCol = leftCol;

        int col = index / EntireItemsInColumn;

        if (col < leftCol)
            newLeftCol = col;
        else
        {
            if (col >= leftCol + EntireColumnsCount)
            {
                newLeftCol = col - EntireColumnsCount + 1;
                if (newLeftCol > ColumnsCount - EntireColumnsCount)
                    newLeftCol = ColumnsCount - EntireColumnsCount;
            }
        }
        newTopIndex = newLeftCol * EntireItemsInColumn;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails

        // napocitam pozici polozky
        int itemTop = FilesRect.top + (index / ColumnsCount) * ItemHeight;
        int itemBottom = itemTop + ItemHeight;

        if (itemTop < TopIndex)
        {
            newTopIndex = itemTop;
        }
        else
        {
            int pageHeight = FilesRect.bottom - FilesRect.top;
            if (itemBottom > TopIndex + pageHeight)
                newTopIndex = itemBottom - pageHeight;
        }
        break;
    }
    }
    return newTopIndex;
}

void CFilesBox::EnsureItemVisible2(int newTopIndex, int index)
{
    if (newTopIndex == TopIndex)
        return;

    // upravim rolovaci plochu tak, aby neobsahovala vybranou polozku
    RECT sRect = FilesRect;
    if (index == newTopIndex)
        sRect.top += ItemHeight;
    else
        sRect.bottom = sRect.top + ItemHeight * (index - newTopIndex);

    if (ImageDragging)
        ImageDragShow(FALSE);
    HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
    ScrollWindowEx(HWindow, 0, ItemHeight * (TopIndex - newTopIndex),
                   &sRect, &sRect, hUpdateRgn, NULL, 0);

    // napocitam region nove polozky a prictu ho k regionu vzniklemu pri odrolovani
    RECT uRect = FilesRect;
    if (index == newTopIndex)
    {
        uRect.top = uRect.top + ItemHeight * (index - newTopIndex);
        uRect.bottom = uRect.top + ItemHeight;
    }
    else
    {
        uRect.top = uRect.top + ItemHeight * (index - newTopIndex);
    }
    HRGN hItemRgn = HANDLES(CreateRectRgn(uRect.left, uRect.top, uRect.right, uRect.bottom));
    CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
    HANDLES(DeleteObject(hItemRgn));

    TopIndex = newTopIndex;
    SetupScrollBars(UPDATE_VERT_SCROLL);
    // pri zmene bitu Selection doslo k nastaveni Diry flagu a my ho pri
    // rolovani nesmime shodit, protoze castecne viditelne polozky neprekreslime
    // kompletne a bude potreba po nasem rolovani kreslit jeste jednou
    PaintAllItems(hUpdateRgn, DRAWFLAG_KEEP_DIRTY);
    HANDLES(DeleteObject(hUpdateRgn));
    if (ImageDragging)
        ImageDragShow(TRUE);
    Parent->VisibleItemsArray.InvalidateArr();
    Parent->VisibleItemsArraySurround.InvalidateArr();
}

void CFilesBox::OnHScroll(int scrollCode, int pos)
{
    if (Parent->DragBox && !Parent->ScrollingWindow) // tahneme klec - zatluceme rolovani od mousewheel
        return;
    if (ViewMode == vmDetailed)
    {
        if (FilesRect.right - FilesRect.left + 1 > ItemWidth)
            return;
        int newXOffset = XOffset;
        switch (scrollCode)
        {
        case SB_LINEUP:
        {
            newXOffset -= ItemHeight;
            break;
        }

        case SB_LINEDOWN:
        {
            newXOffset += ItemHeight;
            break;
        }

        case SB_PAGEUP:
        {
            newXOffset -= FilesRect.right - FilesRect.left;
            break;
        }

        case SB_PAGEDOWN:
        {
            newXOffset += FilesRect.right - FilesRect.left;
            break;
        }

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
        {
            newXOffset = pos;
            break;
        }
        }
        if (newXOffset < 0)
            newXOffset = 0;
        if (newXOffset >= ItemWidth - (FilesRect.right - FilesRect.left) + 1)
            newXOffset = ItemWidth - (FilesRect.right - FilesRect.left);
        if (newXOffset != XOffset)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            if (ImageDragging)
                ImageDragShow(FALSE);
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            ScrollWindowEx(HWindow, XOffset - newXOffset, 0,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            if (HeaderLine.HWindow != NULL)
            {
                HRGN hUpdateRgn2 = HANDLES(CreateRectRgn(0, 0, 0, 0));
                ScrollWindowEx(HeaderLine.HWindow, XOffset - newXOffset, 0,
                               NULL, NULL, hUpdateRgn2, NULL, 0);
                XOffset = newXOffset;
                HDC hDC = HANDLES(GetDC(HeaderLine.HWindow));
                HeaderLine.PaintAllItems(hDC, hUpdateRgn2);
                HANDLES(ReleaseDC(HeaderLine.HWindow, hDC));
                HANDLES(DeleteObject(hUpdateRgn2));
            }
            else
                XOffset = newXOffset;
            PaintAllItems(hUpdateRgn, 0);
            HANDLES(DeleteObject(hUpdateRgn));
            if (ImageDragging)
                ImageDragShow(TRUE);
        }
        if (scrollCode != SB_THUMBTRACK)
            SetupScrollBars(UPDATE_HORZ_SCROLL);
    }
    else
    {
        // vmBrief
        if (EntireColumnsCount >= ColumnsCount)
            return;
        int leftColumn = TopIndex / EntireItemsInColumn;
        int newLeftColumn = TopIndex / EntireItemsInColumn;
        switch (scrollCode)
        {
        case SB_LINEUP:
        {
            newLeftColumn -= 1;
            break;
        }

        case SB_LINEDOWN:
        {
            newLeftColumn += 1;
            break;
        }

        case SB_PAGEUP:
        {
            newLeftColumn -= EntireColumnsCount;
            break;
        }

        case SB_PAGEDOWN:
        {
            newLeftColumn += EntireColumnsCount;
            break;
        }

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
        {
            newLeftColumn = pos;
            break;
        }
        }
        if (newLeftColumn < 0)
            newLeftColumn = 0;
        if (newLeftColumn > ColumnsCount - EntireColumnsCount)
            newLeftColumn = ColumnsCount - EntireColumnsCount;
        if (newLeftColumn != leftColumn)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            if (ImageDragging)
                ImageDragShow(FALSE);
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            ScrollWindowEx(HWindow, ItemWidth * (leftColumn - newLeftColumn), 0,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newLeftColumn * EntireItemsInColumn /*+ topIndexOffset*/;
            PaintAllItems(hUpdateRgn, 0);
            HANDLES(DeleteObject(hUpdateRgn));
            if (ImageDragging)
                ImageDragShow(TRUE);
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
            if (scrollCode == SB_THUMBTRACK)
            {
                Parent->VisibleItemsArray.RefreshArr(Parent);         // tady provedeme refresh natvrdo
                Parent->VisibleItemsArraySurround.RefreshArr(Parent); // tady provedeme refresh natvrdo
            }
        }
        if (scrollCode != SB_THUMBTRACK)
            SetupScrollBars(UPDATE_HORZ_SCROLL);
    }
}

void CFilesBox::OnVScroll(int scrollCode, int pos)
{
    if (Parent->DragBox && !Parent->ScrollingWindow) // tahneme klec - zatluceme rolovani od mousewheel
        return;
    int newTopIndex = TopIndex;
    if (ViewMode == vmDetailed)
    {
        // Detailed
        if (EntireItemsInColumn + 1 > ItemsCount)
            return;
        switch (scrollCode)
        {
        case SB_LINEUP:
        {
            newTopIndex--;
            break;
        }

        case SB_LINEDOWN:
        {
            newTopIndex++;
            break;
        }

        case SB_PAGEUP:
        {
            int delta = EntireItemsInColumn - 1;
            if (delta <= 0)
                delta = 1;
            newTopIndex -= delta;
            break;
        }

        case SB_PAGEDOWN:
        {
            int delta = EntireItemsInColumn - 1;
            if (delta <= 0)
                delta = 1;
            newTopIndex += delta;
            break;
        }

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
        {
            newTopIndex = pos;
            break;
        }
        }
        if (newTopIndex < 0)
            newTopIndex = 0;
        if (newTopIndex >= ItemsCount - EntireItemsInColumn + 1)
            newTopIndex = ItemsCount - EntireItemsInColumn;

        if (newTopIndex != TopIndex)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            if (ImageDragging)
                ImageDragShow(FALSE);
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            ScrollWindowEx(HWindow, 0, ItemHeight * (TopIndex - newTopIndex),
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newTopIndex;
            PaintAllItems(hUpdateRgn, 0);
            HANDLES(DeleteObject(hUpdateRgn));
            if (ImageDragging)
                ImageDragShow(TRUE);
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
            if (scrollCode == SB_THUMBTRACK)
            {
                Parent->VisibleItemsArray.RefreshArr(Parent);         // tady provedeme refresh natvrdo
                Parent->VisibleItemsArraySurround.RefreshArr(Parent); // tady provedeme refresh natvrdo
            }
        }
        if (scrollCode != SB_THUMBTRACK)
            SetupScrollBars(UPDATE_VERT_SCROLL);
    }
    else
    {
        // Icons || Thumbnails

        // na co byl tenhle test? prekazi pri rolovani pri panelu nizsim nez thumbnail/ikona
        //    if (EntireItemsInColumn * ItemHeight >= FilesRect.bottom - FilesRect.top)
        //      return;
        int lineDelta = ItemHeight;
        if (ItemHeight >= FilesRect.bottom - FilesRect.top)
            lineDelta = max(1, FilesRect.bottom - FilesRect.top); // ochrana pred zapornou nebo nulovou deltou
        switch (scrollCode)
        {
        case SB_LINEUP:
        {
            newTopIndex -= lineDelta;
            break;
        }

        case SB_LINEDOWN:
        {
            newTopIndex += lineDelta;
            break;
        }

        case SB_PAGEUP:
        case SB_PAGEDOWN:
        {
            int delta = EntireItemsInColumn * lineDelta; //FilesRect.bottom - FilesRect.top;
            if (delta <= 0)
                delta = 1;
            if (scrollCode == SB_PAGEUP)
                newTopIndex -= delta;
            else
                newTopIndex += delta;
            break;
        }

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
        {
            newTopIndex = pos;
            break;
        }
        }
        if (newTopIndex > ItemsInColumn * ItemHeight - (FilesRect.bottom - FilesRect.top))
            newTopIndex = ItemsInColumn * ItemHeight - (FilesRect.bottom - FilesRect.top);
        if (newTopIndex < 0)
            newTopIndex = 0;

        if (newTopIndex != TopIndex)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

            if (ImageDragging)
                ImageDragShow(FALSE);
            HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
            ScrollWindowEx(HWindow, 0, TopIndex - newTopIndex,
                           &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
            TopIndex = newTopIndex;
            PaintAllItems(hUpdateRgn, 0);
            HANDLES(DeleteObject(hUpdateRgn));
            if (ImageDragging)
                ImageDragShow(TRUE);
            Parent->VisibleItemsArray.InvalidateArr();
            Parent->VisibleItemsArraySurround.InvalidateArr();
            if (scrollCode == SB_THUMBTRACK)
            {
                Parent->VisibleItemsArray.RefreshArr(Parent);         // tady provedeme refresh natvrdo
                Parent->VisibleItemsArraySurround.RefreshArr(Parent); // tady provedeme refresh natvrdo
            }
        }
        if (scrollCode != SB_THUMBTRACK)
            SetupScrollBars(UPDATE_VERT_SCROLL);
    }
}

LRESULT
CFilesBox::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CFilesBox::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HPrivateDC = NOHANDLES(GetDC(HWindow));
        break;
    }

    case WM_SIZE:
    {
        LayoutChilds();
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));
        PaintAllItems(NULL, 0);
        HANDLES(EndPaint(HWindow, &ps));
        SelectClipRgn(HPrivateDC, NULL);
        return 0;
    }

    case WM_HELP:
    {
        if (MainWindow->HasLockedUI())
            break;
        if (MainWindow->HelpMode == HELP_INACTIVE &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0 &&
            (GetKeyState(VK_MENU) & 0x8000) == 0)
        {
            PostMessage(MainWindow->HWindow, WM_COMMAND, CM_HELP_CONTENTS, 0); // jen F1 (zadne modifikatory)
            return TRUE;
        }
        break;
    }

    case WM_SYSCHAR:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnSysChar(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_CHAR:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnChar(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnSysKeyUp(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (MainWindow->HasLockedUI())
            break;
        BOOL firstPress = (lParam & 0x40000000) == 0;
        if (firstPress) // pokud je SHIFT stisteny, chodi autorepeat, ale nas zajima jen ten prvni stisk
            ResetMouseWheelAccumulator();
        LRESULT lResult;
        if (Parent->OnSysKeyDown(uMsg, wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_SETFOCUS:
    {
        ResetMouseWheelAccumulator();
        Parent->OnSetFocus();
        break;
    }

    case WM_KILLFOCUS:
    {
        ResetMouseWheelAccumulator();
        Parent->OnKillFocus((HWND)wParam);
        break;
    }

    case WM_NCPAINT:
    {
        HDC hdc = HANDLES(GetWindowDC(HWindow));
        if (wParam != 1)
        {
            // region je ve screen souradnicich, posuneme ho
            RECT wR;
            GetWindowRect(HWindow, &wR);
            OffsetRgn((HRGN)wParam, -wR.left, -wR.top);

            // oklipujeme dc
            SelectClipRgn(hdc, (HRGN)wParam);
        }
        RECT r;
        GetClientRect(HWindow, &r);
        r.right += 2;
        r.bottom += 2;
        DrawEdge(hdc, &r, BDR_SUNKENOUTER, BF_RECT);
        if (Parent->StatusLine != NULL && Parent->StatusLine->HWindow != NULL)
        {
            r.left = 0;
            r.top = r.bottom - 1;
            r.right = 1;
            r.bottom = r.top + 1;
            FillRect(hdc, &r, HMenuGrayTextBrush);
        }
        HANDLES(ReleaseDC(HWindow, hdc));
        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_UPDATE)
        {
            Parent->AdjustQuickRenameWindow();
            return 0;
        }
        break;
    }

    // chytneme kliknuti na scrollbary a vytahneme Salama nahoru
    case WM_PARENTNOTIFY:
    {
        WORD fwEvent = LOWORD(wParam);
        if (fwEvent == WM_LBUTTONDOWN || fwEvent == WM_MBUTTONDOWN || fwEvent == WM_RBUTTONDOWN)
        {
            if (GetActiveWindow() == NULL)
                SetForegroundWindow(MainWindow->HWindow);
        }
        break;
    }

    case WM_VSCROLL:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        int scrollCode = (int)LOWORD(wParam);
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(HVScrollBar, SB_CTL, &si);
        int pos = si.nTrackPos;
        OnVScroll(scrollCode, pos);
        return 0;
    }

    case WM_HSCROLL:
    {
        if (MainWindow->HasLockedUI())
            break;
        int scrollCode = (int)LOWORD(wParam);
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(HHScrollBar, SB_CTL, &si);
        int pos = si.nTrackPos;
        OnHScroll(scrollCode, pos);
        return 0;
    }

    case WM_MOUSEACTIVATE:
    {
        if (MainWindow->HasLockedUI())
            break; // behem zamceneho stavu chceme, aby kliknuti do panelu vytahlo Salamandera nahoru
        if (LOWORD(lParam) == HTCLIENT)
        {
            if (!IsIconic(MainWindow->HWindow) &&
                HIWORD(lParam) == WM_LBUTTONDOWN)
                return MA_NOACTIVATE;
            if (!IsIconic(MainWindow->HWindow) &&
                HIWORD(lParam) == WM_RBUTTONDOWN)
                return MA_NOACTIVATE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnLButtonDown(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_LBUTTONDBLCLK:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnLButtonDblclk(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_LBUTTONUP:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnLButtonUp(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_RBUTTONDOWN:
    {
        ResetMouseWheelAccumulator();
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnRButtonDown(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_RBUTTONUP:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnRButtonUp(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnMouseMove(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_TIMER:
    {
        if (MainWindow->HasLockedUI())
            break;
        LRESULT lResult;
        if (Parent->OnTimer(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_SETCURSOR:
    {
        if (Parent->TrackingSingleClick)
            return TRUE;
        break;
    }

    case WM_CAPTURECHANGED:
    {
        LRESULT lResult;
        if (Parent->OnCaptureChanged(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_MOUSELEAVE:
    case WM_CANCELMODE:
    {
        LRESULT lResult;
        if (Parent->OnCancelMode(wParam, lParam, &lResult))
            return lResult;
        break;
    }

    case WM_INITMENUPOPUP: // pozor, obdobny kod je jeste v CMainWindow
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:
    {
        if (MainWindow->HasLockedUI())
            break;
        if (Parent->Is(ptPluginFS) && Parent->GetPluginFS()->NotEmpty() &&
            Parent->GetPluginFS()->IsServiceSupported(FS_SERVICE_CONTEXTMENU))
        {
            LRESULT lResult;
            if (Parent->GetPluginFS()->HandleMenuMsg(uMsg, wParam, lParam, &lResult))
                return lResult;
        }
        if (Parent->ContextMenu != NULL)
        {
            IContextMenu3* contextMenu3 = NULL;
            if (uMsg == WM_MENUCHAR)
            {
                if (SUCCEEDED(Parent->ContextMenu->QueryInterface(IID_IContextMenu3, (void**)&contextMenu3)))
                {
                    LRESULT lResult;
                    contextMenu3->HandleMenuMsg2(uMsg, wParam, lParam, &lResult);
                    contextMenu3->Release();
                    return lResult;
                }
            }
            Parent->ContextMenu->HandleMenuMsg(uMsg, wParam, lParam);
        }
        // aby fungovalo New submenu, je potreba jeste preposlat zpravu tam
        if (Parent->ContextSubmenuNew != NULL && Parent->ContextSubmenuNew->MenuIsAssigned())
        {
            CALL_STACK_MESSAGE1("CFilesBox::WindowProc::SafeHandleMenuNewMsg2");
            LRESULT lResult;
            Parent->SafeHandleMenuNewMsg2(uMsg, wParam, lParam, &lResult);
            return lResult;
        }
        if (Parent->ContextMenu != NULL)
            return 0;
        break;
    }

    case WM_USER_MOUSEHWHEEL: // horizontalni rolovani, chodi od Windows Vista
    {
        if (MainWindow->HasLockedUI())
            break;
        if (ViewMode == vmDetailed || ViewMode == vmBrief)
        {
            short zDelta = (short)HIWORD(wParam);
            if ((zDelta < 0 && MouseHWheelAccumulator > 0) || (zDelta > 0 && MouseHWheelAccumulator < 0))
                ResetMouseWheelAccumulator(); // pri zmene smeru naklapeni kolecka je potreba nulovat akumulator

            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
            GetScrollInfo(HHScrollBar, SB_CTL, &si);

            DWORD wheelScroll = 1;
            if (ViewMode == vmDetailed)
            {
                wheelScroll = ItemHeight * GetMouseWheelScrollChars();
                wheelScroll = max(1, min(wheelScroll, si.nPage - 1)); // omezime maximalne na delku stranky
            }

            MouseHWheelAccumulator += 1000 * zDelta;
            int stepsPerChar = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int charsToScroll = MouseHWheelAccumulator / stepsPerChar;
            if (charsToScroll != 0)
            {
                MouseHWheelAccumulator -= charsToScroll * stepsPerChar;
                OnHScroll(SB_THUMBPOSITION, si.nPos + charsToScroll);
            }
        }
        return 0;
    }

    case WM_MOUSEHWHEEL:
    case WM_MOUSEWHEEL:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        // 7.10.2009 - AS253_B1_IB34: Manison nam hlasil, ze mu pod Windows Vista nefunguje horizontalni scroll.
        // Me fungoval (touto cestou). Po nainstalovani Intellipoint ovladacu v7 (predtime jsem na Vista x64
        // nemel zadne spesl ovladace) prestaly WM_MOUSEHWHEEL zpravy prochazet skraz hooka a natejkaly primo
        // do focused okna; zakazal jsem hook a nyni musime chytat zpravy v oknech, ktere mohou mit focus, aby
        // doslo k forwardu.
        // 30.11.2012 - na foru se objevil clovek, kteremu WM_MOUSEHWEEL nechodi skrz message hook (stejna jako drive
        // u Manisona v pripade WM_MOUSEHWHEEL): https://forum.altap.cz/viewtopic.php?f=24&t=6039
        // takze nove budeme zpravu chytat take v jednotlivych oknech, kam muze potencialne chodit (dle focusu)
        // a nasledne ji routit tak, aby se dorucila do okna pod kurzorem, jak jsme to vzdy delali

        // pokud zprava prisla "nedavno" druhym kanalem, budeme tento kanal ignorovat
        if (MouseWheelMSGThroughHook && MouseWheelMSGTime != 0 && (GetTickCount() - MouseWheelMSGTime < MOUSEWHEELMSG_VALID))
            return 0;
        MouseWheelMSGThroughHook = FALSE;
        MouseWheelMSGTime = GetTickCount();

        MSG msg;
        DWORD pos = GetMessagePos();
        msg.pt.x = GET_X_LPARAM(pos);
        msg.pt.y = GET_Y_LPARAM(pos);
        msg.lParam = lParam;
        msg.wParam = wParam;
        msg.hwnd = HWindow;
        msg.message = uMsg;
        PostMouseWheelMessage(&msg);
        return 0;
    }

    case WM_USER_MOUSEWHEEL:
    {
        if (MainWindow->HasLockedUI())
            break;
        // spravna podpora pro kolecko viz http://msdn.microsoft.com/en-us/library/ms997498.aspx "Best Practices for Supporting Microsoft Mouse and Keyboard Devices"

        if (Parent->DragBox) // zamezi chybam v kresleni
            return 0;

        Parent->KillQuickRenameTimer(); // zamezime pripadnemu otevreni QuickRenameWindow

        short zDelta = (short)HIWORD(wParam);
        if ((zDelta < 0 && MouseWheelAccumulator > 0) || (zDelta > 0 && MouseWheelAccumulator < 0))
            ResetMouseWheelAccumulator(); // pri zmene smeru otaceni kolecka je potreba nulovat akumulator

        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        // standardni scrolovani bez modifikacnich klaves
        if (!controlPressed && !altPressed && !shiftPressed)
        {
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;

            if (ViewMode == vmBrief)
            {
                GetScrollInfo(HHScrollBar, SB_CTL, &si);
                MouseWheelAccumulator += 1000 * zDelta;
                int stepsPerCol = max(1, (1000 * WHEEL_DELTA));
                int colsToScroll = MouseWheelAccumulator / stepsPerCol;
                if (colsToScroll != 0)
                {
                    MouseWheelAccumulator -= colsToScroll * stepsPerCol;
                    OnHScroll(SB_THUMBPOSITION, si.nPos - colsToScroll);
                }
            }
            else
            {
                GetScrollInfo(HVScrollBar, SB_CTL, &si);

                DWORD wheelScroll = max(1, ItemHeight);
                if (ViewMode == vmDetailed)
                {
                    wheelScroll = GetMouseWheelScrollLines();             // muze byt az WHEEL_PAGESCROLL(0xffffffff)
                    wheelScroll = max(1, min(wheelScroll, si.nPage - 1)); // omezime maximalne na delku stranky
                }

                // wheelScrollLines je pod WinVista s IntelliMouse Explorer 4.0 a IntelliPoint ovladacich, pri nejvyssi "rychlosti" kolecka rovna 40
                // stepsPerLine by pak vychazelo 120 / 31 = 3,870967741935...., po oriznuti 3, tedy velika zaokrouhlovaci chyba
                // proto hodnoty prenasobim 1000 a posunu chybo o tri rady dal
                MouseWheelAccumulator += 1000 * zDelta;
                int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
                int linesToScroll = MouseWheelAccumulator / stepsPerLine;
                if (linesToScroll != 0)
                {
                    MouseWheelAccumulator -= linesToScroll * stepsPerLine;
                    OnVScroll(SB_THUMBPOSITION, si.nPos - linesToScroll);
                }
            }
        }

        // SHIFT: horizontalni rolovani
        if (!controlPressed && !altPressed && shiftPressed &&
            (ViewMode == vmDetailed || ViewMode == vmBrief))
        {
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
            GetScrollInfo(HHScrollBar, SB_CTL, &si);

            DWORD wheelScroll = 1;
            if (ViewMode == vmDetailed)
            {
                wheelScroll = ItemHeight * GetMouseWheelScrollLines(); // 'delta' muze byt az WHEEL_PAGESCROLL(0xffffffff)
                wheelScroll = max(1, min(wheelScroll, si.nPage));      // omezime maximalne na sirku stranky
            }

            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int linesToScroll = MouseWheelAccumulator / stepsPerLine;
            if (linesToScroll != 0)
            {
                MouseWheelAccumulator -= linesToScroll * stepsPerLine;
                OnHScroll(SB_THUMBPOSITION, si.nPos - linesToScroll);
            }
        }

        // ALT: prepinani rezimu panelu (details, brief, ...)
        if (!controlPressed && altPressed && !shiftPressed)
        {
            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerView = max(1, (1000 * WHEEL_DELTA));
            int viewsToScroll = MouseWheelAccumulator / stepsPerView;
            if (viewsToScroll != 0)
            {
                MouseWheelAccumulator -= viewsToScroll * stepsPerView;
                int newIndex = Parent->GetNextTemplateIndex(viewsToScroll < 0 ? TRUE : FALSE, FALSE);
                Parent->SelectViewTemplate(newIndex, TRUE, FALSE);
            }
        }

        // CTRL: zoomovani thumbnailu
        if (controlPressed && !altPressed && !shiftPressed &&
            ViewMode == vmThumbnails)
        {
            DWORD wheelScroll = 15;
            MouseWheelAccumulator += 1000 * zDelta;
            int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
            int linesToScroll = MouseWheelAccumulator / stepsPerLine;
            if (linesToScroll != 0)
            {
                MouseWheelAccumulator -= linesToScroll * stepsPerLine;

                int size = Configuration.ThumbnailSize;
                size += linesToScroll;
                size = min(THUMBNAIL_SIZE_MAX, max(THUMBNAIL_SIZE_MIN, size));
                if (size != Configuration.ThumbnailSize)
                {
                    Configuration.ThumbnailSize = size;

                    MainWindow->LeftPanel->SetThumbnailSize(size);
                    MainWindow->RightPanel->SetThumbnailSize(size);
                    if (MainWindow->LeftPanel->GetViewMode() == vmThumbnails)
                        MainWindow->LeftPanel->RefreshForConfig();
                    if (MainWindow->RightPanel->GetViewMode() == vmThumbnails)
                        MainWindow->RightPanel->RefreshForConfig();

                    MainWindow->BottomLeftPanel->SetThumbnailSize(size);
                    MainWindow->BottomRightPanel->SetThumbnailSize(size);
                    if (MainWindow->BottomLeftPanel->GetViewMode() == vmThumbnails)
                        MainWindow->BottomLeftPanel->RefreshForConfig();
                    if (MainWindow->BottomRightPanel->GetViewMode() == vmThumbnails)
                        MainWindow->BottomRightPanel->RefreshForConfig();
                }
            }
        }

        if (Parent->TrackingSingleClick)
        {
            // zajistim aktualizaci hot polozky
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(HWindow, &p);
            SendMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
        }
        return 0;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

int CFilesBox::GetIndex(int x, int y, BOOL nearest, RECT* labelRect)
{
    CALL_STACK_MESSAGE4("CFilesBox::GetIndex(%d, %d, %d, )", x, y, nearest);

    if (labelRect != NULL && nearest)
        TRACE_E("CFilesBox::GetIndex nearest && labelRect");

    if (ItemsCount == 0)
        return INT_MAX;

    // prevedu x a y do souradnic FilesRect;
    x -= FilesRect.left;
    y -= FilesRect.top;

    if (nearest)
    {
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        if (x >= FilesRect.right - FilesRect.left)
            x = FilesRect.right - FilesRect.left - 1;
        if (y >= FilesRect.bottom - FilesRect.top)
            y = FilesRect.bottom - FilesRect.top - 1;
    }
    else
    {
        if (x < 0 || x >= FilesRect.right - FilesRect.left ||
            y < 0 || y >= FilesRect.bottom - FilesRect.top)
            return INT_MAX;
    }

    RECT labelR; // label rect
    labelR.left = 0;
    labelR.top = 0;
    labelR.right = 50;
    labelR.bottom = 50;

    int itemIndex = INT_MAX;
    switch (ViewMode)
    {
    case vmBrief:
    {
        int itemY = y / ItemHeight;
        if (itemY <= EntireItemsInColumn - 1)
            itemIndex = TopIndex + (x / ItemWidth) * EntireItemsInColumn + itemY;
        else if (nearest)
            itemIndex = TopIndex + (x / ItemWidth) * EntireItemsInColumn + EntireItemsInColumn - 1;

        if (!nearest && Configuration.FullRowSelect)
        {
            int xPos = x % ItemWidth;
            if (xPos >= ItemWidth - 10) // korekce - musime vytvorit prostor pro tazeni klece
                itemIndex = INT_MAX;
        }

        labelR.top = itemY * ItemHeight;
        labelR.bottom = labelR.top + ItemHeight;
        labelR.left = (x / ItemWidth) * ItemWidth + IconSizes[ICONSIZE_16] + 2 - XOffset;
        labelR.right = labelR.left + ItemWidth - 10 - IconSizes[ICONSIZE_16] - 2;
        break;
    }

    case vmDetailed:
    {
        int itemY = y / ItemHeight;
        if (x + XOffset < ItemWidth || nearest)
            itemIndex = itemY + TopIndex;

        labelR.top = itemY * ItemHeight;
        labelR.bottom = labelR.top + ItemHeight;
        labelR.left = IconSizes[ICONSIZE_16] + 2 - XOffset;
        labelR.right = labelR.left + ItemWidth - IconSizes[ICONSIZE_16] - 2;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        int itemY = (y + TopIndex) / ItemHeight;
        int itemX = x / ItemWidth;
        if (itemX < ColumnsCount)
            itemIndex = itemY * ColumnsCount + itemX;
        else if (nearest)
            itemIndex = itemY * ColumnsCount + ColumnsCount - 1;
        break;
    }
    }

    if (itemIndex < 0 || itemIndex >= ItemsCount)
        itemIndex = INT_MAX;
    if (nearest && itemIndex >= ItemsCount)
        itemIndex = ItemsCount - 1;

    if (itemIndex != INT_MAX)
    {
        // (vmBrief+vmDetailed) pokud neni FullRowSelect, omerime skutecnou delku polozky
        if ((ViewMode == vmBrief || ViewMode == vmDetailed) &&
            !nearest && !Configuration.FullRowSelect)
        {
            if (ViewMode == vmDetailed)
                x += XOffset;
            char formatedFileName[MAX_PATH];
            CFileData* f;
            BOOL isDir = itemIndex < Parent->Dirs->Count;
            if (isDir)
                f = &Parent->Dirs->At(itemIndex);
            else
                f = &Parent->Files->At(itemIndex - Parent->Dirs->Count);

            AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);

            const char* s = formatedFileName;

            int width = IconSizes[ICONSIZE_16] + 2;

            SIZE sz;
            int len;
            if ((!isDir || Configuration.SortDirsByExt) && ViewMode == vmDetailed &&
                Parent->IsExtensionInSeparateColumn() && f->Ext[0] != 0 && f->Ext > f->Name + 1) // vyjimka pro jmena jako ".htaccess", ukazuji se ve sloupci Name i kdyz jde o pripony
            {
                len = (int)(f->Ext - f->Name - 1);
            }
            else
            {
                if (*s == '.' && *(s + 1) == '.' && *(s + 2) == 0)
                {
                    if (ViewMode == vmBrief)
                        width = ItemWidth - 10; // 10 - abychom nebyli roztazeni pres celou sirku
                    else
                        width = Parent->Columns[0].Width - 1;
                    goto SKIP_MES;
                }
                else
                    len = f->NameLen;
            }

            // zjistim skutecnou delku textu
            HDC dc;
            HFONT hOldFont;
            dc = HANDLES(GetDC(HWindow));
            hOldFont = (HFONT)SelectObject(dc, Font);

            GetTextExtentPoint32(dc, s, len, &sz);
            width += 2 + sz.cx + 3;

            if (ViewMode == vmDetailed && width > (int)Parent->Columns[0].Width)
                width = Parent->Columns[0].Width;

            SelectObject(dc, hOldFont);
            HANDLES(ReleaseDC(HWindow, dc));
        SKIP_MES:

            labelR.right = labelR.left + width - IconSizes[ICONSIZE_16] - 2;

            int xPos = x;
            if (ViewMode == vmBrief)
                xPos %= ItemWidth;
            if (xPos >= width)
                itemIndex = INT_MAX;
        }

        if ((ViewMode == vmIcons || ViewMode == vmThumbnails) && !nearest)
        {
            // do xPos a yPos vlozim relativni souradnice
            POINT pt;
            RECT rect;
            pt.x = x % ItemWidth;
            pt.y = (y + TopIndex) % ItemHeight;

            rect.top = 2;
            int iconW = 32;
            int iconH = 32;

            // detekce kliknuti na ikonu
            if (ViewMode == vmThumbnails)
            {
                rect.top = 3;
                iconW = ThumbnailWidth + 2;
                iconH = ThumbnailHeight + 2;
            }
            rect.left = (ItemWidth - iconW) / 2;
            rect.right = rect.left + iconW;
            rect.bottom = rect.top + iconH;
            BOOL hitIcon = PtInRect(&rect, pt);

            // detekce kliknuti na text pod ikonou
            char formatedFileName[MAX_PATH];
            CFileData* f;
            BOOL isItemUpDir = FALSE;
            if (itemIndex < Parent->Dirs->Count)
            {
                f = &Parent->Dirs->At(itemIndex);
                if (itemIndex == 0 && *f->Name == '.' && *(f->Name + 1) == '.' && *(f->Name + 2) == 0) // "up-dir" muze byt jen prvni
                    isItemUpDir = TRUE;
            }
            else
                f = &Parent->Files->At(itemIndex - Parent->Dirs->Count);

            AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0,
                          itemIndex < Parent->Dirs->Count);

            // POZOR: udrzovat v konzistenci s CFilesWindow::SetQuickSearchCaretPos
            char buff[1024];                  // cilovy buffer pro retezce
            int maxWidth = ItemWidth - 4 - 1; // -1, aby se nedotykaly
            char* out1 = buff;
            int out1Len = 512;
            int out1Width;
            char* out2 = buff + 512;
            int out2Len = 512;
            int out2Width;
            HDC hDC = ItemBitmap.HMemDC;
            HFONT hOldFont = (HFONT)SelectObject(hDC, Font);
            SplitText(hDC, formatedFileName, f->NameLen, &maxWidth,
                      out1, &out1Len, &out1Width,
                      out2, &out2Len, &out2Width);
            SelectObject(hDC, hOldFont);
            maxWidth += 4;

            if (isItemUpDir) // updir je pouze "..", musime ho prodlouzit na zobrazovanou velikost
            {
                // viz CFilesWindow::DrawIconThumbnailItem
                maxWidth = max(maxWidth, (Parent->GetViewMode() == vmThumbnails ? ThumbnailWidth : 32) + 4);
            }

            rect.left = (ItemWidth - maxWidth) / 2;
            rect.right = rect.left + maxWidth;
            rect.top = rect.bottom;
            rect.bottom = rect.top + 3 + 2 + FontCharHeight + 2;
            if (out2Len > 0)
                rect.bottom += FontCharHeight;
            BOOL hitText = PtInRect(&rect, pt);

            labelR = rect;
            int itemY = (y + TopIndex) / ItemHeight;
            int itemX = x / ItemWidth;
            labelR.top += 2 + itemY * ItemHeight - TopIndex;
            labelR.bottom += itemY * ItemHeight - TopIndex - 1;
            labelR.left += itemX * ItemWidth;
            labelR.right += itemX * ItemWidth;

            // pokud neni kliknuto na ikonu ani text, zatluceme polozku
            if (!hitIcon && !hitText)
                itemIndex = INT_MAX;
        }

        if ((ViewMode == vmTiles) && !nearest)
        {
            // do xPos a yPos vlozim relativni souradnice
            POINT pt;
            RECT rect;
            pt.x = x % ItemWidth;
            pt.y = (y + TopIndex) % ItemHeight;

            int iconW = IconSizes[ICONSIZE_48];
            int iconH = IconSizes[ICONSIZE_48];

            // detekce kliknuti na ikonu
            rect.top = (ItemHeight - iconH) / 2;
            rect.left = TILE_LEFT_MARGIN;
            rect.right = rect.left + iconW;
            rect.bottom = rect.top + iconH;
            BOOL hitIcon = PtInRect(&rect, pt);

            CFileData* f;
            int isDir = 0;
            if (itemIndex < Parent->Dirs->Count)
            {
                f = &Parent->Dirs->At(itemIndex);
                isDir = itemIndex != 0 || strcmp(f->Name, "..") != 0 ? 1 : 2 /* UP-DIR */;
            }
            else
                f = &Parent->Files->At(itemIndex - Parent->Dirs->Count);

            int itemWidth = rect.right - rect.left; // sirka polozky
            int maxTextWidth = ItemWidth - TILE_LEFT_MARGIN - IconSizes[ICONSIZE_48] - TILE_LEFT_MARGIN - 4;
            int widthNeeded = 0;

            char buff[3 * 512]; // cilovy buffer pro retezce
            char* out0 = buff;
            int out0Len;
            char* out1 = buff + 512;
            int out1Len;
            char* out2 = buff + 1024;
            int out2Len;
            HDC hDC = ItemBitmap.HMemDC;
            HFONT hOldFont = (HFONT)SelectObject(hDC, Font);
            GetTileTexts(f, isDir, hDC, maxTextWidth, &widthNeeded,
                         out0, &out0Len, out1, &out1Len, out2, &out2Len,
                         Parent->ValidFileData, &Parent->PluginData,
                         Parent->Is(ptDisk));
            SelectObject(hDC, hOldFont);
            widthNeeded += 5;

            int visibleLines = 1; // nazev je viditelny urcite
            if (out1[0] != 0)
                visibleLines++;
            if (out2[0] != 0)
                visibleLines++;
            int textH = visibleLines * FontCharHeight + 4;

            // obdelnik textu
            labelR.left = rect.right + 2;
            labelR.right = labelR.left + widthNeeded;
            labelR.top = (ItemHeight - textH) / 2; // centrujeme;
            labelR.bottom = labelR.top + textH;

            BOOL hitText = PtInRect(&labelR, pt);

            // posuneme labelR na realnou pozici
            int itemY = (y + TopIndex) / ItemHeight;
            int itemX = x / ItemWidth;
            labelR.top += itemY * ItemHeight - TopIndex;
            labelR.bottom += itemY * ItemHeight - TopIndex;
            labelR.left += itemX * ItemWidth;
            labelR.right += itemX * ItemWidth;

            // pokud neni kliknuto na ikonu ani text, zatluceme polozku
            if (!hitIcon && !hitText)
                itemIndex = INT_MAX;
        }
    }
    if (labelRect != NULL)
    {
        labelRect->left = labelR.left + FilesRect.left;
        labelRect->top = labelR.top + FilesRect.top;
        labelRect->right = labelR.right + FilesRect.left;
        labelRect->bottom = labelR.bottom + FilesRect.top;
    }
    return itemIndex;
}

BOOL CFilesBox::ShowHideChilds()
{
    BOOL change = FALSE;
    if (HeaderLineVisible && ViewMode != vmDetailed)
    {
        TRACE_E("Header line is supported only in Detailed mode");
        HeaderLineVisible = FALSE;
    }

    // vodorovne rolovatko je pripustne v detailed a brief; jinak ho schovam
    if (ViewMode != vmDetailed && ViewMode != vmBrief && HHScrollBar != NULL)
    {
        DestroyWindow(BottomBar.HWindow);
        BottomBar.HScrollBar = NULL;
        HHScrollBar = NULL;
        change = TRUE;
    }

    // svisle rolovatko schovam pro brief rezim
    if (ViewMode == vmBrief && HVScrollBar != NULL)
    {
        DestroyWindow(HVScrollBar);
        HVScrollBar = NULL;
        change = TRUE;
    }

    // header line je pripustna pouze v detailed (a jeste ji musi user chtit);
    // jinak ji schovam
    if ((ViewMode != vmDetailed || !HeaderLineVisible) &&
        HeaderLine.HWindow != NULL)
    {
        DestroyWindow(HeaderLine.HWindow);
        HeaderLine.HWindow = NULL;
        change = TRUE;
    }

    // pokud jsem v detailed nebo brief, potrebujeme vodorovne rolovatko
    if ((ViewMode == vmDetailed || ViewMode == vmBrief) && HHScrollBar == NULL)
    {
        BottomBar.Create(CWINDOW_CLASSNAME2, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                         0, 0, 0, 0,
                         HWindow,
                         NULL, //HMenu
                         HInstance,
                         &BottomBar);
        HHScrollBar = CreateWindow("scrollbar", "", WS_CHILD | SBS_HORZ | WS_VISIBLE | WS_CLIPSIBLINGS | SBS_HORZ,
                                   0, 0, 0, 0,
                                   BottomBar.HWindow,
                                   NULL, //HMenu
                                   HInstance,
                                   NULL);
        BottomBar.HScrollBar = HHScrollBar;
        change = TRUE;
    }

    // v detailed rezimu nechame ve vodorovnem rolovatku vpravo mezeru pod svislym rolovatkem
    BottomBar.VertScrollSpace = (ViewMode == vmDetailed);

    // svisle rolovatko potrebujeme ve vsech rezimech mimo brief
    if (ViewMode != vmBrief)
    {
        if (HVScrollBar == NULL)
        {
            HVScrollBar = CreateWindow("scrollbar", "", WS_CHILD | SBS_VERT | WS_VISIBLE | WS_CLIPSIBLINGS | SBS_VERT,
                                       0, 0, 0, 0,
                                       HWindow,
                                       NULL, //HMenu
                                       HInstance,
                                       NULL);
            change = TRUE;
        }
    }

    // v detailed rezimu, je-li pozadovana headerline, zajistime jeji vytvoreni
    if (ViewMode == vmDetailed && HeaderLineVisible && HeaderLine.HWindow == NULL)
    {
        HeaderLine.Create(CWINDOW_CLASSNAME2, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                          0, 0, 0, 0,
                          HWindow,
                          NULL, //HMenu
                          HInstance,
                          &HeaderLine);
        change = TRUE;
    }

    return change;
}

void CFilesBox::SetupScrollBars(DWORD flags)
{
    SCROLLINFO si;
    if (HHScrollBar != NULL && flags & UPDATE_HORZ_SCROLL)
    {
        si.cbSize = sizeof(si);
        si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE | SIF_PAGE;
        if (ViewMode == vmDetailed)
            si.nPos = XOffset;
        else
            si.nPos = TopIndex / EntireItemsInColumn;
        si.nMin = 0;
        if (ViewMode == vmDetailed)
        {
            // Detailed
            si.nMax = ItemWidth;
            si.nPage = FilesRect.right - FilesRect.left + 1;
        }
        else
        {
            // Brief
            si.nMax = ColumnsCount;            // celkovy pocet sloupcu
            si.nPage = EntireColumnsCount + 1; // pocet celych zobrazenych sloupcu
        }
        if (OldHorzSI.cbSize == 0 || OldHorzSI.nPos != si.nPos || OldHorzSI.nMax != si.nMax ||
            OldHorzSI.nPage != si.nPage)
        {
            // pokud je treba, zhasneme drag image
            BOOL showImage = FALSE;
            if (ImageDragging)
            {
                RECT r;
                GetWindowRect(HHScrollBar, &r);
                if (ImageDragInterfereRect(&r))
                {
                    ImageDragShow(FALSE);
                    showImage = TRUE;
                }
            }
            // nastavime rolovatka
            OldHorzSI = si;
            SetScrollInfo(HHScrollBar, SB_CTL, &si, TRUE);
            // obnovime drag image
            if (showImage)
                ImageDragShow(TRUE);
        }
    }

    if (HVScrollBar != NULL && flags & UPDATE_VERT_SCROLL)
    {
        si.cbSize = sizeof(si);
        si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE | SIF_PAGE;
        si.nPos = TopIndex;
        si.nMin = 0;

        if (ViewMode == vmDetailed)
        {
            // Detailed
            si.nMax = ItemsCount;
            si.nPage = EntireItemsInColumn + 1;
        }
        else
        {
            // Icons || Thumbnails
            si.nMax = ItemsInColumn * ItemHeight;
            si.nPage = FilesRect.bottom - FilesRect.top + 1;
        }

        if (OldVertSI.cbSize == 0 || OldVertSI.nPos != si.nPos || OldVertSI.nMax != si.nMax ||
            OldVertSI.nPage != si.nPage)
        {
            // pokud je treba, zhasneme drag image
            BOOL showImage = FALSE;
            if (ImageDragging)
            {
                RECT r;
                GetWindowRect(HVScrollBar, &r);
                if (ImageDragInterfereRect(&r))
                {
                    ImageDragShow(FALSE);
                    showImage = TRUE;
                }
            }
            // nastavime rolovatka
            OldVertSI = si;
            SetScrollInfo(HVScrollBar, SB_CTL, &si, TRUE);
            // obnovime drag image
            if (showImage)
                ImageDragShow(TRUE);
        }
    }
}

void CFilesBox::CheckAndCorrectBoundaries()
{
    switch (ViewMode)
    {
    case vmDetailed:
    {
        if (FilesRect.right - FilesRect.left > 0 && FilesRect.bottom - FilesRect.top > 0)
        {
            // zajistim odrolovani v pripade, ze se zvetsilo okno, vpravo nebo dole jsme na dorazu a jeste muzeme rolovat
            int newXOffset = XOffset;
            if (newXOffset > 0 && ItemWidth - newXOffset < (FilesRect.right - FilesRect.left) + 1)
                newXOffset = ItemWidth - (FilesRect.right - FilesRect.left);
            if (ItemWidth < (FilesRect.right - FilesRect.left) + 1)
                newXOffset = 0;

            int newTopIndex = TopIndex;
            if (newTopIndex > 0 && ItemsCount - newTopIndex < EntireItemsInColumn + 1)
                newTopIndex = ItemsCount - EntireItemsInColumn;
            if (ItemsCount < EntireItemsInColumn + 1)
                newTopIndex = 0;

            if (newXOffset != XOffset || newTopIndex != TopIndex)
            {
                if (ImageDragging)
                    ImageDragShow(FALSE);
                HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
                ScrollWindowEx(HWindow, XOffset - newXOffset, ItemHeight * (TopIndex - newTopIndex),
                               &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
                if (HeaderLine.HWindow != NULL && newXOffset != XOffset)
                {
                    HRGN hUpdateRgn2 = HANDLES(CreateRectRgn(0, 0, 0, 0));
                    ScrollWindowEx(HeaderLine.HWindow, XOffset - newXOffset, 0,
                                   NULL, NULL, hUpdateRgn2, NULL, 0);
                    XOffset = newXOffset;
                    HDC hDC = HANDLES(GetDC(HeaderLine.HWindow));
                    HeaderLine.PaintAllItems(hDC, hUpdateRgn2);
                    HANDLES(ReleaseDC(HeaderLine.HWindow, hDC));
                    HANDLES(DeleteObject(hUpdateRgn2));
                }
                else
                    XOffset = newXOffset;
                TopIndex = newTopIndex;
                SetupScrollBars(UPDATE_HORZ_SCROLL | UPDATE_VERT_SCROLL);
                PaintAllItems(hUpdateRgn, 0);
                HANDLES(DeleteObject(hUpdateRgn));
                if (ImageDragging)
                    ImageDragShow(TRUE);
                Parent->VisibleItemsArray.InvalidateArr();
                Parent->VisibleItemsArraySurround.InvalidateArr();
            }
        }
        break;
    }

    case vmBrief:
    {
        int col = ItemsCount / EntireItemsInColumn - TopIndex / EntireItemsInColumn;
        if (col * ItemWidth < FilesRect.right - FilesRect.left - ItemWidth)
        {
            int newTopIndex = (ColumnsCount - EntireColumnsCount) * EntireItemsInColumn;
            if (newTopIndex != TopIndex)
            {
                TopIndex = newTopIndex;
                SetupScrollBars(UPDATE_HORZ_SCROLL);
                InvalidateRect(HWindow, &FilesRect, FALSE);
                Parent->VisibleItemsArray.InvalidateArr();
                Parent->VisibleItemsArraySurround.InvalidateArr();
            }
        }
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        if (FilesRect.right - FilesRect.left > 0 && FilesRect.bottom - FilesRect.top > 0)
        {
            // zajistim odrolovani v pripade, ze se zvetsilo okno, vpravo nebo dole jsme na dorazu a jeste muzeme rolovat
            int newTopIndex = TopIndex;
            if (newTopIndex > 0 && ItemsInColumn * ItemHeight - newTopIndex < FilesRect.bottom - FilesRect.top)
                newTopIndex = ItemsInColumn * ItemHeight - (FilesRect.bottom - FilesRect.top);
            if (newTopIndex < 0)
                newTopIndex = 0;

            if (newTopIndex != TopIndex)
            {
                if (ImageDragging)
                    ImageDragShow(FALSE);
                HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
                ScrollWindowEx(HWindow, 0, TopIndex - newTopIndex,
                               &FilesRect, &FilesRect, hUpdateRgn, NULL, 0);
                TopIndex = newTopIndex;
                SetupScrollBars(UPDATE_HORZ_SCROLL | UPDATE_VERT_SCROLL);
                PaintAllItems(hUpdateRgn, 0);
                HANDLES(DeleteObject(hUpdateRgn));
                if (ImageDragging)
                    ImageDragShow(TRUE);
                Parent->VisibleItemsArray.InvalidateArr();
                Parent->VisibleItemsArraySurround.InvalidateArr();
            }
        }
        break;
    }
    }
}

void CFilesBox::LayoutChilds(BOOL updateAndCheck)
{
    GetClientRect(HWindow, &ClientRect);
    ItemBitmap.Enlarge(ClientRect.right - ClientRect.left, ItemHeight);
    Parent->CancelUI(); // cancel QuickSearch and QuickEdit
    FilesRect = ClientRect;

    if (ClientRect.right > 0 && ClientRect.bottom > 0)
    {
        int deferCount = 0;

        if (HHScrollBar != NULL)
        {
            int scrollH = GetSystemMetrics(SM_CYHSCROLL);

            // umistim vodorovne rolovatko
            BottomBarRect.left = 0;
            BottomBarRect.top = FilesRect.bottom - scrollH;
            BottomBarRect.right = FilesRect.right;
            BottomBarRect.bottom = FilesRect.bottom;
            deferCount++;
            FilesRect.bottom -= scrollH;
        }
        else
            ZeroMemory(&BottomBarRect, sizeof(BottomBarRect));

        // umistim svisle rolovatko
        if (HVScrollBar != NULL)
        {
            int scrollW = GetSystemMetrics(SM_CXVSCROLL);
            VScrollRect.left = FilesRect.right - scrollW;
            VScrollRect.top = 0;
            VScrollRect.right = FilesRect.right;
            VScrollRect.bottom = FilesRect.bottom;
            deferCount++;
            FilesRect.right -= scrollW;
        }
        else
            ZeroMemory(&VScrollRect, sizeof(VScrollRect));

        if (HeaderLine.HWindow != NULL)
        {
            HeaderRect = FilesRect;
            HeaderRect.bottom = HeaderRect.top + FontCharHeight + 4;
            FilesRect.top = HeaderRect.bottom;
            deferCount++;
        }

        RECT oldBR;
        if (HHScrollBar != NULL)
            GetWindowRect(BottomBar.HWindow, &oldBR);
        if (deferCount > 0)
        {
            HDWP hDWP = HANDLES(BeginDeferWindowPos(deferCount));
            if (HeaderLine.HWindow != NULL)
            {
                HeaderLine.Width = HeaderRect.right - HeaderRect.left;
                HeaderLine.Height = HeaderRect.bottom - HeaderRect.top;
                hDWP = HANDLES(DeferWindowPos(hDWP, HeaderLine.HWindow, NULL,
                                              HeaderRect.left, HeaderRect.top,
                                              HeaderRect.right - HeaderRect.left, HeaderRect.bottom - HeaderRect.top,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
            }
            if (HVScrollBar != NULL)
                hDWP = HANDLES(DeferWindowPos(hDWP, HVScrollBar, NULL,
                                              VScrollRect.left, VScrollRect.top,
                                              VScrollRect.right - VScrollRect.left, VScrollRect.bottom - VScrollRect.top,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
            if (HHScrollBar != NULL)
                hDWP = HANDLES(DeferWindowPos(hDWP, BottomBar.HWindow, NULL,
                                              BottomBarRect.left, BottomBarRect.top,
                                              BottomBarRect.right - BottomBarRect.left, BottomBarRect.bottom - BottomBarRect.top,
                                              SWP_NOACTIVATE | SWP_NOZORDER));
            HANDLES(EndDeferWindowPos(hDWP));
        }

        if (HHScrollBar != NULL)
        {
            RECT newBR;
            GetWindowRect(BottomBar.HWindow, &newBR);
            if (oldBR.left == newBR.left && oldBR.top == newBR.top &&
                oldBR.right == newBR.right && oldBR.bottom == newBR.bottom)
                BottomBar.LayoutChilds(); // pokud nedoslo ke zmene rozmeru, zavolam Layout explicitne
        }

        if (updateAndCheck)
        {
            int oldEntireItemsInColumn = EntireItemsInColumn;
            int oldEntireColumnsCount = EntireColumnsCount;

            if (ViewMode == vmDetailed) // prepocitame sirku sloupce Name ve smart-mode detail view
            {
                BOOL leftPanel = Parent->IsLeftPanel();
                CColumn* nameCol = &Parent->Columns[0];

                if (nameCol->FixedWidth == 0 &&
                    (leftPanel && (Parent->ViewTemplate->LeftSmartMode || Parent->ViewTemplate->BottomLeftSmartMode) ||
                     !leftPanel && (Parent->ViewTemplate->RightSmartMode || Parent->ViewTemplate->BottomRightSmartMode)))
                {
                    DWORD newNameWidth = Parent->GetResidualColumnWidth(Parent->FullWidthOfNameCol);
                    DWORD minWidth = Parent->WidthOfMostOfNames;
                    if (minWidth > (DWORD)(0.75 * (FilesRect.right - FilesRect.left)))
                        minWidth = (DWORD)(0.75 * (FilesRect.right - FilesRect.left));
                    if (minWidth < nameCol->MinWidth)
                        minWidth = nameCol->MinWidth;
                    if (newNameWidth < minWidth)
                        newNameWidth = minWidth;
                    if (newNameWidth != nameCol->Width) // sirka sloupce Name se zmenila
                    {
                        int delta = newNameWidth - nameCol->Width;
                        nameCol->Width = newNameWidth;
                        Parent->NarrowedNameColumn = nameCol->Width < Parent->FullWidthOfNameCol;
                        SetItemWidthHeight(ItemWidth + delta, ItemHeight);

                        // provedeme kompletni prekresleni header-line a filesboxu
                        // POZN.: dalo by se jiste optimalizovat (rozesunout/sesunout sloupce pres ScrollWindowEx - viz
                        //        zmena sirky sloupce z header-line), ale vypada to dost rychle+neblikaci i takhle
                        InvalidateRect(HeaderLine.HWindow, NULL, FALSE);
                        InvalidateRect(HWindow, &FilesRect, FALSE);
                    }
                }
            }

            UpdateInternalData();
            SetupScrollBars();

            switch (ViewMode)
            {
            case vmBrief:
            {
                if (oldEntireItemsInColumn != EntireItemsInColumn)
                {
                    TopIndex = (TopIndex / EntireItemsInColumn) * EntireItemsInColumn;
                    InvalidateRect(HWindow, &FilesRect, FALSE);
                }
                break;
            }

            case vmIcons:
            case vmThumbnails:
            case vmTiles:
            {
                if (oldEntireColumnsCount != EntireColumnsCount)
                {
                    //            TopIndex = (TopIndex / EntireItemsInColumn) * EntireItemsInColumn;
                    InvalidateRect(HWindow, &FilesRect, FALSE);
                }
                break;
            }
            }
            CheckAndCorrectBoundaries();
        }
        if (ItemsCount == 0)
            InvalidateRect(HWindow, &FilesRect, FALSE); // zajistime vykresleni textu o prazdnem panelu
    }
    Parent->VisibleItemsArray.InvalidateArr();
    Parent->VisibleItemsArraySurround.InvalidateArr();
}

void CFilesBox::PaintHeaderLine()
{
    if (HeaderLine.HWindow != NULL)
    {
        HDC hDC = HANDLES(GetDC(HeaderLine.HWindow));
        HeaderLine.PaintAllItems(hDC, NULL);
        HANDLES(ReleaseDC(HeaderLine.HWindow, hDC));
    }
}
