#include "bbs.h"
#include "daemons.h"

#define QCAST   int (*)(const void *, const void *)

static char    * const sig_des[] = {
    "", "���", "", "���l��", "�H��", "�t��", "���", "�¥մ�", "���l�X",
};
static char    * const withme_str[] = {
  "�ͤ�", "���l��", "", "�H��", "�t��", "���", "�¥մ�", "���l�X", NULL
};

#define MAX_SHOW_MODE 7
/* M_INT: monitor mode update interval */
#define M_INT 15
/* P_INT: interval to check for page req. in talk/chat */
#define P_INT 20
#define BOARDFRI  1

typedef struct pickup_t {
    userinfo_t     *ui;
    int             friend, uoffset;
}               pickup_t;

// If you want to change this, you have to change shm structure and shmctl.c
#define PICKUP_WAYS     8

static char    * const fcolor[11] = {
    NULL, ANSI_COLOR(36), ANSI_COLOR(32), ANSI_COLOR(1;32),
    ANSI_COLOR(33), ANSI_COLOR(1;33), ANSI_COLOR(1;37), ANSI_COLOR(1;37),
    ANSI_COLOR(31), ANSI_COLOR(1;35), ANSI_COLOR(1;36)
};

static userinfo_t *uip;

int
iswritable_stat(const userinfo_t * uentp, int fri_stat)
{
    if (uentp == currutmp)
	return 0;

    if (HasUserPerm(PERM_SYSOP))
	return 1;

    if (!HasBasicUserPerm(PERM_LOGINOK) || HasUserPerm(PERM_VIOLATELAW))
	return 0;

    return (uentp->pager != PAGER_ANTIWB &&
	    (fri_stat & HFM || uentp->pager != PAGER_FRIENDONLY));
}

int
isvisible_stat(const userinfo_t * me, const userinfo_t * uentp, int fri_stat)
{
    if (!uentp || uentp->userid[0] == 0)
	return 0;

    /* to avoid paranoid users get crazy*/
    if (uentp->mode == DEBUGSLEEPING)
	return 0;

    if (PERM_HIDE(uentp) && !(PERM_HIDE(me)))	/* ��赵�����ΦӧA�S�� */
	return 0;
    else if ((me->userlevel & PERM_SYSOP) ||
	     ((fri_stat & HRM) && (fri_stat & HFM)))
	/* �����ݪ�������H */
	return 1;

    if (uentp->invisible && !(me->userlevel & PERM_SEECLOAK))
	return 0;

    return !(fri_stat & HRM);
}

int query_online(const char *userid)
{
    userinfo_t *uentp;

    if (!userid || !*userid)
	return 0;

    if (!isalnum(*userid))
	return 0;

    if (strchr(userid, '.') || SHM->GV2.e.noonlineuser)
	return 0;

    uentp = search_ulist_userid(userid);

    if (!uentp ||!isvisible(currutmp, uentp))
	return 0;

    return 1;
}

const char           *
modestring(const userinfo_t * uentp, int simple)
{
    static char     modestr[40];
    const char *    notonline = "���b���W";
    register int    mode = uentp->mode;
    register char  *word;
    int             fri_stat;

    /* for debugging */
    if (mode >= MAX_MODES) {
	syslog(LOG_WARNING, "what!? mode = %d", mode);
	word = ModeTypeTable[mode % MAX_MODES];
    } else
	word = ModeTypeTable[mode];

    fri_stat = friend_stat(currutmp, uentp);
    if (!(HasUserPerm(PERM_SYSOP) || HasUserPerm(PERM_SEECLOAK)) &&
	((uentp->invisible || (fri_stat & HRM)) &&
	 !((fri_stat & HFM) && (fri_stat & HRM))))
	return notonline;
    else if (mode == EDITING) {
	snprintf(modestr, sizeof(modestr), "E:%s",
		ModeTypeTable[uentp->destuid < EDITING ? uentp->destuid :
			      EDITING]);
	word = modestr;
    } else if (!mode && *uentp->chatid == 1) {
	if (!simple)
	    snprintf(modestr, sizeof(modestr), "�^�� %s",
		    isvisible_uid(uentp->destuid) ?
		    getuserid(uentp->destuid) : "�Ů�");
	else
	    snprintf(modestr, sizeof(modestr), "�^���I�s");
    }
    else if (!mode && *uentp->chatid == 3)
	snprintf(modestr, sizeof(modestr), "���y�ǳƤ�");
    else if (
#ifdef NOKILLWATERBALL
	     uentp->msgcount > 0
#else
	     (!mode) && *uentp->chatid == 2
#endif
	     )
	if (uentp->msgcount < 10) {
	    const char *cnum[10] =
	    {"", "�@", "��", "�T", "�|", "��",
		 "��", "�C", "�K", "�E"};
	    snprintf(modestr, sizeof(modestr),
		     "��%s�����y", cnum[(int)(uentp->msgcount)]);
	} else
	    snprintf(modestr, sizeof(modestr), "����F @_@");
    else if (!mode)
	return (uentp->destuid == 6) ? uentp->chatid : "�o�b��";

    else if (simple)
	return word;
    else if (uentp->in_chat && mode == CHATING)
	snprintf(modestr, sizeof(modestr), "%s (%s)", word, uentp->chatid);
    else if (mode == TALK || mode == M_FIVE || mode == CHC || mode == UMODE_GO
	    || mode == DARK || mode == M_CONN6) {
	if (!isvisible_uid(uentp->destuid))	/* Leeym ���(����)���� */
	    snprintf(modestr, sizeof(modestr), "%s �Ů�", word);
	/* Leeym * �j�a�ۤv�o���a�I */
	else
	    snprintf(modestr, sizeof(modestr),
		     "%s %s", word, getuserid(uentp->destuid));
    } else if (mode == CHESSWATCHING) {
	snprintf(modestr, sizeof(modestr), "�[��");
    } else if (mode != PAGE && mode != TQUERY)
	return word;
    else
	snprintf(modestr, sizeof(modestr),
		 "%s %s", word, getuserid(uentp->destuid));

    return (modestr);
}

unsigned int
set_friend_bit(const userinfo_t * me, const userinfo_t * ui)
{
    int             unum;
    unsigned int hit = 0;
    const int *myfriends;

    /* �P�_���O�_���ڪ��B�� ? */
    if( intbsearch(ui->uid, me->myfriend, me->nFriends) )
	hit = IFH;

    /* �P�_�ڬO�_����誺�B�� ? */
    if( intbsearch(me->uid, ui->myfriend, ui->nFriends) )
	hit |= HFM;

    /* �P�_���O�_���ڪ����H ? */
    myfriends = me->reject;
    while ((unum = *myfriends++)) {
	if (unum == ui->uid) {
	    hit |= IRH;
	    break;
	}
    }

    /* �P�_�ڬO�_����誺���H ? */
    myfriends = ui->reject;
    while ((unum = *myfriends++)) {
	if (unum == me->uid) {
	    hit |= HRM;
	    break;
	}
    }
    return hit;
}

int
reverse_friend_stat(int stat)
{
    int             stat1 = 0;
    if (stat & IFH)
	stat1 |= HFM;
    if (stat & IRH)
	stat1 |= HRM;
    if (stat & HFM)
	stat1 |= IFH;
    if (stat & HRM)
	stat1 |= IRH;
    if (stat & IBH)
	stat1 |= IBH;
    return stat1;
}

#ifdef UTMPD
int sync_outta_server(int sfd, int do_login)
{
    int i;
    int offset = (int)(currutmp - &SHM->uinfo[0]);

    int cmd, res;
    int nfs;
    ocfs_t  fs[MAX_FRIEND*2];

    cmd = -2;
    if(towrite(sfd, &cmd, sizeof(cmd)) < 0 ||
	    towrite(sfd, &offset, sizeof(offset)) < 0 ||
	    towrite(sfd, &currutmp->uid, sizeof(currutmp->uid)) < 0 ||
	    towrite(sfd, currutmp->myfriend, sizeof(currutmp->myfriend)) < 0 ||
	    towrite(sfd, currutmp->reject, sizeof(currutmp->reject)) < 0)
	return -1;

    if(toread(sfd, &res, sizeof(res)) < 0)
	return -1;

    if(res<0)
	return -1;

    // when we are not doing real login (ex, ctrl-u a or ctrl-u d)
    // the frequency check should be avoided.
    if (!do_login)
    {
	sleep(3);   // utmpserver usually treat 3 seconds as flooding.
	if (res == 2 || res == 1)
	    res = 0;
    }

    if(res==2) {
	close(sfd);
	outs("�n�J���W�c, ���קK�t�έt���L��, �еy��A��\n");
	refresh();
	log_usies("REJECTLOGIN", NULL);
        // We can't do u_exit because some resources like friends are not ready.
        currmode = 0;
	memset(currutmp, 0, sizeof(userinfo_t));
        // user will try to disconnect here and cause abort_bbs.
	sleep(30);
	exit(0);
    }

    if(toread(sfd, &nfs, sizeof(nfs)) < 0)
	return -1;
    if(nfs<0 || nfs>MAX_FRIEND*2) {
	fprintf(stderr, "invalid nfs=%d\n",nfs);
	return -1;
    }

    if(toread(sfd, fs, sizeof(fs[0])*nfs) < 0)
	return -1;

    close(sfd);

    for(i=0; i<nfs; i++) {
	if( SHM->uinfo[fs[i].index].uid != fs[i].uid )
	    continue; // double check, server may not know user have logout
	currutmp->friend_online[currutmp->friendtotal++]
	    = fs[i].friendstat;
	/* XXX: race here */
	if( SHM->uinfo[fs[i].index].friendtotal < MAX_FRIEND )
	    SHM->uinfo[fs[i].index].friend_online[ SHM->uinfo[fs[i].index].friendtotal++ ] = fs[i].rfriendstat;
    }

    if(res==1) {
	vmsg("�Ф��W�c�n�J�H�K�y���t�ιL�׭t��");
    }
    return 0;
}
#endif

void login_friend_online(int do_login)
{
    userinfo_t     *uentp;
    int             i;
    unsigned int    stat, stat1;
    int             offset = (int)(currutmp - &SHM->uinfo[0]);

#ifdef UTMPD
    int sfd;
    /* UTMPD is TOO slow, let's prompt user here. */
    move(b_lines-2, 0); clrtobot();
    outs("\n���b��s�P�P�B�u�W�ϥΪ̤Φn�ͦW��A�t�έt���q�j�ɷ|�ݮɸ��[...\n");
    refresh();

    sfd = toconnect(UTMPD_ADDR);
    if(sfd>=0) {
	int res=sync_outta_server(sfd, do_login);
	if(res==0) // sfd will be closed if return 0
	    return;
	close(sfd);
    }
#endif

    for (i = 0; i < SHM->UTMPnumber && currutmp->friendtotal < MAX_FRIEND; i++) {
	uentp = (&SHM->uinfo[SHM->sorted[SHM->currsorted][0][i]]);
	if (uentp && uentp->uid && (stat = set_friend_bit(currutmp, uentp))) {
	    stat1 = reverse_friend_stat(stat);
	    stat <<= 24;
	    stat |= (int)(uentp - &SHM->uinfo[0]);
	    currutmp->friend_online[currutmp->friendtotal++] = stat;
	    if (uentp != currutmp && uentp->friendtotal < MAX_FRIEND) {
		stat1 <<= 24;
		stat1 |= offset;
		uentp->friend_online[uentp->friendtotal++] = stat1;
	    }
	}
    }
    return;
}

/* TODO merge with util/shmctl.c logout_friend_online() */
int
logout_friend_online(userinfo_t * utmp)
{
    int my_friend_idx, thefriend;
    int k;
    int             offset = (int)(utmp - &SHM->uinfo[0]);
    userinfo_t     *ui;
    for(; utmp->friendtotal>0; utmp->friendtotal--) {
	if( !(0 <= utmp->friendtotal && utmp->friendtotal < MAX_FRIEND) )
	    return 1;
	my_friend_idx=utmp->friendtotal-1;
	thefriend = (utmp->friend_online[my_friend_idx] & 0xFFFFFF);
	utmp->friend_online[my_friend_idx]=0;

	if( !(0 <= thefriend && thefriend < USHM_SIZE) )
	    continue;

	ui = &SHM->uinfo[thefriend];
	if(ui->pid==0 || ui==utmp)
	    continue;
	if(ui->friendtotal > MAX_FRIEND || ui->friendtotal<0)
	    continue;
	for (k = 0; k < ui->friendtotal && k < MAX_FRIEND &&
	    (int)(ui->friend_online[k] & 0xFFFFFF) != offset; k++);
	if (k < ui->friendtotal && k < MAX_FRIEND) {
	  ui->friendtotal--;
	  ui->friend_online[k] = ui->friend_online[ui->friendtotal];
	  ui->friend_online[ui->friendtotal] = 0;
	}
    }
    return 0;
}


int
friend_stat(const userinfo_t * me, const userinfo_t * ui)
{
    int             i, j;
    unsigned int    hit = 0;
    /* �ݪO�n�� (�b�P�ݪO���䥦�ϥΪ�) */
    if (me->brc_id && ui->brc_id == me->brc_id) {
	hit = IBH;
    }
    for (i = 0; me->friend_online[i] && i < MAX_FRIEND; i++) {
	j = (me->friend_online[i] & 0xFFFFFF);
	if (VALID_USHM_ENTRY(j) && ui == &SHM->uinfo[j]) {
	    hit |= me->friend_online[i] >> 24;
	    break;
	}
    }
    if (PERM_HIDE(ui))
	return hit & ST_FRIEND;
    return hit;
}

