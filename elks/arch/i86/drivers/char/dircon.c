/*
 * Direct video memory display driver
 * Saku Airila 1996
 * Color originally written by Mbit and Davey
 * Re-wrote for new ntty iface
 * Al Riddoch  1999
 *
 * Rewritten by Greg Haerr <greg@censoft.com> July 1999
 * added reverse video, cleaned up code, reduced size
 * added enough ansi escape sequences for visual editing
 */
#include <linuxmt/types.h>
#include <linuxmt/config.h>
#include <linuxmt/errno.h>
#include <linuxmt/fcntl.h>
#include <linuxmt/fs.h>
#include <linuxmt/kernel.h>
#include <linuxmt/major.h>
#include <linuxmt/mm.h>
#include <linuxmt/sched.h>
#include <linuxmt/chqueue.h>
#include <linuxmt/ntty.h>

#include <arch/io.h>
#include <arch/keyboard.h>

#ifdef CONFIG_CONSOLE_DIRECT

/* Assumes ASCII values. */
#define isalpha(c) (((unsigned char)(((c) | 0x20) - 'a')) < 26)

#if 0
/* public interface of dircon.c: */

void con_charout(char Ch);
int Console_write(struct tty *tty);
void Console_release(struct tty *tty);
int Console_open(struct tty *tty);
struct tty_ops dircon_ops;
void init_console(void);
#endif

#define A_DEFAULT 	0x07
#define A_BOLD 		0x08
#define A_BLINK 	0x80
#define A_REVERSE	0x70
#define A_BLANK		0x00

/* character definitions*/
#define BS		'\b'
#define NL		'\n'
#define CR		'\r'
#define TAB		'\t'
#define ESC		'\x1B'
#define BEL		'\x07'

#define MAXPARMS	10

struct console;
typedef struct console Console;

struct console {
    int cx, cy;			/* cursor position */
    void (*fsm)(register Console *, char);
    unsigned int vseg;		/* video segment for page */
    int basepage;		/* start of video ram */
    unsigned char attr;		/* current attribute */
    unsigned char XN;		/* delayed newline on column 80 */
    unsigned char color;	/* fg/bg attr */
#ifdef CONFIG_EMUL_VT52
    unsigned char tmp;		/* ESC Y ch save */
#endif
#ifdef CONFIG_EMUL_ANSI
    int savex, savey;		/* saved cursor position */
    unsigned char *parmptr;	/* ptr to params */
    unsigned char params[MAXPARMS];	/* ANSI params */
#endif
};

static struct wait_queue glock_wait;
static Console Con[MAX_CONSOLES], *Visible;
static Console *glock;		/* Which console owns the graphics hardware */
static void *CCBase;
static int Width, MaxCol, Height, MaxRow;
static unsigned short int NumConsoles = MAX_CONSOLES;

/* from keyboard.c */
extern int Current_VCminor;
extern int kraw;

#ifdef CONFIG_EMUL_ANSI
#define TERM_TYPE " emulating ANSI "
#elif CONFIG_EMUL_VT52
#define TERM_TYPE " emulating vt52 "
#else
#define TERM_TYPE " dumb "
#endif

static void std_char(register Console *, char);
extern void AddQueue(unsigned char Key);	/* From xt_key.c */

static void SetDisplayPage(register Console * C)
{
    register char *CCBasep;

    CCBasep = (char *) CCBase;
    outw((unsigned short int) ((C->basepage & 0xff00) | 0x0c), CCBasep);
    outw((unsigned short int) ((C->basepage << 8) | 0x0d), CCBasep);
}

static void PositionCursor(register Console * C)
{
    register char *CCBasep = (char *) CCBase;
    int Pos;

    Pos = C->cx + Width * C->cy + C->basepage;
    outb(14, CCBasep);
    outb((unsigned char) ((Pos >> 8) & 0xFF), CCBasep + 1);
    outb(15, CCBasep);
    outb((unsigned char) (Pos & 0xFF), CCBasep + 1);
}

static void VideoWrite(register Console * C, char c)
{
    pokew((word_t)((C->cx + C->cy * Width) << 1),
    		(seg_t) C->vseg,
	  ((word_t)C->attr << 8) | ((word_t)c));
}

