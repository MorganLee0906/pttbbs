/**
 * edit.c, �ΨӴ��� bbs�W����r�s�边, �Y ve.
 * �{�b�o�@�ӬO�c�d�L������, �����í�w, �Τ���h�� cpu, ���O�i�H�٤U�\�h
 * ���O���� (�H Ptt����, �b�E�d�H�W�����ɭ�, ���i�٤U 50MB ���O����)
 * �p�G�z�{���u�� cpu���O����v�ä��X�G�z�����D, �z�i�H�Ҽ{��ϥέץ��e��
 * ���� (Revision 782)
 *
 * �쥻 ve �����k�O, �]���C�@��̤j�i�H��J WRAPMARGIN �Ӧr, ��O�N���C�@
 * ��O�d�F WRAPMARGIN �o��j���Ŷ� (�� 512 bytes) . ���O��ڤW, ���b�ץ�
 * �����̤p���Ҷq�W, �ڭ̥u���n�ϱo��ЩҦb�o�@����� WRAPMARGIN �o��j,
 * ��L�C�@���ꤣ���n�o��h���Ŷ�. ��O�o�� patch�N�b�C����Цb�涡����
 * ���ɭ�, �N�쥻������O�����Y�p, �A�N�s���쪺���歫�s�[�j, �H�F���̤p��
 * �O����ζq.
 * �H�W�����o�Ӱʧ@�b adjustline() ������, adjustline()�t�~�]�A�ץ��ƭ�
 * global pointer, �H�קK dangling pointer .
 * �t�~�Y�w�q DEBUG, �b textline_t ���c���N�[�J mlength, ���ܸӦ��ڦ���
 * �O����j�p. �H��K���յ��G.
 *
 * XXX �ѩ�U�ؾާ@�����| maintain �϶��аO�Ҧ��� pointer/state,
 * �]�����|�y���W�R line ���\��, ���o�۰ʨ����аO�Ҧ�(call block_cancel()).
 *
 * 20071230 piaip
 * BBSmovie ���H�@�X�F 1.9G ���ɮ�, �ݨӭn�� hard limit �� soft limit
 * [�� 7426572/7426572 �� (100%)  �ثe���: �� 163384551~163384573 ��]
 * ����լd BBSmovie �ݪO�P��ذϡA�����ɮ׬Ҧb 5M �H�U
 * �̤j���� 16M �� Haruhi OP (avi ���� with massive ANSI)
 * [�� 2953/2953 �� (100%)  �ثe���: �� 64942~64964 ��]
 * �t�~���ʰg�c���j�p��
 * [�� 1408/1408 �� (100%)  �ثe���: �� 30940~30962 ��]
 * �O�H�w�q:
 * 32M �� size limit
 * 1M �� line limit
 * �S�A���M�o�{���e totaln �������O short... �ҥH 65536 �N���F?
 * ���: ���G�O�� announce �� append �@�X�Ӫ��A���ݨ� > --- <- mark�C
 *
 * FIXME screen resize �y�� b_lines ����, �i��|�y�� state ����. �ר�O tty
 * mode �H�ɳ��i�� resize. TODO editor_internal_t �h�����O screen size,
 * ���ާ@��@�q������~ check b_lines �O�_������.
 */
#include "bbs.h"

#define EDIT_SIZE_LIMIT (32768*1024)
#define EDIT_LINE_LIMIT (65530) // (1048576)

#ifndef POST_MONEY_RATIO
#define POST_MONEY_RATIO (0.5f)
#endif

#ifndef ENTROPY_RATIO
#define ENTROPY_RATIO	(0.25f)
#endif

#define ENTROPY_MAX	(MAX_POST_MONEY/ENTROPY_RATIO)

#if 0
#define DEBUG
#define inline
#endif

/**
 * data ��쪺�Ϊk:
 * �C�� allocate �@�� textline_t �ɡA�|�t���L (sizeof(textline_t) + string
 * length - 1) ���j�p�C�p���i�����s�� data �Ӥ����B�~�� malloc�C
 */
typedef struct textline_t {
    struct textline_t *prev;
    struct textline_t *next;
    short           len;
#ifdef DEBUG
    short           mlength;
#endif
    char            data[1];
}               textline_t;

#define KEEP_EDITING    -2

enum {
    NOBODY, MANAGER, SYSOP
};

/**
 * �o�ӻ����|�N��� edit.c �B�@�������a�L�A�D�n�|�q editor_internal_t ��
 * data structure �Ͱ_�C���C�@�� data member ���Բӥ\��A�Ш� sturcture
 * �������ѡC
 *
 * �峹�����e (�H�U�� content) �H�u��v�����A�D�n�H firstline, lastline,
 * totaln �ӰO���C
 *
 * User �b�e�����ݨ쪺�e���A�m��@�ӡu window �v���A�o�� window �|�b
 * content �W���ʡAwindow �̭����d��Y���n show �X�Ӫ��d��C���ΤF����
 * ���ӰO���A�]�A currline, top_of_win, currln, currpnt, curr_window_line,
 * edit_margin�C��ܥX�Ӫ��ĪG���M���u�O�a�o�X�Ӹ�ơA�ٷ|���L��즳�椬
 * �@�ΡA�Ҧp �m��s��Ҧ��B�S���Ÿ��s�� �����C�䤤�̽����������O�b��� block
 * �]����^���ɭԡC
 *
 * editor ���ϥΤW�ثe������ inclusive �� mode�G
 *   insert mode:
 *     ���J/���N
 *   ansi mode:
 *     �m��s��
 *   indent mode:
 *     �۰��Y��
 *   phone mode:
 *     �S���Ÿ��s��
 *   raw mode:
 *     ignore Ctrl('S'), Ctrl('Q'), Ctrl('T')
 *     �٤�: �o�������? �ݰ_�ӬO modem �W�ǥ� (�S�H�b�γo�ӤF�a)
 *     ���ӷ� dbcs option �a
 *
 * editor �䴩�F�϶���ܪ��\��]�h���� �� ��椤�����q�^�A���@�� selected
 * block�A�i�H cut, copy, cancel, �Ϊ̦s��Ȩ��ɡA�ƦܬO����/�k shift�C�Ԩ�
 * block_XXX�C
 *
 * �� Ctrl('Y') �R�������@��|�Q�O�� deleted_line �o����줤�Cundelete_line()
 * �i�H�� undelete ���ʧ@�C deleted_line ��l�Ȭ� NULL�A�C���u���\�s�@��A�ҥH
 * �b�s�U�ӮɡA�o�{�Ȥ��� NULL �|���� free ���ʧ@�Ceditor �����ɡA�b
 * edit_buffer_destructor() ���p�G�o�{�� deleted_line�A�|�b�o�����񱼡C��L�a
 * ��]���ۦP���@�k�A�Ҧp searched_string�Csearched_string �O�b�j�M�峹���e
 * �ɡA�n�M�䪺 key word�C
 *
 * �٦��@�Ӧ��쪺�S�I�A�u�A���ǰt�v�I�欰�N�p�P�b vim �̭��@�ˡC�c�A���M�S��
 * ��j�աA���ܤ֦b�t�� c-style comment �� c-style string �ɬO�諸��C�o�Ӱ�
 * �@�w�q�� match_paren() ���C
 *
 * �t�~�A�p�G���ݭn�s�W�s�����A�бN��l�ơ]���ݭn���ܡ^���ʧ@�g�b
 * edit_buffer_constructor ���C���M�]���� edit_buffer_destructor �i�H�ϥΡC
 *
 * ���~�A���F���Ѥ@�� reentrant �� editor�Aprev ���V�e�@�� editor ��
 * editor_internal_t�Center_edit_buffer �� exit_edit_buffer ���Ѷi�X�������A
 * �̭����O�|�I�s constructor �� destructor�C
 *
 * Victor Hsieh <victor@csie.org>
 * Thu, 03 Feb 2005 15:18:00 +0800
 */
typedef struct editor_internal_t {

    textline_t *firstline;	/* first line of the article. */
    textline_t *lastline;	/* last line of the article. */

    textline_t *currline;	/* current line of the article(window). */
    textline_t *blockline;	/* the first selected line of the block. */
    textline_t *top_of_win;	/* top line of the article in the window. */

    textline_t *deleted_line;	/* deleted line. Just keep one deleted line. */
    textline_t *oldcurrline;

    struct editor_internal_t *prev;

    int   flags;                /* editor flags. */
    int   currln;		/* current line of the article. */
    int   totaln;		/* last line of the article. */
    int   curr_window_line;	/* current line to the window. */
    int   blockln;		/* the row you started to select block. */
    short currpnt;		/* current column of the article. */
    short last_margin;
    short edit_margin;		/* when the cursor moves out of range (say,
				   t_columns), shift this length of the string
				   so you won't see the first edit_margin-th
				   character. */
    short lastindent;
    char last_phone_mode;

    char ifuseanony		:1;
    char redraw_everything	:1;

    char insert_mode		:1;
    char ansimode		:1;
    char indent_mode		:1;
    char phone_mode		:1;
    char raw_mode		:1;
    char synparser;		// syntax parser

    char *searched_string;
    char *sitesig_string;
    char *(*substr_fp) ();

} editor_internal_t;
// } __attribute__ ((packed))

static editor_internal_t *curr_buf = NULL;

static const char * const fp_bak = "bak";

// forward declare
static inline int has_block_selection(void);
static textline_t * alloc_line(short length);
static void block_cancel(void);

static const char * const BIG5[13] = {
  "�A�F�G�B�N�C�H�I�E�T�]�^��������",
  "�b�c�d�e�f�g�h�i�j�k�l�m�n�o�p�� ",
  "���󡷡������������������������",
  "�ˡ\\�[�¡ġX�����y���@�������A�B",
  "�ϡСѡҡԡӡסݡڡܡء١ա֡��",
  "�ۡ������������ޡߡ���",
  "����������������",
  "�i�j�u�v�y�z�q�r�m�n�e�f�a�b�_�`",
  "�g�h�c�d�k�l�s�t�o�p�w�x�{�|",
  "���������Ρ������I����K�L�ơ�",
  "�\\�]�^�_�`�a�b�c�d�e�f�g�h�i�j�k",
  "�l�m�n�o�p�q�r�s�G�K�N�S�U�X�Z�[",
  "��������������������"
};

static const char * const BIG_mode[13] = {
  "���I",
  "�϶�",
  "�аO",
  "�нu",
  "�Ƥ@",
  "�ƤG",
  "�b�Y",
  "�A�@",
  "�A�G",
  "��L",
  "�Ƥ@",
  "�ƤG",
  "�Ʀr"
};

static const char *table[8] = {
  "�x�w�|�r�}�u�q�t�z�s�{",
  "����������������������",
  "���w������������������",
  "�x�����������������",
  "�x�w���r���u�q�t�~�s��",
  "�������䢣������~�ޢ�",
  "���w������������������",
  "�x�����������������"
};

static const char *table_mode[6] = {
  "����",
  "�s��",
  "�q",
  "��",
  "��",
  "��"
};

#ifdef DBCSAWARE
static char mbcs_mode		=1;

#define IS_BIG5_HI(x) (0x81 <= (x) && (x) <= 0xfe)
#define IS_BIG5_LOS(x) (0x40 <= (x) && (x) <= 0x7e)
#define IS_BIG5_LOE(x) (0x80 <= (x) && (x) <= 0xfe)
#define IS_BIG5_LO(x) (IS_BIG5_LOS(x) || IS_BIG5_LOE(x))
#define IS_BIG5(hi,lo) (IS_BIG5_HI(hi) && IS_BIG5_LO(lo))

int mchar_len(unsigned char *str)
{
  return ((str[0] != '\0' && str[1] != '\0' && IS_BIG5(str[0], str[1])) ?
            2 :
            1);
}

#define FC_RIGHT (0)
#define FC_LEFT  (1)

/* Return the cursor position aligned to the beginning of a character.
 * If `pos' is in the middle of a character, the alignment determines on `dir':
 *     FC_LEFT: aligned to the current character.
 *     FC_RIGHT: aligned to the next character.
 */
static int
fix_cursor(char *str, int pos, int dir)
{
    int newpos = 0, w = 0;
    assert(dir == FC_RIGHT || dir == FC_LEFT);
    assert(pos >= 0);

    while (*str != '\0' && newpos < pos) {
	w = mchar_len((unsigned char *) str);
	str += w;
	newpos += w;
    }
    if (dir == FC_LEFT && newpos > pos)
	newpos -= w;

    return newpos;
}

#endif

/* �O����޲z�P�s��B�z */
static inline void
edit_buffer_constructor(editor_internal_t *buf)
{
    /* all unspecified columns are 0 */
    buf->blockln = -1;
    buf->insert_mode = 1;
    buf->redraw_everything = 1;
    buf->lastindent = -1;

    buf->oldcurrline = buf->currline = buf->top_of_win =
	buf->firstline = buf->lastline = alloc_line(WRAPMARGIN);

}

static inline void
enter_edit_buffer(void)
{
    editor_internal_t *p = curr_buf;
    curr_buf = (editor_internal_t *)malloc(sizeof(editor_internal_t));
    memset(curr_buf, 0, sizeof(editor_internal_t));
    curr_buf->prev = p;
    edit_buffer_constructor(curr_buf);
}

static inline void
free_line(textline_t *p)
{
    if (p == curr_buf->oldcurrline)
	curr_buf->oldcurrline = NULL;
    p->next = (textline_t*)0x12345678;
    p->prev = (textline_t*)0x87654321;
    p->len = -12345;
    free(p);
}

static inline void
edit_buffer_destructor(void)
{
    textline_t *p, *pnext;
    for (p = curr_buf->firstline; p; p = pnext) {
	pnext = p->next;
	free_line(p);
    }
    if (curr_buf->deleted_line != NULL)
	free_line(curr_buf->deleted_line);

    if (curr_buf->searched_string != NULL)
	free(curr_buf->searched_string);
    if (curr_buf->sitesig_string != NULL)
	free(curr_buf->sitesig_string);
}

static inline void
exit_edit_buffer(void)
{
    editor_internal_t *p = curr_buf;

    edit_buffer_destructor();
    curr_buf = p->prev;
    free(p);
}

/**
 * transform position ansix in an ansi string of textline_t to the same
 * string without escape code.
 * @return position in the string without escape code.
 */
static int
ansi2n(int ansix, textline_t * line)
{
    char  *data, *tmp;
    char   ch;

    data = tmp = line->data;

    while (*tmp) {
	if (*tmp == KEY_ESC) {
	    while ((ch = *tmp) && !isalpha((int)ch))
		tmp++;
	    if (ch)
		tmp++;
	    continue;
	}
	if (ansix <= 0)
	    break;
	tmp++;
	ansix--;
    }
    return tmp - data;
}

/**
 * opposite to ansi2n, according to given textline_t.
 * @return position in the string with escape code.
 */
static short
n2ansi(short nx, textline_t * line)
{
    short  ansix = 0;
    char  *tmp, *nxp;
    char   ch;

    tmp = nxp = line->data;
    nxp += nx;

    while (*tmp) {
	if (*tmp == KEY_ESC) {
	    while ((ch = *tmp) && !isalpha((int)ch))
		tmp++;
	    if (ch)
		tmp++;
	    continue;
	}
	if (tmp >= nxp)
	    break;
	tmp++;
	ansix++;
    }
    return ansix;
}

/* �ù��B�z�G���U�T���B��ܽs�褺�e */

