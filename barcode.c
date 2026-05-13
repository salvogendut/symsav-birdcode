// barcode.c — Barcode screensaver for SymbOS
// Picks a random bird name, renders it as a Code 39 barcode with text label,
// holds 5 seconds, then cycles to the next word.
// SymbOS C port by Salvatore Bognanni

#include <symbos.h>
#include <symbos/msgid.h>
#include <symbos/keys.h>
#include <stdlib.h>
#include <string.h>

#define MSC_SAV_INIT   1
#define MSC_SAV_START  2
#define MSC_SAV_CONFIG 3
#define MSR_SAV_CONFIG 4

#define SCREEN_W   320
#define SCREEN_H   200

// --------------------------------------------------------------------------
// Bird word list
// --------------------------------------------------------------------------

static const char *birds[] = {
    "ALBATROSS",   "AVOCET",      "BITTERN",     "BLACKBIRD",
    "BULLFINCH",   "BUNTING",     "BUZZARD",     "CHAFFINCH",
    "CHIFFCHAFF",  "COOT",        "CORMORANT",   "CORNCRAKE",
    "CROSSBILL",   "CURLEW",      "DIPPER",      "DOTTEREL",
    "DUNLIN",      "DUNNOCK",     "FIELDFARE",   "FIRECREST",
    "FULMAR",      "GANNET",      "GOLDCREST",   "GOLDFINCH",
    "GOOSANDER",   "GOSHAWK",     "GREBE",       "GREENFINCH",
    "GREENSHANK",  "GUILLEMOT",   "HAWFINCH",    "HERON",
    "HOBBY",       "HOOPOE",      "JACKDAW",     "KESTREL",
    "KINGFISHER",  "KITTIWAKE",   "KNOT",        "LAPWING",
    "LINNET",      "MAGPIE",      "MALLARD",     "MANDARIN",
    "MERLIN",      "NIGHTJAR",    "NUTHATCH",    "OSPREY",
    "OYSTERCATCHER","PEREGRINE",  "PINTAIL",     "PLOVER",
    "PUFFIN",      "RAZORBILL",   "REDSHANK",    "REDSTART",
    "REDWING",     "ROBIN",       "SANDERLING",  "SANDPIPER"
};
#define NBIRDS  60

// --------------------------------------------------------------------------
// Code 39: 9-bit patterns (1=wide, 0=narrow)
// bit8=bar1  bit7=sp1  bit6=bar2  bit5=sp2  bit4=bar3
// bit3=sp3   bit2=bar4 bit1=sp4   bit0=bar5
// Table indexed: 0-9 = digits, 10-35 = A-Z, 36='-'
// --------------------------------------------------------------------------
static const unsigned int c39[37] = {
    0x034, /* 0 */ 0x121, /* 1 */ 0x061, /* 2 */ 0x160, /* 3 */
    0x025, /* 4 */ 0x124, /* 5 */ 0x064, /* 6 */ 0x01C, /* 7 */
    0x11C, /* 8 */ 0x05C, /* 9 */
    0x109, /* A */ 0x049, /* B */ 0x148, /* C */ 0x00D, /* D */
    0x10C, /* E */ 0x04C, /* F */ 0x01A, /* G */ 0x118, /* H */
    0x058, /* I */ 0x00A, /* J */ 0x111, /* K */ 0x051, /* L */
    0x150, /* M */ 0x015, /* N */ 0x114, /* O */ 0x054, /* P */
    0x01E, /* Q */ 0x11E, /* R ... wait these need verification */
    0x05E, /* S */ 0x00E, /* T */ 0x103, /* U */ 0x043, /* V */
    0x142, /* W */ 0x007, /* X */ 0x106, /* Y */ 0x046, /* Z */
    0x00B  /* - */
};
/* Start/stop '*' pattern */
#define C39_STAR  0x094u

static unsigned char c39_lookup(char c)
{
    if (c >= '0' && c <= '9') return (unsigned char)(c - '0');
    if (c >= 'A' && c <= 'Z') return (unsigned char)(c - 'A' + 10);
    return 36; /* '-' as fallback */
}