static void ClearRange(register Console * C, int x, int y, int xx, int yy)
{
    register __u16 *vp;

    xx = xx - x + 1;
    vp = (__u16 *)((__u16)(x + y * Width) << 1);
    do {
	for (x = 0; x < xx; x++)
	    pokew((word_t) (vp++), (seg_t) C->vseg, (((word_t) C->attr << 8) | ' '));
	vp += (Width - xx);
    } while (++y <= yy);
}

static void ScrollUp(register Console * C, int y)
{
    register __u16 *vp;

    vp = (__u16 *)((__u16)(y * Width) << 1);
    if ((unsigned int)y < MaxRow)
	fmemcpyb((byte_t *)vp, (seg_t) C->vseg,
		(byte_t *)(vp + Width), (seg_t) C->vseg, (MaxRow - y)*(Width << 1));
    ClearRange(C, 0, MaxRow, MaxCol, MaxRow);
}

#if defined (CONFIG_EMUL_VT52) || defined (CONFIG_EMUL_ANSI)
static void ScrollDown(register Console * C, int y)
{
    register __u16 *vp;
    int yy = MaxRow;

    vp = (__u16 *)((__u16)(yy * Width) << 1);
    while (--yy >= y) {
	fmemcpyb((byte_t *)vp, (seg_t) C->vseg,
		(byte_t *)(vp - Width), (seg_t) C->vseg, (Width << 1));
	vp -= Width;
    }
    ClearRange(C, 0, y, MaxCol, y);
}
#endif

#if defined (CONFIG_EMUL_VT52) || defined (CONFIG_EMUL_ANSI)
static void Console_gotoxy(register Console * C, int x, int y)
{
    register int xp = x;

    C->cx = (xp >= MaxCol) ? MaxCol : (xp < 0) ? 0 : xp;
    xp = y;
    C->cy = (xp >= MaxRow) ? MaxRow : (xp < 0) ? 0 : xp;
    C->XN = 0;
}
#endif

#ifdef CONFIG_EMUL_ANSI
static int parm1(register unsigned char *buf)
{
    int n;

    if (!(n = atoi((char *)buf)))
	n = 1;
    return n;
}

static int parm2(register unsigned char *buf)
{
    while (*buf != ';' && *buf)
	buf++;
    if (*buf)
	buf++;
    return parm1(buf);
}

static void itoaQueue(int i)
{
   unsigned char a[6];
   unsigned char *b = a + sizeof(a) - 1;

   *b = 0;
   do {
      *--b = '0' + (i % 10);
      i /= 10;
   } while (i);
   while (*b)
	AddQueue(*b++);
}

static void AnsiCmd(register Console * C, char c)
{
    int n;

    /* ANSI param gathering and processing */
    if (C->parmptr < &C->params[MAXPARMS - 1])
	*C->parmptr++ = (unsigned char) c;
    if (!isalpha(c)) {
	return;
    }
    *(C->parmptr) = 0;

    switch (c) {
    case 's':			/* Save the current location */
	C->savex = C->cx;
	C->savey = C->cy;
	break;
    case 'u':			/* Restore saved location */
	C->cx = C->savex;
	C->cy = C->savey;
	break;
    case 'A':			/* Move up n lines */
	C->cy -= parm1(C->params);
	if (C->cy < 0)
	    C->cy = 0;
	break;
    case 'B':			/* Move down n lines */
	C->cy += parm1(C->params);
	if (C->cy > MaxRow)
	    C->cy = MaxRow;
	break;
    case 'C':			/* Move right n characters */
	C->cx += parm1(C->params);
	if (C->cx > MaxCol)
	    C->cx = MaxCol;
	break;
    case 'D':			/* Move left n characters */
	C->cx -= parm1(C->params);
	if (C->cx < 0)
	    C->cx = 0;
	break;
    case 'H':			/* cursor move */
	Console_gotoxy(C, parm2(C->params) - 1, parm1(C->params) - 1);
	break;
    case 'J':			/* erase */
	if (parm1(C->params) == 2)
	    ClearRange(C, 0, 0, MaxCol, MaxRow);
	break;
    case 'K':			/* Clear to EOL */
	ClearRange(C, C->cx, C->cy, MaxCol, C->cy);
	break;
    case 'L':			/* insert line */
	ScrollDown(C, C->cy);
	break;
    case 'M':			/* remove line */
	ScrollUp(C, C->cy);
	break;
    case 'm':			/* ansi color */
	n = atoi((char *)C->params);
	if (n >= 30 && n <= 37) {
	    C->attr &= 0xf8;
	    C->attr |= (n-30) & 0x07;
	    C->color = C->attr;
	}
	else if (n >= 40 && n <= 47) {
	    C->attr &= 0x8f;
	    C->attr |= ((n-40) << 4) & 0x70;
	    C->color = C->attr;
	}
	else switch (n) {
	    case 0:
		C->attr = C->color;
		break;
	    case 1:
		C->attr |= A_BOLD;
		break;
	    case 5:
		C->attr |= A_BLINK;
		break;
	    case 7:
		n = C->attr;
		C->attr &= ~(A_DEFAULT | A_REVERSE);
		C->attr |= ((n >> 4) & 0x07) | ((n << 4) & 0x70);
		break;
	    case 8:
		C->attr = A_BLANK;
		break;
	    case 39:
	    case 49:
	    default:
		C->attr = C->color = A_DEFAULT;
		break;
	}
	break;
    case 'n':
	if (parm1(C->params) == 6) {		/* device status report*/
	    //FIXME sequence can be interrupted by kbd input
	    AddQueue(ESC);
	    AddQueue('[');
	    itoaQueue(C->cy+1);
	    AddQueue(';');
	    itoaQueue(C->cx+1);
	    AddQueue('R');
	}
	break;
    }
    C->fsm = std_char;
}

