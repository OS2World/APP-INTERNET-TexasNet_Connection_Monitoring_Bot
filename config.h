/************************************************************
* MrsBot by Hendrix <jimi@texas.net>                        *
* config.h                                                  *
*   Global definitions obtained by including this file.     *
************************************************************/
/*
   define this if you don't like a local echo back from /dcc chat
*/
/* #define NO_LOCAL_ECHO */

/*
   define this if you are a LEAF 
   Just gives a message telling stats h's you aren't a hub :-)
*/
#undef NOT_A_HUB

/*
  define this is you aren't running a +th server and you want 
  pseudo stats p
*/
#define NOT_TH

/* The default tcm config file , can be overridden with a file name
   argument to tcm
*/

#define CONFIG_FILE "tcm.cf"

#define USERLIST_FILE "userlist.load"
#define BOTLIST_FILE  "botlist.load"

#define HELP_FILE "help"
#define MOTD_FILE "tcm.motd"

/* Only needed for BSDI */
#define DOMAIN "webbnet.org"

#define LOGFILE "clone_log"
#define LAST_LOG_NAME "last_clone_log_name"

#define KILL_KLINE_LOG "kills_klines.log"

#define DEBUG_LOGFILE "tcm.log"

/* How to email, sendmail would work too */
/* This is obviously a SUNos ism */

#define HOW_TO_MAIL "sendmail "

/* For linux this is suggested by zaph */
/* #define HOW_TO_MAIL "/bin/mail -s" */

/* Uncomment this if you want only your opers to connect to tcm */
/* #define OPERS_ONLY */

/* Uncomment this if you are brave, and want to try Phishers
   (dkemp@frontiernet.net) auto kline code 
*/
#define AUTO_KLINE


/* Define this if you wish TCM to auto-kline or auto-kill abusers reported
   from any services as running clones on your server. It will check
   hostlist and opers first
   - Phisher (thanks Phisher, Dianora)

   if AUTO_KLINE_SERVICES is defined, TCM will auto-kline, 
   if AUTO_KLINE_SERVICES is not defined, TCM will will auto-kill
   the cloners (depending on your policy)
   - Dianora
*/

#define AUTOKSERVICES

#define AUTO_KLINE_SERVICES

/* Uncomment this if you don't want hosts listed in
   hostlist.load to trigger a warning at all about cloners etc.
   (requested by Temp)
*/
#undef DONT_WARN_OUR_CLONES

/* Uncomment this if you wish to auto kill users who nick flood
   This can be defined without AUTO_KLINE, and tcm will auto kill
   nick flooders, and not do anything about cloners except report them.
*/
#define AUTO_KILL_NICK_FLOODING

/* undef if you don't want klines reported - Toast */
#define REPORT_KLINES

/*
   Uncomment this if you wish tcm to auto kill link lookers
*/
#define AUTO_KILL_LINK_LOOKERS

/* Uncomment this if you trust remotely connected opers to
   do klines on your tcm

   IF you define this, NONE of the remote .kline .kill .kbot .kclone
   commands will work until they have .registered, you MUST have
   a valid user@host:-DCC-:password:ok userlist.load file in the
   current working directory of tcm. I am absolutely concerned that
   a user might discover how to match an oper user@host and go
   on a kline kill spree... - Dianora
*/
#define REMOTE_KLINE

/*
  If you don't want to allow registered opers on other tcm's
  to do remote klines define this..
  this (for now) disables the ability to do .kill @tcmnick user@host reason
*/
#define RESTRICT_REMOTE_KLINE

/*
  I STRONGLY recommend using PARANOID_PRIVS
  This will disallow even opers who have connected, from
  using the .op, .killlist, .cycle commands until they have .registered
*/
#define PARANOID_PRIVS

/* Define AUTOPILOT_DEFAULT to YES if you want it ON when
   tcm starts up, define to to NO if you want it OFF when
   tcm starts up.
   */

/* #define AUTOPILOT_DEFAULT YES */
#define AUTOPILOT_DEFAULT NO

/* Uncomment this and recompile to get a debug log file */
#undef  DEBUGMODE

/* Maximum number of reconnections to a server allowed before quitting. */
#define MAXRECONNECTS 5

/* Maximum users allowed in MrsBot's userlist */
#define MAXUSERS 100

/* Maximum linked bots allowed in MrsBot's botlist */
#define MAXBOTS 20

/* maximum number of hosts not to auto kline */
#define MAXHOSTS 100

/* whom to message for a global clone report */
/* in the U.S. services */
#define SERVICES_NICK "services"

/* name to expect services reply from */
#define SERVICES_NAME "services.us"

/* name of canadian services server */
#define CA_SERVICES_NAME "services.ca"

/* how many clones to look for globally */
#define SERVICES_CLONE_THRESHOLD 4

/* how often to check for global clones */
#define SERVICES_CHECK_TIME 60

/* Maximum DCC chat connections */
#define MAXDCCCONNS 20

/*
   You can leave these, or change them to suit... - Dianora

   NICK_CHANGE_T1_TIME is the time in seconds, each nick that has
   changed, will get decremented its nick change count, if the user
   stops changing their nick.

   NICK_CHANGE_T2_TIME is how long in seconds, a nick will "live" until its
   purged from the nick change table. Note, that it should be expired
   by the NICK_CHANGE_T1_TIME eventually, but if a nick manages to make
   a horrendous number of nick changes in a short time before being killed
   or k-lined, this will ensure it gets purged within a reasonable length
   of time (set to 5 minutes here)

   NICK_CHANGE_MAX_COUNT is the number of nick changes in a row allowed
   without NICK_CHANGE_T1_TIME before a nick flood is reported.

   - Dianora
*/

#define NICK_CHANGE_T1_TIME  10
#define NICK_CHANGE_T2_TIME 300
#define NICK_CHANGE_MAX_COUNT 5

/* 
   change if you wish - Dianora

   Link looker parameters

   Allow a user MAX_LINK_LOOKS (4) link looks within 
   MAX_LINK_TIME (35) seconds

*/
#define MAX_LINK_LOOKS 4
#define MAX_LINK_TIME 35

/* used in domain report
   paragod reports 1000 for this, is too small on his servers
*/

#define MAXDOMAINS     2000

/* What you've all been waiting for */
/* remote tcm linking options */

/* For this version, I'm going to hard code the input port here */
#define TCM_PORT 6800

/* How long to let a remote tcm attempt to connect, in seconds */
#define TCM_REMOTE_TIMEOUT 30

/* allow table size */
/* This table is used to .allow certain bot nicks
   i.e. see the "spam" from those bots
*/
#define MAX_ALLOW_SIZE 20

