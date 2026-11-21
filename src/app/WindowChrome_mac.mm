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

// The desktop shows through the window: a behind-window effect view holds Qt's view,
// which draws with transparency over it.
bool setBackdrop(QWindow *window, bool on)
{
    NSWindow *ns = nativeWindow(window);
    if (!ns)
        return false;
    NSView *qtView = reinterpret_cast<NSView *>(window->winId());
    NSView *holder = qtView.superview;
    const bool wrapped = [holder isKindOfClass:[NSVisualEffectView class]];
    if (!on) {
        if (wrapped)
            static_cast<NSVisualEffectView *>(holder).state = NSVisualEffectStateInactive;
        return false;
    }
    if (!wrapped) {
        NSVisualEffectView *effect = [[NSVisualEffectView alloc] initWithFrame:qtView.frame];
        effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        effect.material = NSVisualEffectMaterialUnderWindowBackground;
        effect.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        ns.contentView = effect;
        qtView.frame = effect.bounds;
        qtView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [effect addSubview:qtView];
#if !__has_feature(objc_arc)
        [effect release];
#endif
        holder = effect;
    }
    static_cast<NSVisualEffectView *>(holder).state = NSVisualEffectStateFollowsWindowActiveState;
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