static inline void
show_phone_mode_panel(void)
{
    int i;

    move(b_lines - 1, 0);
    clrtoeol();

    if (curr_buf->last_phone_mode < 20) {
	int len;
	prints(ANSI_COLOR(1;46) "�i%s��J�j ", BIG_mode[curr_buf->last_phone_mode - 1]);
	len = strlen(BIG5[curr_buf->last_phone_mode - 1]) / 2;
	for (i = 0; i < len; i++)
	    prints(ANSI_COLOR(37) "%c" ANSI_COLOR(34) "%2.2s",
		    i + 'A', BIG5[curr_buf->last_phone_mode - 1] + i * 2);
	for (i = 0; i < 16 - len; i++)
	    outs("   ");
	outs(ANSI_COLOR(37) " `1~9-=���� Z����" ANSI_RESET);
    }
    else {
	prints(ANSI_COLOR(1;46) "�i����ø�s�j /=%s *=%s��   ",
		table_mode[(curr_buf->last_phone_mode - 20) / 4],
		table_mode[(curr_buf->last_phone_mode - 20) % 4 + 2]);
	for (i = 0;i < 11;i++)
	    prints(ANSI_COLOR(37) "%c" ANSI_COLOR(34) "%2.2s", i ? i + '/' : '.',
		    table[curr_buf->last_phone_mode - 20] + i * 2);
	outs(ANSI_COLOR(37) "          Z���X " ANSI_RESET);
    }
}

/**
 * Show the bottom status/help bar, and BIG5/table in phone_mode.
 */
static void
edit_msg(void)
{
    char buf[STRLEN];
    int n = curr_buf->currpnt;

    if (curr_buf->ansimode)		/* Thor: �@ ansi �s�� */
	n = n2ansi(n, curr_buf->currline);

    if (curr_buf->phone_mode)
	show_phone_mode_panel();

    snprintf(buf, sizeof(buf),
	    " (^Z/F1)���� (^P/^G)���J�Ÿ�/�d�� (^X/^Q)���}\t"
	    "��%s�x%c%c%c%c��%3d:%3d",
	    curr_buf->insert_mode ? "���J" : "���N",
	    curr_buf->ansimode ? 'A' : 'a',
	    curr_buf->indent_mode ? 'I' : 'i',
	    curr_buf->phone_mode ? 'P' : 'p',
	    curr_buf->raw_mode ? 'R' : 'r',
	    curr_buf->currln + 1, n + 1);

    vs_footer(" �s��峹 ", buf);
}

static const char *
get_edit_kind_prompt(int flags)
{
    int has_reply_post = (flags & EDITFLAG_KIND_REPLYPOST),
	has_reply_mail = (flags & EDITFLAG_KIND_SENDMAIL),
	has_new_post   = (flags & EDITFLAG_KIND_NEWPOST);

    if (has_reply_post && has_reply_mail)
	return ANSI_COLOR(0;1;37;45)
	    "�`�N�G�z�Y�N �^�H����@�� �æP�� �^�Ф峹�ܬݪO�W" ANSI_RESET;
    if (has_reply_mail)
	return ANSI_COLOR(0;1;31) "�`�N�G�z�Y�N �H�X �p�H�H��" ANSI_RESET;
    if (has_reply_post)
	return ANSI_COLOR(0;1;33) "�`�N�G�z�Y�N �^�� �峹�ܬݪO�W" ANSI_RESET;
    if (has_new_post)
	return ANSI_COLOR(0;1;32) "�`�N�G�z�Y�N �o�� �s�峹�ܬݪO�W" ANSI_RESET;
    return NULL;
}

static const char *
get_edit_warn_prompt(int flags) {
    int no_self_sel    = (flags & EDITFLAG_WARN_NOSELFDEL);

    // FIXME not always on boards, maybe
    if (no_self_sel)
        return ANSI_COLOR(0;1;31)
            "�`�N: ���ݪO�T��ۧR�峹�A�o�X��N�u���O�D�H�W�~��R��!!!" ANSI_RESET;

    return NULL;
}

//#define SLOW_CHECK_DETAIL
static void
edit_buffer_check_healthy(textline_t *line)
{
    assert(line);

    if (line->next)
	assert(line->next->prev == line);
    if (line->prev)
	assert(line->prev->next == line);
    assert(0 <= line->len);
    assert(line->len <= WRAPMARGIN);
#ifdef DEBUG
    assert(line->len <= line->mlength);
#endif
#ifdef SLOW_CHECK_DETAIL
    assert(strlen(line->data) == line->len);
#endif
}

static inline int visible_window_height(void);

static void
edit_check_healthy()
{
#ifdef SLOW_CHECK_DETAIL
    int i;
    textline_t *p;
#endif

    edit_buffer_check_healthy(curr_buf->firstline);
    assert(curr_buf->firstline->prev == NULL);

    edit_buffer_check_healthy(curr_buf->lastline);
    assert(curr_buf->lastline->next == NULL);

    if (curr_buf->oldcurrline)
	edit_buffer_check_healthy(curr_buf->oldcurrline);

    // currline
    edit_buffer_check_healthy(curr_buf->currline);
    assert(0 <= curr_buf->currpnt && curr_buf->currpnt <= curr_buf->currline->len);

    if (curr_buf->deleted_line) {
	edit_buffer_check_healthy(curr_buf->deleted_line);
	assert(curr_buf->deleted_line->next == NULL);
	assert(curr_buf->deleted_line->prev == NULL);
	assert(curr_buf->deleted_line != curr_buf->firstline);
	assert(curr_buf->deleted_line != curr_buf->lastline);
	assert(curr_buf->deleted_line != curr_buf->currline);
	assert(curr_buf->deleted_line != curr_buf->blockline);
	assert(curr_buf->deleted_line != curr_buf->top_of_win);
    }

    // lines
    assert(0 <= curr_buf->currln);
    assert(curr_buf->currln <= curr_buf->totaln);

    // window
    assert(curr_buf->curr_window_line < visible_window_height());

#ifdef SLOW_CHECK_DETAIL
    // firstline -> currline (0 -> currln)
    p = curr_buf->firstline;
    for (i = 0; i < curr_buf->currln; i++) {
	edit_buffer_check_healthy(p);
	p = p->next;
    }
    assert(p == curr_buf->currline);

    // currline -> lastline (currln -> totaln)
    p = curr_buf->currline;
    for (i = 0; i < curr_buf->totaln - curr_buf->currln; i++) {
	edit_buffer_check_healthy(p);
	p = p->next;
    }
    assert(p == curr_buf->lastline);

    // top_of_win -> currline (curr_window_line)
    p = curr_buf->top_of_win;
    for (i = 0; i < curr_buf->curr_window_line; i++)
	p = p->next;
    assert(p == curr_buf->currline);

#endif

    // block
    assert(curr_buf->blockln < 0 || curr_buf->blockline);
    if (curr_buf->blockline) {
	edit_buffer_check_healthy(curr_buf->blockline);
#ifdef SLOW_CHECK_DETAIL
	p = curr_buf->firstline;
	for (i = 0; i < curr_buf->blockln; i++)
	    p = p->next;
	assert(p == curr_buf->blockline);
#endif
    }
}

/**
 * return the middle line of the window.
 */
static inline int
middle_line(void)
{
    return p_lines / 2 + 1;
}

/**
 * Return the previous 'num' line.  Stop at the first line if there's
 * not enough lines.
 */
static textline_t *
back_line(textline_t * pos, int num, bool changeln)
{
    while (num-- > 0) {
	textline_t *item;

	if (pos && (item = pos->prev)) {
	    pos = item;
	    if (changeln)
		curr_buf->currln--;
	}
	else
	    break;
    }
    return pos;
}

static inline int
visible_window_height(void)
{
    if (curr_buf->phone_mode)
	return b_lines - 1;
    else
	return b_lines;
}

/**
 * Return the next 'num' line.  Stop at the last line if there's not
 * enough lines.
 */
static textline_t *
forward_line(textline_t * pos, int num, bool changeln)
{
    while (num-- > 0) {
	textline_t *item;

	if (pos && (item = pos->next)) {
	    pos = item;
	    if (changeln)
		curr_buf->currln++;
	}
	else
	    break;
    }
    return pos;
}

/**
 * move the cursor to the next line with ansimode fixed.
 */
static inline void
cursor_to_next_line(void)
{
    short pos;

    if (curr_buf->currline->next == NULL)
	return;

    curr_buf->currline = curr_buf->currline->next;
    curr_buf->curr_window_line++;
    curr_buf->currln++;

    if (curr_buf->ansimode) {
	pos = n2ansi(curr_buf->currpnt, curr_buf->currline->prev);
	curr_buf->currpnt = ansi2n(pos, curr_buf->currline);
    }
    else {
	curr_buf->currpnt = (curr_buf->currline->len > curr_buf->lastindent)
	    ? curr_buf->lastindent : curr_buf->currline->len;
    }
}

/**
 * opposite to cursor_to_next_line.
 */
static inline void
cursor_to_prev_line(void)
{
    short pos;

    if (curr_buf->currline->prev == NULL)
	return;

    curr_buf->curr_window_line--;
    curr_buf->currln--;
    curr_buf->currline = curr_buf->currline->prev;

    if (curr_buf->ansimode) {
	pos = n2ansi(curr_buf->currpnt, curr_buf->currline->next);
	curr_buf->currpnt = ansi2n(pos, curr_buf->currline);
    }
    else {
	curr_buf->currpnt = (curr_buf->currline->len > curr_buf->lastindent)
	    ? curr_buf->lastindent : curr_buf->currline->len;
    }
}

static inline void
edit_window_adjust(void)
{
    int offset = 0;
    if (curr_buf->curr_window_line < 0) {
	offset = curr_buf->curr_window_line;
	curr_buf->curr_window_line = 0;
	curr_buf->top_of_win = curr_buf->currline;
    }

    if (curr_buf->curr_window_line >= visible_window_height()) {
	offset = curr_buf->curr_window_line - visible_window_height() + 1;
	curr_buf->curr_window_line = visible_window_height() - 1;
	curr_buf->top_of_win = back_line(curr_buf->currline, visible_window_height() - 1, false);
    }

    if (offset == -1)
	rscroll();
    else if (offset == 1) {
	move(visible_window_height(), 0);
	clrtoeol();
	scroll();
    } else if (offset != 0) {
	curr_buf->redraw_everything = YEA;
    }
}

static inline void
edit_window_adjust_middle(void)
{
    if (curr_buf->currln < middle_line()) {
	curr_buf->top_of_win = curr_buf->firstline;
	curr_buf->curr_window_line = curr_buf->currln;
    } else {
	int i;
	textline_t *p = curr_buf->currline;
	curr_buf->curr_window_line = middle_line();
	for (i = curr_buf->curr_window_line; i; i--)
	    p = p->prev;
	curr_buf->top_of_win = p;
    }
    curr_buf->redraw_everything = YEA;
}

/**
 * Get the current line number in the window now.
 */
static int
get_lineno_in_window(void)
{
    int             cnt = 0;
    textline_t     *p = curr_buf->currline;

    while (p && (p != curr_buf->top_of_win)) {
	cnt++;
	p = p->prev;
    }
    return cnt;
}

/**
 * shift given raw data s with length len to left by one byte.
 */
static void
raw_shift_left(char *s, int len)
{
    int i;
    for (i = 0; i < len && s[i] != 0; ++i)
	s[i] = s[i + 1];
}

/**
 * shift given raw data s with length len to right by one byte.
 */
static void
raw_shift_right(char *s, int len)
{
    int i;
    for (i = len - 1; i >= 0; --i)
	s[i + 1] = s[i];
}

/**
 * Return the pointer to the next non-space position.
 */
static char *
next_non_space_char(char *s)
{
    while (*s == ' ')
	s++;
    return s;
}

/**
 * allocate a textline_t with length length.
 */
static textline_t *
alloc_line(short length)
{
    textline_t *p;

    if ((p = (textline_t *) malloc(length + sizeof(textline_t)))) {
	memset(p, 0, length + sizeof(textline_t));
#ifdef DEBUG
	p->mlength = length;
#endif
	return p;
    }
    assert(p);
    abort_bbs(0);
    return NULL;
}

/**
 * clone a textline_t
 */
static textline_t *
clone_line(const textline_t *line)
{
    textline_t *p;

    p = alloc_line(line->len);
    p->len = line->len;
    strlcpy(p->data, line->data, p->len + 1);

    return p;
}

/**
 * Insert p after line in list. Keeps up with last line
 */
static void
insert_line(textline_t *line, textline_t *p)
{
    textline_t *n;

    if ((p->next = n = line->next))
	n->prev = p;
    else
	curr_buf->lastline = p;
    line->next = p;
    p->prev = line;
}

/**
 * delete_line deletes 'line' from the line list.
 * @param saved  true if you want to keep the line in deleted_line
 */
static void
delete_line(textline_t * line, int saved)
{
    textline_t *p = line->prev;
    textline_t *n = line->next;

    if (!p && !n) {
	line->data[0] = line->len = 0;
	return;
    }
    assert(line != curr_buf->top_of_win);
    if (n)
	n->prev = p;
    else
	curr_buf->lastline = p;
    if (p)
	p->next = n;
    else
	curr_buf->firstline = n;

    curr_buf->totaln--;

    if (saved) {
	if  (curr_buf->deleted_line != NULL)
	    free_line(curr_buf->deleted_line);
	curr_buf->deleted_line = line;
	curr_buf->deleted_line->next = NULL;
	curr_buf->deleted_line->prev = NULL;
	if (line == curr_buf->oldcurrline)
	    curr_buf->oldcurrline = NULL;
    }
    else {
	free_line(line);
    }
}

/**
 * Return the indent space number according to CURRENT line and the FORMER
 * line. It'll be the first line contains non-space character.
 * @return space number from the beginning to the first non-space character,
 *         return 0 if non or not in indent mode.
 */
static int
indent_space(void)
{
    textline_t     *p;
    int             spcs;

    if (!curr_buf->indent_mode)
	return 0;

    for (p = curr_buf->currline; p; p = p->prev) {
	for (spcs = 0; p->data[spcs] == ' '; ++spcs);
	    /* empty loop */
	if (p->data[spcs])
	    return spcs;
    }
    return 0;
}

/**
 * adjustline(oldp, len);
 * �ΨӱN oldp ���쪺���@��, ���s�ץ��� len�o���.
 *
 * In FreeBSD:
 * �b�o��@�@���F�⦸�� memcpy() , �Ĥ@���q heap ���� stack ,
 * ���ӰO���� free() ��, �S���s�b stack�W malloc() �@��,
 * �M��A�����^��.
 * �D�n�O�� sbrk() �[��쪺���G, �o�ˤl�~�u�����Y��O����ζq.
 * �Ԩ� /usr/share/doc/papers/malloc.ascii.gz (in FreeBSD)
 */
static textline_t *
adjustline(textline_t *oldp, short len)
{
    // XXX write a generic version ?
    char tmpl[sizeof(textline_t) + WRAPMARGIN];
    textline_t *newp;

    assert(0 <= oldp->len && oldp->len <= WRAPMARGIN);
    assert(oldp != curr_buf->deleted_line);

    memcpy(tmpl, oldp, oldp->len + sizeof(textline_t));
    free_line(oldp);

    newp = alloc_line(len);
    memcpy(newp, tmpl, len + sizeof(textline_t));
#ifdef DEBUG
    newp->mlength = len;
#endif
    if( oldp == curr_buf->firstline ) curr_buf->firstline = newp;
    if( oldp == curr_buf->lastline )  curr_buf->lastline  = newp;
    if( oldp == curr_buf->currline )  curr_buf->currline  = newp;
    if( oldp == curr_buf->blockline ) curr_buf->blockline = newp;
    if( oldp == curr_buf->top_of_win) curr_buf->top_of_win= newp;
    if(curr_buf->oldcurrline == NULL && len == WRAPMARGIN)
	curr_buf->oldcurrline = curr_buf->currline;
    if( newp->prev != NULL ) newp->prev->next = newp;
    if( newp->next != NULL ) newp->next->prev = newp;
    //    vmsg("adjust %x to %x, length: %d", (int)oldp, (int)newp, len);
    return newp;
}

