#include "word_wrap.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "jtext.h"
#include "text_font.h"

namespace fallout {

// 0x4BC6F0 word_wrap
int wordWrap(const char* string, int width, short* breakpoints, short* breakpointsLengthPtr)
{
    breakpoints[0] = 0;
    *breakpointsLengthPtr = 1;

    for (int index = 1; index < WORD_WRAP_MAX_COUNT; index++) {
        breakpoints[index] = -1;
    }

    if (fontGetMonospacedCharacterWidth() > width) {
        return -1;
    }

    // nettoge.com: Japanese line breaking (kinsoku, no spaces needed).
    if (jtext_active()) {
        int pos = 0;
        int len = (int)strlen(string);
        while (pos < len) {
            int next;
            jtext_fit(string + pos, width, &next);
            if (next <= 0) {
                next = 1;
            }
            pos += next;
            if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
                return -1;
            }
            breakpoints[*breakpointsLengthPtr] = (short)(pos < len ? pos : len + 1);
            *breakpointsLengthPtr += 1;
        }
        return 0;
    }

    if (fontGetStringWidth(string) < width) {
        breakpoints[*breakpointsLengthPtr] = (short)strlen(string);
        *breakpointsLengthPtr += 1;
        return 0;
    }

    int gap = fontGetLetterSpacing();

    int accum = 0;
    const char* prevSpaceOrHyphen = nullptr;
    const char* pch = string;
    while (*pch != '\0') {
        accum += gap + fontGetCharacterWidth(*pch & 0xFF);
        if (accum <= width) {
            // NOTE: quests.txt #807 uses extended ascii.
            if (isspace(*pch & 0xFF) || *pch == '-') {
                prevSpaceOrHyphen = pch;
            }
        } else {
            if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
                return -1;
            }

            if (prevSpaceOrHyphen != nullptr) {
                // Word wrap.
                breakpoints[*breakpointsLengthPtr] = prevSpaceOrHyphen - string + 1;
                *breakpointsLengthPtr += 1;

                pch = prevSpaceOrHyphen;
            } else {
                // Character wrap.
                breakpoints[*breakpointsLengthPtr] = pch - string;
                *breakpointsLengthPtr += 1;

                pch--;
            }

            prevSpaceOrHyphen = nullptr;
            accum = 0;
        }
        pch++;
    }

    if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
        return -1;
    }

    breakpoints[*breakpointsLengthPtr] = pch - string + 1;
    *breakpointsLengthPtr += 1;

    return 0;
}

} // namespace fallout