int
isvisible_uid(int tuid)
{
    userinfo_t     *uentp;

    if (!tuid || !(uentp = search_ulist(tuid)))
	return 1;
    return isvisible(currutmp, uentp);
}

/* �u��ʧ@ */
static void
my_kick(userinfo_t * uentp)
{
    char            genbuf[200];

    getdata(1, 0, msg_sure_ny, genbuf, 4, LCECHO);
    clrtoeol();
    if (genbuf[0] == 'y') {
	snprintf(genbuf, sizeof(genbuf),
		 "%s (%s)", uentp->userid, uentp->nickname);
	log_usies("KICK ", genbuf);
	if ((uentp->pid <= 0 || kill(uentp->pid, SIGHUP) == -1) && (errno == ESRCH))
	    purge_utmp(uentp);
	outs("��X�h�o");
    } else
	outs(msg_cancel);
    pressanykey();
}

int
my_query(const char *uident)
{
    userec_t        muser;
    int             tuid, fri_stat = 0;
    int		    is_self = 0;
    userinfo_t     *uentp;
    static time_t last_query;

    BEGINSTAT(STAT_QUERY);
    if ((tuid = getuser(uident, &muser))) {
	move(1, 0);
	clrtobot();
	move(1, 0);
	setutmpmode(TQUERY);
	currutmp->destuid = tuid;
	reload_money();

	if ((uentp = (userinfo_t *) search_ulist(tuid)))
	    fri_stat = friend_stat(currutmp, uentp);
	if (strcmp(muser.userid, cuser.userid) == 0)
	    is_self =1;

	// ------------------------------------------------------------

	prints( "�m�עҼʺ١n%s (%s)%*s",
	       muser.userid,
	       muser.nickname,
	       strlen(muser.userid) + strlen(muser.nickname) >= 25 ? 0 :
		   (int)(25 - strlen(muser.userid) - strlen(muser.nickname)), "");

	prints( "�m�g�٪��p�n%s",
	       money_level(muser.money));
	if (uentp && ((fri_stat & HFM && !uentp->invisible) || is_self))
	    prints(" ($%d)", muser.money);
	outc('\n');

	// ------------------------------------------------------------

	prints("�m" STR_LOGINDAYS "�n%d " STR_LOGINDAYS_QTY, muser.numlogindays);
#ifdef SHOW_LOGINOK
	if (!(muser.userlevel & PERM_LOGINOK))
	    outs(" (�|���q�L�{��)");
        else
#endif
            outs(" (�P�Ѥ��u�p�@��)");

	move(vgety(), 40);
	prints("�m���Ĥ峹�n%d �g", muser.numposts);
#ifdef ASSESS
	prints(" (�h:%d)", muser.badpost);
#endif
	outc('\n');

	// ------------------------------------------------------------

	prints(ANSI_COLOR(1;33) "�m�ثe�ʺA�n%-28.28s" ANSI_RESET,
	       (uentp && isvisible_stat(currutmp, uentp, fri_stat)) ?
		   modestring(uentp, 0) : "���b���W");

	if ((uentp && ISNEWMAIL(uentp)) || load_mailalert(muser.userid))
	    outs("�m�p�H�H�c�n���s�i�H���٨S��\n");
	else
	    outs("�m�p�H�H�c�n�̪�L�s�H��\n");

	// ------------------------------------------------------------
        if (muser.role & ROLE_HIDE_FROM) {
            // do nothing
        } else {
            prints("�m�W���W���n%-28.28s�m�W���G�m�n",
                   PERM_HIDE(&muser) ? "���K" :
                   Cdate(muser.lastseen ? &muser.lastseen : &muser.lastlogin));
            // print out muser.lasthost
#ifdef USE_MASKED_FROMHOST
            if(!HasUserPerm(PERM_SYSOP|PERM_ACCOUNTS))
                obfuscate_ipstr(muser.lasthost);
#endif // !USE_MASKED_FROMHOST
            outs(muser.lasthost[0] ? muser.lasthost : "(����)");
            outs("\n");
        }

	// ------------------------------------------------------------

	prints("�m ���l�� �n%5d �� %5d �� %5d �M  "
	       "�m�H�Ѿ��Z�n%5d �� %5d �� %5d �M\n",
	       muser.five_win, muser.five_lose, muser.five_tie,
	       muser.chc_win, muser.chc_lose, muser.chc_tie);

	showplans_userec(&muser);

	ENDSTAT(STAT_QUERY);

	if(HasUserPerm(PERM_SYSOP|PERM_POLICE) )
	{
          if(vmsg("T: �}�߻@��")=='T')
		  violate_law(&muser, tuid);
	}
	else
	   pressanykey();
	if(now-last_query<1)
	    sleep(2);
	else if(now-last_query<2)
	    sleep(1);
	last_query=now;
	return FULLUPDATE;
    }
    else {
	ENDSTAT(STAT_QUERY);
    }

    return DONOTHING;
}

static char     t_last_write[80];

void check_water_init(void)
{
    if(water==NULL) {
	water = (water_t*)malloc(sizeof(water_t) * (WB_OFO_USER_NUM + 1));
	memset(water, 0, sizeof(water_t) * (WB_OFO_USER_NUM + 1));
	water_which = &water[0];

	strlcpy(water[0].userid, " ���� ", sizeof(water[0].userid));
    }
}

static void
ofo_water_scr(const water_t * tw, int which, char type)
{
    if (type == 1) {
	int             i;
	const int colors[] = {33, 37, 33, 37, 33};
	move(8 + which, 28);
	SOLVE_ANSI_CACHE();
	prints(ANSI_COLOR(0;1;37;45) "  %c %-14s " ANSI_RESET,
	       tw->uin ? ' ' : 'x',
	       tw->userid);
	for (i = 0; i < 5; ++i) {
	    move(16 + i, 4);
            SOLVE_ANSI_CACHE();
	    if (tw->msg[(tw->top - i + 4) % 5].last_call_in[0] != 0)
		prints("   " ANSI_COLOR(1;%d;44) "��%-64s" ANSI_RESET "   \n",
		       colors[i],
		       tw->msg[(tw->top - i + 4) % 5].last_call_in);
	    else
		outs("�@\n");
	}

	move(21, 4);
	SOLVE_ANSI_CACHE();
	prints("   " ANSI_COLOR(1;37;46) "%-66s" ANSI_RESET "   \n",
	       tw->msg[5].last_call_in);

	move(0, 0);
	SOLVE_ANSI_CACHE();
	clrtoeol();
#ifdef PLAY_ANGEL
	if (tw->msg[0].msgmode == MSGMODE_TOANGEL)
	    outs("�^���p�D�H: ");
	else
#endif
	prints("���� %s: ", tw->userid);
    } else {

	move(8 + which, 28);
	SOLVE_ANSI_CACHE();
	prints(ANSI_COLOR(0;1;37;44) "  %c %-13s�@" ANSI_RESET,
	       tw->uin ? ' ' : 'x',
	       tw->userid);
    }
}

void
ofo_my_write(void)
{
    int             i, ch, currstat0;
    char            genbuf[256], msg[80], done = 0, c0, which;
    water_t        *tw;
    unsigned char   mode0;

    check_water_init();
    if (swater[0] == NULL)
	return;
    wmofo = REPLYING;
    currstat0 = currstat;
    c0 = currutmp->chatid[0];
    mode0 = currutmp->mode;
    currutmp->mode = 0;
    currutmp->chatid[0] = 3;
    currstat = DBACK;

    //init screen
    move(WB_OFO_USER_TOP-1, 0);
    SOLVE_ANSI_CACHE();
    clrtoln(WB_OFO_MSG_BOTTOM+1);
    SOLVE_ANSI_CACHE();
    // extra refresh to solve stupid screen.c escape caching issue
#ifndef USE_PFTERM
    refresh();
#endif
    mvouts(WB_OFO_USER_TOP, WB_OFO_USER_LEFT,
           ANSI_COLOR(1;33;46) " �� ���y������H ��" ANSI_RESET);
    for (i = 0; i < WB_OFO_USER_NUM; ++i)
	if (swater[i] == NULL || swater[i]->pid == 0)
	    break;
	else {
	    if (swater[i]->uin &&
		(swater[i]->pid != swater[i]->uin->pid ||
		 swater[i]->userid[0] != swater[i]->uin->userid[0]))
		swater[i]->uin = search_ulist_pid(swater[i]->pid);
	    ofo_water_scr(swater[i], i, 0);
	}
    move(WB_OFO_MSG_TOP, WB_OFO_MSG_LEFT);
    outs(ANSI_RESET " " ANSI_COLOR(1;35) "��" ANSI_COLOR(1;36)
         "�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w"
         ANSI_COLOR(1;35) "��" ANSI_RESET " ");
    move(WB_OFO_MSG_BOTTOM, WB_OFO_MSG_LEFT);
    outs(" " ANSI_COLOR(1;35) "��" ANSI_COLOR(1;36)
         "�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w"
         ANSI_COLOR(1;35) "��" ANSI_RESET " ");
    ofo_water_scr(swater[0], 0, 1);
    refresh();

    which = 0;
    do {
	switch ((ch = vkey())) {
	case Ctrl('T'):
	case KEY_UP:
	    if (water_usies != 1) {
		assert(0 < water_usies && water_usies <= WB_OFO_USER_NUM);
		ofo_water_scr(swater[(int)which], which, 0);
		which = (which - 1 + water_usies) % water_usies;
		ofo_water_scr(swater[(int)which], which, 1);
		refresh();
	    }
	    break;

	case KEY_DOWN:
	case Ctrl('R'):
	    if (water_usies != 1) {
		assert(0 < water_usies && water_usies <= WB_OFO_USER_NUM);
		ofo_water_scr(swater[(int)which], which, 0);
		which = (which + 1 + water_usies) % water_usies;
		ofo_water_scr(swater[(int)which], which, 1);
		refresh();
	    }
	    break;

	case KEY_LEFT:
	    done = 1;
	    break;

	case KEY_UNKNOWN:
	    break;

	default:
	    done = 1;
	    tw = swater[(int)which];

	    if (!tw->uin)
		break;

            // TODO(piaip) �o�̫ܦM�I�C�ϥΪ̥i��X���ö�F��i buf.
            // �t�~ KEY_UP �������O >0xFF �ҥH�γ\�ڭ̸ӹ��d�@�U isascii ���P�_�C
            if ((ch < 0x100 && !isascii(ch)) || isprint(ch)) {
		msg[0] = ch, msg[1] = 0;
	    } else
		msg[0] = 0;
	    move(0, 0);
            SOLVE_ANSI_CACHE();
	    outs(ANSI_RESET);
	    clrtoeol();
#ifdef PLAY_ANGEL
            switch(tw->msg[0].msgmode) {
                case MSGMODE_WRITE:
                case MSGMODE_ALOHA:
                    snprintf(genbuf, sizeof(genbuf), "���� %s:", tw->userid);
                    i = WATERBALL_CONFIRM;
                    break;

                case MSGMODE_TOANGEL:
                    strlcpy(genbuf, "�^���p�D�H:", sizeof(genbuf));
                    i = WATERBALL_CONFIRM_ANSWER;
                    break;

                case MSGMODE_FROMANGEL:
                    strlcpy(genbuf, "�A�ݥL�@���G", sizeof(genbuf));
                    i = WATERBALL_CONFIRM_ANGEL;
                    break;
            }
#else
	    snprintf(genbuf, sizeof(genbuf), "���� %s:", tw->userid);
	    i = WATERBALL_CONFIRM;
#endif
	    if (!getdata_buf(0, 0, genbuf, msg,
                             80 - strlen(tw->userid) - 6, DOECHO))
		break;

	    if (my_write(tw->pid, msg, tw->userid, i, tw->uin))
		strlcpy(tw->msg[5].last_call_in, t_last_write,
			sizeof(tw->msg[5].last_call_in));
	    break;
	}
    } while (!done);

    currstat = currstat0;
    currutmp->chatid[0] = c0;
    currutmp->mode = mode0;
    if (wmofo == RECVINREPLYING) {
	wmofo = NOTREPLYING;
	write_request(0);
    }
    wmofo = NOTREPLYING;
}

/*
 * �Q�I�s���ɾ�:
 * 1. ��s�դ��y flag = WATERBALL_PREEDIT, 1 (pre-edit)
 * 2. �^���y     flag = WATERBALL_GENERAL, 0
 * 3. �W��aloha  flag = WATERBALL_ALOHA,   2 (pre-edit)
 * 4. �s��       flag = WATERBALL_SYSOP,   3 if SYSOP
 *               flag = WATERBALL_PREEDIT, 1 otherwise
 * 5. ����y     flag = WATERBALL_GENERAL, 0
 * 6. ofo_my_write  flag = WATERBALL_CONFIRM, 4 (pre-edit but confirm)
 * 7. (when defined PLAY_ANGEL)
 *    �I�s�p�Ѩ� flag = WATERBALL_ANGEL,   5 (id = "�p�Ѩ�")
 * 8. (when defined PLAY_ANGEL)
 *    �^���p�D�H flag = WATERBALL_ANSWER,  6 (���� id)
 * 9. (when defined PLAY_ANGEL)
 *    �I�s�p�Ѩ� flag = WATERBALL_CONFIRM_ANGEL, 7 (pre-edit)
 * 10. (when defined PLAY_ANGEL)
 *    �^���p�D�H flag = WATERBALL_CONFIRM_ANSWER, 8 (pre-edit)
 */
