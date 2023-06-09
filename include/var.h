/*
 * This header file is auto-generated from pttbbs/mbbsd/var.c .
 * Please do NOT edit this file directly.
 */

#ifndef INCLUDE_VAR_H
#define INCLUDE_VAR_H
#include "bbs.h"
#define INCLUDE_VAR_H
extern const char * const str_permid[] ;
#ifdef PLAY_ANGEL
#else
#endif
extern const char * const str_roleid[] ;
extern const char * const str_permboard[] ;
#ifdef USE_COOLDOWN
#else
#endif
#ifdef USE_AUTOCPLOG
#else
#endif
extern const char * const str_pager_modes[PAGER_MODES] ;
extern int             usernum;
extern int             currmode ;
extern int             currsrmode ;
extern int             currbid;
extern char            quote_file[80] ;
extern char            quote_user[80] ;
extern char            currtitle[TTLEN + 1] ;
extern const char     *currboard ;
extern char            currBM[IDLEN * 3 + 10];
extern char            margs[64] ;	
extern pid_t           currpid;	
extern time4_t         login_start_time, last_login_time;
extern time4_t         start_time;
extern userec_t        pwcuser;	
extern unsigned int    currbrdattr;
extern unsigned int    currstat;
extern char * const fn_passwd ;
extern char * const fn_board ;
extern const char * const fn_plans ;
extern const char * const fn_writelog ;
extern const char * const fn_talklog ;
extern const char * const fn_overrides ;
extern const char * const fn_reject ;
extern const char * const fn_notes ;
extern const char * const fn_water ;
extern const char * const fn_visable ;
extern const char * const fn_mandex ;
extern const char * const fn_boardlisthelp ;
extern const char * const fn_boardhelp ;
extern char           * const loginview_file[NUMVIEWFILE][2] ;
extern char           * const msg_separator ;
extern char           * const msg_cancel ;
extern char           * const msg_usr_left ;
extern char           * const msg_sure_ny ;
extern char           * const msg_sure_yn ;
extern char           * const msg_bid ;
extern char           * const msg_uid ;
extern char           * const msg_del_ok ;
extern char           * const msg_del_ny ;
extern char           * const msg_fwd_ok ;
extern char           * const msg_fwd_err1 ;
extern char           * const msg_fwd_err2 ;
extern char           * const err_board_update ;
extern char           * const err_bid ;
extern char           * const err_uid ;
extern char           * const err_filename ;
extern char           * const str_mail_address ;
extern char           * const str_reply ;
extern char           * const str_forward ;
extern char           * const str_legacy_forward ;
extern char           * const str_space ;
extern char           * const str_sysop ;
extern char           * const str_author1 ;
extern char           * const str_author2 ;
extern char           * const str_post1 ;
extern char           * const str_post2 ;
extern char           * const BBSName ;
extern char           * const ModeTypeTable[] ;
extern int             b_lines ; 
extern int             t_lines ; 
extern int             p_lines ; 
extern int             t_columns ;
extern char           * const strtstandout ;
extern const int       strtstandoutlen ;
extern char           * const endstandout ;
extern const int        endstandoutlen ;
extern char           * const clearbuf ;
extern const int        clearbuflen ;
extern char           * const cleolbuf ;
extern const int        cleolbuflen ;
extern char           * const scrollrev ;
extern const int       scrollrevlen ;
extern int             automargins ;
extern time4_t         now;
extern int             KEY_ESC_arg;
extern int             watermode ;
extern int             wmofo ;
extern SHM_t          *SHM;
extern boardheader_t  *bcache;
extern userinfo_t     *currutmp;
extern int             TagNum ;		
extern int		TagBoard ;		
extern char            currdirect[64];		
extern char            real_name[IDLEN + 1];
extern char            fromhost[STRLEN] ;
extern char		fromhost_masked[32] ; 
extern char            from_cc[STRLEN] ;
extern char            water_usies ;
extern char            is_first_login_of_today ;
extern char            is_login_ready ;
extern FILE           *fp_writelog ;
extern water_t         *water, *swater[WB_OFO_USER_NUM], *water_which;
#ifdef CHESSCOUNTRY
extern int user_query_mode;
#endif /* defined(CHESSCOUNTRY) */
#define SCR_COLS        ANSILINELEN
extern screenline_t   *big_picture ;
extern char            roll ;
extern char		msg_occupied ;
extern const char     * const bw_chess[] ;
extern char           *friend_file[8] ;
#ifdef NOKILLWATERBALL
extern char    reentrant_write_request ;
#endif
#ifdef PTTBBS_UTIL
    #define COMMON_TIME (time(0))
#else
    #define COMMON_TIME (now)
#endif
#endif /* INCLUDE_VAR_H */