/**
 * split 'line' right before the character pos
 *
 * @return the latter line after splitting
 */
static textline_t *
split(textline_t * line, int pos, int indent)
{
    if (pos <= line->len) {
	textline_t *p = alloc_line(WRAPMARGIN);
	char  *ptr;
	int             spcs = indent;

	curr_buf->totaln++;

	p->len = line->len - pos + spcs;
	line->len = pos;

	memset(p->data, ' ', spcs);
	p->data[spcs] = 0;

	ptr = line->data + pos;
	if (curr_buf->indent_mode) {
	    ptr = next_non_space_char(ptr);
	    p->len = strlen(ptr) + spcs;
	}
	strcat(p->data + spcs, ptr);
	line->data[line->len] = '\0';

	if (line == curr_buf->currline && pos <= curr_buf->currpnt) {
	    line = adjustline(line, line->len);
	    insert_line(line, p);
	    curr_buf->currline = p;
	    if (pos == curr_buf->currpnt)
		curr_buf->currpnt = spcs;
	    else {
		curr_buf->currpnt = curr_buf->currpnt - pos + spcs;
		// In indent_mode, the length may be shorter.
		if (curr_buf->currpnt > curr_buf->currline->len)
		    curr_buf->currpnt = curr_buf->currline->len;
	    }
	    curr_buf->curr_window_line++;
	    curr_buf->currln++;

	    /* split may cause cursor hit bottom */
	    edit_window_adjust();
	} else {
	    p = adjustline(p, p->len);
	    insert_line(line, p);
	}
	curr_buf->redraw_everything = YEA;
	edit_buffer_check_healthy(line);
	edit_buffer_check_healthy(line->next);
    }
#ifdef DEBUG
    assert(curr_buf->currline->mlength == WRAPMARGIN);
#endif
    return line;
}

/**
 * Insert a character ch to current line.
 *
 * The line will be split if the length is >= WRAPMARGIN.  It'll be split
 * from the last space if any, or start a new line after the last character.
 */
static void
insert_char(int ch)
{
    textline_t *p = curr_buf->currline;
    char  *s;
    int             wordwrap = YEA;

    assert(curr_buf->currpnt <= p->len);
#ifdef DEBUG
    assert(curr_buf->currline->mlength == WRAPMARGIN);
#endif

    block_cancel();
    if (curr_buf->currpnt < p->len && !curr_buf->insert_mode) {
	p->data[curr_buf->currpnt++] = ch;
	/* Thor: ansi �s��, �i�Hoverwrite, ���\�� ansi code */
	if (curr_buf->ansimode)
	    curr_buf->currpnt = ansi2n(n2ansi(curr_buf->currpnt, p), p);
    } else {
#ifdef DEBUG
	assert(p->len < p->mlength);
#endif
	raw_shift_right(p->data + curr_buf->currpnt, p->len - curr_buf->currpnt + 1);
	p->data[curr_buf->currpnt++] = ch;
	++(p->len);
    }
    if (p->len < WRAPMARGIN)
	return;

    s = p->data + (p->len - 1);
    while (s != p->data && *s == ' ')
	s--;
    while (s != p->data && *s != ' ')
	s--;
    if (s == p->data) {
	wordwrap = NA;
	s = p->data + (p->len - 2);
    }

    p = split(p, (s - p->data) + 1, 0);

    p = p->next;
    if (wordwrap && p->len >= 1) {
	if (p != curr_buf->currline)
	    p = adjustline(p, p->len + 1);
#ifdef DEBUG
	assert(p->len < p->mlength);
#endif
	if (p->data[p->len - 1] != ' ') {
	    p->data[p->len] = ' ';
	    p->data[p->len + 1] = '\0';
	    p->len++;
	}
    }
#ifdef DEBUG
    assert(curr_buf->currline->mlength == WRAPMARGIN);
#endif
}

/**
 * insert_char twice.
 */
static void
insert_dchar(const char *dchar)
{
    insert_char(*dchar);
    insert_char(*(dchar+1));
}

static void
insert_tab(void)
{
    do {
	insert_char(' ');
	edit_buffer_check_healthy(curr_buf->currline);
    } while (curr_buf->currpnt & 0x7);
}

/**
 * Insert a string.
 *
 * All printable and ESC_CHR will be directly printed out.
 * '\t' will be printed to align every 8 byte.
 * '\n' will split the line.
 * The other character will be ignore.
 */
static void
insert_string(const char *str)
{
    char ch;

    block_cancel();
    while ((ch = *str++)) {
	if (isprint2(ch) || ch == ESC_CHR)
	    insert_char(ch);
	else if (ch == '\t')
	    insert_tab();
	else if (ch == '\n')
	    split(curr_buf->currline, curr_buf->currpnt, 0);
    }
}

/**
 * undelete the deleted line.
 *
 * return NULL if there's no deleted_line, otherwise, return currline.
 */
static textline_t *
undelete_line(void)
{
    textline_t *p;

    if (!curr_buf->deleted_line)
	return NULL;

    block_cancel();
    p = clone_line(curr_buf->deleted_line);

    // insert in front of currline
    p->prev = curr_buf->currline->prev;
    p->next = curr_buf->currline->next;
    if (curr_buf->currline->prev)
	curr_buf->currline->prev->next = p;
    curr_buf->currline->prev = p;
    curr_buf->totaln++;

    curr_buf->currline = adjustline(curr_buf->currline, curr_buf->currline->len);

    // maintain special line pointer
    if (curr_buf->top_of_win == curr_buf->currline)
	curr_buf->top_of_win = p;
    if (curr_buf->firstline == curr_buf->currline)
	curr_buf->firstline = p;

    // change currline
    curr_buf->currline = p;
    curr_buf->currline = adjustline(curr_buf->currline, WRAPMARGIN);
    curr_buf->currpnt = 0;

    curr_buf->redraw_everything = YEA;

    return curr_buf->currline;
}

/*
 * join $line and $line->next
 *
 * line: A1 A2
 * next: B1 B2
 * ....: C1 C2
 *
 * case B=empty:
 *	delete_line B
 * 	return YEA
 *
 * case A+B < WRAPMARGIN:
 * 	line: A1 A2 B1 B2
 * 	next: C1 C2
 * 	return YEA
 * 	NOTE It assumes $line has allocated WRAPMARGIN length of data buffer.
 *
 * case A+B1+B2 > WRAPMARGIN, A+B1<WRAPMARGIN
 * 	line: A1 A2 B1
 * 	next: B2
 */
static int
join(textline_t * line)
{
    textline_t *n;
    int    ovfl;

    if (!(n = line->next))
	return YEA;
    if (!*next_non_space_char(n->data)) {
	delete_line(n, 0);
	return YEA;
    }


    ovfl = line->len + n->len - WRAPMARGIN;
    if (ovfl < 0) {
	strcat(line->data, n->data);
	line->len += n->len;
	delete_line(n, 0);
	return YEA;
    } else {
	char  *s; /* the split point */

	s = n->data + n->len - ovfl - 1;
	while (s != n->data && *s == ' ')
	    s--;
	while (s != n->data && *s != ' ')
	    s--;
	if (s == n->data) {
	    // TODO don't give up
	    return YEA;
	}
	split(n, (s - n->data) + 1, 0);
	assert(line->len + line->next->len < WRAPMARGIN);
	join(line);
	return NA;
    }
}

static void
delete_char(void)
{
    int    len;

    if ((len = curr_buf->currline->len)) {
	assert(curr_buf->currpnt < len);

	raw_shift_left(curr_buf->currline->data + curr_buf->currpnt, curr_buf->currline->len - curr_buf->currpnt + 1);
	curr_buf->currline->len--;
    }
}

static void
load_file(FILE * fp, off_t offSig)
{
    char buf[WRAPMARGIN + 2];
    int indent_mode0 = curr_buf->indent_mode;
    size_t szread = 0;

    assert(fp);
    curr_buf->indent_mode = 0;
    while (fgets(buf, sizeof(buf), fp))
    {
	szread += strlen(buf);
	if (offSig < 0 || szread <= (size_t)offSig)
	{
	    insert_string(buf);
	}
	else
	{
	    // this is the site sig
	    break;
	}
    }
    curr_buf->indent_mode = indent_mode0;
}

/* �Ȧs�� */
const char           *
ask_tmpbuf(int y)
{
    static char     fp_buf[] = "buf.0";
    char     msg[] = "�п�ܼȦs�� (0-9)[0]: ";
    char choice[2];

    msg[19] = fp_buf[4];
    do {
	if (!getdata(y, 0, msg, choice, sizeof(choice), DOECHO))
	    choice[0] = fp_buf[4];
    } while (choice[0] < '0' || choice[0] > '9');
    fp_buf[4] = choice[0];
    return fp_buf;
}

static void
read_tmpbuf(int n)
{
    FILE           *fp;
    char            fp_tmpbuf[80];
    char            tmpfname[] = "buf.0";
    const char     *tmpf;
    char            ans[4] = "y";

    if (curr_buf->totaln >= EDIT_LINE_LIMIT)
    {
	vmsg("�ɮפw�W�L�̤j����A�L�k�AŪ�J�Ȧs�ɡC");
	return;
    }

    if (0 <= n && n <= 9) {
	tmpfname[4] = '0' + n;
	tmpf = tmpfname;
    } else {
	tmpf = ask_tmpbuf(3);
	n = tmpf[4] - '0';
    }

    setuserfile(fp_tmpbuf, tmpf);
    if (n != 0 && n != 5 && more(fp_tmpbuf, NA) != -1)
	getdata(b_lines - 1, 0, "�T�wŪ�J��(Y/N)?[Y]", ans, sizeof(ans), LCECHO);
    if (*ans != 'n' && (fp = fopen(fp_tmpbuf, "r"))) {
	load_file(fp, -1);
	fclose(fp);
    }
}

static void
write_tmpbuf(void)
{
    FILE           *fp;
    char            fp_tmpbuf[80], ans[4];
    textline_t     *p;
    off_t	    sz = 0;

    setuserfile(fp_tmpbuf, ask_tmpbuf(3));
    if (dashf(fp_tmpbuf)) {
	more(fp_tmpbuf, NA);
	getdata(b_lines - 1, 0, "�Ȧs�ɤw����� (A)���[ (W)�мg (Q)�����H[A] ",
		ans, sizeof(ans), LCECHO);

	if (ans[0] == 'q')
	    return;
    }
    if (ans[0] != 'w') // 'a'
    {
	sz = dashs(fp_tmpbuf);
	if (sz > EDIT_SIZE_LIMIT)
	{
	    vmsg("�Ȧs�ɤw�W�L�j�p����A�L�k�A���[�C");
	    return;
	}
    }
    if ((fp = fopen(fp_tmpbuf, (ans[0] == 'w' ? "w" : "a+")))) {
	for (p = curr_buf->firstline; p; p = p->next) {
	    if (p->next || p->data[0])
		fprintf(fp, "%s\n", p->data);
	}
	fclose(fp);
    }
}

static void
erase_tmpbuf(void)
{
    char            fp_tmpbuf[80];
    char            ans[4] = "n";

    setuserfile(fp_tmpbuf, ask_tmpbuf(3));
    if (more(fp_tmpbuf, NA) != -1)
	getdata(b_lines - 1, 0, "�T�w�R����(Y/N)?[N]",
		ans, sizeof(ans), LCECHO);
    if (*ans == 'y')
	unlink(fp_tmpbuf);
}

/**
 * �s�边�۰ʳƥ�
 *(�̦h�ƥ� 512 �� (?))
 */
void
auto_backup(void)
{
    if (curr_buf == NULL)
	return;

    if (curr_buf->currline) {
	FILE           *fp;
	textline_t     *p, *v;
	char            bakfile[PATHLEN];
	int             count = 0;

	curr_buf->currline = NULL;

	setuserfile(bakfile, fp_bak);
	if ((fp = fopen(bakfile, "w"))) {
	    for (p = curr_buf->firstline; p != NULL && count < 512; p = v, count++) {
		v = p->next;
		fprintf(fp, "%s\n", p->data);
		free_line(p);
	    }
	    fclose(fp);
	}
    }
}

/**
 * ���^�s�边�ƥ�
 */
void
restore_backup(void)
{
    char            bakfile[80], buf[80], ans[3];

    setuserfile(bakfile, fp_bak);
    while (dashf(bakfile)) {
	vs_hdr("�s�边�۰ʴ_��");
        mvouts(3, 0, "== �H�U�����������峹�������e ==\n");
        show_file(bakfile, 4, 15, SHOWFILE_ALLOW_NONE);
	getdata(1, 0, "�z���@�g�峹�|�������A(S)�g�J�Ȧs�� (Q)��F�H[S] ",
		buf, 4, LCECHO);
        if (*buf == 'q') {
            unlink(bakfile);
            break;
        }
        setuserfile(buf, ask_tmpbuf(2));
        if (dashs(buf) > 0) {
            vs_hdr("�Ȧs�ɤw�����e");
            mvouts(2, 0, "== �H�U���Ȧs�ɳ������e ==\n");
            show_file(buf, 3, 18, SHOWFILE_ALLOW_NONE);
            getdata(1, 0, "��w�Ȧs�ɤw���U�C���e�A�T�w�n�л\\�����H [y/N] ",
                    ans, sizeof(ans), LCECHO);
            if (*ans != 'y')
                continue;
        }
        Rename(bakfile, buf);
    }
}

/* �ޥΤ峹 */

static int
garbage_line(const char *str)
{
    int             qlevel = 0;

    while (*str == ':' || *str == '>') {
	if (*(++str) == ' ')
	    str++;
	if (qlevel++ >= 1)
	    return 1;
    }
    while (*str == ' ' || *str == '\t')
	str++;
    if (qlevel >= 1) {
	if (!strncmp(str, "�� ", 3) || !strncmp(str, "==>", 3) ||
	    strstr(str, ") ����:\n"))
	    return 1;
    }
    return (*str == '\n');
}

static void
quote_strip_ansi_inline(unsigned char *is)
{
    unsigned char *os = is;

    while (*is)
    {
	if(*is != ESC_CHR)
	    *os++ = *is;
	else
	{
	    is ++;
	    if(*is == '*')
	    {
		/* ptt prints, keep it as normal */
		*os++ = '*';
		*os++ = '*';
	    }
	    else
	    {
		/* normal ansi, strip them out. */
		while (*is && ANSI_IN_ESCAPE(*is))
		    is++;
	    }
	}
	is++;

    }

    *os = 0;
}