int
my_write(pid_t pid, const char *prompt, const char *id, int flag, userinfo_t * puin)
{
    int             len, currstat0 = currstat, fri_stat = -1;
    char            msg[80], destid[IDLEN + 1];
    char            genbuf[200], buf[200], c0 = currutmp->chatid[0];
    unsigned char   mode0 = currutmp->mode;
    userinfo_t     *uin;
    uin = (puin != NULL) ? puin : (userinfo_t *) search_ulist_pid(pid);
    strlcpy(destid, id, sizeof(destid));
    check_water_init();

    /* what if uin is NULL but other conditions are not true?
     * will this situation cause SEGV?
     * should this "!uin &&" replaced by "!uin ||" ?
     */
    if ((!uin || !uin->userid[0]) && !((flag == WATERBALL_GENERAL
#ifdef PLAY_ANGEL
		|| flag == WATERBALL_ANGEL || flag == WATERBALL_ANSWER
#endif
            )
	    && water_which->count > 0)) {
	vmsg("�V�|! ���w���]�F(���b���W)! ");
	watermode = -1;
	return 0;
    }
    currutmp->mode = 0;
    currutmp->chatid[0] = 3;
    currstat = DBACK;

    if (flag == WATERBALL_GENERAL
#ifdef PLAY_ANGEL
	    || flag == WATERBALL_ANGEL || flag == WATERBALL_ANSWER
#endif
	    ) {
	/* �@����y */
	watermode = 0;

	switch(currutmp->pager)
	{
	    case PAGER_DISABLE:
	    case PAGER_ANTIWB:
                if (HasUserPerm(PERM_SYSOP | PERM_ACCOUNTS | PERM_BOARD)) {
                    // Admins are free to bother people.
                    move(1, 0);  clrtoeol();
                    outs(ANSI_COLOR(1;31)
                         "�A���I�s���ثe�������O�H����y�A���i��L�k�^�ܡC"
                         ANSI_RESET);
                } else {
                    // Normal users should not bother people.
                    if ('n' == vans("�z���I�s���ثe�]�w�������C"
                                    "�n���}����?[Y/n] "))
                        return 0;
                    // enable pager
                    currutmp->pager = PAGER_ON;
                }
		break;

	    case PAGER_FRIENDONLY:
#if 0
		// �p�G��西�b�U���A�o�Ӧn������í�| crash (?) */
		if (uin && uin->userid[0])
		{
		    fri_stat = friend_stat(currutmp, uin);
		    if(fri_stat & HFM)
			break;
		}
#endif
		move(1, 0);  clrtoeol();
		outs(ANSI_COLOR(1;31) "�A���I�s���ثe�u�����n�ͥ���y�A�Y���D�n�ͫh�i��L�k�^�ܡC" ANSI_RESET);
		break;
	}

	if (!(len = getdata(0, 0, prompt, msg, 56, DOECHO))) {
	    currutmp->chatid[0] = c0;
	    currutmp->mode = mode0;
	    currstat = currstat0;
	    watermode = -1;
	    return 0;
	}

	if (watermode > 0) {
	    int             i;

	    i = (water_which->top - watermode + MAX_REVIEW) % MAX_REVIEW;
	    uin = (userinfo_t *) search_ulist_pid(water_which->msg[i].pid);
#ifdef PLAY_ANGEL
	    if (water_which->msg[i].msgmode == MSGMODE_FROMANGEL)
		flag = WATERBALL_ANGEL;
	    else if (water_which->msg[i].msgmode == MSGMODE_TOANGEL)
		flag = WATERBALL_ANSWER;
	    else
		flag = WATERBALL_GENERAL;
#endif
	    strlcpy(destid, water_which->msg[i].userid, sizeof(destid));
	}
    } else {
	/* pre-edit �����y */
	strlcpy(msg, prompt, sizeof(msg));
	len = strlen(msg);
    }

    strip_ansi(msg, msg, STRIP_ALL);
    if (uin && *uin->userid &&
	    (flag == WATERBALL_GENERAL || flag == WATERBALL_CONFIRM
#ifdef PLAY_ANGEL
	     || flag == WATERBALL_ANGEL || flag == WATERBALL_ANSWER
	     || flag == WATERBALL_CONFIRM_ANGEL
	     || flag == WATERBALL_CONFIRM_ANSWER
#endif
	     ))
    {
	snprintf(buf, sizeof(buf), "�� %s: %s [Y/n]?", destid, msg);

	getdata(0, 0, buf, genbuf, 3, LCECHO);
	if (genbuf[0] == 'n') {
	    currutmp->chatid[0] = c0;
	    currutmp->mode = mode0;
	    currstat = currstat0;
	    watermode = -1;
	    return 0;
	}
    }
    watermode = -1;
    if (!uin || !*uin->userid || (strcasecmp(destid, uin->userid)
#ifdef PLAY_ANGEL
	    && flag != WATERBALL_ANGEL && flag != WATERBALL_CONFIRM_ANGEL) ||
	    // check if user is changed of angelpause.
	    // XXX if flag == WATERBALL_ANGEL, shuold be (uin->angelpause) only.
	    ((flag == WATERBALL_ANGEL || flag == WATERBALL_CONFIRM_ANGEL)
             && (strcasecmp(cuser.myangel, uin->userid) ||
                 uin->angelpause >= ANGELPAUSE_REJALL)
#endif
	    )) {
	bell();
	vmsg("�V�|! ���w���]�F(���b���W)! ");
	currutmp->chatid[0] = c0;
	currutmp->mode = mode0;
	currstat = currstat0;
	return 0;
    }
    if(fri_stat < 0)
	fri_stat = friend_stat(currutmp, uin);
    // else, fri_stat was already calculated. */

    if (flag != WATERBALL_ALOHA) {	/* aloha �����y���Φs�U�� */
	/* �s��ۤv�����y�� */
	if (!fp_writelog) {
	    sethomefile(genbuf, cuser.userid, fn_writelog);
	    fp_writelog = fopen(genbuf, "a");
	}
	if (fp_writelog) {
	    fprintf(fp_writelog, "To %s: %s [%s]\n",
		    destid, msg, Cdatelite(&now));
	    snprintf(t_last_write, 66, "To %s: %s", destid, msg);
	} else {
            vmsg("��p�A�ثe�t�β��`�A�ȮɵL�k�ǰe��ơC");
            return 0;
        }
    }
    if (flag == WATERBALL_SYSOP && uin->msgcount) {
	/* ���� */
	uin->destuip = currutmp - &SHM->uinfo[0];
	uin->sig = 2;
	if (uin->pid > 0)
	    kill(uin->pid, SIGUSR1);
    } else if ((flag != WATERBALL_ALOHA &&
#ifdef PLAY_ANGEL
	       flag != WATERBALL_ANGEL &&
	       flag != WATERBALL_ANSWER &&
	       flag != WATERBALL_CONFIRM_ANGEL &&
	       flag != WATERBALL_CONFIRM_ANSWER &&
	       /* Angel accept or not is checked outside.
		* Avoiding new users don't know what pager is. */
#endif
	       !HasUserPerm(PERM_SYSOP) &&
	       (uin->pager == PAGER_ANTIWB ||
		uin->pager == PAGER_DISABLE ||
		(uin->pager == PAGER_FRIENDONLY &&
		 !(fri_stat & HFM))))
#ifdef PLAY_ANGEL
	       || ((flag == WATERBALL_ANGEL || flag == WATERBALL_CONFIRM_ANGEL)
		   && angel_reject_me(uin))
#endif
	       ) {
	outmsg(ANSI_COLOR(1;33;41) "�V�|! ��訾���F! " ANSI_COLOR(37) "~>_<~" ANSI_RESET);
    } else {
	int     write_pos = uin->msgcount; /* try to avoid race */
	if ( write_pos < (MAX_MSGS - 1) ) { /* race here */
	    unsigned char   pager0 = uin->pager;

	    uin->msgcount = write_pos + 1;
	    uin->pager = PAGER_DISABLE;
	    uin->msgs[write_pos].pid = currpid;
#ifdef PLAY_ANGEL
	    if (flag == WATERBALL_ANSWER || flag == WATERBALL_CONFIRM_ANSWER)
                angel_load_my_fullnick(uin->msgs[write_pos].userid,
                                       sizeof(uin->msgs[write_pos].userid));
	    else
#endif
            strlcpy(uin->msgs[write_pos].userid, cuser.userid,
                    sizeof(uin->msgs[write_pos].userid));
	    strlcpy(uin->msgs[write_pos].last_call_in, msg,
		    sizeof(uin->msgs[write_pos].last_call_in));
	    switch (flag) {
#ifdef PLAY_ANGEL
		case WATERBALL_ANGEL:
                    angel_log_msg_to_angel();
		    uin->msgs[write_pos].msgmode = MSGMODE_TOANGEL;
		    break;

		case WATERBALL_CONFIRM_ANGEL:
		    uin->msgs[write_pos].msgmode = MSGMODE_TOANGEL;
		    break;

		case WATERBALL_ANSWER:
		case WATERBALL_CONFIRM_ANSWER:
		    uin->msgs[write_pos].msgmode = MSGMODE_FROMANGEL;
		    break;
#endif
                case WATERBALL_ALOHA:
                    uin->msgs[write_pos].msgmode = MSGMODE_ALOHA;
                    break;

                default:
                    uin->msgs[write_pos].msgmode = MSGMODE_WRITE;
                    break;
            }
	    uin->pager = pager0;
	} else if (flag != WATERBALL_ALOHA)
	    outmsg(ANSI_COLOR(1;33;41) "�V�|! ��褣��F! (����Ӧh���y) " ANSI_COLOR(37) "@_@" ANSI_RESET);

	if (uin->msgcount >= 1 &&
#ifdef NOKILLWATERBALL
	    !(uin->wbtime = now) /* race */
#else
	    (uin->pid <= 0 || kill(uin->pid, SIGUSR2) == -1)
#endif
	    && flag != WATERBALL_ALOHA)
	    outmsg(ANSI_COLOR(1;33;41) "�V�|! �S����! " ANSI_COLOR(37) "~>_<~" ANSI_RESET);
	else if (uin->msgcount == 1 && flag != WATERBALL_ALOHA)
	    outmsg(ANSI_COLOR(1;33;44) "���y�{�L�h�F! " ANSI_COLOR(37) "*^o^*" ANSI_RESET);
	else if (uin->msgcount > 1 && uin->msgcount < MAX_MSGS &&
		flag != WATERBALL_ALOHA)
	    outmsg(ANSI_COLOR(1;33;44) "�A�ɤW�@��! " ANSI_COLOR(37) "*^o^*" ANSI_RESET);

#if defined(NOKILLWATERBALL) && defined(PLAY_ANGEL)
	/* Questioning and answering should better deliver immediately. */
	if ((flag == WATERBALL_ANGEL || flag == WATERBALL_ANSWER ||
	    flag == WATERBALL_CONFIRM_ANGEL ||
	    flag == WATERBALL_CONFIRM_ANSWER) && uin->pid)
	    kill(uin->pid, SIGUSR2);
#endif
    }

    clrtoeol();

    currutmp->chatid[0] = c0;
    currutmp->mode = mode0;
    currstat = currstat0;
    return 1;
}

void
getmessage(msgque_t msg)
{
    int     write_pos = currutmp->msgcount;
    if ( write_pos < (MAX_MSGS - 1) ) {
            unsigned char pager0 = currutmp->pager;
 	    currutmp->msgcount = write_pos+1;
            memcpy(&currutmp->msgs[write_pos], &msg, sizeof(msgque_t));
            currutmp->pager = pager0;
   	    write_request(SIGUSR1);
        }
}

void
t_display_new(void)
{
    static int      t_display_new_flag = 0;
    int             i, off = 2;
    if (t_display_new_flag)
	return;
    else
	t_display_new_flag = 1;

    check_water_init();
    if (PAGER_UI_IS(PAGER_UI_ORIG))
	water_which = &water[0];
    else
	off = 3;

    if (water[0].count && watermode > 0) {
	move(1, 0);
	outs("�w�w�w�w�w�w�w���w�y�w�^�w�U�w�w�w");
	outs(PAGER_UI_IS(PAGER_UI_ORIG) ?
	     "�w�w�w�w�w�w��[Ctrl-R Ctrl-T]������w�w�w�w�w" :
	     "��[Ctrl-R Ctrl-T Ctrl-F Ctrl-G ]������w�w�w�w");
	if (PAGER_UI_IS(PAGER_UI_NEW)) {
	    move(2, 0);
	    clrtoeol();
	    for (i = 0; i < 6; i++) {
		if (i > 0)
		    if (swater[i - 1]) {

			if (swater[i - 1]->uin &&
			    (swater[i - 1]->pid != swater[i - 1]->uin->pid ||
			     swater[i - 1]->userid[0] != swater[i - 1]->uin->userid[0]))
			    swater[i - 1]->uin = (userinfo_t *) search_ulist_pid(swater[i - 1]->pid);
			prints("%s%c%-13.13s" ANSI_RESET,
			       swater[i - 1] != water_which ? "" :
			       swater[i - 1]->uin ? ANSI_COLOR(1;33;47) :
			       ANSI_COLOR(1;33;45),
			       !swater[i - 1]->uin ? '#' : ' ',
			       swater[i - 1]->userid);
		    } else
			outs("              ");
		else
		    prints("%s ����  " ANSI_RESET,
			   water_which == &water[0] ? ANSI_COLOR(1;33;47) " " :
			   " "
			);
	    }
	}
	for (i = 0; i < water_which->count; i++) {
	    int a = (water_which->top - i - 1 + MAX_REVIEW) % MAX_REVIEW;
	    int len = 75 - strlen(water_which->msg[a].last_call_in)
		- strlen(water_which->msg[a].userid);
	    if (len < 0)
		len = 0;

	    move(i + (PAGER_UI_IS(PAGER_UI_ORIG) ? 2 : 3), 0);
	    clrtoeol();
	    if (watermode - 1 != i)
		prints(ANSI_COLOR(1;33;46) " %s " ANSI_COLOR(37;45) " %s " ANSI_RESET "%*s",
		       water_which->msg[a].userid,
		       water_which->msg[a].last_call_in, len,
		       "");
	    else
		prints(ANSI_COLOR(1;44) ">" ANSI_COLOR(1;33;47) "%s "
		       ANSI_COLOR(37;45) " %s " ANSI_RESET "%*s",
		       water_which->msg[a].userid,
		       water_which->msg[a].last_call_in,
		       len, "");
	}

	if (t_last_write[0]) {
	    move(i + off, 0);
	    clrtoeol();
	    outs(t_last_write);
	    i++;
	}
	move(i + off, 0);
	outs("�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w"
	     "�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w�w");
	if (PAGER_UI_IS(PAGER_UI_NEW))
	    while (i++ <= water[0].count) {
		move(i + off, 0);
		clrtoeol();
	    }
    }
    t_display_new_flag = 0;
}