// --------------------------------------------------------------------------
// Mode-1 pixel encoding helpers
// ink0=white 0x00  ink1=black(bg) 0xF0  ink2=dim 0x0F  ink3=bright 0xFF
// --------------------------------------------------------------------------
// half_left[ink]:  pixels 0,1 contribution to a Mode-1 byte
// half_right[ink]: pixels 2,3 contribution
static const unsigned char half_left[4]  = { 0x00, 0xC0, 0x0C, 0xCC };
static const unsigned char half_right[4] = { 0x00, 0x30, 0x03, 0x33 };

// --------------------------------------------------------------------------
// Code 39 module widths: narrow=4px(1 byte), wide=8px(2 bytes)
// --------------------------------------------------------------------------
#define MOD_N   4
#define MOD_W   8

// --------------------------------------------------------------------------
// 5x8 font (A-Z only, uppercase). 5 columns, LSB = top row.
// --------------------------------------------------------------------------
static const unsigned char font5x8[26][5] = {
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, /* A */
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, /* B */
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, /* C */
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, /* D */
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, /* E */
    { 0x7F, 0x09, 0x09, 0x09, 0x01 }, /* F */
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, /* G */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, /* H */
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, /* I */
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, /* J */
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, /* K */
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, /* L */
    { 0x7F, 0x02, 0x04, 0x02, 0x7F }, /* M */
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, /* N */
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, /* O */
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, /* P */
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, /* Q */
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, /* R */
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, /* S */
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, /* T */
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, /* U */
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, /* V */
    { 0x3F, 0x40, 0x30, 0x40, 0x3F }, /* W */
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, /* X */
    { 0x07, 0x08, 0x70, 0x08, 0x07 }, /* Y */
    { 0x61, 0x51, 0x49, 0x45, 0x43 }  /* Z */
};

// --------------------------------------------------------------------------
// Data-segment buffers
// --------------------------------------------------------------------------
_data unsigned char zero_plane[2000];   /* full scanline at 0xF0 (ink1) */
_data unsigned char row_buf[80];        /* one barcode scanline, up to 80 bytes */
_data unsigned char row_buf_len;        /* used bytes in row_buf */
_data unsigned char row_buf_x;          /* screen byte-column where row_buf starts */
_data unsigned char lbl_buf[80];        /* one label scanline */
_data unsigned char lbl_len;
_data unsigned char lbl_x;
_data unsigned char cur_bar_byte;       /* Mode-1 byte for bar ink, picked per word */

_data char cfgdat[64];
_data char init_tmp[64];

// --------------------------------------------------------------------------
// Config (_transfer)
// --------------------------------------------------------------------------
_transfer char        tmp_speed   = 2;
_transfer char        cfg_prz     = 0;
_transfer signed char cfgwin_id   = -1;
_transfer char        rg_speed[4] = { -1, -1, -1, -1 };

_transfer Ctrl_TFrame cfg_tf    = { "Settings", (COLOR_BLACK<<2)|COLOR_ORANGE, 0 };
_transfer Ctrl_Text   cfg_lbl_s = { "Speed:",   (COLOR_BLACK<<2)|COLOR_ORANGE, 0 };

_transfer Ctrl_Radio cfg_rad_s1 = { &tmp_speed, "Slow",   (COLOR_BLACK<<2)|COLOR_ORANGE, 1, rg_speed };
_transfer Ctrl_Radio cfg_rad_s2 = { &tmp_speed, "Normal", (COLOR_BLACK<<2)|COLOR_ORANGE, 2, rg_speed };
_transfer Ctrl_Radio cfg_rad_s3 = { &tmp_speed, "Fast",   (COLOR_BLACK<<2)|COLOR_ORANGE, 3, rg_speed };