static void
do_quote(void)
{
    int             op;
    char            buf[512];

    getdata(b_lines - 1, 0, "�аݭn�ޥέ���(Y/N/All/Repost)�H[Y] ",
	    buf, 3, LCECHO);
    op = buf[0];

    if (op != 'n') {
	FILE           *inf;

	if ((inf = fopen(quote_file, "r"))) {
	    char           *ptr;
	    int             indent_mode0 = curr_buf->indent_mode;

	    fgets(buf, sizeof(buf), inf);
	    if ((ptr = strrchr(buf, ')')))
		ptr[1] = '\0';
	    else if ((ptr = strrchr(buf, '\n')))
		ptr[0] = '\0';

	    if ((ptr = strchr(buf, ':'))) {
		char           *str;

		while (*(++ptr) == ' ');

		/* ����o�ϡA���o author's address */
		if ((curr_buf->flags & EDITFLAG_KIND_SENDMAIL) &&
                    (curr_buf->flags & EDITFLAG_KIND_REPLYPOST) &&
                    (str = strchr(quote_user, '.'))) {
		    strcpy(++str, ptr);
		    str = strchr(str, ' ');
		    assert(str);
		    str[0] = '\0';
		}
	    } else
		ptr = quote_user;

	    curr_buf->indent_mode = 0;
	    insert_string("�� �ޭz�m");
	    insert_string(ptr);
	    insert_string("�n���ʨ��G\n");

	    if (op != 'a')	/* �h�� header */
		while (fgets(buf, sizeof(buf), inf) && buf[0] != '\n');
	    /* FIXME by MH:
	         �p�G header �줺�夤���S���Ŧ���j�A�|�y�� All �H�~���Ҧ�
	         ���ޤ��줺��C
	     */

	    if (op == 'a')
		while (fgets(buf, sizeof(buf), inf)) {
		    insert_char(':');
		    insert_char(' ');
		    quote_strip_ansi_inline((unsigned char *)buf);
		    insert_string(buf);
		}
	    else if (op == 'r')
		while (fgets(buf, sizeof(buf), inf)) {
		    /* repost, keep anything */
		    // quote_strip_ansi_inline((unsigned char *)buf);
		    insert_string(buf);
		}
	    else {
                /* �h�� mail list �� header */
		if (curr_buf->flags & EDITFLAG_KIND_MAILLIST)
		    while (fgets(buf, sizeof(buf), inf) && (!strncmp(buf, "�� ", 3)));
		while (fgets(buf, sizeof(buf), inf)) {
		    if (!strcmp(buf, "--\n"))
			break;
		    if (!garbage_line(buf)) {
			insert_char(':');
			insert_char(' ');
			quote_strip_ansi_inline((unsigned char *)buf);
			insert_string(buf);
		    }
		}
	    }
	    curr_buf->indent_mode = indent_mode0;
	    fclose(inf);
	}
    }
}

/**
 * �f�d user �ި����ϥ�
 */
static int
check_quote(void)
{
    textline_t *p = curr_buf->firstline;
    char  *str;
    int             post_line;
    int             included_line;

    post_line = included_line = 0;
    while (p) {
	if (!strcmp(str = p->data, "--"))
	    break;
	if (str[1] == ' ' && ((str[0] == ':') || (str[0] == '>')))
	    included_line++;
	else {
	    while (*str == ' ' || *str == '\t')
		str++;
	    if (*str)
		post_line++;
	}
	p = p->next;
    }

    if ((included_line >> 2) > post_line) {
	move(4, 0);
	outs("���g�峹���ި���ҶW�L 80%�A�бz���ǷL���ץ��G\n\n"
	     ANSI_COLOR(1;33) "1) �W�[�@�Ǥ峹 ��  2) �R�������n���ި�"
             ANSI_RESET "\n");
	{
	    char            ans[4];

	    getdata(12, 12, "(E)�~��s�� (W)�j��g�J�H[E] ",
		    ans, sizeof(ans), LCECHO);
	    if (ans[0] == 'w')
		return 0;
	}
	return 1;
    }
    return 0;
}

/* �ɮ׳B�z�GŪ�ɡB�s�ɡB���D�Bñ�W�� */
off_t loadsitesig(const char *fname);

static int
read_file(const char *fpath, int splitSig)
{
    FILE  *fp;
    off_t offSig = -1;

    if (splitSig)
	offSig = loadsitesig(fpath);

    if ((fp = fopen(fpath, "r")) == NULL) {
	int fd;
	if ((fd = creat(fpath, DEFAULT_FILE_CREATE_PERM)) >= 0) {
	    close(fd);
	    return 0;
	}

	return -1;
    }
    load_file(fp, offSig);
    fclose(fp);

    return 0;
}

void
write_header(FILE * fp,  const char *mytitle)
{
    assert(mytitle);
    // cross_post may call this without setting curr_buf.
    // TODO Isolate curr_buf so we don't need to hack around.
    if (curr_buf &&
        curr_buf->flags & (EDITFLAG_KIND_MAILLIST | EDITFLAG_KIND_SENDMAIL)) {
	fprintf(fp, "%s %s (%s)\n", str_author1, cuser.userid,
		cuser.nickname
	);
    } else {
	const char *ptr = mytitle;
        const char *nickname = cuser.nickname;
	struct {
	    char            author[IDLEN + 1];
	    char            board[IDLEN + 1];
	    char            title[66];
	    time4_t         date;	/* last post's date */
	    int             number;	/* post number */
	}               postlog;

	memset(&postlog, 0, sizeof(postlog));
	strlcpy(postlog.author, cuser.userid, sizeof(postlog.author));
	if (curr_buf)
	    curr_buf->ifuseanony = 0;
#ifdef HAVE_ANONYMOUS
	if (currbrdattr & BRD_ANONYMOUS) {
	    int defanony = (currbrdattr & BRD_DEFAULTANONYMOUS);
            char default_name[IDLEN + 1] = "";
            char ans[3];
            int use_userid = 1;

#if defined(PLAY_ANGEL)
            // dirty hack here... sorry
            if (HasUserPerm(PERM_ANGEL) && (currbrdattr & BRD_ANGELANONYMOUS)) {
                angel_load_my_fullnick(default_name, sizeof(default_name));
            }
#endif // PLAY_ANGEL

            do {
                getdata_str(3, 0, defanony ?
                    "�п�J�A�Q�Ϊ�ID�A�Ϊ�����[Enter]�ʦW�A�Ϋ�[r][R]�ίu�W�G" :
                    "�п�J�A�Q�Ϊ�ID�A�]�i������[Enter]��[r]��[R]�ϥέ�ID�G",
                    real_name, sizeof(real_name), DOECHO, default_name);

                // sanity checks
                if (real_name[0] == '-') {
                    // names with '-' prefix are considered as 'deleted'.
                    mvouts(4, 0, "��p�A�ФŨϥΥH - �}�Y���W�١C");
                    continue;
                }
                trim(real_name);
                //  defanony:  "" = Anonymous, "r" = cuser.userid.
                // !defanony:  "" "r" = cuser.userid.
                if (strcmp(real_name, "R") == 0)
                    strcpy(real_name, "r");

                if (!*real_name) {
                    if (defanony)
                        strlcpy(real_name, "Anonymous", sizeof(real_name));
                    else
                        strlcpy(real_name, "r", sizeof(real_name));
                }

                if (strcmp("r", real_name) == 0)
                    use_userid = 1;
                else
                    use_userid = 0;

                mvprints(3, 0, "�ϥΦW��: %s",
                         use_userid ? cuser.userid : real_name);
                if (getdata(4, 0, "�T�w[y/N]? ", ans, sizeof(ans), LCECHO) < 1 ||
                    ans[0] != 'y') {
                    move(4, 0); clrtobot();
                    continue;
                }
                break;
            } while (1);

            if (use_userid) {
                strlcpy(postlog.author, cuser.userid, sizeof(postlog.author));
            } else {
                snprintf(postlog.author, sizeof(postlog.author),
                         "%s.", real_name);
                nickname = "�q�q�ڬO�� ? ^o^";
                if (curr_buf)
                    curr_buf->ifuseanony = 1;
            }
	}
#endif
	strlcpy(postlog.board, currboard, sizeof(postlog.board));
        ptr = subject(ptr);
	strlcpy(postlog.title, ptr, sizeof(postlog.title));
	postlog.date = now;
	postlog.number = 1;
	append_record(".post", (fileheader_t *) &postlog, sizeof(postlog));
	fprintf(fp, "%s %s (%s) %s %s\n", str_author1, postlog.author, nickname,
		str_post1, currboard);

    }
    fprintf(fp, "���D: %s\n�ɶ�: %s\n", mytitle, ctime4(&now));
}

off_t
loadsitesig(const char *fname)
{
    int fd = 0;
    off_t sz = 0, ret = -1;
    char *start, *sp;

    sz = dashs(fname);
    if (sz < 1)
	return -1;
    fd = open(fname, O_RDONLY);
    if (fd < 0)
	return -1;
    start = (char*)mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
    if (start)
    {
	sp = start + sz - 4 - 1; // 4 = \n--\n
	while (sp > start)
	{
	    if ((*sp == '\n' && strncmp(sp, "\n--\n", 4) == 0) ||
		(*sp == '\r' && strncmp(sp, "\r--\r", 4) == 0) )
	    {
		size_t szSig = sz - (sp-start+1);
		ret = sp - start + 1;
		// allocate string
		curr_buf->sitesig_string = (char*) malloc (szSig + 1);
		if (curr_buf->sitesig_string)
		{
		    memcpy(curr_buf->sitesig_string, sp+1, szSig);
		    curr_buf->sitesig_string[szSig] = 0;
		}
		break;
	    }
	    sp --;
	}
	munmap(start, sz);
    }

    close(fd);
    return ret;
}

void
addforwardsignature(FILE *fp, const char *host) {
    char temp[STRLEN];

    if (!host && from_cc[0]) {
	snprintf(temp, sizeof(temp), "%s %s", FROMHOST, from_cc);
        host = temp;
    } else if (!host) {
        host = FROMHOST;
    }
    syncnow();
    fprintf(fp, "\n"
                "�� �o�H��: " BBSNAME "(" MYHOSTNAME ")\n"
                "�� �����: %s (%s), %s\n"
                , cuser.userid, host, Cdatelite(&now));
}

void
addsimplesignature(FILE *fp, const char *host) {
    char temp[STRLEN];

    if (!host && from_cc[0]) {
	snprintf(temp, sizeof(temp), "%s (%s)", FROMHOST, from_cc);
        host = temp;
    } else if (!host) {
        host = FROMHOST;
    }
    fprintf(fp,
            "\n--\n�� �o�H��: " BBSNAME "(" MYHOSTNAME "), �Ӧ�: %s\n", host);
}

void
addsignature(FILE * fp, int ifuseanony)
{
    FILE           *fs;
    int             i;
    int             idx_pos;
    char            buf[WRAPMARGIN + 1];
    char            fpath[STRLEN];

    char            ch;

    if (!strcmp(cuser.userid, STR_GUEST)) {
        addsimplesignature(fp, NULL);
	return;
    }
    if (!ifuseanony) {

	int browsing = 0;
	SigInfo	    si;
	memset(&si, 0, sizeof(si));

browse_sigs:
	showsignature(fpath, &idx_pos, &si);

	if (si.total > 0){
	    char msg[64];

	    ch = isdigit(cuser.signature) ? cuser.signature : 'x';
	    sprintf(msg,
		    (browsing || (si.max > si.show_max))  ?
		    "�п��ñ�W�� (1-9, 0=���[ n=½�� x=�H��)[%c]: ":
		    "�п��ñ�W�� (1-9, 0=���[ x=�H��)[%c]: ",
		    ch);
	    getdata(0, 0, msg, buf, 4, LCECHO);

	    if(buf[0] == 'n')
	    {
		si.show_start = si.show_max + 1;
		if(si.show_start > si.max)
		    si.show_start = 0;
		browsing = 1;
		goto browse_sigs;
	    }

	    if (!buf[0])
		buf[0] = ch;

	    if (isdigit((int)buf[0]))
		ch = buf[0];
	    else
		ch = '1' + random() % (si.max+1);
	    pwcuSetSignature(buf[0]);

	    if (ch != '0') {
		fpath[idx_pos] = ch;
		do
		{
		    if ((fs = fopen(fpath, "r"))) {
			fputs("\n--\n", fp);
			for (i = 0; i < MAX_SIGLINES &&
                                    fgets(buf, sizeof(buf), fs); i++) {
                            strip_ansi_movecmd(buf);
                            strip_esc_star(buf);
			    fputs(buf, fp);
                        }
			fclose(fs);
			fpath[idx_pos] = ch;
		    }
		    else
			fpath[idx_pos] = '1' + (fpath[idx_pos] - '1' + 1) % (si.max+1);
		} while (!isdigit((int)buf[0]) && si.max > 0 && ch != fpath[idx_pos]);
	    }
	}
    }
#ifdef HAVE_ORIGIN
#ifdef HAVE_ANONYMOUS
    if (ifuseanony)
        addsimplesignature(fp, "�ΦW�ѨϪ��a");
    else
#endif
    {
        addsimplesignature(fp, NULL);
    }
#endif
}

#ifdef USE_POST_ENTROPY
static int
get_string_entropy(const char *s)
{
    int ent = 0;
    while (*s)
    {
	char c = *s++;
	if (!isascii(c) || isalnum(c))
	    ent++;
    }
    return ent;
}
#endif

#ifdef EXP_EDIT_UPLOAD
static void upload_file(void);
#endif // EXP_EDIT_UPLOAD

// return	EDIT_ABORTED	if aborted
// 		KEEP_EDITING	if keep editing
// 		0		if write ok & exit
static int
write_file(const char *fpath, int saveheader, char mytitle[STRLEN],
           int flags, int *pentropy)
{
    FILE           *fp = NULL;
    textline_t     *p;
    char            ans[TTLEN], *msg;
    int             aborted = 0, line = 0;
    int             entropy = 0;

    int upload = (flags & EDITFLAG_UPLOAD) ? 1 : 0;
    int chtitle = (flags & EDITFLAG_ALLOWTITLE) ? 1 : 0;
    const char *kind_prompt = get_edit_kind_prompt(flags);
    const char *warn_prompt = get_edit_warn_prompt(flags);

    assert(!chtitle || mytitle);
    vs_hdr("�ɮ׳B�z");
    move(1,0);

#ifdef EDIT_UPLOAD_ALLOWALL
    upload = 1;
#endif // EDIT_UPLOAD_ALLOWALL

    {
        const char *msgSave = "�x�s";

        if (flags & (EDITFLAG_KIND_NEWPOST |
                     EDITFLAG_KIND_REPLYPOST)) {
            msgSave = "�o��";
        } else if (flags & (EDITFLAG_KIND_SENDMAIL |
                            EDITFLAG_KIND_MAILLIST)) {
            msgSave = "�o�H";
        }

        // common trail
        prints("[S]%s", msgSave);
    }

#ifdef EXP_EDIT_UPLOAD
    if (upload)
	outs(" (U)�W�Ǹ��");
#endif // EXP_EDIT_UPLOAD

    if (chtitle)
	outs(" (T)����D");

    outs(" (A)��� (E)�~�� (R/W/D)Ū�g�R�Ȧs��");

    // TODO FIXME what if prompt is multiline?
    if (kind_prompt)
	mvouts(4, 0, kind_prompt);
    if (warn_prompt)
	mvouts(6, 0, warn_prompt);
    getdata(2, 0, "�T�w�n�x�s�ɮ׶ܡH ", ans, 2, LCECHO);

    // avoid lots pots
    if (ans[0] != 'a')
	sleep(1);

    switch (ans[0]) {
    case 'a':
	outs("�峹" ANSI_COLOR(1) " �S�� " ANSI_RESET "�s�J");
	aborted = EDIT_ABORTED;
	break;
    case 'e':
	return KEEP_EDITING;
#ifdef EXP_EDIT_UPLOAD
    case 'u':
	if (upload)
	    upload_file();
	return KEEP_EDITING;
#endif // EXP_EDIT_UPLOAD
    case 'r':
	read_tmpbuf(-1);
	return KEEP_EDITING;
    case 'w':
	write_tmpbuf();
	return KEEP_EDITING;
    case 'd':
	erase_tmpbuf();
	return KEEP_EDITING;
    case 't':
	if (!chtitle)
	    return KEEP_EDITING;
	move(3, 0);
	prints("�¼��D�G%s", mytitle);
	strlcpy(ans, mytitle, sizeof(ans));
	if (getdata_buf(4, 0, "�s���D�G", ans, sizeof(ans), DOECHO))
	    strlcpy(mytitle, ans, STRLEN);
	return KEEP_EDITING;
    case 's':
        break;
    }

    if (!aborted) {

	if (saveheader && !(curr_buf->flags & EDITFLAG_KIND_SENDMAIL) &&
            check_quote())
	    return KEEP_EDITING;

	assert(*fpath);
	if ((fp = fopen(fpath, "w")) == NULL) {
	    assert(fp);
	    abort_bbs(0);
	}
	if (saveheader)
	    write_header(fp, mytitle);
    }
    if (!aborted) {
	for (p = curr_buf->firstline; p; p = p->next) {
	    msg = p->data;

	    if (p->next == NULL && !msg[0]) // ignore lastline is empty
		continue;

	    trim(msg);
            strip_ansi_movecmd(msg);
	    line++;

#ifdef USE_POST_ENTROPY
	    // calculate the real content of msg
	    if (entropy < ENTROPY_MAX)
		entropy += get_string_entropy(msg);
	    else
#endif
		entropy = ENTROPY_MAX;
	    // write the message body
	    fprintf(fp, "%s\n", msg);
	}
    }
    curr_buf->currline = NULL;

    *pentropy = entropy;
    if (aborted)
	return aborted;

    if (curr_buf->sitesig_string)
	fputs(curr_buf->sitesig_string, fp);

    if (currstat == POSTING || currstat == SMAIL)
    {
	addsignature(fp, curr_buf->ifuseanony);
    }
    else if (currstat == REEDIT)
    {
#ifndef ALL_REEDIT_LOG
	// why force signature in SYSOP board?
	if(strcmp(currboard, BN_SYSOP) == 0)
#endif
	{
	    fprintf(fp,
		    "�� �s��: %s (%s%s%s), %s\n",
		    cuser.userid, FROMHOST, from_cc[0] ? " " : "", from_cc,
		    Cdatelite(&now));
	}
    }

    fclose(fp);
    return 0;
}