int
t_display(void) {
    char genbuf[PATHLEN], ans[4];
    if (fp_writelog) {
        // Why not simply fflush here? Because later when user enter (M) or (C),
        // fp_writelog must be re-opened -- and there will be a race condition.
	fclose(fp_writelog);
	fp_writelog = NULL;
    }
    setuserfile(genbuf, fn_writelog);
    if (more(genbuf, YEA) == -1) {
        vmsg("�ȵL�T���O��");
        return FULLUPDATE;
    } else {
	grayout(0, b_lines-5, GRAYOUT_DARK);
	move(b_lines - 4, 0);
	clrtobot();
	outs(ANSI_COLOR(1;33;45) "�����y��z�{�� " ANSI_RESET "\n"
	     "�����z: �i�N���y�s�J�H�c(M)��, ��i�l����j�ӫH��e�� u,\n"
	     "�t�η|�N���y�������s��z��H�e���z��! " ANSI_RESET "\n");

	getdata(b_lines - 1, 0, "�M��(C) �s�J�H�c(M) �O�d(R) (C/M/R)?[R]",
		ans, sizeof(ans), LCECHO);
	if (*ans == 'm') {
	    // only delete if success because the file can be re-used.
	    if (mail_log2id(cuser.userid, "���u�O��", genbuf, "[��.��.��]", 0, 1) == 0)
		unlink(genbuf);
	    else
		vmsg("�H�c�x�s���ѡC");
	} else if (*ans == 'c') {
	    getdata(b_lines - 1, 0, "�T�w�M���H(y/N) [N] ",
	            ans, sizeof(ans), LCECHO);
	    if(*ans == 'Y' || *ans == 'y')
	        unlink(genbuf);
	    else
	        vmsg("�����M���C");
	}
	return FULLUPDATE;
    }
    return DONOTHING;
}

#define lockreturn(unmode, state) if(lockutmpmode(unmode, state)) return

int make_connection_to_somebody(userinfo_t *uin, int timeout){
    int sock, length, pid, ch;
    struct sockaddr_in server;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
	perror("sock err");
	unlockutmpmode();
	return -1;
    }
    server.sin_family = PF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = 0;
    if (bind(sock, (struct sockaddr *) & server, sizeof(server)) < 0) {
	close(sock);
	perror("bind err");
	unlockutmpmode();
	return -1;
    }
    length = sizeof(server);
    if (getsockname(sock, (struct sockaddr *) & server, (socklen_t *) & length) < 0) {
	close(sock);
	perror("sock name err");
	unlockutmpmode();
	return -1;
    }
    currutmp->sockactive = YEA;
    currutmp->sockaddr = server.sin_port;
    currutmp->destuid = uin->uid;
    // WORKAROUND setutmpmode() checks currstat as cache of currutmp->mode.
    // however if you invoke page -> rejected -> do something -> page again,
    // the currstat=PAGE but currutmp->mode!=PAGE, and then the paging will fail.
    // so, let's temporary break currstat here.
    currstat = IDLE;
    setutmpmode(PAGE);
    uin->destuip = currutmp - &SHM->uinfo[0];
    pid = uin->pid;
    if (pid > 0)
	kill(pid, SIGUSR1);
    clear();
    prints("���I�s %s.....\n��J Ctrl-D ����....", uin->userid);

    if(listen(sock, 1)<0) {
	close(sock);
	return -1;
    }

    vkey_attach(sock);

    while (1) {
	if (vkey_poll(timeout * MILLISECONDS)) {
	    ch = vkey();
	} else { // if (ch == I_TIMEOUT) {
	    ch = uin->mode;
	    if (!ch && uin->chatid[0] == 1 &&
		    uin->destuip == currutmp - &SHM->uinfo[0]) {
		bell();
		outmsg("���^����...");
		refresh();
	    } else if (ch == EDITING || ch == TALK || ch == CHATING ||
		    ch == PAGE || ch == MAILALL || ch == MONITOR ||
		    ch == M_FIVE || ch == CHC || ch == M_CONN6 ||
		    (!ch && (uin->chatid[0] == 1 ||
			     uin->chatid[0] == 3))) {
		vkey_detach();
		close(sock);
		currutmp->sockactive = currutmp->destuid = 0;
		vmsg("�H�a�b����");
		unlockutmpmode();
		return -1;
	    } else {
		// change to longer timeout
		timeout = 20;
		move(0, 0);
		outs("�A");
		bell();
		refresh();

		uin->destuip = currutmp - &SHM->uinfo[0];
		if (pid <= 0 || kill(pid, SIGUSR1) == -1) {
		    close(sock);
		    currutmp->sockactive = currutmp->destuid = 0;
		    vkey_detach();
		    vmsg(msg_usr_left);
		    unlockutmpmode();
		    return -1;
		}
		continue;
	    }
	}
	if (ch == I_OTHERDATA)
	    break;

	if (ch == Ctrl('D')) {
	    vkey_detach();
	    close(sock);
	    currutmp->sockactive = currutmp->destuid = 0;
	    unlockutmpmode();
	    return -1;
	}
    }
    return sock;
}

void
my_talk(userinfo_t * uin, int fri_stat, char defact)
{
    int             sock, msgsock, ch;
    pid_t           pid;
    char            c;
    char            genbuf[4];
    unsigned char   mode0 = currutmp->mode;

    genbuf[0] = defact;
    ch = uin->mode;

    if (ch == EDITING || ch == TALK || ch == CHATING || ch == PAGE ||
	ch == MAILALL || ch == MONITOR || ch == M_FIVE || ch == CHC ||
	ch == DARK || ch == UMODE_GO || ch == CHESSWATCHING || ch == REVERSI ||
	ch == M_CONN6 ||
	(!ch && (uin->chatid[0] == 1 || uin->chatid[0] == 3)) ||
	uin->lockmode == M_FIVE || uin->lockmode == M_CONN6 || uin->lockmode == CHC) {
	if (ch == CHC || ch == M_FIVE || ch == UMODE_GO ||
	    ch == M_CONN6 || ch == CHESSWATCHING || ch == REVERSI) {
	    sock = make_connection_to_somebody(uin, 20);
	    if (sock < 0)
		vmsg("�L�k�إ߳s�u");
	    else {
		msgsock = accept(sock, (struct sockaddr *) 0, (socklen_t *) 0);
		if (msgsock == -1) {
		    perror("accept");
		    close(sock);
		    return;
		}
		close(sock);
		strlcpy(currutmp->mateid, uin->userid, sizeof(currutmp->mateid));

		switch (uin->sig) {
		    case SIG_CHC:
			chc(msgsock, CHESS_MODE_WATCH);
			break;

		    case SIG_GOMO:
			gomoku(msgsock, CHESS_MODE_WATCH);
			break;

		    case SIG_GO:
			gochess(msgsock, CHESS_MODE_WATCH);
			break;

		    case SIG_REVERSI:
			reversi(msgsock, CHESS_MODE_WATCH);
			break;

		    case SIG_CONN6:
			connect6(msgsock, CHESS_MODE_WATCH);
			break;
		}
	    }
	}
	else
	    outs("�H�a�b����");
    } else if (!HasUserPerm(PERM_SYSOP) &&
	       (((fri_stat & HRM) && !(fri_stat & HFM)) ||
		((!uin->pager) && !(fri_stat & HFM)))) {
	outs("��������I�s���F");
    } else if (!HasUserPerm(PERM_SYSOP) &&
	     (((fri_stat & HRM) && !(fri_stat & HFM)) || uin->pager == PAGER_DISABLE)) {
	outs("���ޱ��I�s���F");
    } else if (!HasUserPerm(PERM_SYSOP) &&
	       !(fri_stat & HFM) && uin->pager == PAGER_FRIENDONLY) {
	outs("���u�����n�ͪ��I�s");
    } else if (!(pid = uin->pid) /* || (kill(pid, 0) == -1) */ ) {
	//resetutmpent();
	outs(msg_usr_left);
    } else {
	int i,j;

	if (!defact) {
	    showplans(uin->userid);
	    move(2, 0);
	    for(i=0;i<2;i++) {
		if(uin->withme & (WITHME_ALLFLAG<<i)) {
		    if(i==0)
			outs("�w���ڡG");
		    else
			outs("�ЧO��ڡG");
		    for(j=0; j<32 && withme_str[j/2]; j+=2)
			if(uin->withme & (1<<(j+i)))
			    if(withme_str[j/2]) {
				outs(withme_str[j/2]);
				outc(' ');
			    }
		    outc('\n');
		}
	    }
	    move(4, 0);
	    outs("�n�M�L(�o) (T)�ͤ�(F)���l��"
		    "(C)�H��(D)�t��(G)���(R)�¥մ�(6)���l�X");
	    getdata(5, 0, "           (N)�S�Ƨ���H�F?[N] ", genbuf, 4, LCECHO);
	}

	switch (*genbuf) {
	case 'y':
	case 't':
	    uin->sig = SIG_TALK;
	    break;
	case 'f':
	    lockreturn(M_FIVE, LOCK_THIS);
	    uin->sig = SIG_GOMO;
	    break;
	case 'c':
	    lockreturn(CHC, LOCK_THIS);
	    uin->sig = SIG_CHC;
	    break;
	case '6':
	    lockreturn(M_CONN6, LOCK_THIS);
	    uin->sig = SIG_CONN6;
	    break;
	case 'd':
	    uin->sig = SIG_DARK;
	    break;
	case 'g':
	    uin->sig = SIG_GO;
	    break;
	case 'r':
	    uin->sig = SIG_REVERSI;
	    break;
	default:
	    return;
	}

	uin->turn = 1;
	currutmp->turn = 0;
	strlcpy(uin->mateid, currutmp->userid, sizeof(uin->mateid));
	strlcpy(currutmp->mateid, uin->userid, sizeof(currutmp->mateid));

	sock = make_connection_to_somebody(uin, 5);
	if(sock==-1) {
	    vmsg("�L�k�إ߳s�u");
	    return;
	}

	msgsock = accept(sock, (struct sockaddr *) 0, (socklen_t *) 0);
	if (msgsock == -1) {
	    perror("accept");
	    close(sock);
	    unlockutmpmode();
	    return;
	}
	vkey_detach();
	close(sock);
	currutmp->sockactive = NA;

	if (uin->sig == SIG_CHC || uin->sig == SIG_GOMO ||
	    uin->sig == SIG_GO || uin->sig == SIG_REVERSI ||
	    uin->sig == SIG_CONN6)
	    ChessEstablishRequest(msgsock);

	vkey_attach(msgsock);
	while ((ch = vkey()) != I_OTHERDATA) {
	    if (ch == Ctrl('D')) {
		vkey_detach();
		close(msgsock);
		unlockutmpmode();
		return;
	    }
	}

	if (read(msgsock, &c, sizeof(c)) != sizeof(c))
	    c = 'n';
	vkey_detach();
        // alert that we dot a response
        bell();

	if (c == 'y') {
	    switch (uin->sig) {
	    case SIG_DARK:
		main_dark(msgsock, uin);
		break;
	    case SIG_GOMO:
		gomoku(msgsock, CHESS_MODE_VERSUS);
		break;
	    case SIG_CHC:
		chc(msgsock, CHESS_MODE_VERSUS);
		break;
	    case SIG_GO:
		gochess(msgsock, CHESS_MODE_VERSUS);
		break;
	    case SIG_REVERSI:
		reversi(msgsock, CHESS_MODE_VERSUS);
		break;
	    case SIG_CONN6:
		connect6(msgsock, CHESS_MODE_VERSUS);
		break;
	    case SIG_TALK:
	    default:
		ccw_talk(msgsock, currutmp->destuid);
		setutmpmode(XINFO);
		break;
	    }
	} else {
	    move(9, 9);
	    outs("�i�^���j ");
	    switch (c) {
	    case 'a':
		outs("�ڲ{�b�ܦ��A�е��@�|��A call �ڡA�n�ܡH");
		break;
	    case 'b':
		prints("�藍�_�A�ڦ��Ʊ������A %s....", sig_des[uin->sig]);
		break;
	    case 'd':
		outs("�ڭn�����o..�U���A��a..........");
		break;
	    case 'c':
		outs("�Ф��n�n�ڦn�ܡH");
		break;
	    case 'e':
		outs("��ڦ��ƶܡH�Х��ӫH��....");
		break;
	    case 'f':
		{
		    char            msgbuf[60];

		    read(msgsock, msgbuf, 60);
		    prints("�藍�_�A�ڲ{�b�����A %s�A�]��\n", sig_des[uin->sig]);
		    move(10, 18);
		    outs(msgbuf);
		}
		break;
	    case '1':
		prints("%s�H����$100��..", sig_des[uin->sig]);
		break;
	    case '2':
		prints("%s�H����$1000��..", sig_des[uin->sig]);
		break;
	    default:
		prints("�ڲ{�b���Q %s ��.....:)", sig_des[uin->sig]);
	    }
	    close(msgsock);
	}
    }
    currutmp->mode = mode0;
    currutmp->destuid = 0;
    unlockutmpmode();
    pressanykey();
}