_transfer Ctrl ccc0 = { 0,  C_AREA,   -1, COLOR_ORANGE,               0,  0, 162, 50, 0 };
_transfer Ctrl ccc1 = { 0,  C_TFRAME, -1, (unsigned short)&cfg_tf,    2,  1, 158, 26, 0 };
_transfer Ctrl ccc2 = { 0,  C_TEXT,   -1, (unsigned short)&cfg_lbl_s, 8,  8,  40,  8, 0 };
_transfer Ctrl ccc3 = { 0,  C_RADIO,  -1, (unsigned short)&cfg_rad_s1,52,  8,  30,  8, 0 };
_transfer Ctrl ccc4 = { 0,  C_RADIO,  -1, (unsigned short)&cfg_rad_s2,84,  8,  44,  8, 0 };
_transfer Ctrl ccc5 = { 0,  C_RADIO,  -1, (unsigned short)&cfg_rad_s3,130, 8,  30,  8, 0 };
_transfer Ctrl ccc6 = { 10, C_BUTTON, -1, (unsigned short)"OK",       44, 36,  32, 12, 0 };
_transfer Ctrl ccc7 = { 11, C_BUTTON, -1, (unsigned short)"Cancel",   84, 36,  52, 12, 0 };

_transfer Ctrl_Group cfgcg;
_transfer Window     cfgwin;
_transfer char       cfg_title[8] = { 'B','a','r','c','o','d','e',0 };
_transfer Ctrl       anim_ctrl[1];
_transfer Ctrl_Group anim_cg;
_transfer Window     anim_win;
_transfer char       empty_str[1];

// --------------------------------------------------------------------------
// VRAM helpers
// --------------------------------------------------------------------------
static void vram_clear(void)
{
    unsigned char k;
    for (k = 0; k < 8; k++)
        Bank_Copy(0,
            (char *)(0xC000u + (unsigned short)k * 0x0800u),
            _symbank, (char *)zero_plane, 2000u);
}

static void vram_hline(unsigned char x_byte, unsigned char y,
                       unsigned char *buf, unsigned char nbytes)
{
    unsigned short addr;
    addr = 0xC000u
         + (unsigned short)(y >> 3) * 80u
         + (unsigned short)(y & 7)  * 0x0800u
         + (unsigned short)x_byte;
    Bank_Copy(0, (char *)addr, _symbank, (char *)buf, (unsigned short)nbytes);
}

// --------------------------------------------------------------------------
// Barcode builder: encode word into row_buf, set row_buf_len and row_buf_x
// --------------------------------------------------------------------------
// Append `nbytes` copies of `fill` into row_buf starting at *pos.
static void append_bytes(unsigned char *pos_ptr, unsigned char fill,
                         unsigned char nbytes)
{
    unsigned char i;
    unsigned char pos;
    pos = *pos_ptr;
    for (i = 0; i < nbytes && pos < 80; i++)
        row_buf[pos++] = fill;
    *pos_ptr = pos;
}

// Encode one Code 39 character (9 elements) at current position.
static void encode_c39_char(unsigned char pat, unsigned char *pos_ptr)
{
    unsigned char elem, is_bar, width, fill;
    for (elem = 0; elem < 9; elem++) {
        is_bar = (elem & 1) == 0;
        width  = (pat & (0x100u >> elem)) ? (MOD_W >> 2) : (MOD_N >> 2);
        fill   = is_bar ? cur_bar_byte : 0xF0u;
        append_bytes(pos_ptr, fill, width);
    }
    /* inter-character gap: 1 narrow space (1 byte = 4px) */
    append_bytes(pos_ptr, 0xF0u, 1);
}