/* Escape character processing */
static void esc_char(register Console * C, char c)
{
    /* Parse CSI sequence */
    C->parmptr = C->params;
    C->fsm = (c == '[' ? AnsiCmd : std_char);
}
#endif

#ifdef CONFIG_EMUL_VT52
static void esc_Y2(register Console * C, char c)
{
    Console_gotoxy(C, c - ' ', C->tmp);
    C->fsm = std_char;
}

static void esc_YS(register Console * C, char c)
{
    switch(C->tmp) {
    case 'Y':
	C->tmp = (unsigned char) (c - ' ');
	C->fsm = esc_Y2;
	break;
    case '/':
	C->tmp = 'Z';		/* Discard next char */
	break;
    case 'Z':
	C->fsm = std_char;
    }
}

/* Escape character processing */
static void esc_char(register Console * C, char c)
{
    /* process single ESC char sequences */
    C->fsm = std_char;
    switch (c) {
    case 'I':			/* linefeed reverse */
	if (!C->cy) {
	    ScrollDown(C, 0);
	    break;
	}
    case 'A':			/* up */
	if (C->cy)
	    --C->cy;
	break;
    case 'B':			/* down */
	if (C->cy < MaxRow)
	    ++C->cy;
	break;
    case 'C':			/* right */
	if (C->cx < MaxCol)
	    ++C->cx;
	break;
    case 'D':			/* left */
	if (C->cx)
	    --C->cx;
	break;
    case 'H':			/* home */
	C->cx = C->cy = 0;
	break;
    case 'J':			/* clear to eoscreen */
	ClearRange(C, 0, C->cy+1, MaxCol, MaxRow);
    case 'K':			/* clear to eol */
	ClearRange(C, C->cx, C->cy, MaxCol, C->cy);
	break;
    case 'L':			/* insert line */
	ScrollDown(C, C->cy);
	break;
    case 'M':			/* remove line */
	ScrollUp(C, C->cy);
	break;
    case '/':			/* Remove echo for identify response */
    case 'Y':			/* cursor move */
	C->tmp = c;
	C->fsm = esc_YS;
	break;
    case 'Z':			/* identify as VT52 */
	AddQueue(ESC);
	AddQueue('/');
	AddQueue('K');
    }
}
#endif

/* Normal character processing */
static void std_char(register Console * C, char c)
{
    switch(c) {
    case BEL:
	bell();
	break;
    case '\b':
	C->XN = 0;
	if (C->cx > 0) {
	    --C->cx;
	    VideoWrite(C, ' ');
	}
	break;
    case '\t':
	C->cx = (C->cx | 0x07) + 1;
	goto linewrap;
    case '\n':
	C->XN = 0;
	++C->cy;
	break;
    case '\r':
	C->XN = 0;
	C->cx = 0;
	break;

#if defined (CONFIG_EMUL_VT52) || defined (CONFIG_EMUL_ANSI)
    case ESC:
	C->fsm = esc_char;
	break;
#endif

    default:
	if (C->XN) {
	    C->XN = 0;
	    C->cx = 0;
	    C->cy++;
	    if (C->cy > MaxRow) {
		ScrollUp(C, 0);
		C->cy = MaxRow;
	    }
	}
	VideoWrite(C, c);
	C->cx++;
      linewrap:
	if (C->cx > MaxCol) {

#ifdef CONFIG_EMUL_VT52
	    C->cx = MaxCol;
#else
	    C->XN = 1;
	    C->cx = MaxCol;
#endif

	}
    }
    if (C->cy > MaxRow) {
	ScrollUp(C, 0);
	C->cy = MaxRow;
    }
}