/* ��榡��Ѥ��� */
#define US_PICKUP       1234
#define US_RESORT       1233
#define US_ACTION       1232
#define US_REDRAW       1231

static const char
*hlp_talkbasic[] = {
    "�i���ʴ�Сj", NULL,
    "  ���W�@��", "�� k",
    "  ���U�@��", "�� j n",
    "  ���e½��", "^B PgUp",
    "  ����½��", "^F PgDn �ť���",
    "  �C���}�Y", "Home 0",
    "  �C������", "End  $",
    "  ����...",  "1-9�Ʀr��",
    "  �j�MID",	  "s",
    "  �������}", "��   e",
    NULL,
},
*hlp_talkcfg[] = {
    "�i�ק��ơj", NULL,
    "  �ק�ʺ�",    "N",
    "  ��������",    "C",
    "  �����I�s��",  "p",
    "  ���y�Ҧ�",    "^W",
    "  �W�[�n��",    "a",
    "  �R���n��",    "d",
    "  �ק�n��",    "o",
    "  ���ʦ^���覡","y",
    NULL,
},
*hlp_talkdisp[] = {
    "�i�d�߸�T�j", NULL,
    "  �d�ߦ��H",    "q",
    "  ��J�d��ID",  "Q",
    "  �d���d��",    "c",
    "", "",
    "�i��ܤ覡�j", NULL,
    "  �վ�Ƨ�",	"TAB",
    "  �ӷ�/�y�z/���Z",	"S",
    "  ����/�n�� �C��",	"f",
    NULL,
},
*hlp_talktalk[] = {
    "�i��ͤ��ʡj", NULL,
    "  �P�L���",    "�� t Enter",
    "  ���u���y",    "w",
    "  �Y�ɦ^��",    "^R (�n��������y)",
    "  �n�ͼs��",    " b (�n�b�n�ͦC��)",
    "  �^�U�T��",    "l",
    "  �H�H���L",    "m",
    "  ����" MONEYNAME,"g",
    NULL,
},
*hlp_talkmisc[] = {
    "�i�䥦�j", NULL,
    "  �\\Ū�H��",   "r",
    "  �ϥλ���",    "h",
    NULL,
},
*hlp_talkadmin[] = {
    "�i�����M�Ρj", NULL,
    "  �]�w�ϥΪ�",   "u",
    "  �������μҦ�", "H",
    "  ��H",	      "K",
#if defined(SHOWBOARD) && defined(DEBUG)
    "  ��ܩҦb�ݪO", "Y",
#endif
    NULL,
};

static void
t_showhelp(void)
{
    const char ** p1[3] = { hlp_talkbasic, hlp_talkdisp, hlp_talkcfg },
	       ** p2[3] = { hlp_talktalk,  hlp_talkmisc, hlp_talkadmin };
    const int  cols[3] = { 31, 25, 22 },    // column witdh
               desc[3] = { 12, 18, 16 };    // desc width
    clear();
    showtitle("�𶢲��", "�ϥλ���");
    outs("\n");
    vs_multi_T_table_simple(p1, 3, cols, desc,
	    HLP_CATEGORY_COLOR, HLP_DESCRIPTION_COLOR, HLP_KEYLIST_COLOR);
    if (HasUserPerm(PERM_PAGE))
    vs_multi_T_table_simple(p2, HasUserPerm(PERM_SYSOP)?3:2, cols, desc,
	    HLP_CATEGORY_COLOR, HLP_DESCRIPTION_COLOR, HLP_KEYLIST_COLOR);
    PRESSANYKEY();
}

/* Kaede show friend description */
static char    *
friend_descript(const userinfo_t * uentp, char *desc_buf, int desc_buflen)
{
    char           *space_buf = "", *flag;
    char            fpath[80], name[IDLEN + 2], *desc, *ptr;
    int             len;
    FILE           *fp;
    char            genbuf[STRLEN];

    STATINC(STAT_FRIENDDESC);
    if((set_friend_bit(currutmp,uentp)&IFH)==0)
	return space_buf;

    setuserfile(fpath, friend_file[0]);

    STATINC(STAT_FRIENDDESC_FILE);
    if ((fp = fopen(fpath, "r"))) {
	snprintf(name, sizeof(name), "%s ", uentp->userid);
	len = strlen(name);
	desc = genbuf + 13;

	/* TODO maybe none linear search, or fread, or cache */
	while ((flag = fgets(genbuf, STRLEN, fp))) {
	    if (!memcmp(genbuf, name, len)) {
		if ((ptr = strchr(desc, '\n')))
		    ptr[0] = '\0';
		break;
	    }
	}
	fclose(fp);
	if (flag)
	    strlcpy(desc_buf, desc, desc_buflen);
	else
	    return space_buf;

	return desc_buf;
    } else
	return space_buf;
}

static const char    *
descript(int show_mode, const userinfo_t * uentp, int diff, char *description, int len)
{
    switch (show_mode) {
    case 1:
	return friend_descript(uentp, description, len);
    case 0:
	return (((uentp->pager != PAGER_DISABLE && uentp->pager != PAGER_ANTIWB && diff) ||
		 HasUserPerm(PERM_SYSOP)) ?  uentp->from : "*");
    case 2:
	snprintf(description, len, "%4d/%4d/%2d %c",
                 uentp->five_win, uentp->five_lose, uentp->five_tie,
		 (uentp->withme&WITHME_FIVE)?'o':
                 (uentp->withme&WITHME_NOFIVE)?'x':' ');
	return description;
    case 3:
	snprintf(description, len, "%4d/%4d/%2d %c",
                 uentp->chc_win, uentp->chc_lose, uentp->chc_tie,
		 (uentp->withme&WITHME_CHESS)?'o':
                 (uentp->withme&WITHME_NOCHESS)?'x':' ');
	return description;
    case 4:
	snprintf(description, len,
		 "%4d %s", uentp->chess_elo_rating,
		 (uentp->withme&WITHME_CHESS)?"��ڤU��":
                 (uentp->withme&WITHME_NOCHESS)?"�O���":"");
	return description;
    case 5:
	snprintf(description, len, "%4d/%4d/%2d %c",
                 uentp->go_win, uentp->go_lose, uentp->go_tie,
		 (uentp->withme&WITHME_GO)?'o':
                 (uentp->withme&WITHME_NOGO)?'x':' ');
	return description;
    case 6:
	snprintf(description, len, "%4d/%4d/%2d %c",
                 uentp->dark_win, uentp->dark_lose, uentp->dark_tie,
		 (uentp->withme&WITHME_DARK)?'o':
                 (uentp->withme&WITHME_NODARK)?'x':' ');
	return description;
    case 7:
	snprintf(description, len, "%s",
		 (uentp->withme&WITHME_CONN6)?"��ڤU��":
                 (uentp->withme&WITHME_NOCONN6)?"�O���":"");
	return description;

    default:
	syslog(LOG_WARNING, "damn!!! what's wrong?? show_mode = %d",
	       show_mode);
	return "";
    }
}

/*
 * userlist
 *
 * ���O���L�j���� bbs�b��@�ϥΪ̦W���, ���O�N�Ҧ� online users ���@����
 * local space ��, ���өҶ��n���覡 sort �n (�p���� userid , ���l��, �ӷ���
 * ��) . �o�N�y���j�q�����O: ������C�ӤH���n���F���ͳo�@���� 20 �ӤH�����
 * �ӥh sort ��L�@�U�H�����?
 *
 * �@��ӻ�, �@�����㪺�ϥΪ̦W��i�H�����u�n�Ͱϡv�M�u�D�n�Ͱϡv. ���P�H��
 * �u�n�Ͱϡv���ӷ|�������@��, ���L�u�D�n�Ͱϡv���ӬO�����@�˪�. �w��o���S
 * ��, ��Ϧ����P����@�覡.
 *
 * + �n�Ͱ�
 *   �n�Ͱϥu���b�ƦC�覡�� [��! �B��] ���ɭԡu�i��v�|�Ψ�.
 *   �C�� process�i�H�z�L currutmp->friend_online[]�o�줬�۶����n�����Y����
 *   �� (���]�A�O��, �O�ͬO�t�~�ͥX�Ӫ�) ���L friend_online[]�O unorder��.
 *   �ҥH���n����Ҧ����H���X��, ���s sort �@��.
 *   �n�Ͱ� (���ۥ��@�観�]�n��+ �O��) �̦h�u�|�� MAX_FRIENDS��
 *   �]�����ͦn�ͰϪ� cost �۷���, "�ण���ʹN���n����"
 *
 * + �D�n�Ͱ�
 *   �z�L shmctl utmpsortd , �w�� (�q�`�@���@��) �N�������H���ӦU�ؤ��P����
 *   �� sort �n, ��m�b SHM->sorted��.
 *
 * ���U��, �ڭ̨C���u�q�T�w���_�l��m��, �S�O�O���D�����n, ���M���|�h���ͦn
 * �Ͱ�.
 *
 * �U�� function �K�n
 * sort_cmpfriend()   sort function, key: friend type
 * pickup_maxpages()  # pages of userlist
 * pickup_myfriend()  ���ͦn�Ͱ�
 * pickup_bfriend()   ���ͪO��
 * pickup()           ���ͬY�@���ϥΪ̦W��
 * draw_pickup()      ��e����X
 * userlist()         �D�禡, �t�d�I�s pickup()/draw_pickup() �H�Ϋ���B�z
 *
 * SEE ALSO
 *     include/pttstruct.h
 *
 * BUGS
 *     �j�M���ɭԨS����k����ӤH�W��
 *
 * AUTHOR
 *     in2 <in2@in2home.org>
 */
char    nPickups;

static int
sort_cmpfriend(const void *a, const void *b)
{
    if (((((pickup_t *) a)->friend) & ST_FRIEND) ==
	((((pickup_t *) b)->friend) & ST_FRIEND))
	return strcasecmp(((pickup_t *) a)->ui->userid,
			  ((pickup_t *) b)->ui->userid);
    else
	return (((pickup_t *) b)->friend & ST_FRIEND) -
	    (((pickup_t *) a)->friend & ST_FRIEND);
}

int
pickup_maxpages(int pickupway, int nfriends)
{
    int             number;
    if (HasUserFlag(UF_FRIEND))
	number = nfriends;
    else
	number = SHM->UTMPnumber +
	    (pickupway == 0 ? nfriends : 0);
    return (number - 1) / nPickups + 1;
}

static int
pickup_myfriend(pickup_t * friends,
		int *myfriend, int *friendme, int *badfriend)
{
    userinfo_t     *uentp;
    int             i, where, frstate, ngets = 0;

    STATINC(STAT_PICKMYFRIEND);
    *badfriend = 0;
    *myfriend = *friendme = 1;
    for (i = 0; i < MAX_FRIEND && currutmp->friend_online[i]; ++i) {
	where = currutmp->friend_online[i] & 0xFFFFFF;
	if (VALID_USHM_ENTRY(where) &&
	    (uentp = &SHM->uinfo[where]) && uentp->pid &&
	    uentp != currutmp &&
	    isvisible_stat(currutmp, uentp,
			   frstate =
			   currutmp->friend_online[i] >> 24)){
	    if( frstate & IRH )
		++*badfriend;
	    if( !(frstate & IRH) || ((frstate & IRH) && (frstate & IFH)) ){
		friends[ngets].ui = uentp;
		friends[ngets].uoffset = where;
		friends[ngets++].friend = frstate;
		if (frstate & IFH)
		    ++* myfriend;
		if (frstate & HFM)
		    ++* friendme;
	    }
	}
    }
    /* ��ۤv�[�J�n�Ͱ� */
    friends[ngets].ui = currutmp;
    friends[ngets++].friend = (IFH | HFM);
    return ngets;
}

static int
pickup_bfriend(pickup_t * friends, int base)
{
    userinfo_t     *uentp;
    int             i, ngets = 0;
    int             currsorted = SHM->currsorted, number = SHM->UTMPnumber;

    STATINC(STAT_PICKBFRIEND);
    friends = friends + base;
    for (i = 0; i < number && ngets < MAX_FRIEND - base; ++i) {
	uentp = &SHM->uinfo[SHM->sorted[currsorted][0][i]];
	/* TODO isvisible() ���ƥΨ�F friend_stat() */
	if (uentp && uentp->pid && uentp->brc_id == currutmp->brc_id &&
	    currutmp != uentp && isvisible(currutmp, uentp) &&
	    (base || !(friend_stat(currutmp, uentp) & (IFH | HFM)))) {
	    friends[ngets].ui = uentp;
	    friends[ngets++].friend = IBH;
	}
    }
    return ngets;
}

