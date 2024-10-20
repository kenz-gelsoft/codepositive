/*
 * Copyright (C) 2007 Andrea Anzani <andrea.anzani@gmail.com>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2010 Michael Lotz <mmlr@mlotz.ch>
 * Copyright (C) 2010 Rene Gollent <rene@gollent.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "LauncherWindow.h"

#include "AuthenticationPanel.h"
#include "LauncherApp.h"
#include "WebPage.h"
#include "WebView.h"
#include "WebViewConstants.h"

#include <interface/StringView.h>
#include <Button.h>
#include <Entry.h>
#include <FilePanel.h>
#include <GridLayoutBuilder.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>
#include <StatusBar.h>
#include <StringView.h>
#include <TextControl.h>

#include <stdio.h>

enum {
    RELOAD = 'reld',

    TEXT_SIZE_INCREASE = 'tsin',
    TEXT_SIZE_DECREASE = 'tsdc',
    TEXT_SIZE_RESET = 'tsrs',
};

LauncherWindow::LauncherWindow(BRect frame, ToolbarPolicy toolbarPolicy)
    : BWebWindow(frame, "CodePositive",
        B_DOCUMENT_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
        B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS)
{
	init(new BWebView("web view"), toolbarPolicy);
}

LauncherWindow::LauncherWindow(BRect frame, BWebView* webView,
		ToolbarPolicy toolbarPolicy)
    : BWebWindow(frame, "CodePositive",
        B_DOCUMENT_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
        B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS)
{
	init(webView, toolbarPolicy);
}

LauncherWindow::~LauncherWindow()
{}

void LauncherWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
    case RELOAD:
        CurrentWebView()->Reload();
        break;
    case B_SIMPLE_DATA: {
        // User possibly dropped files on this window.
        // If there is more than one entry_ref, let the app handle it (open one
        // new page per ref). If there is one ref, open it in this window.
        type_code type;
        int32 countFound;
        if (message->GetInfo("refs", &type, &countFound) != B_OK
            || type != B_REF_TYPE) {
            break;
        }
        if (countFound > 1) {
            message->what = B_REFS_RECEIVED;
            be_app->PostMessage(message);
            break;
        }
        entry_ref ref;
        if (message->FindRef("refs", &ref) != B_OK)
            break;
        BEntry entry(&ref, true);
        BPath path;
        if (!entry.Exists() || entry.GetPath(&path) != B_OK)
            break;
        CurrentWebView()->LoadURL(path.Path());
        break;
    }

    case TEXT_SIZE_INCREASE:
        CurrentWebView()->IncreaseZoomFactor(true);
        break;
    case TEXT_SIZE_DECREASE:
        CurrentWebView()->DecreaseZoomFactor(true);
        break;
    case TEXT_SIZE_RESET:
        CurrentWebView()->ResetZoomFactor();
        break;

    default:
        BWebWindow::MessageReceived(message);
        break;
    }
}

bool LauncherWindow::QuitRequested()
{
    BMessage message(WINDOW_CLOSED);
    message.AddRect("window frame", Frame());
    be_app->PostMessage(&message);
    return BWebWindow::QuitRequested();
}

// #pragma mark - Notification API

void LauncherWindow::NavigationRequested(const BString& url, BWebView* view)
{
}

void LauncherWindow::NewWindowRequested(const BString& url, bool primaryAction)
{
    // Always open new windows in the application thread, since
    // creating a BWebView will try to grab the application lock.
    // But our own WebPage may already try to lock us from within
    // the application thread -> dead-lock. Thus we can't wait for
    // a reply here.
    BMessage message(NEW_WINDOW);
    message.AddString("url", url);
    be_app->PostMessage(&message);
}

void LauncherWindow::NewPageCreated(BWebView* view, BRect windowFrame,
	bool modalDialog, bool resizable, bool activate)
{
	if (!windowFrame.IsValid())
		windowFrame = Frame().OffsetByCopy(10, 10);
	LauncherWindow* window = new LauncherWindow(windowFrame, view, HaveToolbar);
	window->Show();
}

void LauncherWindow::LoadNegotiating(const BString& url, BWebView* view)
{
    BString status("Requesting: ");
    status << url;
    StatusChanged(status, view);
}

void LauncherWindow::LoadCommitted(const BString& url, BWebView* view)
{
    BString status("Loading: ");
    status << url;
    StatusChanged(status, view);
}

void LauncherWindow::LoadProgress(float progress, BWebView* view)
{
    if (m_loadingProgressBar) {
        if (progress < 100 && m_loadingProgressBar->IsHidden())
            m_loadingProgressBar->Show();
        m_loadingProgressBar->SetTo(progress);
    }
}

void LauncherWindow::LoadFailed(const BString& url, BWebView* view)
{
    BString status(url);
    status << " failed.";
    StatusChanged(status, view);
    if (m_loadingProgressBar && !m_loadingProgressBar->IsHidden())
        m_loadingProgressBar->Hide();
}

void LauncherWindow::LoadFinished(const BString& url, BWebView* view)
{
    BString status(url);
    status << " finished.";
    StatusChanged(status, view);
    if (m_loadingProgressBar && !m_loadingProgressBar->IsHidden())
        m_loadingProgressBar->Hide();
}

void LauncherWindow::SetToolBarsVisible(bool flag, BWebView* view)
{
    // TODO
}

void LauncherWindow::SetStatusBarVisible(bool flag, BWebView* view)
{
    // TODO
}

void LauncherWindow::SetMenuBarVisible(bool flag, BWebView* view)
{
    // TODO
}

void LauncherWindow::TitleChanged(const BString& title, BWebView* view)
{
    updateTitle(title);
}

void LauncherWindow::StatusChanged(const BString& statusText, BWebView* view)
{
    if (m_statusText)
        m_statusText->SetText(statusText.String());
}

void LauncherWindow::NavigationCapabilitiesChanged(bool canGoBackward,
    bool canGoForward, bool canStop, BWebView* view)
{}


bool
LauncherWindow::AuthenticationChallenge(BString message, BString& inOutUser,
	BString& inOutPassword, bool& inOutRememberCredentials,
	uint32 failureCount, BWebView* view)
{
	AuthenticationPanel* panel = new AuthenticationPanel(Frame());
		// Panel auto-destructs.
	bool success = panel->getAuthentication(message, inOutUser, inOutPassword,
		inOutRememberCredentials, failureCount > 0, inOutUser, inOutPassword,
		&inOutRememberCredentials);
	return success;
}


void LauncherWindow::init(BWebView* webView, ToolbarPolicy toolbarPolicy)
{
	SetCurrentWebView(webView);

    if (toolbarPolicy == HaveToolbar) {
        // Status Bar
        m_statusText = new BStringView("status", "");
        m_statusText->SetAlignment(B_ALIGN_LEFT);
        m_statusText->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
        m_statusText->SetExplicitMinSize(BSize(150, 12));
            // Prevent the window from growing to fit a long status message...
        BFont font(be_plain_font);
        font.SetSize(ceilf(font.Size() * 0.8));
        m_statusText->SetFont(&font, B_FONT_SIZE);

        // Loading progress bar
        m_loadingProgressBar = new BStatusBar("progress");
        m_loadingProgressBar->SetMaxValue(100);
        m_loadingProgressBar->Hide();
        m_loadingProgressBar->SetBarHeight(12);

        // Text Size Buttons
        m_IncreaseButton = new BButton("", "+", new BMessage(TEXT_SIZE_INCREASE));
        m_ResetSizeButton = new BButton("", "100%", new BMessage(TEXT_SIZE_RESET));
        m_DecreaseButton = new BButton("", "-", new BMessage(TEXT_SIZE_DECREASE));

        const float kInsetSpacing = 5;
        const float kElementSpacing = 7;

        // Layout
        AddChild(BGroupLayoutBuilder(B_VERTICAL, 0)
            .Add(webView)
            .Add(new BSeparatorView(B_HORIZONTAL, B_PLAIN_BORDER))
            .Add(BGroupLayoutBuilder(B_HORIZONTAL, kElementSpacing)
                .Add(m_statusText)
                .Add(m_loadingProgressBar, 0.2)
	            .Add(BGridLayoutBuilder(0, 0)
	                .Add(m_IncreaseButton, 0, 0)
	                .Add(m_ResetSizeButton, 1, 0)
	                .Add(m_DecreaseButton, 2, 0)
	                , 0.1
	            )
                .AddStrut(12 - kElementSpacing)
                .SetInsets(kInsetSpacing, 0, kInsetSpacing, 0)
            )
        );
    } else {
        m_statusText = 0;
        m_loadingProgressBar = 0;
        m_IncreaseButton = 0;
        m_ResetSizeButton = 0;
        m_DecreaseButton = 0;

        AddChild(BGroupLayoutBuilder(B_VERTICAL)
            .Add(webView)
        );
    }

    AddShortcut('R', B_COMMAND_KEY, new BMessage(RELOAD));

    be_app->PostMessage(WINDOW_OPENED);
}

void LauncherWindow::updateTitle(const BString& title)
{
    BString windowTitle = title;
//    if (windowTitle.Length() > 0)
//        windowTitle << " - ";
//    windowTitle << "HaikuLauncher";
    SetTitle(windowTitle.String());
}