static void WriteChar(register Console * C, char c)
{
    /* check for graphics lock */
    while (glock) {
	if (glock == C)
	    return;
	sleep_on(&glock_wait);
    }
    C->fsm(C, c);
}

void con_charout(char Ch)
{
    if (Ch == '\n')
	WriteChar(Visible, '\r');
    WriteChar(Visible, Ch);
    PositionCursor(Visible);
}

/* This also tells the keyboard driver which tty to direct it's output to...
 * CAUTION: It *WILL* break if the console driver doesn't get tty0-X.
 */

void Console_set_vc(unsigned int N)
{
    if ((N >= NumConsoles)
	|| (Visible == &Con[N])
	|| (glock))
	return;
    Visible = &Con[N];

    SetDisplayPage(Visible);
    PositionCursor(Visible);
    Current_VCminor = (int) N;
}

static int Console_ioctl(struct tty *tty, int cmd, char *arg)
{
    register Console *C = &Con[tty->minor];

    switch (cmd) {
    case DCGET_GRAPH:
	if (!glock) {
	    glock = C;
	    return 0;
	}
	return -EBUSY;
    case DCREL_GRAPH:
	if (glock == C) {
	    glock = NULL;
	    wake_up(&glock_wait);
	    return 0;
	}
	break;
    case DCSET_KRAW:
	if (glock == C) {
	    kraw = 1;
	    return 0;
	}
	break;
    case DCREL_KRAW:
	if ((glock == C) && (kraw == 1)) {
	    kraw = 0;
	    return 1;
	}
	break;
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
	return 0;
    }

    return -EINVAL;
}

static int Console_write(register struct tty *tty)
{
    register Console *C = &Con[tty->minor];
    int cnt = 0;

    while ((tty->outq.len > 0) && !glock) {
	WriteChar(C, (char)tty_outproc(tty));
	cnt++;
    }
    if (C == Visible)
	PositionCursor(C);
    return cnt;
}

static void Console_release(struct tty *tty)
{
/* Do nothing */
}

static int Console_open(register struct tty *tty)
{
    return (tty->minor >= NumConsoles) ? -ENODEV : 0;
}

struct tty_ops dircon_ops = {
    Console_open,
    Console_release,
    Console_write,
    NULL,
    Console_ioctl,
};

void init_console(void)
{
    register Console *C;
    register int i;
    unsigned PageSizeW;
    unsigned VideoSeg;

    MaxCol = (Width = peekb(0x4a, 0x40)) - 1;  /* BIOS data segment */

    /* Trust this. Cga does not support peeking at 0x40:0x84. */
    MaxRow = (Height = 25) - 1;
    CCBase = (void *) peekw(0x63, 0x40);
    PageSizeW = ((unsigned int)peekw(0x4C, 0x40) >> 1);

    VideoSeg = 0xb800;
    if (peekb(0x49, 0x40) == 7) {
	VideoSeg = 0xB000;
	NumConsoles = 1;
    }

    C = Con;
    Visible = C;

    for (i = 0; i < NumConsoles; i++) {
	C->cx = C->cy = 0;
	if (!i) {
	    C->cx = peekb(0x50, 0x40);
	    C->cy = peekb(0x51, 0x40);
	}
	C->fsm = std_char;
	C->basepage = i * PageSizeW;
	C->vseg = VideoSeg + (C->basepage >> 3);
	C->attr = A_DEFAULT;
	C->color = A_DEFAULT;

#ifdef CONFIG_EMUL_ANSI

	C->savex = C->savey = 0;

#endif

	/* Do not erase early printk() */
	/* ClearRange(C, 0, C->cy, MaxCol, MaxRow); */

	C++;
    }

    printk("Console: Direct %ux%u"TERM_TYPE"(%u virtual consoles)\n",
	   Width, Height, NumConsoles);
}

#endif