static void
pickup(pickup_t * currpickup, int pickup_way, int *page,
       int *nfriend, int *myfriend, int *friendme, int *bfriend, int *badfriend)
{
    /* avoid race condition */
    int             currsorted = SHM->currsorted;
    int             utmpnumber = SHM->UTMPnumber;
    int             friendtotal = currutmp->friendtotal;

    int    *ulist;
    userinfo_t *u;
    int             which, sorted_way, size = 0, friend;

    if (friendtotal == 0)
	*myfriend = *friendme = 1;

    /* ���ͦn�Ͱ� */
    which = *page * nPickups;
    if( (HasUserFlag(UF_FRIEND)) || /* �u��ܦn�ͼҦ� */
	((pickup_way == 0) &&          /* [��! �B��] mode */
	 (
	  /* �t�O��, �n�Ͱϳ̦h�u�|�� (friendtotal + �O��) ��*/
	  (currutmp->brc_id && which < (friendtotal + 1 +
					getbcache(currutmp->brc_id)->nuser)) ||

	  /* ���t�O��, �̦h�u�|�� friendtotal�� */
	  (!currutmp->brc_id && which < friendtotal + 1)
	  ))) {
	pickup_t        friends[MAX_FRIEND + 1]; /* +1 include self */

	/* TODO �� friendtotal<which �ɥu����ܪO��, ���� pickup_myfriend */
	*nfriend = pickup_myfriend(friends, myfriend, friendme, badfriend);

	if (pickup_way == 0 && currutmp->brc_id != 0
#ifdef USE_COOLDOWN
		&& !(getbcache(currutmp->brc_id)->brdattr & BRD_COOLDOWN)
#endif
		){
	    /* TODO �u�ݭn which+nPickups-*nfriend �ӪO��, ���@�w�n��ӱ��@�M */
	    *nfriend += pickup_bfriend(friends, *nfriend);
	    *bfriend = SHM->bcache[currutmp->brc_id - 1].nuser;
	}
	else
	    *bfriend = 0;
	if (*nfriend > which) {
	    /* �u���b�n�q�X�~�����n sort */
	    /* TODO �n�͸�O�ͥi�H���} sort, �i��u�ݭn��@ */
	    /* TODO �n�ͤW�U���~�ݭn sort �@��, ���ݭn�C�� sort.
	     * �i���@�@�� dirty bit ���ܬO�_ sort �L.
	     * suggested by WYchuang@ptt */
	    qsort(friends, *nfriend, sizeof(pickup_t), sort_cmpfriend);
	    size = *nfriend - which;
	    if (size > nPickups)
		size = nPickups;
	    memcpy(currpickup, friends + which, sizeof(pickup_t) * size);
	}
    } else
	*nfriend = 0;

    if (!(HasUserFlag(UF_FRIEND)) && size < nPickups) {
	sorted_way = ((pickup_way == 0) ? 7 : (pickup_way - 1));
	ulist = SHM->sorted[currsorted][sorted_way];
	which = *page * nPickups - *nfriend;
	if (which < 0)
	    which = 0;

	for (; which < utmpnumber && size < nPickups; which++) {
	    u = &SHM->uinfo[ulist[which]];

	    friend = friend_stat(currutmp, u);
	    /* TODO isvisible() ���ƥΨ�F friend_stat() */
	    if ((pickup_way ||
		 (currutmp != u && !(friend & ST_FRIEND))) &&
		isvisible(currutmp, u)) {
		currpickup[size].ui = u;
		currpickup[size++].friend = friend;
	    }
	}
    }

    for (; size < nPickups; ++size)
	currpickup[size].ui = 0;
}

#define ULISTCOLS (9)

// userlist column definition
static const VCOL ulist_coldef[ULISTCOLS] = {
    {NULL, 8, 9, 0, {0, 1}},	// "�s��" �]����оa�o�ҥH�]���Ӫ��Ӥj
    {NULL, 2, 2, 0, {0, 0, 1}}, // "P" (pager, no border)
    {NULL, IDLEN+1, IDLEN+3}, // "�N��"
    {NULL, 17,25, 2}, // "�ʺ�", sizeof(userec_t::nickname)
    {NULL, 17,27, 1}, // "�G�m/�������Z/���Ť�"
    {NULL, 12,23, 1}, // "�ʺA" (�̤j�h�֤~�X�z�H) modestring size=40 ��...
    {NULL, 4, 4, 0, {0, 0, 1}}, // "<�q�r>" (��߱�)
    {NULL, 6, 6, -1, {0, 1, 1}}, // "�o�b" (optional?)
    {NULL, 0, VCOL_MAXW, -1}, // for middle alignment
};

static void
draw_pickup(int drawall, pickup_t * pickup, int pickup_way,
	    int page, int show_mode, int show_uid, int show_board,
	    int show_pid, int myfriend, int friendme, int bfriend, int badfriend)
{
    char           *msg_pickup_way[PICKUP_WAYS] = {
        "��! �B��", "���ͥN��", "���ͰʺA", "�o�b�ɶ�", "�Ӧۦ��",
        " ���l�� ", "  �H��  ", "  ���  ",
    };
    char           *MODE_STRING[MAX_SHOW_MODE] = {
	"�G�m", "�n�ʹy�z", "���l�Ѿ��Z", "�H�Ѿ��Z", "�H�ѵ��Ť�", "��Ѿ��Z",
        "�t�Ѿ��Z",
    };
    char            pagerchar[6] = "* -Wf";

    userinfo_t     *uentp;
    int             i, ch, state, friend;

    // print buffer
    char pager[3];
    char num[10];
    char xuid[IDLEN+1+20]; // must carry IDLEN + ANSI code.
    char description[30];
    char idlestr[32];
    int idletime = 0;

    static int scrw = 0, scrh = 0;
    static VCOLW cols[ULISTCOLS];

#ifdef SHOW_IDLE_TIME
    idletime = 1;
#endif

    // re-layout if required.
    if (scrw != t_columns || scrh != t_lines)
    {
	vs_cols_layout(ulist_coldef, cols, ULISTCOLS);
	scrw = t_columns; scrh = t_lines;
    }

    if (drawall) {
	showtitle((HasUserFlag(UF_FRIEND)) ? "�n�ͦC��" : "�𶢲��",
		  BBSName);

	move(2, 0);
	outs(ANSI_REVERSE);
	vs_cols(ulist_coldef, cols, ULISTCOLS,
                // Columns Header (9 args)
		show_uid ? "UID" : "�s��",
		"P",
                "�N��",
                "�ʺ�",
		MODE_STRING[show_mode],
		show_board ? "�ݪO" : "�ʺA",
		show_pid ? "PID" : "",
                idletime ? "�o�b": "",
		"");
	outs(ANSI_RESET);

#ifdef PLAY_ANGEL
	if (HasUserPerm(PERM_ANGEL) && currutmp)
	{
	    // modes should match ANGELPAUSE*
	    static const char *modestr[ANGELPAUSE_MODES] = {
		ANSI_COLOR(0;30;47) "�}��",
		ANSI_COLOR(0;32;47) "����",
		ANSI_COLOR(0;31;47) "����",
	    };
	    // reduced version
	    // TODO use vs_footer to replace.
	    move(b_lines, 0);
	    vbarf( ANSI_COLOR(34;46) " �𶢲�� "
		   ANSI_COLOR(31;47) " (TAB/f)" ANSI_COLOR(30) "�Ƨ�/�n�� "
		   ANSI_COLOR(31) "(p)" ANSI_COLOR(30) "�@��I�s�� "
		   ANSI_COLOR(31) "(^P)" ANSI_COLOR(30) "���٩I�s��\t"
		   ANSI_COLOR(1;30;47) "[���٩I�s��] %s ",
		   modestr[currutmp->angelpause % ANGELPAUSE_MODES]);
	} else
#endif
	vs_footer(" �𶢲�� ",
		" (TAB/f)�Ƨ�/�n�� (a/o)��� (q/w)�d��/����y (t/m)���/�g�H\t(h)����");
    }

    move(1, 0);
    prints("  �Ƨ�:[%s] �W���H��:%-4d "
	    ANSI_COLOR(1;32) "�ڪ��B��:%-3d "
	   ANSI_COLOR(33) "�P�ڬ���:%-3d "
	   ANSI_COLOR(36) "�O��:%-4d "
	   ANSI_COLOR(31) "�a�H:%-2d"
	   ANSI_RESET "\n",
	   msg_pickup_way[pickup_way], SHM->UTMPnumber,
	   myfriend, friendme, currutmp->brc_id ? bfriend : 0, badfriend);

    for (i = 0, ch = page * nPickups + 1; i < nPickups; ++i, ++ch) {
        char *mind = "";

	move(i + 3, 0);
	SOLVE_ANSI_CACHE();
	uentp = pickup[i].ui;
	friend = pickup[i].friend;
	if (uentp == NULL) {
	    outc('\n');
	    continue;
	}

	if (!uentp->pid) {
	    vs_cols(ulist_coldef, cols, 3,
		    "", "", "< ������..>");
	    continue;
	}

	// prepare user data

	if (PERM_HIDE(uentp))
	    state = 9;
	else if (currutmp == uentp)
	    state = 10;
	else if (friend & IRH && !(friend & IFH))
	    state = 8;
	else
	    state = (friend & ST_FRIEND) >> 2;

        idlestr[0] = 0;
#ifdef SHOW_IDLE_TIME
	idletime = (now - uentp->lastact);
	if (idletime > DAY_SECONDS)
	    strlcpy(idlestr, " -----", sizeof(idlestr));
	else if (idletime >= 3600)
	    snprintf(idlestr, sizeof(idlestr), "%dh%02d",
		     idletime / 3600, (idletime / 60) % 60);
	else if (idletime > 0)
	    snprintf(idlestr, sizeof(idlestr), "%d'%02d",
		     idletime / 60, idletime % 60);
#endif

	if ((uentp->userlevel & PERM_VIOLATELAW))
            mind = ANSI_COLOR(1;31) "�H�W";

	snprintf(num, sizeof(num), "%d",
#ifdef SHOWUID
		show_uid ? uentp->uid :
#endif
		ch);

	pager[0] = (friend & HRM) ? 'X' : pagerchar[uentp->pager % 5];
	pager[1] = (uentp->invisible ? ')' : ' ');
	pager[2] = 0;

	/* color of userid, userid */
	if(fcolor[state])
	    snprintf(xuid, sizeof(xuid), "%s%s",
		    fcolor[state], uentp->userid);

	vs_cols(ulist_coldef, cols, ULISTCOLS,
                // Columns data (9 params)
		num,
                pager,
		fcolor[state] ? xuid : uentp->userid,
		uentp->nickname,
                descript(show_mode, uentp, uentp->pager & !(friend & HRM),
                         description, sizeof(description)),
#if defined(SHOWBOARD) && defined(DEBUG)
		show_board ? (uentp->brc_id == 0 ? "" :
		    getbcache(uentp->brc_id)->brdname) :
#endif
                    modestring(uentp, 0),
                mind,
		idlestr,
	        "");
    }
}

void set_withme_flag(void)
{
    int i;
    char genbuf[20];
    int line;

    move(1, 0);
    clrtobot();

    do {
	move(1, 0);
	line=1;
	for(i=0; i<16 && withme_str[i]; i++) {
	    clrtoeol();
            if (!*withme_str[i])
                continue;
	    if(currutmp->withme&(1<<(i*2)))
		prints("[%c] �ګܷQ��H%s, �w�����H���\n",'a'+i, withme_str[i]);
	    else if(currutmp->withme&(1<<(i*2+1)))
		prints("[%c] �ڤ��ӷQ%s\n",'a'+i, withme_str[i]);
	    else
		prints("[%c] (%s)�S�N��\n",'a'+i, withme_str[i]);
	    line++;
	}
	getdata(line,0,"�Φr������ [�Q/���Q/�S�N��]",genbuf, sizeof(genbuf), DOECHO);
	for(i=0;genbuf[i];i++) {
	    int ch=genbuf[i];
	    ch=tolower(ch);
	    if('a'<=ch && ch<'a'+16) {
		ch-='a';
		if(currutmp->withme&(1<<ch*2)) {
		    currutmp->withme&=~(1<<ch*2);
		    currutmp->withme|=1<<(ch*2+1);
		} else if(currutmp->withme&(1<<(ch*2+1))) {
		    currutmp->withme&=~(1<<(ch*2+1));
		} else {
		    currutmp->withme|=1<<(ch*2);
		}
	    }
	}
    } while(genbuf[0]!='\0');
}

int
call_in(const userinfo_t * uentp, int fri_stat)
{
    if (iswritable_stat(uentp, fri_stat)) {
	char            genbuf[60];
	snprintf(genbuf, sizeof(genbuf), "�� %s ���y: ", uentp->userid);
	my_write(uentp->pid, genbuf, uentp->userid, WATERBALL_GENERAL, NULL);
	return 1;
    }
    return 0;
}

