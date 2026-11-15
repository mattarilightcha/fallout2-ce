// nettoge.com Japanese text layer for Fallout 2 CE.
// MODIFIED FILE NOTICE: this file is an addition to fallout2-ce (Sustainable
// Use License 1.0, see LICENSE.md). It is not part of the original sources.
//
// When the game language is "japanese", strings are read as UTF-8. Single-byte
// characters keep the game's own fonts; other characters are drawn from
// "text\japanese\fonts\jfont<N>.jfn", where <N> is the game's font number.

#ifndef FALLOUT_JTEXT_H_
#define FALLOUT_JTEXT_H_

#include "text_font.h"

namespace fallout {

// Turns the layer on (language == "japanese") and loads the glyph files.
void jtext_init(const char* language);
void jtext_exit();

// True when the layer is on.
bool jtext_active();

// Called by fontSetCurrent() after the base manager's functions are installed.
// Saves them and installs the UTF-8 aware wrappers when a glyph file exists
// for the font.
void jtext_on_font(int font_num, FontManager* mgr);

// Decodes one UTF-8 character. Returns its byte length (1 for ASCII and for
// bytes that do not start a valid sequence) and stores the code point.
int jtext_decode(const char* str, unsigned int* cp);

// Line breaking for the current font. Measures how much of `str` fits into
// `max_width` pixels (stopping at '\n'). Prefers breaking after a space or a
// hyphen, otherwise breaks between Japanese characters following kinsoku
// rules. Returns the byte length of the line to draw; `*next` receives the
// byte offset where the next line starts (spaces and one '\n' skipped).
int jtext_fit(const char* str, int max_width, int* next);

// nettoge.com: lists whose rows are fixed by the art at 11 px (the character
// editor's skills and folder) cannot take 12 px glyphs: the lines would touch.
// While on, font 101 is drawn from "jfont101s.jfn" (10 px) when that file exists.
void jtext_small(bool on);

} // namespace fallout

#endif /* FALLOUT_JTEXT_H_ */