static inline int
has_block_selection(void)
{
    return curr_buf->blockln >= 0;
}

/**
 * a block is continual lines of the article.
 */

/**
 * stop the block selection.
 */
static void
block_cancel(void)
{
    if (has_block_selection()) {
	curr_buf->blockln = -1;
	curr_buf->blockline = NULL;
	curr_buf->redraw_everything = YEA;
    }
}

static inline void
setup_block_begin_end(textline_t **begin, textline_t **end)
{
    if (curr_buf->currln >= curr_buf->blockln) {
	*begin = curr_buf->blockline;
	*end = curr_buf->currline;
    } else {
	*begin = curr_buf->currline;
	*end = curr_buf->blockline;
    }
}

#define BLOCK_TRUNCATE	0
#define BLOCK_APPEND	1
/**
 * save the selected block to file 'fname.'
 * mode: BLOCK_TRUNCATE  truncate mode
 *       BLOCK_APPEND    append mode
 */
static void
block_save_to_file(const char *fname, int mode)
{
    textline_t *begin, *end;
    char fp_tmpbuf[80];
    FILE *fp;

    if (!has_block_selection())
	return;

    setup_block_begin_end(&begin, &end);

    setuserfile(fp_tmpbuf, fname);
    if ((fp = fopen(fp_tmpbuf, mode == BLOCK_APPEND ? "a+" : "w+"))) {

	textline_t *p;

	for (p = begin; p != end; p = p->next)
	    fprintf(fp, "%s\n", p->data);
	fprintf(fp, "%s\n", end->data);
	fclose(fp);
    }
}

/**
 * delete selected block
 */
static void
block_delete(void)
{
    textline_t *begin, *end;
    textline_t *p;

    if (!has_block_selection())
	return;

    setup_block_begin_end(&begin, &end);

    // the block region is (currln, block) or (blockln, currln).

    textline_t *pnext;
    int deleted_line_count = abs(curr_buf->currln - curr_buf->blockln) + 1;
    if (begin->prev)
	begin->prev->next = end->next;
    if (end->next)
	end->next->prev = begin->prev;

    if (curr_buf->currln > curr_buf->blockln) {
	// case (blockln, currln)
	curr_buf->currln = curr_buf->blockln;
	curr_buf->curr_window_line -= deleted_line_count - 1;
    } else {
	// case (currln, blockln)
    }
    curr_buf->totaln -= deleted_line_count;

    curr_buf->currline = end->next;
    if (curr_buf->currline == NULL) {
	curr_buf->currline = begin->prev;
	curr_buf->currln--;
	curr_buf->curr_window_line--;
    }
    if (curr_buf->currline == NULL) {
	assert(curr_buf->currln == -1);
	assert(curr_buf->totaln == -1);

	curr_buf->currline = alloc_line(WRAPMARGIN);
	curr_buf->currln++;
	curr_buf->totaln++;
    } else {
	curr_buf->currline = adjustline(curr_buf->currline, WRAPMARGIN);
    }

    // maintain special line pointer
    if (curr_buf->firstline == begin)
	curr_buf->firstline = curr_buf->currline;
    if (curr_buf->lastline == end)
	curr_buf->lastline = curr_buf->currline;
    if (curr_buf->curr_window_line <= 0) {
	curr_buf->curr_window_line = 0;
	curr_buf->top_of_win = curr_buf->currline;
    }

    // remove buffer
    end->next = NULL;
    for (p = begin; p; p = pnext) {
	pnext = p->next;
	free_line(p);
	deleted_line_count--;
    }
    assert(deleted_line_count == 0);

    curr_buf->currpnt = 0;

    block_cancel();
}

static void
block_cut(void)
{
    if (!has_block_selection())
	return;

    block_save_to_file("buf.0", BLOCK_TRUNCATE);
    block_delete();
}

static void
block_copy(void)
{
    if (!has_block_selection())
	return;

    block_save_to_file("buf.0", BLOCK_TRUNCATE);

    block_cancel();
}

static void
block_prompt(void)
{
    char fp_tmpbuf[80];
    char tmpfname[] = "buf.0";
    char mode[2] = "w";
    char choice[2];

    move(b_lines - 1, 0);
    clrtoeol();

    if (!getdata(b_lines - 1, 0, "��϶����ܼȦs�� (0:Cut, 5:Copy, 6-9, q: Cancel)[0] ", choice, sizeof(choice), LCECHO))
	choice[0] = '0';

    if (choice[0] < '0' || choice[0] > '9')
	goto cancel_block;

    if (choice[0] == '0') {
	block_cut();
	return;
    }
    else if (choice[0] == '5') {
	block_copy();
	return;
    }

    tmpfname[4] = choice[0];
    setuserfile(fp_tmpbuf, tmpfname);
    if (dashf(fp_tmpbuf)) {
	more(fp_tmpbuf, NA);
	getdata(b_lines - 1, 0, "�Ȧs�ɤw����� (A)���[ (W)�мg (Q)�����H[W] ", mode, sizeof(mode), LCECHO);
	if (mode[0] == 'q')
	    goto cancel_block;
	else if (mode[0] != 'a')
	    mode[0] = 'w';
    }

    if (vans("�R���϶�(Y/N)?[N] ") != 'y')
	goto cancel_block;

    block_save_to_file(tmpfname, mode[0] == 'a' ? BLOCK_APPEND : BLOCK_TRUNCATE);

cancel_block:
    block_cancel();
}

static void
block_select(void)
{
    curr_buf->blockln = curr_buf->currln;
    curr_buf->blockline = curr_buf->currline;
}

/////////////////////////////////////////////////////////////////////////////
// Syntax Highlight

#define PMORE_USE_ASCII_MOVIE // disable this if you don't enable ascii movie
#define ENABLE_PMORE_ASCII_MOVIE_SYNTAX // disable if you don't want rich colour syntax

#ifdef PMORE_USE_ASCII_MOVIE
// pmore movie header support
unsigned char *
    mf_movieFrameHeader(unsigned char *p, unsigned char *end);

#endif // PMORE_USE_ASCII_MOVIE

enum {
    EOATTR_NORMAL   = 0x00,
    EOATTR_SELECTED = 0x01,	// selected (reverse)
    EOATTR_MOVIECODE= 0x02,	// pmore movie
    EOATTR_BBSLUA   = 0x04,	// BBS Lua (header)
    EOATTR_COMMENT  = 0x08,	// comment syntax

};

static const char * const luaKeywords[] = {
    "and",   "break", "do",  "else", "elseif",
    "end",   "for",   "if",  "in",   "not",  "or",
    "repeat","return","then","until","while",
    NULL
};

static const char * const luaDataKeywords[] = {
    "false", "function", "local", "nil", "true",
    NULL
};

static const char * const luaFunctions[] = {
    "assert", "print", "tonumber", "tostring", "type",
    NULL
};

static const char * const luaMath[] = {
    "abs", "acos", "asin", "atan", "atan2", "ceil", "cos", "cosh", "deg",
    "exp", "floor", "fmod", "frexp", "ldexp", "log", "log10", "max", "min",
    "modf", "pi", "pow", "rad", "random", "randomseed", "sin", "sinh",
    "sqrt", "tan", "tanh",
    NULL
};

static const char * const luaTable[] = {
    "concat", "insert", "maxn", "remove", "sort",
    NULL
};

static const char * const luaString[] = {
    "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len",
    "lower", "match", "rep", "reverse", "sub", "upper", NULL
};

static const char * const luaBbs[] = {
    "ANSI_COLOR", "ANSI_RESET", "ESC", "addstr", "clear", "clock",
    "clrtobot", "clrtoeol", "color", "ctime", "getch","getdata",
    "getmaxyx", "getstr", "getyx", "interface", "kball", "kbhit", "kbreset",
    "move", "moverel", "now", "outs", "pause", "print", "rect", "refresh",
    "setattr", "sitename", "sleep", "strip_ansi", "time", "title",
    "userid", "usernick",
    NULL
};

static const char * const luaToc[] = {
    "author", "date", "interface", "latestref",
    "notes", "title", "version",
    NULL
};

static const char * const luaBit[] = {
    "arshift", "band", "bnot", "bor", "bxor", "cast", "lshift", "rshift",
    NULL
};

static const char * const luaStore[] = {
    "USER", "GLOBAL", "iolimit", "limit", "load", "save",
    NULL
};

static const char * const luaLibs[] = {
    "bbs", "bit", "math", "store", "string", "table", "toc",
    NULL
};
static const char* const * const luaLibAPI[] = {
    luaBbs, luaBit, luaMath, luaStore, luaString, luaTable, luaToc,
    NULL
};

int synLuaKeyword(const char *text, int n, char *wlen)
{
    int i = 0;
    const char * const *tbl = NULL;
    if (*text >= 'A' && *text <= 'Z')
    {
	// normal identifier
	while (n-- > 0 && (isalnum(*text) || *text == '_'))
	{
	    text++;
	    (*wlen) ++;
	}
	return 0;
    }
    if (*text >= '0' && *text <= '9')
    {
	// digits
	while (n-- > 0 && (isdigit(*text) || *text == '.' || *text == 'x'))
	{
	    text++;
	    (*wlen) ++;
	}
	return 5;
    }
    if (*text == '#')
    {
	text++;
	(*wlen) ++;
	// length of identifier
	while (n-- > 0 && (isalnum(*text) || *text == '_'))
	{
	    text++;
	    (*wlen) ++;
	}
	return -2;
    }

    // ignore non-identifiers
    if (!(*text >= 'a' && *text <= 'z'))
	return 0;

    // 1st, try keywords
    for (i = 0; luaKeywords[i] && *text >= *luaKeywords[i]; i++)
    {
	int l = strlen(luaKeywords[i]);
	if (n < l)
	    continue;
	if (isalnum(text[l]))
	    continue;
	if (strncmp(text, luaKeywords[i], l) == 0)
	{
	    *wlen = l;
	    return 3;
	}
    }
    for (i = 0; luaDataKeywords[i] && *text >= *luaDataKeywords[i]; i++)
    {
	int l = strlen(luaDataKeywords[i]);
	if (n < l)
	    continue;
	if (isalnum(text[l]))
	    continue;
	if (strncmp(text, luaDataKeywords[i], l) == 0)
	{
	    *wlen = l;
	    return 2;
	}
    }
    for (i = 0; luaFunctions[i] && *text >= *luaFunctions[i]; i++)
    {
	int l = strlen(luaFunctions[i]);
	if (n < l)
	    continue;
	if (isalnum(text[l]))
	    continue;
	if (strncmp(text, luaFunctions[i], l) == 0)
	{
	    *wlen = l;
	    return 6;
	}
    }
    for (i = 0; luaLibs[i]; i++)
    {
	int l = strlen(luaLibs[i]);
	if (n < l)
	    continue;
	if (text[l] != '.' && text[l] != ':')
	    continue;
	if (strncmp(text, luaLibs[i], l) == 0)
	{
	    *wlen = l+1;
	    text += l; text ++;
	    n -= l; n--;
	    break;
	}
    }

    tbl = luaLibAPI[i];
    if (!tbl)
    {
	// calcualte wlen
	while (n-- > 0 && (isalnum(*text) || *text == '_'))
	{
	    text++;
	    (*wlen) ++;
	}
	return 0;
    }

    for (i = 0; tbl[i]; i++)
    {
	int l = strlen(tbl[i]);
	if (n < l)
	    continue;
	if (isalnum(text[l]))
	    continue;
	if (strncmp(text, tbl[i], l) == 0)
	{
	    *wlen += l;
	    return 6;
	}
    }
    // luaLib. only
    return -6;
}

void syn_pmore_render(char *os, int len, char *buf)
{
    // XXX buf should be same length as s.
    char *s = (char *)mf_movieFrameHeader((unsigned char*)os, (unsigned char*)os + len);
    char attr = 1;
    char prefix = 0;

    memset(buf, 0, len);
    if (!len || !s) return;

    // render: frame header
    memset(buf, attr++, (s - os));
    len -= s - os;
    buf += s - os;

    while (len-- > 0)
    {
	switch (*s++)
	{
	    case 'P':
	    case 'E':
		*buf++ = attr++;
		return;

	    case 'S':
		*buf++ = attr++;
		continue;

	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
	    case '8': case '9': case '.':
		*buf++ = attr;
		while (len > 0 && isascii(*s) &&
			(isalnum(*s) || *s == '.') )
		{
		    *buf++ = attr;
		    len--; s++;
		}
		return;

	    case '#':
		*buf++ = attr++;
		while (len > 0)
		{
		    *buf++ = attr;
		    if (*s == '#') attr++;
		    len--; s++;
		}
		return;

	    case ':':
		*buf++ = attr;
		while (len > 0 && isascii(*s) &&
			(isalnum(*s) || *s == ':') )
		{
		    *buf++ = attr;
		    len--;
		    if (*s++ == ':') break;
		}
		attr++;
		continue;

	    case 'I':
	    case 'G':
		*buf++ = attr++;
		prefix = 1;
		while (len > 0 &&
			( (isascii(*s) && isalnum(*s)) ||
			  strchr("+-,:lpf", *s)) )
		{
		    if (prefix)
		    {
			if (!strchr(":lpf", *s))
			    break;
			prefix = 0;
		    }
		    *buf++ = attr;
		    if (*s == ',')
		    {
			attr++;
			prefix = 1;
		    }
		    s++; len--;
		}
		attr++;
		return;

	    case 'K':
		*buf++ = attr;
		if (*s != '#')
		    return;
		*buf++ = attr; s++; len--; // #
		while (len >0)
		{
		    *buf++ = attr;
		    len--;
		    if (*s++ == '#') break;
		}
		attr++;
		continue;


	    default: // unknown
		return;
	}
    }
}