inline static void
userlist(void)
{
    pickup_t       *currpickup;
    userinfo_t     *uentp;
    static char     show_mode = 0;
    static char     show_uid = 0;
    static char     show_board = 0;
    static char     show_pid = 0;
    static int      pickup_way = 0;
    char            skippickup = 0, redraw, redrawall;
    int             page, offset, ch, leave, fri_stat;
    int             nfriend, myfriend, friendme, bfriend, badfriend, i;
    time4_t          lastupdate;

    nPickups = b_lines - 3;
    currpickup = (pickup_t *)malloc(sizeof(pickup_t) * nPickups);
    page = offset = 0 ;
    nfriend = myfriend = friendme = bfriend = badfriend = 0;
    leave = 0;
    redrawall = 1;
    /*
     * �U�� flag :
     * redraw:    ���s pickup(), draw_pickup() (�Ȥ�����, ���t���D�C����)
     * redrawall: �������e (�t���D�C����, ���A���w redraw �~�|����)
     * leave:     ���}�ϥΪ̦W��
     */
    while (!leave && !ZA_Waiting()) {
	if( !skippickup )
	    pickup(currpickup, pickup_way, &page,
		   &nfriend, &myfriend, &friendme, &bfriend, &badfriend);
	draw_pickup(redrawall, currpickup, pickup_way, page,
		    show_mode, show_uid, show_board, show_pid,
		    myfriend, friendme, bfriend, badfriend);

	/*
	 * �p�G�]���������ɭ�, �o�@�������H�Ƥ����,
	 * (�q�`���O�̫�@���H�Ƥ������ɭ�) ���n���s�p�� offset
	 * �H�K����S���H���a��
	 */
	if (offset == -1 || currpickup[offset].ui == NULL) {
	    for (offset = (offset == -1 ? nPickups - 1 : offset);
		 offset >= 0; --offset)
		if (currpickup[offset].ui != NULL)
		    break;
	    if (offset == -1) {
		if (--page < 0)
		    page = pickup_maxpages(pickup_way, nfriend) - 1;
		offset = 0;
		continue;
	    }
	}
	skippickup = redraw = redrawall = 0;
	lastupdate = now;
	while (!redraw && !ZA_Waiting()) {
	    ch = cursor_key(offset + 3, 0);
	    uentp = currpickup[offset].ui;
	    fri_stat = currpickup[offset].friend;

	    switch (ch) {
	    case Ctrl('Z'):
		redrawall = redraw = 1;
		if (ZA_Select())
		    leave = 1;
		break;

	    case KEY_LEFT:
	    case 'e':
	    case 'E':
		redraw = leave = 1;
		break;

	    case KEY_TAB:
		pickup_way = (pickup_way + 1) % PICKUP_WAYS;
		redraw = 1;
		redrawall = 1;
		break;

	    case KEY_DOWN:
	    case 'n':
	    case 'j':
		if (++offset == nPickups || currpickup[offset].ui == NULL) {
		    redraw = 1;
		    if (++page >= pickup_maxpages(pickup_way,
						  nfriend))
			offset = page = 0;
		    else
			offset = 0;
		}
		break;

	    case '0':
	    case KEY_HOME:
		page = offset = 0;
		redraw = 1;
		break;

	    case 'H':
		if (HasUserPerm(PERM_SYSOP)||HasUserPerm(PERM_OLDSYSOP)) {
		    currutmp->userlevel ^= PERM_SYSOPHIDE;
		    redrawall = redraw = 1;
		}
		break;

	    case 'C':
		currutmp->invisible ^= 1;
		redrawall = redraw = 1;
		break;

	    case ' ':
	    case KEY_PGDN:
	    case Ctrl('F'):{
		    int             newpage;
		    if ((newpage = page + 1) >= pickup_maxpages(pickup_way,
								nfriend))
			newpage = offset = 0;
		    if (newpage != page) {
			page = newpage;
			redraw = 1;
		    } else if (now >= lastupdate + 2)
			redrawall = redraw = 1;
		}
		break;

	    case KEY_UP:
	    case 'k':
		if (--offset == -1) {
		    offset = nPickups - 1;
		    if (--page == -1)
			page = pickup_maxpages(pickup_way, nfriend)
			    - 1;
		    redraw = 1;
		}
		break;

	    case KEY_PGUP:
	    case Ctrl('B'):
	    case 'P':
		if (--page == -1)
		    page = pickup_maxpages(pickup_way, nfriend) - 1;
		offset = 0;
		redraw = 1;
		break;

	    case KEY_END:
	    case '$':
		page = pickup_maxpages(pickup_way, nfriend) - 1;
		offset = -1;
		redraw = 1;
		break;

	    case '/':
		/*
		 * getdata_buf(b_lines-1,0,"�п�J�ʺ�����r:", keyword,
		 * sizeof(keyword), DOECHO); state = US_PICKUP;
		 */
		break;

	    case 's':
		if (!(HasUserFlag(UF_FRIEND))) {
		    int             si;	/* utmpshm->sorted[X][0][si] */
		    int             fi;	/* allpickuplist[fi] */
		    char            swid[IDLEN + 1];
		    move(1, 0);

		    // XXX si �w�g�g���F pickup_way = 0
		    // �Y�ϥΪ̦b pickup_way != 0 �ɫ� 's'...

		    si = CompleteOnlineUser(msg_uid, swid);
		    if (si >= 0) {
			pickup_t        friends[MAX_FRIEND + 1];
			int             nGots;
			int *ulist =
			    SHM->sorted[SHM->currsorted]
			    [((pickup_way == 0) ? 0 : (pickup_way - 1))];

			fi = ulist[si];
			nGots = pickup_myfriend(friends, &myfriend,
						&friendme, &badfriend);
			for (i = 0; i < nGots; ++i)
			    if (friends[i].uoffset == fi)
				break;

			fi = 0;
			offset = 0;
			if( i != nGots ){
			    page = i / nPickups;
			    for( ; i < nGots && fi < nPickups ; ++i )
				if( isvisible(currutmp, friends[i].ui) )
				    currpickup[fi++] = friends[i];
			    i = 0;
			}
			else{
			    page = (si + nGots) / nPickups;
			    i = si;
			}

			for( ; fi < nPickups && i < SHM->UTMPnumber ; ++i )
			{
			    userinfo_t *u;
			    u = &SHM->uinfo[ulist[i]];
			    if( isvisible(currutmp, u) ){
				currpickup[fi].ui = u;
				currpickup[fi++].friend = 0;
			    }
			}
			skippickup = 1;
		    }
		    redrawall = redraw = 1;
		}
		/*
		 * if ((i = search_pickup(num, actor, pklist)) >= 0) num = i;
		 * state = US_ACTION;
		 */
		break;

	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		{		/* Thor: �i�H���Ʀr����ӤH */
		    int             tmp;
		    if ((tmp = search_num(ch, SHM->UTMPnumber)) >= 0) {
			if (tmp / nPickups == page) {
			    /*
			     * in2:�ت��b�ثe�o�@��, ���� ��s offset ,
			     * ���έ��e�e��
			     */
			    offset = tmp % nPickups;
			} else {
			    page = tmp / nPickups;
			    offset = tmp % nPickups;
			}
			redrawall = redraw = 1;
		    }
		}
		break;

#ifdef SHOWUID
	    case 'U':
		if (HasUserPerm(PERM_SYSOP)) {
		    show_uid ^= 1;
		    redrawall = redraw = 1;
		}
		break;
#endif
#if defined(SHOWBOARD) && defined(DEBUG)
	    case 'Y':
		if (HasUserPerm(PERM_SYSOP)) {
		    show_board ^= 1;
		    redrawall = redraw = 1;
		}
		break;
#endif
#ifdef  SHOWPID
	    case '#':
		if (HasUserPerm(PERM_SYSOP)) {
		    show_pid ^= 1;
		    redrawall = redraw = 1;
		}
		break;
#endif

	    case 'b':		/* broadcast */
		if (HasUserFlag(UF_FRIEND) || HasUserPerm(PERM_SYSOP)) {
		    char            genbuf[60]="[�s��]";
		    char            ans[4];

		    if (!getdata(0, 0, "�s���T��:", genbuf+6, 54, DOECHO))
			break;

		    if (!getdata(0, 0, "�T�w�s��? [N]",
				ans, sizeof(ans), LCECHO) ||
			ans[0] != 'y')
			break;
		    if (!(HasUserFlag(UF_FRIEND)) && HasUserPerm(PERM_SYSOP)) {
			msgque_t msg;
			getdata(1, 0, "�A���T�w�����s��? [N]",
				ans, sizeof(ans), LCECHO);
			if( ans[0] != 'y'){
			    vmsg("abort");
			    break;
			}

			msg.pid = currpid;
			strlcpy(msg.userid, cuser.userid, sizeof(msg.userid));
			snprintf(msg.last_call_in, sizeof(msg.last_call_in),
				 "[�s��]%s", genbuf);
			for (i = 0; i < SHM->UTMPnumber; ++i) {
			    // XXX why use sorted list?
			    //     can we just scan uinfo with proper checking?
			    uentp = &SHM->uinfo[
                                      SHM->sorted[SHM->currsorted][0][i]];
			    if (uentp->pid && kill(uentp->pid, 0) != -1){
				int     write_pos = uentp->msgcount;
				if( write_pos < (MAX_MSGS - 1) ){
				    uentp->msgcount = write_pos + 1;
				    memcpy(&uentp->msgs[write_pos], &msg,
					   sizeof(msg));
#ifdef NOKILLWATERBALL
				    uentp->wbtime = now;
#else
				    kill(uentp->pid, SIGUSR2);
#endif
				}
			    }
			}
		    } else {
			userinfo_t     *uentp;
			int             where, frstate;
			for (i = 0; i < MAX_FRIEND && currutmp->friend_online[i]; ++i) {
			    where = currutmp->friend_online[i] & 0xFFFFFF;
                            if (!VALID_USHM_ENTRY(where))
                                continue;
                            uentp = &SHM->uinfo[where];
                            if (!uentp || !uentp->pid)
                                continue;
                            frstate = currutmp->friend_online[i] >> 24;
                            // Only to people who I've friended him.
                            if (!(frstate & IFH))
                                continue;
                            if (!isvisible_stat(currutmp, uentp, frstate))
                                continue;
                            if (uentp->pager == PAGER_ANTIWB)
                                continue;
                            if (uentp->pager == PAGER_FRIENDONLY &&
                                !(frstate & HFM))
                                continue;
                            // HRM + HFM = super friend.
                            if ((frstate & HRM) && !(frstate & HFM))
                                continue;
                            if (kill(uentp->pid, 0) == -1)
                                continue;
                            my_write(uentp->pid, genbuf, uentp->userid,
                                     WATERBALL_PREEDIT, NULL);
			}
		    }
		    redrawall = redraw = 1;
		}
		break;

	    case 'S':		/* ��ܦn�ʹy�z */
#ifdef HAVE_DARK_CHESS_LOG
		show_mode = (show_mode+1) % MAX_SHOW_MODE;
#else
		show_mode = (show_mode+1) % (MAX_SHOW_MODE - 1);
#endif
#ifdef CHESSCOUNTRY
		if (show_mode == 2)
		    user_query_mode = 1;
		else if (show_mode == 3 || show_mode == 4)
		    user_query_mode = 2;
		else if (show_mode == 5)
		    user_query_mode = 3;
		else if (show_mode == 6)
		    user_query_mode = 4;
		else
		    user_query_mode = 0;
#endif /* defined(CHESSCOUNTRY) */
		redrawall = redraw = 1;
		break;

	    case 'u':		/* �u�W�ק��� */
		if (HasUserPerm(PERM_ACCOUNTS|PERM_SYSOP)) {
		    int             id;
		    userec_t        muser;
		    vs_hdr("�ϥΪ̳]�w");
		    move(1, 0);
		    if ((id = getuser(uentp->userid, &muser)) > 0) {
			user_display(&muser, 1);
			if( HasUserPerm(PERM_ACCOUNTS) )
			    uinfo_query(muser.userid, 1, id);
			else
			    pressanykey();
		    }
		    redrawall = redraw = 1;
		}
		break;

	    case Ctrl('S'):
		break;

	    case KEY_RIGHT:
	    case KEY_ENTER:
	    case 't':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    if (uentp->pid != currpid &&
			    strcmp(uentp->userid, cuser.userid) != 0) {
			move(1, 0);
			clrtobot();
			move(3, 0);
			my_talk(uentp, fri_stat, 0);
			redrawall = redraw = 1;
		    }
		}
		break;
	    case 'K':
		if (HasUserPerm(PERM_ACCOUNTS|PERM_SYSOP)) {
		    my_kick(uentp);
		    redrawall = redraw = 1;
		}
		break;
	    case 'w':
		if (call_in(uentp, fri_stat))
		    redrawall = redraw = 1;
		break;
	    case 'a':
		if (HasBasicUserPerm(PERM_LOGINOK) && !(fri_stat & IFH)) {
		    if (vans("�T�w�n�[�J�n�Ͷ� [N/y]") == 'y') {
			friend_add(uentp->userid, FRIEND_OVERRIDE,uentp->nickname);
			friend_load(FRIEND_OVERRIDE, 0);
		    }
		    redrawall = redraw = 1;
		}
		break;

	    case 'd':
		if (HasBasicUserPerm(PERM_LOGINOK) && (fri_stat & IFH)) {
		    if (vans("�T�w�n�R���n�Ͷ� [N/y]") == 'y') {
			friend_delete(uentp->userid, FRIEND_OVERRIDE);
			friend_load(FRIEND_OVERRIDE, 0);
		    }
		    redrawall = redraw = 1;
		}
		break;

	    case 'o':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    t_override();
		    redrawall = redraw = 1;
		}
		break;

	    case 'f':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    pwcuToggleFriendList();
                    // reset cursor
                    page = offset = 0;
		    redrawall = redraw = 1;
		}
		break;

		/*
	    case 'G':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    p_give();
		    // give_money_ui(NULL);
		    redrawall = redraw = 1;
		}
		break;
		*/

	    case 'g':
		if (HasBasicUserPerm(PERM_LOGINOK) && cuser.money) {
		    give_money_ui(uentp->userid);
		    redrawall = redraw = 1;
		}
		break;

	    case 'm':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    char   userid[IDLEN + 1];
		    strlcpy(userid, uentp->userid, sizeof(userid));
		    vs_hdr("�H  �H");
		    prints("[�H�H] ���H�H�G%s", userid);
		    my_send(userid);
		    setutmpmode(LUSERS);
		    redrawall = redraw = 1;
		}
		break;

	    case 'q':
		my_query(uentp->userid);
		setutmpmode(LUSERS);
		redrawall = redraw = 1;
		break;

	    case 'Q':
		t_query();
		setutmpmode(LUSERS);
		redrawall = redraw = 1;
		break;

	    case 'c':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    chicken_query(uentp->userid);
		    redrawall = redraw = 1;
		}
		break;

	    case 'l':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    t_display();
		    redrawall = redraw = 1;
		}
		break;

	    case 'h':
		t_showhelp();
		redrawall = redraw = 1;
		break;

	    case 'p':
		if (HasUserPerm(PERM_BASIC)) {
		    t_pager();
		    redrawall = redraw = 1;
		}
		break;

