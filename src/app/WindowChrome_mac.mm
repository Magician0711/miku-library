// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "WindowChrome.h"

#include <QWindow>

#import <AppKit/AppKit.h>

namespace {
NSWindow *nativeWindow(QWindow *window)
{
    auto *view = reinterpret_cast<NSView *>(window->winId());
    return view ? view.window : nil;
}
}

namespace MacWindow {

// The content runs under a transparent title bar that keeps the system's buttons (Qt 6.8
// has no window flag for this); the title itself would sit over the page, so it goes.
void prepare(QWindow *window)
{
    NSWindow *ns = nativeWindow(window);
    if (!ns)
        return;
    ns.titleVisibility = NSWindowTitleHidden;
    ns.titlebarAppearsTransparent = YES;
    ns.styleMask |= NSWindowStyleMaskFullSizeContentView;
    ns.movableByWindowBackground = NO;
}

// The desktop shows through the window: a behind-window effect view sits in the window's
// frame, under Qt's view, which draws with transparency over it. Qt's view stays the
// content view: Qt tracks a window's exposure through it, and a window whose view had been
// moved into another one was never reported as exposed, so it stopped drawing.
static NSVisualEffectView *backdropOf(NSWindow *ns)
{
    for (NSView *view in ns.contentView.superview.subviews) {
        if ([view isKindOfClass:[NSVisualEffectView class]] && [view.identifier isEqualToString:@"MikuBackdrop"])
            return static_cast<NSVisualEffectView *>(view);
    }
    return nil;
}

bool setBackdrop(QWindow *window, bool on)
{
    NSWindow *ns = nativeWindow(window);
    if (!ns || !ns.contentView.superview)
        return false;
    NSVisualEffectView *effect = backdropOf(ns);
    if (!on) {
        if (effect)
            effect.state = NSVisualEffectStateInactive;
        return false;
    }
    if (!effect) {
        NSView *frame = ns.contentView.superview;
        effect = [[NSVisualEffectView alloc] initWithFrame:frame.bounds];
        effect.identifier = @"MikuBackdrop";
        effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        effect.material = NSVisualEffectMaterialUnderWindowBackground;
        effect.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [frame addSubview:effect positioned:NSWindowBelow relativeTo:ns.contentView];
#if !__has_feature(objc_arc)
        [effect release];
#endif
    }
    effect.state = NSVisualEffectStateFollowsWindowActiveState;
    ns.opaque = NO;
    ns.backgroundColor = NSColor.clearColor;
    return true;
}

void setDark(QWindow *window, bool dark)
{
    NSWindow *ns = nativeWindow(window);
    if (ns)
        ns.appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

}
