// page_fade.h

#ifndef PAGE_FADE_H
#define PAGE_FADE_H

class QStackedWidget;

namespace PageFade
{

// Switches `stack` to `index` with a crossfade: the outgoing page is captured
// as it looks on screen (window background included) and painted over the
// new page while its opacity falls to zero. Painting the old frame at opacity
// a over the new one is exactly a linear blend, so only one extra pixmap is
// drawn per frame and the new page stays live and interactive throughout.
// Switching again mid-fade captures the blend in progress, so nothing jumps.
// Without a visible stack (startup) the switch is immediate.
void switchTo(QStackedWidget *stack, int index);

}

#endif // PAGE_FADE_H