/**
 * Just like outs, but print out '*' instead of 27(decimal) in the given string.
 *
 * FIXME column could not start from 0
 */

static void
edit_outs_attr_n(const char *text, int n, int attr)
{
    int    column = 0;
    unsigned char inAnsi = 0;
    unsigned char ch;
    int doReset = 0;
    const char *reset = ANSI_RESET;

    // syntax attributes
    char fComment = 0,
	 fSingleQuote = 0,
	 fDoubleQuote = 0,
	 fSquareQuote = 0,
	 fWord = 0;

    // movie syntax rendering
#ifdef ENABLE_PMORE_ASCII_MOVIE_SYNTAX
    char movie_attrs[WRAPMARGIN+10] = {0};
    char *pmattr = movie_attrs, mattr = 0;
#endif

#ifdef COLORED_SELECTION
    if ((attr & EOATTR_SELECTED) &&
	(attr & ~EOATTR_SELECTED))
    {
	reset = ANSI_COLOR(0;7;36);
	doReset = 1;
	outs(reset);
    }
    else
#endif // if not defined, color by  priority - selection first
    if (attr & EOATTR_SELECTED)
    {
	reset = ANSI_COLOR(0;7);
	doReset = 1;
	outs(reset);
    }
    else if (attr & EOATTR_MOVIECODE)
    {
	reset = ANSI_COLOR(0;36);
	doReset = 1;
	outs(reset);
#ifdef ENABLE_PMORE_ASCII_MOVIE_SYNTAX
	syn_pmore_render((char*)text, n, movie_attrs);
#endif
    }
    else if (attr & EOATTR_BBSLUA)
    {
	reset = ANSI_COLOR(0;1;31);
	doReset = 1;
	outs(reset);
    }
    else if (attr & EOATTR_COMMENT)
    {
	reset = ANSI_COLOR(0;1;34);
	doReset = 1;
	outs(reset);
    }

#ifdef DBCSAWARE
    /* 0 = N/A, 1 = leading byte printed, 2 = ansi in middle */
    unsigned char isDBCS = 0;
#endif

    while ((ch = *text++) && (++column < t_columns) && n-- > 0)
    {
#ifdef ENABLE_PMORE_ASCII_MOVIE_SYNTAX
	mattr = *pmattr++;
#endif

	if(inAnsi == 1)
	{
	    if(ch == ESC_CHR)
		outc('*');
	    else
	    {
		outc(ch);

		if(!ANSI_IN_ESCAPE(ch))
		{
		    inAnsi = 0;
		    outs(reset);
		}
	    }

	}
	else if(ch == ESC_CHR)
	{
	    inAnsi = 1;
#ifdef DBCSAWARE
	    if(isDBCS == 1)
	    {
		isDBCS = 2;
		outs(ANSI_COLOR(1;33) "?");
		outs(reset);
	    }
#endif
	    outs(ANSI_COLOR(1) "*");
	}
	else
	{
#ifdef DBCSAWARE
	    if(isDBCS == 1)
		isDBCS = 0;
	    else if (isDBCS == 2)
	    {
		/* ansi in middle. */
		outs(ANSI_COLOR(0;33) "?");
		outs(reset);
		isDBCS = 0;
		continue;
	    }
	    else
		if(IS_BIG5_HI(ch))
		{
		    isDBCS = 1;
		    // peak next char
		    if(n > 0 && *text == ESC_CHR)
			continue;
		}
#endif

	    // Lua Parser!
	    if (!attr && curr_buf->synparser && !fComment)
	    {
		// syntax highlight!
		if (fSquareQuote) {
		    if (ch == ']' && n > 0 && *(text) == ']')
		    {
			fSquareQuote = 0;
			doReset = 0;
			// directly print quotes
			outc(ch); outc(ch);
			text++, n--;
			outs(ANSI_RESET);
			continue;
		    }
		} else if (fSingleQuote) {
		    if (ch == '\'')
		    {
			fSingleQuote = 0;
			doReset = 0;
			// directly print quotes
			outc(ch);
			outs(ANSI_RESET);
			continue;
		    }
		} else if (fDoubleQuote) {
		    if (ch == '"')
		    {
			fDoubleQuote = 0;
			doReset = 0;
			// directly print quotes
			outc(ch);
			outs(ANSI_RESET);
			continue;
		    }
		} else if (ch == '-' && n > 0 && *(text) == '-') {
		    fComment = 1;
		    doReset = 1;
		    outs(ANSI_COLOR(0;1;34));
		} else if (ch == '[' && n > 0 && *(text) == '[') {
		    fSquareQuote = 1;
		    doReset = 1;
		    fWord = 0;
		    outs(ANSI_COLOR(1;35));
		} else if (ch == '\'' || ch == '"') {
		    if (ch == '"')
			fDoubleQuote = 1;
		    else
			fSingleQuote = 1;
		    doReset = 1;
		    fWord = 0;
		    outs(ANSI_COLOR(1;35));
		} else {
		    // normal words
		    if (fWord)
		    {
			// inside a word.
			if (--fWord <= 0){
			    fWord = 0;
			    doReset = 0;
			    outc(ch);
			    outs(ANSI_RESET);
			    continue;
			}
		    } else if (isalnum(tolower(ch)) || ch == '#') {
			char attr[] = ANSI_COLOR(0;1;37);
			int x = synLuaKeyword(text-1, n+1, &fWord);
			if (fWord > 0)
			    fWord --;
			if (x != 0)
			{
			    // sorry, fixed string here.
			    // 7 = *[0;1;3?
			    if (x<0) {  attr[4] = '0'; x= -x; }
			    attr[7] = '0' + x;
			    outs(attr);
			    doReset = 1;
			}
			if (!fWord)
			{
			    outc(ch);
			    outs(ANSI_RESET);
			    doReset = 0;
			    continue;
			}
		    }
		}
	    }
	    outc(ch);

#ifdef ENABLE_PMORE_ASCII_MOVIE_SYNTAX
	    // pmore Movie Parser!
	    if (attr & EOATTR_MOVIECODE)
	    {
		// only render when attribute was changed.
		if (mattr != *pmattr)
		{
		    if (*pmattr)
		    {
			prints(ANSI_COLOR(1;3%d),
				8 - ((mattr-1) % 7+1) );
		    } else {
			outs(ANSI_RESET);
		    }
		}
	    }
#endif // ENABLE_PMORE_ASCII_MOVIE_SYNTAX
	}
    }

    // this must be ANSI_RESET, not "reset".
    if(inAnsi || doReset)
	outs(ANSI_RESET);
}

static void
edit_outs_attr(const char *text, int attr)
{
    edit_outs_attr_n(text, strlen(text), attr);
}

static void
edit_ansi_outs_n(const char *str, int n, int attr GCC_UNUSED)
{
    char c;
    while (n-- > 0 && (c = *str++)) {
	if(c == ESC_CHR && *str == '*')
	{
	    // ptt prints
	    /* Because moving within ptt_prints is too hard
	     * let's just display it as-is.
	     */
	    outc('*');
	} else {
	    outc(c);
	}
    }
}

static void
edit_ansi_outs(const char *str, int attr)
{
    return edit_ansi_outs_n(str, strlen(str), attr);
}

static int
detect_attr(const char *ps, size_t len)
{
    int attr = 0;

#ifdef PMORE_USE_ASCII_MOVIE
    if (mf_movieFrameHeader((unsigned char*)ps, (unsigned char*)ps+len))
	attr |= EOATTR_MOVIECODE;
#endif
#ifdef USE_BBSLUA
    if (bbslua_isHeader(ps, ps + len))
    {
	attr |= EOATTR_BBSLUA;
	if (!curr_buf->synparser)
	{
	    curr_buf->synparser = 1;
	    // if you need indent, toggle by hotkey.
	    // enabling indent by default may cause trouble to copy pasters
	    // curr_buf->indent_mode = 1;
	}
    }
#endif
    return attr;
}

static inline void
display_textline_internal(textline_t *p, int i)
{
    short tmp;
    void (*output)(const char *, int)	    = edit_outs_attr;

    int attr = EOATTR_NORMAL;

    move(i, 0);
    clrtoeol();

    if (!p) {
	outc('~');
	outs(ANSI_CLRTOEND);
	return;
    }

    if (curr_buf->ansimode) {
	output = edit_ansi_outs;
    }

    tmp = curr_buf->currln - curr_buf->curr_window_line + i;

    // parse attribute of line

    // selected attribute?
    if (has_block_selection() &&
	    ( (curr_buf->blockln <= curr_buf->currln &&
	       curr_buf->blockln <= tmp && tmp <= curr_buf->currln) ||
	      (curr_buf->currln <= tmp && tmp <= curr_buf->blockln)) )
    {
	// outs(ANSI_REVERSE); // remove me when EOATTR is ready...
	attr |= EOATTR_SELECTED;
    }

    attr |= detect_attr(p->data, p->len);

#ifdef DBCSAWARE
    if(mbcs_mode && curr_buf->edit_margin > 0)
    {
	if(curr_buf->edit_margin >= p->len)
	{
	    (*output)("", attr);
	} else {

	    int newpnt = curr_buf->edit_margin;
	    unsigned char *pdata = (unsigned char*)
		(&p->data[0] + curr_buf->edit_margin);

	    if(mbcs_mode)
		newpnt = fix_cursor(p->data, newpnt, FC_LEFT);

	    if(newpnt == curr_buf->edit_margin-1)
	    {
		/* this should be always 'outs'? */
		// (*output)(ANSI_COLOR(1) "<" ANSI_RESET);
		outs(ANSI_COLOR(1) "<" ANSI_RESET);
		pdata++;
	    }
	    (*output)((char*)pdata, attr);
	}

    } else
#endif
    (*output)((curr_buf->edit_margin < p->len) ?
	    &p->data[curr_buf->edit_margin] : "", attr);

    if (attr)
	outs(ANSI_RESET);

    // workaround poor terminal
    outs(ANSI_CLRTOEND);
}

static void
refresh_window(void)
{
    textline_t *p;
    int    i;

    for (p = curr_buf->top_of_win, i = 0; i < visible_window_height(); i++) {
	display_textline_internal(p, i);

	if (p)
	    p = p->next;
    }
    edit_msg();
}

static void
goto_line(int lino)
{
    if (lino > 0 && lino <= curr_buf->totaln + 1) {
	textline_t     *p;

	p = curr_buf->firstline;
	curr_buf->currln = lino - 1;

	while (--lino && p->next)
	    p = p->next;

	if (p)
	    curr_buf->currline = p;
	else {
	    curr_buf->currln = curr_buf->totaln;
	    curr_buf->currline = curr_buf->lastline;
	}

	curr_buf->currpnt = 0;

	edit_window_adjust_middle();
    }
    curr_buf->redraw_everything = YEA;
}

static void
prompt_goto_line(void)
{
    char buf[10];

    if (getdata(b_lines - 1, 0, "���ܲĴX��:", buf, sizeof(buf), DOECHO))
	goto_line(atoi(buf));
}

/**
 * search string interactively.
 * @param mode 0: prompt
 *             1: forward
 *            -1: backward
 */
static void
search_str(int mode)
{
    const int max_keyword = 65;
    char *str;
    char            ans[4] = "n";

    if (curr_buf->searched_string == NULL) {
	if (mode != 0)
	    return;
	curr_buf->searched_string = (char *)malloc(max_keyword * sizeof(char));
	curr_buf->searched_string[0] = 0;
    }

    str = curr_buf->searched_string;

    if (!mode) {
	if (getdata_buf(b_lines - 1, 0, "[�j�M]����r:",
			str, max_keyword, DOECHO))
	    if (*str) {
		if (getdata(b_lines - 1, 0, "�Ϥ��j�p�g(Y/N/Q)? [N] ",
			    ans, sizeof(ans), LCECHO) && *ans == 'y')
		    curr_buf->substr_fp = strstr;
		else
		    curr_buf->substr_fp = strcasestr;
	    }
    }
    if (*str && *ans != 'q') {
	textline_t     *p;
	char           *pos = NULL;
	int             lino;
	bool found = false;

	if (mode >= 0) {
	    for (lino = curr_buf->currln, p = curr_buf->currline; p; p = p->next, lino++) {
		int offset = (lino == curr_buf->currln ? MIN(curr_buf->currpnt + 1, curr_buf->currline->len) : 0);
		pos = (*curr_buf->substr_fp)(p->data + offset, str);
		if (pos) {
		    found = true;
		    break;
		}
	    }
	} else {
	    for (lino = curr_buf->currln, p = curr_buf->currline; p; p = p->prev, lino--) {
		pos = (*curr_buf->substr_fp)(p->data, str);
		if (pos &&
		    (lino != curr_buf->currln || pos - p->data < curr_buf->currpnt)) {
		    found = true;
		    break;
		}
	    }
	}
	if (found) {
	    /* move window */
	    curr_buf->currline = p;
	    curr_buf->currln = lino;
	    curr_buf->currpnt = pos - p->data;

	    edit_window_adjust_middle();
	}
    }
    if (!mode)
	curr_buf->redraw_everything = YEA;
}

/**
 * move the cursor from bracket to corresponding bracket.
 */
static void
match_paren(void)
{
    char           *parens = "()[]{}";
    char           *ptype;
    textline_t     *p;
    int             lino;
    int             i = 0;
    bool found = false;
    char findch;
    char quotech = '\0';
    enum MatchState {
	MATCH_STATE_NORMAL,
	MATCH_STATE_C_COMMENT,
	MATCH_STATE_QUOTE
    };
    enum MatchState state = MATCH_STATE_NORMAL;
    int dir;
    int nested = 0;

    char cursorch = curr_buf->currline->data[curr_buf->currpnt];
    if (cursorch == '\0')
	return;
    if (!(ptype = strchr(parens, cursorch)))
	return;

    dir = (ptype - parens) % 2 == 0 ? 1 : -1;
    findch = *(ptype + dir);

    p = curr_buf->currline;
    lino = curr_buf->currln;
    i = curr_buf->currpnt;
    while (p && !found) {
	// next position
	i += dir;
	while (p && i < 0) {
	    p = p->prev;
	    if (p)
		i = p->len - 1;
	    lino--;
	}
	while (p && i >= p->len) {
	    p = p->next;
	    i = 0;
	    lino++;
	}
	if (!p)
	    break;
	assert(0 <= i && i < p->len);

	// match char
	switch (state) {
	    case MATCH_STATE_NORMAL:
		if (nested == 0 && p->data[i] == findch) {
		    found = true;
		    break;
		}
		if (p->data[i] == cursorch)
		    nested++;
		else if (p->data[i] == findch)
		    nested--;
		if (p->data[i] == '\'' || p->data[i] == '"') {
		    quotech = p->data[i];
		    state = MATCH_STATE_QUOTE;
		} else if ((i+dir) >= 0 && p->data[i] == '/' && p->data[i+dir] == '*') {
		    state = MATCH_STATE_C_COMMENT;
		    i += dir;
		}
		break;
	    case MATCH_STATE_C_COMMENT:
		if ((i+dir) >= 0 && p->data[i] == '*' && p->data[i+dir] == '/') {
		    state = MATCH_STATE_NORMAL;
		    i += dir;
		}
		break;
	    case MATCH_STATE_QUOTE:
		if (p->data[i] == quotech) {
		    if (i==0 || p->data[i-1] != '\\')
			state = MATCH_STATE_NORMAL;
		}
		break;
	}
    }
    if (found) {
	int             top = curr_buf->currln - curr_buf->curr_window_line;
	int             bottom = top + visible_window_height() - 1;

	assert(p);
	curr_buf->currpnt = i;
	curr_buf->currline = p;
	curr_buf->curr_window_line += lino - curr_buf->currln;
	curr_buf->currln = lino;

	if (lino < top || lino > bottom) {
	    edit_window_adjust_middle();
	}
    }
}