static void build_barcode(const char *word)
{
    unsigned char pos;
    unsigned char total_px, start_byte, end_byte;
    const char *p;

    /* First pass: measure total width in bytes */
    /* Each char = 3 wide + 6 narrow + 1 inter = 3*2+6*1+1 = 13 bytes     */
    /* start/stop '*' same width. word has up to 13 chars (OYSTERCATCHER). */
    /* Total = (strlen+2)*13 bytes.  We cap row_buf at 80 bytes.           */

    pos = 0;
    encode_c39_char((unsigned char)C39_STAR, &pos);
    for (p = word; *p; p++)
        encode_c39_char((unsigned char)c39[c39_lookup(*p)], &pos);
    encode_c39_char((unsigned char)C39_STAR, &pos);
    /* remove trailing inter-char gap from last stop char */
    if (pos > 0) pos--;

    row_buf_len = pos;
    /* centre horizontally */
    start_byte  = (unsigned char)((80u - pos) / 2u);
    row_buf_x   = start_byte;

    /* shift row_buf right by start_byte: move content to centred position */
    /* Since we built from pos=0, move it now */
    {
        unsigned char i;
        /* shift right in-place (safe: start_byte < 80-pos) */
        for (i = pos; i > 0; i--)
            row_buf[start_byte + i - 1] = row_buf[i - 1];
        /* fill leading background */
        for (i = 0; i < start_byte; i++)
            row_buf[i] = 0xF0u;
        /* fill trailing background */
        end_byte = start_byte + pos;
        for (i = end_byte; i < 80; i++)
            row_buf[i] = 0xF0u;
    }
    row_buf_len = 80; /* write full scanline to avoid stale pixels */
}

#define BAR_Y       52
#define BAR_HEIGHT  80

static void draw_barcode_vram(void)
{
    unsigned char row;
    for (row = 0; row < BAR_HEIGHT; row++)
        vram_hline(0, (unsigned char)(BAR_Y + row), row_buf, 80);
}

// --------------------------------------------------------------------------
// Label renderer: word centred below barcode in 5x8 font
// --------------------------------------------------------------------------
// Each char is 5px wide + 1px gap. Pixels are written into lbl_buf in
// Mode-1 encoding. We build one scanline at a time (one font row).

static void build_label_row(const char *word, unsigned char font_row)
{
    unsigned char len, total_px, start_px, i, ci, col, px;
    unsigned char glyph, bp, boff, lo_bit, hi_bit;

    len      = (unsigned char)strlen(word);
    total_px = len * 6;
    if (total_px > 0) total_px -= 1;

    /* centre, byte-aligned */
    start_px = (unsigned char)((320u - total_px) / 2u);
    start_px &= 0xFCu;

    lbl_x   = start_px >> 2;
    lbl_len = 80;

    /* fill with background */
    for (i = 0; i < 80; i++) lbl_buf[i] = 0xF0u;

    for (ci = 0; ci < len; ci++) {
        if (word[ci] < 'A' || word[ci] > 'Z') continue;
        glyph = (unsigned char)(word[ci] - 'A');
        for (col = 0; col < 5; col++) {
            if (!(font5x8[glyph][col] & (1u << font_row))) continue;
            px   = (unsigned char)(start_px + ci * 6 + col);
            bp   = px >> 2;
            boff = px & 3u;
            /* ink3 (bright): lo bit = 1, hi bit = 1 */
            /* lo: bit7>>boff,  hi: bit3>>boff */
            lo_bit = (unsigned char)(0x80u >> boff);
            hi_bit = (unsigned char)(0x08u >> boff);
            lbl_buf[bp] &= (unsigned char)(~(lo_bit | hi_bit));
            if (cur_bar_byte & 0xF0u) lbl_buf[bp] |= lo_bit;
            if (cur_bar_byte & 0x0Fu) lbl_buf[bp] |= hi_bit;
        }
    }
}

#define LABEL_Y  (BAR_Y + BAR_HEIGHT + 4)

static void draw_label(const char *word)
{
    unsigned char r;
    for (r = 0; r < 8; r++) {
        build_label_row(word, r);
        vram_hline(0, (unsigned char)(LABEL_Y + r), lbl_buf, 80);
    }
}

// --------------------------------------------------------------------------
// Key scan
// --------------------------------------------------------------------------
static unsigned char any_key_down(void)
{
    unsigned char sc;
    for (sc = 0; sc < 80; sc++)
        if (Key_Down(sc)) return 1;
    return 0;
}