#ifdef PLAY_ANGEL
	    case Ctrl('P'):
		if (HasBasicUserPerm(PERM_ANGEL) && currutmp) {
                    angel_toggle_pause();
		    redrawall = redraw = 1;
		}
		break;
#endif // PLAY_ANGEL

	    case Ctrl('W'):
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    static const char *wm[PAGER_UI_TYPES] = {"�@��", "�i��", "����"};

		    pwcuSetPagerUIType((cuser.pager_ui_type +1) % PAGER_UI_TYPES_USER);
		    /* vmsg cannot support multi lines */
		    move(b_lines - 4, 0);
		    clrtobot();
		    move(b_lines - 3, 0);
		    outs("�t�δ��Ѽƺؤ��y�Ҧ��i�ѿ��\n"
		    "�b������Х��`�U�u�A���s�n�J, �H�T�O���c���T\n");
		    vmsgf( "�ثe������ [%s] ���y�Ҧ�", wm[cuser.pager_ui_type%PAGER_UI_TYPES]);
		    redrawall = redraw = 1;
		}
		break;

	    case 'r':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
                    // XXX in fact we should check size here...
                    // chkmailbox();
                    m_read();
                    setutmpmode(LUSERS);
		    redrawall = redraw = 1;
		}
		break;

	    case 'N':
		if (HasBasicUserPerm(PERM_LOGINOK)) {
		    char tmp_nick[sizeof(cuser.nickname)];
		    if (getdata_str(1, 0, "�s���ʺ�: ",
				tmp_nick, sizeof(tmp_nick), DOECHO, cuser.nickname) > 0)
		    {
			pwcuSetNickname(tmp_nick);
			strlcpy(currutmp->nickname, cuser.nickname, sizeof(currutmp->nickname));
		    }
		    redrawall = redraw = 1;
		}
		break;

	    case 'y':
		set_withme_flag();
		redrawall = redraw = 1;
		break;

	    default:
		if (now >= lastupdate + 2)
		    redraw = 1;
	    }
	}
    }
    free(currpickup);
}

int
t_users(void)
{
    int             destuid0 = currutmp->destuid;
    int             mode0 = currutmp->mode;
    int             stat0 = currstat;

    if (!HasBasicUserPerm(PERM_LOGINOK) ||
        HasUserPerm(PERM_VIOLATELAW))
        return 0;

    assert(strncmp(cuser.userid, currutmp->userid, IDLEN)==0);
    if( strncmp(cuser.userid , currutmp->userid, IDLEN) != 0 ){
	abort_bbs(0);
    }

    // cannot do ZA for re-entrant.
    // usually happens when doing ^U, ^Z with non-return
    // env like editor.
    if (ZA_Waiting())
	ZA_Drop();

    // TODO drop if we were already in t_users?

    setutmpmode(LUSERS);
    userlist();
    currutmp->mode = mode0;
    currutmp->destuid = destuid0;
    currstat = stat0;
    return 0;
}

int
t_pager(void)
{
    currutmp->pager = (currutmp->pager + 1) % PAGER_MODES;
    return 0;
}

int
t_qchicken(void)
{
    char            uident[STRLEN];

    vs_hdr("�d���d��");
    usercomplete(msg_uid, uident);
    if (uident[0])
	chicken_query(uident);
    return 0;
}

int
t_query(void)
{
    char            uident[STRLEN];

    vs_hdr("�d�ߺ���");
    usercomplete(msg_uid, uident);
    if (uident[0])
	my_query(uident);
    return 0;
}

int
t_talk(void)
{
    char            uident[16];
    int             tuid, unum, ucount;
    userinfo_t     *uentp;
    char            genbuf[4];
    /*
     * if (count_ulist() <= 1){ outs("�ثe�u�W�u���z�@�H�A���ܽЪB�ͨӥ��{�i"
     * BBSNAME "�j�a�I"); return XEASY; }
     */
    vs_hdr("���}�ܧX�l");
    CompleteOnlineUser(msg_uid, uident);
    if (uident[0] == '\0')
	return 0;

    move(3, 0);
    if (!(tuid = searchuser(uident, uident)) || tuid == usernum) {
	outs(err_uid);
	pressanykey();
	return 0;
    }
    /* multi-login check */
    unum = 1;
    while ((ucount = count_logins(tuid, 0)) > 1) {
	outs("(0) ���Q talk �F...\n");
	count_logins(tuid, 1);
	getdata(1, 33, "�п�ܤ@�ӥ�͹�H [0]�G", genbuf, 4, DOECHO);
	unum = atoi(genbuf);
	if (unum == 0)
	    return 0;
	move(3, 0);
	clrtobot();
	if (unum > 0 && unum <= ucount)
	    break;
    }

    if ((uentp = (userinfo_t *) search_ulistn(tuid, unum)))
	my_talk(uentp, friend_stat(currutmp, uentp), 0);

    return 0;
}

static int
reply_connection_request(const userinfo_t *uip)
{
    char            buf[4], genbuf[200];

    if (uip->mode != PAGE) {
	snprintf(genbuf, sizeof(genbuf),
		 "%s �w����I�s�A��Enter�~��...", uip->userid);
	getdata(0, 0, genbuf, buf, sizeof(buf), LCECHO);
	return -1;
    }
    return establish_talk_connection(uip);
}

int
establish_talk_connection(const userinfo_t *uip)
{
    int                    a;
    struct sockaddr_in sin;

    currutmp->msgcount = 0;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = uip->sockaddr;
    if ((a = socket(sin.sin_family, SOCK_STREAM, 0)) < 0) {
	perror("socket err");
	return -1;
    }
    if ((connect(a, (struct sockaddr *) & sin, sizeof(sin)))) {
	//perror("connect err");
	close(a);
	return -1;
    }
    return a;
}

/* ���H�Ӧ���l�F�A�^���I�s�� */
void
talkreply(void)
{
    char            buf[4];
    char            genbuf[200];
    int             a, sig = currutmp->sig;
    int             currstat0 = currstat;
    int             r;
    int             is_chess;
    userec_t        xuser;
    void          (*sig_pipe_handle)(int);

    uip = &SHM->uinfo[currutmp->destuip];
    currutmp->destuid = uip->uid;
    currstat = REPLY;		/* �קK�X�{�ʵe */

    is_chess = (sig == SIG_CHC || sig == SIG_GOMO || sig == SIG_CONN6 ||
		sig == SIG_GO || sig == SIG_REVERSI);

    a = reply_connection_request(uip);
    if (a < 0) {
	clear();
	currstat = currstat0;
	return;
    }
    if (is_chess)
	ChessAcceptingRequest(a);

    clear();

    outs("\n\n");
    // FIXME CRASH here
    assert(sig>=0 && sig<(int)(sizeof(sig_des)/sizeof(sig_des[0])));
    prints("       (Y) ���ڭ� %s �a�I"
	   "       (A) �ڲ{�b�ܦ��A�е��@�|��A call ��\n", sig_des[sig]);
    prints("       (N) �ڲ{�b���Q %s "
	   "       (B) �藍�_�A�ڦ��Ʊ������A %s\n",
	    sig_des[sig], sig_des[sig]);
    prints("       (C) �Ф��n�n�ڦn�ܡH"
	   "       (D) �ڭn�����o..�U���A��a.......\n");
    prints("       (E) ���ƶܡH�Х��ӫH"
	   "       (F) " ANSI_COLOR(1;33) "<�ۦ��J�z��>..." ANSI_RESET "\n");
    prints("       (1) %s�H����$100��"
	   "       (2) %s�H����$1000��..\n\n", sig_des[sig], sig_des[sig]);

    getuser(uip->userid, &xuser);
    currutmp->msgs[0].pid = uip->pid;
    strlcpy(currutmp->msgs[0].userid, uip->userid, sizeof(currutmp->msgs[0].userid));
    strlcpy(currutmp->msgs[0].last_call_in, "�I�s�B�I�s�Ať��Ц^�� (Ctrl-R)",
	    sizeof(currutmp->msgs[0].last_call_in));
    currutmp->msgs[0].msgmode = MSGMODE_TALK;
    prints("���Ӧ� [%s]�A" STR_LOGINDAYS " %d " STR_LOGINDAYS_QTY "�A�峹�@ %d �g\n",
	    uip->from, xuser.numlogindays, xuser.numposts);

    if (is_chess)
	ChessShowRequest();
    else {
	showplans(uip->userid);
	show_call_in(0, 0);
    }

    snprintf(genbuf, sizeof(genbuf),
	    "�A�Q�� %s (%s) %s�ܡH�п��[N]: ",
	    uip->userid, uip->nickname, sig_des[sig]);
    getdata(0, 0, genbuf, buf, sizeof(buf), LCECHO);

    if (!buf[0] || !strchr("yabcdef12", buf[0]))
	buf[0] = 'n';

    sig_pipe_handle = Signal(SIGPIPE, SIG_IGN);
    r = write(a, buf, 1);
    if (buf[0] == 'f' || buf[0] == 'F') {
	if (!getdata(b_lines, 0, "���઺��]�G", genbuf, 60, DOECHO))
	    strlcpy(genbuf, "���i�D�A�� !! ^o^", sizeof(genbuf));
	r = write(a, genbuf, 60);
    }
    Signal(SIGPIPE, sig_pipe_handle);

    if (r == -1) {
	close(a);
	snprintf(genbuf, sizeof(genbuf),
		 "%s �w����I�s�A��Enter�~��...", uip->userid);
	getdata(0, 0, genbuf, buf, sizeof(buf), LCECHO);
	clear();
	currstat = currstat0;
	return;
    }

    uip->destuip = currutmp - &SHM->uinfo[0];
    if (buf[0] == 'y')
	switch (sig) {
	case SIG_DARK:
	    main_dark(a, uip);
	    break;
	case SIG_GOMO:
	    gomoku(a, CHESS_MODE_VERSUS);
	    break;
	case SIG_CHC:
	    chc(a, CHESS_MODE_VERSUS);
	    break;
	case SIG_GO:
	    gochess(a, CHESS_MODE_VERSUS);
	    break;
	case SIG_REVERSI:
	    reversi(a, CHESS_MODE_VERSUS);
	    break;
	case SIG_CONN6:
	    connect6(a, CHESS_MODE_VERSUS);
	    break;
	case SIG_TALK:
	default:
	    ccw_talk(a, currutmp->destuid);
	    setutmpmode(XINFO);
	    break;
	}
    else
	close(a);
    clear();
    currstat = currstat0;
}

int
t_chat(void)
{
    static time4_t lastEnter = 0;
    int    fd;

    if (!HasBasicUserPerm(PERM_CHAT)) {
	vmsg("�v�������A�L�k�i�J��ѫǡC");
	return -1;
    }

    if(HasUserPerm(PERM_VIOLATELAW))
    {
       vmsg("�Х�ú�@��~��ϥβ�ѫ�!");
       return -1;
    }

#ifdef CHAT_GAPMINS
    if ((now - lastEnter)/60 < CHAT_GAPMINS)
    {
       vmsg("�z�~�����}��ѫǡA�̭����b��z���C�еy��A�աC");
       return 0;
    }
#endif

#ifdef CHAT_REGDAYS
    if (cuser.numlogindays < CHAT_REGDAYS) {
	vmsgf("�z�|���F��i�J���󭭨� (" STR_LOGINDAYS ": %d, �ݭn: %d)",
              cuser.numlogindays, CHAT_REGDAYS);
	return 0;
    }
#endif

    // start to create connection.
    syncnow();
    move(b_lines, 0); clrtoeol();
    outs(" ���b�P��ѫǳs�u... ");
    refresh();
    fd = toconnect(XCHATD_ADDR);
    move(b_lines-1, 0); clrtobot();
    if (fd < 0) {
	outs(" ��ѫǥ��b��z���A�еy�ԦA�աC");
	system("bin/xchatd");
	pressanykey();
	return -1;
    }

    // mark for entering
    syncnow();
    lastEnter = now;

    return ccw_chat(fd);
}