static void
currline_shift_left(void)
{
    int currpnt0;

    if (curr_buf->currline->len <= 0)
	return;

    currpnt0 = curr_buf->currpnt;
    curr_buf->currpnt = 0;
    delete_char();
    curr_buf->currpnt = (currpnt0 <= curr_buf->currline->len) ? currpnt0 : currpnt0 - 1;
    if (curr_buf->ansimode)
	curr_buf->currpnt = ansi2n(n2ansi(curr_buf->currpnt, curr_buf->currline), curr_buf->currline);
}

static void
currline_shift_right(void)
{
    int currpnt0;

    if (curr_buf->currline->len >= WRAPMARGIN - 1)
	return;

    currpnt0 = curr_buf->currpnt;
    curr_buf->currpnt = 0;
    insert_char(' ');
    curr_buf->currpnt = currpnt0;
}

static void
cursor_to_next_word(void)
{
    while (curr_buf->currpnt < curr_buf->currline->len &&
	    isalnum((int)curr_buf->currline->data[++curr_buf->currpnt]));
    while (curr_buf->currpnt < curr_buf->currline->len &&
	    isspace((int)curr_buf->currline->data[++curr_buf->currpnt]));
}

static void
cursor_to_prev_word(void)
{
    while (curr_buf->currpnt && isspace((int)curr_buf->currline->data[--curr_buf->currpnt]));
    while (curr_buf->currpnt && isalnum((int)curr_buf->currline->data[--curr_buf->currpnt]));
    if (curr_buf->currpnt > 0)
	curr_buf->currpnt++;
}

static void
delete_current_word(void)
{
    while (curr_buf->currpnt < curr_buf->currline->len) {
	delete_char();
	if (!isalnum((int)curr_buf->currline->data[curr_buf->currpnt]))
	    break;
    }
    while (curr_buf->currpnt < curr_buf->currline->len) {
	delete_char();
	if (!isspace((int)curr_buf->currline->data[curr_buf->currpnt]))
	    break;
    }
}

/**
 * transform every "*[" in given string to KEY_ESC "["
 */
static void
transform_to_color(char *line)
{
    while (line[0] && line[1])
	if (line[0] == '*' && line[1] == '[') {
	    line[0] = KEY_ESC;
	    line += 2;
	} else
	    ++line;
}

static void
block_color(void)
{
    textline_t     *begin, *end, *p;

    setup_block_begin_end(&begin, &end);

    p = begin;
    while (1) {
	assert(p);
	transform_to_color(p->data);
	if (p == end)
	    break;
	else
	    p = p->next;
    }
    block_cancel();
}

/**
 * insert ansi code
 */
static void
insert_ansi_code(void)
{
    int ch = curr_buf->insert_mode;
    curr_buf->insert_mode = curr_buf->redraw_everything = YEA;
    if (!curr_buf->ansimode)
	insert_string(ANSI_RESET);
    else {
	char            ans[4];
	move(b_lines - 2, 55);
	outs(ANSI_COLOR(1;33;40) "B" ANSI_COLOR(41) "R" ANSI_COLOR(42) "G" ANSI_COLOR(43) "Y" ANSI_COLOR(44) "L"
		ANSI_COLOR(45) "P" ANSI_COLOR(46) "C" ANSI_COLOR(47) "W" ANSI_RESET);
	if (getdata(b_lines - 1, 0,
		    "�п�J  �G��/�e��/�I��[���`�զr�©�][0wb]�G",
		    ans, sizeof(ans), LCECHO))
	{
	    const char      t[] = "BRGYLPCW";
	    char            color[15];
	    char           *tmp, *apos = ans;
	    int             fg, bg;

	    strcpy(color, ESC_STR "[");
	    if (isdigit((int)*apos)) {
		sprintf(color,"%s%c", color, *(apos++));
		if (*apos)
		    strcat(color, ";");
	    }
	    if (*apos) {
		if ((tmp = strchr(t, toupper(*(apos++)))))
		    fg = tmp - t + 30;
		else
		    fg = 37;
		sprintf(color, "%s%d", color, fg);
	    }
	    if (*apos) {
		if ((tmp = strchr(t, toupper(*(apos++)))))
		    bg = tmp - t + 40;
		else
		    bg = 40;
		sprintf(color, "%s;%d", color, bg);
	    }
	    strcat(color, "m");
	    insert_string(color);
	} else
    	    insert_string(ANSI_RESET);
    }
    curr_buf->insert_mode = ch;
}

static inline void
phone_mode_switch(void)
{
    if (curr_buf->phone_mode)
	curr_buf->phone_mode = 0;
    else {
	curr_buf->phone_mode = 1;
	if (!curr_buf->last_phone_mode)
	    curr_buf->last_phone_mode = 2;
    }
}

/**
 * return coresponding phone char of given key c
 */
static const char*
phone_char(char c)
{
    if (curr_buf->last_phone_mode > 0 && curr_buf->last_phone_mode < 20) {
	if (tolower(c) < 'a' ||
            (tolower(c)-'a') >= (int)strlen(BIG5[curr_buf->last_phone_mode - 1]) / 2)
	    return 0;
	return BIG5[curr_buf->last_phone_mode - 1] + (tolower(c) - 'a') * 2;
    }
    else if (curr_buf->last_phone_mode >= 20) {
	if (c == '.') c = '/';

	if (c < '/' || c > '9')
	    return 0;

	return table[curr_buf->last_phone_mode - 20] + (c - '/') * 2;
    }
    return 0;
}

/**
 * When get the key for phone mode, handle it (e.g. edit_msg) and return the
 * key.  Otherwise return 0.
 */
static inline char
phone_mode_filter(char ch)
{
    if (!curr_buf->phone_mode)
	return 0;

    switch (ch) {
	case 'z':
	case 'Z':
	    if (curr_buf->last_phone_mode < 20)
		curr_buf->last_phone_mode = 20;
	    else
		curr_buf->last_phone_mode = 2;
	    edit_msg();
	    return ch;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    if (curr_buf->last_phone_mode < 20) {
		curr_buf->last_phone_mode = ch - '0' + 1;
		curr_buf->redraw_everything = YEA;
		return ch;
	    }
	    break;
	case '-':
	    if (curr_buf->last_phone_mode < 20) {
		curr_buf->last_phone_mode = 11;
		curr_buf->redraw_everything = YEA;
		return ch;
	    }
	    break;
	case '=':
	    if (curr_buf->last_phone_mode < 20) {
		curr_buf->last_phone_mode = 12;
		curr_buf->redraw_everything = YEA;
		return ch;
	    }
	    break;
	case '`':
	    if (curr_buf->last_phone_mode < 20) {
		curr_buf->last_phone_mode = 13;
		curr_buf->redraw_everything = YEA;
		return ch;
	    }
	    break;
	case '/':
	    if (curr_buf->last_phone_mode >= 20) {
		curr_buf->last_phone_mode += 4;
		if (curr_buf->last_phone_mode > 27)
		    curr_buf->last_phone_mode -= 8;
		curr_buf->redraw_everything = YEA;
		return ch;
	    }
	    break;
	case '*':
	    if (curr_buf->last_phone_mode >= 20) {
		curr_buf->last_phone_mode++;
		if ((curr_buf->last_phone_mode - 21) % 4 == 3)
		    curr_buf->last_phone_mode -= 4;
		curr_buf->redraw_everything = YEA;
		return ch;
	    }
	    break;
    }

    return 0;
}

#ifdef EXP_EDIT_UPLOAD

static void
upload_file(void)
{
    size_t szdata = 0;
    int c = 1;
    char promptmsg = 0;

    clear();
    block_cancel();
    vs_hdr("�W�Ǥ�r�ɮ�");
    move(3,0);
    outs("�Q�Υ��A�ȱz�i�H�W�Ǹ��j����r�� (�����p�J�Z�O)�C\n"
	 "\n"
	 "�W�Ǵ����z�����r�Ȯɤ��|�X�{�b�ù��W�A���F Ctrl-U �|�Q�ഫ�� ANSI \n"
	 "����X�� ESC �~�A�䥦�S����@�ߨS���@�ΡC\n"
	 "\n"
	 "�Цb�z���q�������ݽƻs�n���e��K�W�Y�i�}�l�ǰe�C\n");

    do {
	if (!vkey_is_ready())
	{
	    move(10, 0); clrtobot();
	    prints("\n\n��Ʊ�����... %u �줸�աC\n", (unsigned int)szdata);
	    outs(ANSI_COLOR(1)
		    "��������������U End �� ^X/^Q/^C �Y�i�^��s��e���C"
		    ANSI_RESET "\n");
	    promptmsg = 0;
	}

	c = vkey();
	if (c < 0x100 && isprint2(c))
	{
	    insert_char(c);
	    szdata ++;
	}
	else if (c == Ctrl('U') || c == ESC_CHR)
	{
	    insert_char(ESC_CHR);
	    szdata ++;
	}
	else if (c == Ctrl('I'))
	{
	    insert_tab();
	    szdata ++;
	}
	else if (c == KEY_ENTER)
	{
	    split(curr_buf->currline, curr_buf->currpnt, 0);
	    szdata ++;
	    promptmsg = 1;
	}

	if (!promptmsg)
	    promptmsg = (szdata && szdata % 1024 == 0);

	// all other keys are ignored.
    } while (c != KEY_END && c != Ctrl('X') &&
	     c != Ctrl('C') && c != Ctrl('Q') &&
	     curr_buf->totaln <= EDIT_LINE_LIMIT &&
	     szdata <= EDIT_SIZE_LIMIT);

    move(12, 0);
    prints("�ǰe����: ���� %u �줸�աC", (unsigned int)szdata);
    vmsgf("�^��s��e��");
}

#endif // EXP_EDIT_UPLOAD


/** �s��B�z�G�D�{���B��L�B�z
 * @param	title		NULL, �_�h���� STRLEN
 * @return	EDIT_ABORTED	abort
 * 		>= 0		�s�����
 * �ѩ�U�B���H == EDIT_ABORTED �P�_, �Y�Q�Ǧ^��L�t�ȭn�`�N
 */