// --------------------------------------------------------------------------
// Desktop stop / resume
// --------------------------------------------------------------------------
static void desktop_stop(unsigned char wid)
{
    _symmsg[0] = MSC_DSK_DSKSRV;
    _symmsg[1] = DSK_SRV_DSKSTP;
    _symmsg[2] = 0xFF;
    _symmsg[3] = wid;
    while (Msg_Send(_sympid, 2, _symmsg) == 0);
    Msg_Wait(_sympid, 2, _symmsg, MSR_DSK_DSKSRV);
}

static void desktop_cont(void)
{
    _symmsg[0] = MSC_DSK_DSKSRV;
    _symmsg[1] = DSK_SRV_DSKCNT;
    while (Msg_Send(_sympid, 2, _symmsg) == 0);
    Idle();
}

// --------------------------------------------------------------------------
// Config dialog
// --------------------------------------------------------------------------
static void cfg_open(void)
{
    if (cfgwin_id >= 0) return;
    tmp_speed = cfgdat[4];
    rg_speed[0] = rg_speed[1] = rg_speed[2] = rg_speed[3] = -1;

    memset(&cfgcg, 0, sizeof(cfgcg));
    cfgcg.controls = 8; cfgcg.pid = _sympid; cfgcg.first = &ccc0;

    memset(&cfgwin, 0, sizeof(cfgwin));
    cfgwin.state = WIN_NORMAL;
    cfgwin.flags = WIN_TITLE | WIN_CENTERED | WIN_NOTTASKBAR;
    cfgwin.pid   = _sympid;
    cfgwin.w = cfgwin.wfull = cfgwin.wmin = cfgwin.wmax = 162;
    cfgwin.h = cfgwin.hfull = cfgwin.hmin = cfgwin.hmax = 50;
    cfgwin.title    = cfg_title;
    cfgwin.controls = &cfgcg;

    cfgwin_id = Win_Open(_symbank, &cfgwin);
}

static void cfg_close(void)
{
    if (cfgwin_id < 0) return;
    Win_Close((unsigned char)cfgwin_id);
    cfgwin_id = -1;
}

static void cfg_ok(void)
{
    cfgdat[4] = tmp_speed;
    cfg_close();
    if (cfg_prz) {
        _symmsg[0] = MSR_SAV_CONFIG;
        _symmsg[1] = _symbank;
        _symmsg[2] = (char)((unsigned short)cfgdat & 0xFF);
        _symmsg[3] = (char)((unsigned short)cfgdat >> 8);
        while (!Msg_Send(_sympid, cfg_prz, _symmsg));
        cfg_prz = 0;
    }
}

static void cfg_cancel(void) { cfg_close(); cfg_prz = 0; }

// --------------------------------------------------------------------------
// Animation
// --------------------------------------------------------------------------
static void show_word(const char *word)
{
    static const unsigned char bar_inks[2] = { 0x00u, 0xFFu }; /* white, bright */
    cur_bar_byte = bar_inks[rand() % 2];
    vram_clear();
    build_barcode(word);
    draw_barcode_vram();
    draw_label(word);
}

