#if defined (AUTO_KLINE) || defined (AUTO_KILL_NICK_FLOODING) ||\
 defined (AUTO_KILL_LINK_LOOKERS)
#define AUTOPILOT
#endif

#define VERSION1 "TexasNet Connection Monitor Service by Hendrix [jimi@texas.net]"
#define VERSION2 "tcm-dianora-v0.4.2 db@db.net,db@ottawa.net"
#define VERSION3 "auto-kline by Phisher dkemp@frontiernet.net"
#define VERSION4 "ported to OS/2 by Nighthawk nth.tech@iname.com"

/* Buffer sizes */

/* Size of read buffer on DCC or server connections */
#define BUFFERSIZE     1024

/* scratch buffer size */
#define MAX_BUFF       512

/* small scratch buffer size */
#define SMALL_BUFF	16

#define DCCBUFF_SIZE   150
#define NOTICE_SIZE    150

/* config sizes */
#define MAX_NICK	10	/* - Dianora */

#define MAX_CONFIG	80	/* - Dianora */
#define MAX_CHANNEL	80	/* - Dianora */

#define MAX_HOST	80	/* - Dianora */
#define MAX_DOMAIN	80	/* - Dianora */

/* Macros for universal OS handling of signal vectors */
#define sysvhold notice

/* Why aren't these predefined?!?!?!? */
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

#define INVALID (-1)

#define YES 1
#define NO 0

/*
This structure defines who is connected to this tcm.
*/

typedef struct connection {
  char *buffer;
  char *buffend;
  int  socket;
  char type;		/* type or privs this user has */
  char userhost[MAX_HOST];
  char nick[MAX_NICK+2];	/* allow + 2 for incoming bot names */
  time_t last_message_time;
  }CONNECTION;

/*
   authentication structure set up in userlist.c
   used for .register command
*/

typedef struct 
{
  char *userathost;
  char *usernick;
  char *password;
  int  type;
}AUTH_FILE_ENTRY;

/*

*/

typedef struct 
{
  char *userathost;
  char *theirnick;
  char *password;
  int  port;
}BOT_FILE_ENTRY;

/*
  services struct
  Used to store info about global clones derived from services.us
*/

typedef struct
{
  time_t last_checked_time;
  char   cloning_host[MAX_HOST];
  char   user_count[SMALL_BUFF];
  int    kline_suggested;
}SERVICES_ENTRY;



#define TYPE_OPER		0x01	/* user has .bots privs etc. */
#define TYPE_REGISTERED		0x02	/* user has .kline privs etc. */
#define TYPE_GLINE		0x04	/* user has .gline privs */ 
#define TYPE_BOT		0x08
#define TYPE_IGNORE		0x10    /* user wants to ignore normal chat */

/* Not a type really, just a state.. I couldn't see wasting
   another entire byte for it 
*/
#define TYPE_PENDING		0x80

/* types for sendtoalldcc */

#define SEND_ALL_USERS	0
#define SEND_OPERS_ONLY	1
/* changed my mind, for reasons detailed in serverif.c */
/* #define BOTS_ONLY	2 */

/* Time out for no response from the server 
   5 minutes should be plenty to receive a PING from the server
*/
#define SERVER_TIME_OUT 300

/*
 * global variable to keep track if we are an oper yet
 */
unsigned	amianoper;


/* tokens */

#define K_CLONES   	1
#define K_NFLOOD   	2
#define K_REHASH   	3
#define K_TRACE    	4
#define K_FAILURES 	5
#define K_DOMAINS  	6
#define K_CHECKVERSION 	7
#define K_BOTS     	8
#define K_NFIND    	9
#define K_LIST    	10
#define K_KILLLIST 	11
#define K_GLINE   	12
#define K_KLINE   	13
#define K_KCLONE  	14
#define K_KFLOOD  	15
#define K_KPERM   	16
#define K_KBOT    	17
#define K_KILL    	18
#define K_REGISTER 	19
#define K_OPERS   	20
#define K_TCMLIST 	21
#define K_TCMCONN 	22
#define K_ALLOW   	23
#define K_AUTOPILOT 	24
#define K_CONNECTIONS 	25
#define K_DISCONNECT 	26
#define K_HELP    	27
#define K_CLOSE   	28
#define K_OP      	29
#define K_CYCLE   	30
#define K_DIE     	31
#define K_IGNORE  	32
#define K_TCMINTRO   	33
#define K_TCMIDENT   	34

typedef struct
{
  char   *token_name;
  int    token_value;
}TOKEN_IN;