int
vedit2(const char *fpath, int saveheader, char title[STRLEN], int flags)
{
    char            last = 0;	/* the last key you press */
    int             ch, tmp;

    int             mode0 = currutmp->mode;
    int             destuid0 = currutmp->destuid;
    int             money = 0, entropy = 0;
    int             interval = 0;
    time4_t         th = now;
    int             count = 0, tin = 0, quoted = 0;
    char            trans_buffer[256];

    STATINC(STAT_VEDIT);
    currutmp->mode = EDITING;
    currutmp->destuid = currstat;

#ifdef DBCSAWARE
    mbcs_mode = ISDBCSAWARE() ? 1 : 0;
#endif

    enter_edit_buffer();
    curr_buf->flags = flags;


    if (*fpath) {
	int tmp = read_file(fpath, (flags & EDITFLAG_TEXTONLY) ? 1 : 0);

	if (tmp < 0)
	    return tmp;
    }

    if (*quote_file) {
	do_quote();
	*quote_file = '\0';
	quoted = 1;
    }

    if(	curr_buf->oldcurrline != curr_buf->firstline ||
	curr_buf->currline != curr_buf->firstline) {
	/* we must adjust because cursor (currentline) moved. */
	curr_buf->oldcurrline = curr_buf->currline = curr_buf->top_of_win =
           curr_buf->firstline= adjustline(curr_buf->firstline, WRAPMARGIN);
    }
#ifdef DEBUG
    assert(curr_buf->currline->mlength == WRAPMARGIN);
#endif

    /* No matter you quote or not, just start the cursor from (0,0) */
    curr_buf->currpnt = curr_buf->currln = curr_buf->curr_window_line =
    curr_buf->edit_margin = curr_buf->last_margin = 0;

    /* if quote, move to end of file. */
    if(quoted)
    {
	/* maybe do this in future. */
    }

    while (1) {
	edit_check_healthy();

	if (curr_buf->redraw_everything || has_block_selection()) {
	    refresh_window();
	    curr_buf->redraw_everything = NA;
	}
	if( curr_buf->oldcurrline != curr_buf->currline ){
	    if (curr_buf->oldcurrline != NULL)
		curr_buf->oldcurrline = adjustline(curr_buf->oldcurrline, curr_buf->oldcurrline->len);
	    curr_buf->oldcurrline = curr_buf->currline = adjustline(curr_buf->currline, WRAPMARGIN);
	}
#ifdef DEBUG
	assert(curr_buf->currline->mlength == WRAPMARGIN);
#endif

	if (curr_buf->ansimode)
	    ch = n2ansi(curr_buf->currpnt, curr_buf->currline);
	else
	    ch = curr_buf->currpnt - curr_buf->edit_margin;
	move(curr_buf->curr_window_line, ch);

	ch = vkey();
	/* jochang debug */
	if ((interval = (now - th))) {
	    th = now;
	    if ((char)ch != last) {
		money++;
		last = (char)ch;
	    }
	}
	if (interval && interval == tin)
          {  // Ptt : +- 1 ���]��
	    count++;
            if(count>60)
            {
             money = 0;
             count = 0;
/*
             log_file("etc/illegal_money",  LOG_CREAT | LOG_VF,
             ANSI_COLOR(1;33;46) "%s " ANSI_COLOR(37;45) " �ξ����H�o���峹 " ANSI_COLOR(37) " %s" ANSI_RESET "\n",
             cuser.userid, Cdate(&now));
             post_violatelaw(cuser.userid, BBSMNAME "�t��ĵ��",
                 "�ξ����H�o���峹", "�j������");
             abort_bbs(0);
*/
            }
          }
	else if(interval){
	    count = 0;
	    tin = interval;
	}
#ifndef DBCSAWARE
	/* this is almost useless! */
	if (curr_buf->raw_mode) {
	    switch (ch) {
	    case Ctrl('S'):
	    case Ctrl('Q'):
	    case Ctrl('T'):
		continue;
	    }
	}
#endif

	if (phone_mode_filter(ch))
	    continue;

	if (ch < 0x100 && isprint2(ch)) {
	    const char *pstr;
            if(curr_buf->phone_mode && (pstr=phone_char(ch)))
	   	insert_dchar(pstr);
	    else
		insert_char(ch);
	    curr_buf->lastindent = -1;
	} else {
	    if (ch == KEY_UP || ch == KEY_DOWN ){
		if (curr_buf->lastindent == -1)
		    curr_buf->lastindent = curr_buf->currpnt;
	    } else
		curr_buf->lastindent = -1;
	    if (ch == KEY_ESC)
		switch (KEY_ESC_arg) {
		case ',':
		    ch = Ctrl(']');
		    break;
		case '.':
		    ch = Ctrl('T');
		    break;
		case 'v':
		    ch = KEY_PGUP;
		    break;
		case 'a':
		case 'A':
		    ch = Ctrl('V');
		    break;
		case 'X':
		    ch = Ctrl('X');
		    break;
		case 'q':
		    ch = Ctrl('Q');
		    break;
		case 'o':
		    ch = Ctrl('O');
		    break;
		case 's':
		    ch = Ctrl('S');
		    break;
                case 'S':
                    curr_buf->synparser = !curr_buf->synparser;
                    break;
		}

	    switch (ch) {
	    case KEY_F10:
	    case Ctrl('X'):	/* Save and exit */
		block_cancel();
                tmp = write_file(fpath, saveheader, title, flags,
                                 &entropy);
		if (tmp != KEEP_EDITING) {
		    currutmp->mode = mode0;
		    currutmp->destuid = destuid0;

		    exit_edit_buffer();

		    // adjust final money
		    money *= POST_MONEY_RATIO;
		    // money or entropy?
		    if (money > (entropy * ENTROPY_RATIO) && entropy >= 0)
			money = (entropy * ENTROPY_RATIO) + 1;

		    if (!tmp)
			return money;
		    else
			return tmp;
		}
		curr_buf->redraw_everything = YEA;
		break;
	    case KEY_F5:
		prompt_goto_line();
		curr_buf->redraw_everything = YEA;
		break;
	    case KEY_F8:
		t_users();
		curr_buf->redraw_everything = YEA;
		break;
	    case Ctrl('W'):
		block_cut();
		break;
	    case Ctrl('Q'):	/* Quit without saving */
		grayout(0, b_lines-1, GRAYOUT_DARK);
		ch = vmsg("���������x�s [y/N]? ");
		if (ch == 'y' || ch == 'Y') {
		    currutmp->mode = mode0;
		    currutmp->destuid = destuid0;
		    exit_edit_buffer();
		    return -1;
		}
		curr_buf->redraw_everything = YEA;
		break;
	    case Ctrl('C'):
		insert_ansi_code();
		break;
	    case KEY_ESC:
		switch (KEY_ESC_arg) {
		case 'U':
		    t_users();
		    curr_buf->redraw_everything = YEA;
		    break;
		case 'n':
		    search_str(1);
		    break;
		case 'p':
		    search_str(-1);
		    break;
		case 'L':
		case 'J':
		    prompt_goto_line();
		    curr_buf->redraw_everything = YEA;
		    break;
		case ']':
		    match_paren();
		    break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		    read_tmpbuf(KEY_ESC_arg - '0');
		    curr_buf->redraw_everything = YEA;
		    break;
		case 'l':	/* block delete */
		case ' ':
		    if (has_block_selection())
			block_prompt();
		    else
			block_select();
		    break;
		case 'u':
		    block_cancel();
		    break;
		case 'c':
		    block_copy();
		    break;
		case 'y':
		    undelete_line();
		    break;
		case 'R':
#ifdef DBCSAWARE
		case 'r':
		    mbcs_mode =! mbcs_mode;
#endif
		    curr_buf->raw_mode ^= 1;
		    break;
		case 'I':
		    curr_buf->indent_mode ^= 1;
		    break;
		case 'j':
		    currline_shift_left();
		    break;
		case 'k':
		    currline_shift_right();
		    break;
		case 'f':
		    cursor_to_next_word();
		    break;
		case 'b':
		    cursor_to_prev_word();
		    break;
		case 'd':
		    delete_current_word();
		    break;
		}
		break;
	    case Ctrl('S'):
	    case KEY_F3:
		search_str(0);
		break;
	    case Ctrl('U'):
		insert_char(ESC_CHR);
		break;
	    case Ctrl('V'):	/* Toggle ANSI color */
		curr_buf->ansimode ^= 1;
		if (curr_buf->ansimode && has_block_selection())
		    block_color();
		clear();
		curr_buf->redraw_everything = YEA;
		break;
	    case Ctrl('I'):
		insert_tab();
		break;
	    case KEY_ENTER:
		block_cancel();
		if (curr_buf->totaln >= EDIT_LINE_LIMIT)
		{
		    vmsg("�ɮפw�W�L�̤j����A�L�k�A�W�[��ơC");
		    break;
		}

#ifdef MAX_EDIT_LINE
		if(curr_buf->totaln ==
			((flags & EDITFLAG_ALLOWLARGE) ?
			 MAX_EDIT_LINE_LARGE : MAX_EDIT_LINE))
		{
		    vmsg("�w��F�̤j��ƭ���C");
		    break;
		}
#endif
		split(curr_buf->currline, curr_buf->currpnt, indent_space());
		break;
	    case Ctrl('G'):
		{
		    unsigned int    currstat0 = currstat;
		    int mode0 = currutmp->mode;
		    setutmpmode(EDITEXP);
		    a_menu("�s�軲�U��", "etc/editexp",
			   (HasUserPerm(PERM_SYSOP) ? SYSOP : NOBODY),
			   0,
			   trans_buffer, NULL);
		    currstat = currstat0;
		    currutmp->mode = mode0;
		}
		if (trans_buffer[0]) {
		    FILE *fp1;
		    if ((fp1 = fopen(trans_buffer, "r"))) {
			int indent_mode0 = curr_buf->indent_mode;
    			char buf[WRAPMARGIN + 2];

			curr_buf->indent_mode = 0;
			while (fgets(buf, sizeof(buf), fp1)) {
			    if (!strncmp(buf, "�@��:", 5) ||
				!strncmp(buf, "���D:", 5) ||
				!strncmp(buf, "�ɶ�:", 5))
				continue;
			    insert_string(buf);
			}
			fclose(fp1);
			curr_buf->indent_mode = indent_mode0;
			edit_window_adjust();
		    }
		}
		curr_buf->redraw_everything = YEA;
		break;

		// XXX This breaks emacs compatibility. Really bad,
		// especially when some terminals just cannot send
		// vt100 arrow keys. However since this key binding
		// has been changed for a long long time...
		// We may need to change this back in the future, but
		// not sure when.
            case Ctrl('P'):
		phone_mode_switch();
                curr_buf->redraw_everything = YEA;
		break;

	    case KEY_F1:
	    case Ctrl('Z'):	/* Help */
		more("etc/ve.hlp", YEA);
		curr_buf->redraw_everything = YEA;
		break;
	    case Ctrl('L'):
		clear();
		curr_buf->redraw_everything = YEA;
		break;
	    case KEY_LEFT:
		if (curr_buf->currpnt) {
		    if (curr_buf->ansimode)
			curr_buf->currpnt = n2ansi(curr_buf->currpnt, curr_buf->currline);
		    curr_buf->currpnt--;
		    if (curr_buf->ansimode)
			curr_buf->currpnt = ansi2n(curr_buf->currpnt, curr_buf->currline);
#ifdef DBCSAWARE
		    if(mbcs_mode)
		      curr_buf->currpnt = fix_cursor(curr_buf->currline->data, curr_buf->currpnt, FC_LEFT);
#endif
		} else if (curr_buf->currline->prev) {
		    curr_buf->curr_window_line--;
		    curr_buf->currln--;
		    curr_buf->currline = curr_buf->currline->prev;
		    curr_buf->currpnt = curr_buf->currline->len;
		}
		break;
	    case KEY_RIGHT:
		if (curr_buf->currline->len != curr_buf->currpnt) {
		    if (curr_buf->ansimode)
			curr_buf->currpnt = n2ansi(curr_buf->currpnt, curr_buf->currline);
		    curr_buf->currpnt++;
		    if (curr_buf->ansimode)
			curr_buf->currpnt = ansi2n(curr_buf->currpnt, curr_buf->currline);
#ifdef DBCSAWARE
		    if(mbcs_mode)
		      curr_buf->currpnt = fix_cursor(curr_buf->currline->data, curr_buf->currpnt, FC_RIGHT);
#endif
		} else if (curr_buf->currline->next) {
		    curr_buf->currpnt = 0;
		    curr_buf->curr_window_line++;
		    curr_buf->currln++;
		    curr_buf->currline = curr_buf->currline->next;
		}
		break;
	    case KEY_UP:
	    // case Ctrl('P'):	// XXX check phone_mode to see why this is not enabled
		cursor_to_prev_line();
		break;
	    case KEY_DOWN:
	    // case Ctrl('N'):	// XXX check phone_mode to see why this is not enabled
		cursor_to_next_line();
		break;

	    case Ctrl('B'):
	    case KEY_PGUP:
	   	curr_buf->top_of_win = back_line(curr_buf->top_of_win, visible_window_height() - 1, false);
	 	curr_buf->currline = back_line(curr_buf->currline, visible_window_height() - 1, true);
		curr_buf->curr_window_line = get_lineno_in_window();
		if (curr_buf->currpnt > curr_buf->currline->len)
		    curr_buf->currpnt = curr_buf->currline->len;
		curr_buf->redraw_everything = YEA;
	 	break;

	    case Ctrl('F'):
	    case KEY_PGDN:
		curr_buf->top_of_win = forward_line(curr_buf->top_of_win, visible_window_height() - 1, false);
		curr_buf->currline = forward_line(curr_buf->currline, visible_window_height() - 1, true);
		curr_buf->curr_window_line = get_lineno_in_window();
		if (curr_buf->currpnt > curr_buf->currline->len)
		    curr_buf->currpnt = curr_buf->currline->len;
		curr_buf->redraw_everything = YEA;
		break;

	    case KEY_END:
	    case Ctrl('E'):
		curr_buf->currpnt = curr_buf->currline->len;
		break;
	    case Ctrl(']'):	/* start of file */
		curr_buf->currline = curr_buf->top_of_win = curr_buf->firstline;
		curr_buf->currpnt = curr_buf->currln = curr_buf->curr_window_line = 0;
		curr_buf->redraw_everything = YEA;
		break;
	    case Ctrl('T'):	/* tail of file */
		curr_buf->top_of_win = back_line(curr_buf->lastline, visible_window_height() - 1, false);
		curr_buf->currline = curr_buf->lastline;
		curr_buf->curr_window_line = get_lineno_in_window();
		curr_buf->currln = curr_buf->totaln;
		curr_buf->redraw_everything = YEA;
		curr_buf->currpnt = 0;
		break;
	    case KEY_HOME:
	    case Ctrl('A'):
		curr_buf->currpnt = 0;
		break;
	    case Ctrl('O'):	// better not use ^O - UNIX not sending.
	    case KEY_INS:	/* Toggle insert/overwrite */
		curr_buf->insert_mode ^= 1;
		break;
	    case KEY_BS:	/* backspace */
		block_cancel();
		if (curr_buf->ansimode) {
		    curr_buf->ansimode = 0;
		    clear();
		    curr_buf->redraw_everything = YEA;
		} else {
		    if (curr_buf->currpnt == 0) {
			if (!curr_buf->currline->prev)
			    break;
			curr_buf->curr_window_line--;
			curr_buf->currln--;

			curr_buf->currline = adjustline(curr_buf->currline, curr_buf->currline->len);
			curr_buf->currline = curr_buf->currline->prev;
			curr_buf->currline = adjustline(curr_buf->currline, WRAPMARGIN);
			curr_buf->oldcurrline = curr_buf->currline;

			curr_buf->currpnt = curr_buf->currline->len;
			curr_buf->redraw_everything = YEA;
			if (curr_buf->currline->next == curr_buf->top_of_win) {
			    curr_buf->top_of_win = curr_buf->currline;
			    curr_buf->curr_window_line = 0;
			}
			join(curr_buf->currline);
			break;
		    }
#ifndef DBCSAWARE
		    curr_buf->currpnt--;
		    delete_char();
#else
		    {
		      int newpnt = curr_buf->currpnt - 1;

		      if(mbcs_mode)
		        newpnt = fix_cursor(curr_buf->currline->data, newpnt, FC_LEFT);

		      for(; curr_buf->currpnt > newpnt;)
		      {
		        curr_buf->currpnt --;
		        delete_char();
		      }
		    }
#endif
		}
		break;
	    case Ctrl('D'):
	    case KEY_DEL:	/* delete current character */
		block_cancel();
		if (curr_buf->currline->len == curr_buf->currpnt) {
		    join(curr_buf->currline);
		    curr_buf->redraw_everything = YEA;
		} else {
#ifndef DBCSAWARE
		    delete_char();
#else
		    {
		      int w = 1;

		      if(mbcs_mode)
		        w = mchar_len((unsigned char*)(curr_buf->currline->data + curr_buf->currpnt));

		      for(; w > 0; w --)
		        delete_char();
		    }
#endif
		    if (curr_buf->ansimode)
			curr_buf->currpnt = ansi2n(n2ansi(curr_buf->currpnt, curr_buf->currline), curr_buf->currline);
		}
		break;
	    case Ctrl('Y'):	/* delete current line */
		curr_buf->currpnt = 0;
	    case Ctrl('K'):	/* delete to end of line */
		block_cancel();
		if (ch == Ctrl('Y') || curr_buf->currline->len == 0) {
		    textline_t     *p = curr_buf->currline->next;
		    if (!p) {
			p = curr_buf->currline->prev;
			if (!p) {
			    curr_buf->currline->data[0] = 0;
			    curr_buf->currline->len = 0;
			    break;
			}
			if (curr_buf->curr_window_line > 0) {
			    curr_buf->curr_window_line--;
			}
			curr_buf->currln--;
		    }
		    if (curr_buf->currline == curr_buf->top_of_win)
			curr_buf->top_of_win = p;

		    delete_line(curr_buf->currline, 1);
		    curr_buf->currline = p;
		    curr_buf->redraw_everything = YEA;
		    adjustline(curr_buf->currline, WRAPMARGIN);
		    break;
		}
		else if (curr_buf->currline->len == curr_buf->currpnt) {
		    join(curr_buf->currline);
		    curr_buf->redraw_everything = YEA;
		    break;
		}
		curr_buf->currline->len = curr_buf->currpnt;
		curr_buf->currline->data[curr_buf->currpnt] = '\0';
		break;
	    }

	    if (curr_buf->currln < 0)
		curr_buf->currln = 0;

	    edit_window_adjust();
#ifdef DBCSAWARE
	    if(mbcs_mode)
	      curr_buf->currpnt = fix_cursor(curr_buf->currline->data, curr_buf->currpnt, FC_LEFT);
#endif
	}

	if (curr_buf->ansimode)
	    tmp = n2ansi(curr_buf->currpnt, curr_buf->currline);
	else
	    tmp = curr_buf->currpnt;

	if (tmp < t_columns - 1)
	    curr_buf->edit_margin = 0;
	else
	    curr_buf->edit_margin = tmp / (t_columns - 8) * (t_columns - 8);

	if (!curr_buf->redraw_everything) {
	    if (curr_buf->edit_margin != curr_buf->last_margin) {
		curr_buf->last_margin = curr_buf->edit_margin;
		curr_buf->redraw_everything = YEA;
	    } else {
		move(curr_buf->curr_window_line, 0);
		clrtoeol();
		if (curr_buf->ansimode)
		    outs(curr_buf->currline->data);
		else
		{
		    int attr = EOATTR_NORMAL;
		    attr |= detect_attr(curr_buf->currline->data, curr_buf->currline->len);
		    edit_outs_attr(&curr_buf->currline->data[curr_buf->edit_margin], attr);
		}
		outs(ANSI_RESET ANSI_CLRTOEND);
		edit_msg();
	    }
	} else {
	    curr_buf->last_margin = curr_buf->edit_margin;
	}
    } /* main event loop */

    exit_edit_buffer();
}

int
vedit(const char *fpath, int saveheader, char title[STRLEN])
{
    assert(title);
    return vedit2(fpath, saveheader, title, EDITFLAG_ALLOWTITLE);
}

/**
 * �s��@���ɮ� (�D�ݪO�峹/�H��).
 *
 * �]�����|�� header, title, local save ���Ѽ�.
 */
int
veditfile(const char *fpath)
{
    return vedit2(fpath, NA, NULL, 0);
}

/* vim:sw=4:nofoldenable
 */