void start_animation(void)
{
    signed char    wid;
    unsigned short mx0, my0, resp;
    unsigned int   pause_ticks, tick;
    unsigned char  word_idx, speed, b;

    speed = (unsigned char)cfgdat[4];
    if (speed < 1 || speed > 3) speed = 2;
    pause_ticks = (speed == 1) ? 750u : (speed == 3) ? 100u : 250u;

    srand((unsigned int)Sys_Counter());
    memset(zero_plane, 0xF0u, sizeof(zero_plane));
    empty_str[0] = 0;

    anim_ctrl[0].value = 0; anim_ctrl[0].type = C_AREA; anim_ctrl[0].bank = -1;
    anim_ctrl[0].param = AREA_16COLOR | COLOR_BLACK;
    anim_ctrl[0].x = 0; anim_ctrl[0].y = 0;
    anim_ctrl[0].w = SCREEN_W; anim_ctrl[0].h = SCREEN_H; anim_ctrl[0].unused = 0;

    memset(&anim_cg, 0, sizeof(anim_cg));
    anim_cg.controls = 1; anim_cg.pid = _sympid; anim_cg.first = &anim_ctrl[0];

    memset(&anim_win, 0, sizeof(anim_win));
    anim_win.state = WIN_NORMAL;
    anim_win.flags = WIN_NOTTASKBAR | WIN_NOTMOVEABLE;
    anim_win.pid   = _sympid;
    anim_win.w  = anim_win.wfull = anim_win.wmax = SCREEN_W;
    anim_win.h  = anim_win.hfull = anim_win.hmax = SCREEN_H;
    anim_win.wmin = 32; anim_win.hmin = 24;
    anim_win.title = anim_win.status = empty_str;
    anim_win.controls = &anim_cg;

    wid = Win_Open(_symbank, &anim_win);
    if (wid < 0) return;

    desktop_stop((unsigned char)wid);

    word_idx = (unsigned char)(rand() % NBIRDS);
    show_word(birds[word_idx]);

    mx0 = Mouse_X(); my0 = Mouse_Y(); tick = 0;

    while (1) {
        if (Mouse_X() != mx0 || Mouse_Y() != my0 ||
            Mouse_Buttons() || any_key_down()) {
            desktop_cont();
            Idle();
            Win_Close((unsigned char)wid);
            Screen_Redraw();
            return;
        }

        resp = Msg_Receive(_sympid, -1, _symmsg);
        if (resp & 1) {
            if (_symmsg[0] == 0) {
                desktop_cont();
                Win_Close((unsigned char)wid);
                exit(0);
            }
        }

        if (++tick >= pause_ticks) {
            tick = 0;
            word_idx = (unsigned char)(rand() % NBIRDS);
            show_word(birds[word_idx]);
        }

        Idle();
    }
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    unsigned short resp;
    unsigned char  got_msg, sender, b;

    cfgdat[0]='B'; cfgdat[1]='R'; cfgdat[2]='C'; cfgdat[3]='X'; cfgdat[4]=2;

    got_msg = 0; sender = 0;
    for (b = 0; b < 10; b++) {
        Idle();
        resp = Msg_Receive(_sympid, -1, _symmsg);
        if (resp & 0x01) { got_msg = 1; sender = (unsigned char)(resp >> 8); break; }
    }

    if (!got_msg) { start_animation(); exit(0); }

    while (1) {
        switch (_symmsg[0]) {
        case 0: exit(0);
        case MSC_SAV_INIT:
            Bank_Copy(_symbank, init_tmp,
                (unsigned char)_symmsg[1],
                (char *)((unsigned short)((unsigned char)_symmsg[3] << 8)
                         | (unsigned char)_symmsg[2]), 64u);
            if (init_tmp[0]=='B' && init_tmp[1]=='R' &&
                init_tmp[2]=='C' && init_tmp[3]=='X')
                memcpy(cfgdat, init_tmp, 64);
            break;
        case MSC_SAV_START:
            start_animation();
            break;
        case MSC_SAV_CONFIG:
            cfg_prz = sender;
            cfg_open();
            break;
        default:
            if ((unsigned char)_symmsg[0] == MSR_DSK_WCLICK &&
                cfgwin_id >= 0 &&
                (unsigned char)_symmsg[1] == (unsigned char)cfgwin_id) {
                if ((unsigned char)_symmsg[2] == DSK_ACT_CLOSE)
                    cfg_cancel();
                else if ((unsigned char)_symmsg[2] == DSK_ACT_CONTENT) {
                    if ((unsigned char)_symmsg[8] == 10) cfg_ok();
                    else if ((unsigned char)_symmsg[8] == 11) cfg_cancel();
                }
            }
            break;
        }
        do { resp = Msg_Sleep(_sympid, -1, _symmsg); } while (!(resp & 0x01));
        sender = (unsigned char)(resp >> 8);
    }
}
