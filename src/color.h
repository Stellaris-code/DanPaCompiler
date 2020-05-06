#ifndef COLOR_H
#define COLOR_H

#define USE_COLOR 1

#if USE_COLOR // && !defined(_WIN32)

#define ESC_FONT_NORMAL "\e[0m"
#define ESC_FONT_BOLD   "\e[1m"
#define ESC_FG(r, g, b) "\e[38;2;"#r";"#g";"#b"m"
#define ESC_BG(r, g, b) "\e[48;2;"#r";"#g";"#b"m"
#define ESC_POP_COLOR "\e[39m"

#else

#define ESC_FONT_NORMAL ""
#define ESC_FONT_BOLD   ""
#define ESC_FG(r, g, b) ""
#define ESC_BG(r, g, b) ""
#define ESC_POP_COLOR ""

#endif

#endif // COLOR_H
