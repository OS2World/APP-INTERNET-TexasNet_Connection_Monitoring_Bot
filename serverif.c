/***********************************************************
* MrsBot (used in tcm) by Hendrix <jimi@texas.net>          *
*                                                           *
*   Main program is here.                                   *
*   Code to log into server and parse server commands       *
*    and call routine based on the type of message (public, *
*    private, mode change, etc.)                            *
*   Code to create and manage BabyBot here.                 *
*   Based heavily on Adam Roach's bot skeleton.             *
* Includes routines:                                        *
*   int bindsocket                                          *
*   void prnt                                               *
*   char rdpt                                               *
*   void signon                                             *
*   void pong                                               *
*   void linkclosed                                         *
*   void makeconn                                           *
*   void closeconn                                          *
*   void proc                                               *
*   void gracefuldie                                        *
*   void privmsg					    *
*   void onnick()					    *
*   void onnicktaken()					    *
*   void cannotjoin()					    *
*   void ontraceuser()					    *
*   void ontraceclass()					    *
*   void reload_user_list - Dianora                         *
*   void check_services   - Dianora			    *
*   void on_services_notice - Dianora			    *
*   void connect_remote_tcm - Dianora			    *
*   void sendto_all_linkedbots - Dianora		    *
*   int  add_connection - Dianora			    *
*   int  already_have_tcm - Dianora			    *
*   main                                                    *
************************************************************/

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#if defined LINUX || defined __EMX__
# include <sys/socketcall.h>
#else
# include <sys/socketvar.h>
#endif

#ifdef __EMX__
#include <os2.h>
#define NCASECMP	strnicmp
#define CASECMP	stricmp
#else
#define NCASECMP	strncasecmp
#define CASECMP	strcasecmp
#endif

#ifdef AIX
# include <sys/select.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "config.h"
#include "tcm.h"

/* Kludge for funky Linux FD set handlers from Splat */
/* need time.h for logging anyway */

#ifdef LINUX
#include <sys/time.h>
#else
#include <time.h>
#endif

static char *version="$Id: serverif.c,v 1.32 1997/06/01 18:04:15 db Exp db $";

extern int errno;          /* The Unix internal error number */
extern int *timerc;

extern AUTH_FILE_ENTRY userlist[];
extern BOT_FILE_ENTRY botlist[];


#ifdef AUTOPILOT
extern int autopilot;
#endif

extern void list_nicks();		/* - Dianora bothunt.c */
extern void init_nick_change_table();	/* - Dianora bothunt.c */
extern void report_nick_flooders();	/* - Dianora bothunt.c */
extern void initopers();		/* - Dianora bothunt.c */
extern void on_stats_o();		/* - Dianora bothunt.c */
extern void load_hostlist();		/* - Phisher userlist.c */
extern void onservnotice();		/* - Hendrix bothunc.c */
extern void print_help();		/* - Hendrix bothunt.c */

/* All of this should be in one struct */
/*
  These should all be in a CONFIG struct - Dianora
*/

extern char username_config[];
extern char virtual_host_config[];
extern char server_config[];
extern char server_name[];
extern char server_port[];
extern char ircname_config[];
extern char email_config[];
extern int  tcm_port;
extern char mychannel[MAX_CHANNEL];

int GettingCASVCSReports = 0;

char ourhostname[MAX_HOST];   /* This is our hostname with domainname */
char serverhost[MAX_HOST];    /* Server tcm will use. */
char defchannel[MAX_CHANNEL]; /* Channel tcm will use. */
char dfltnick[MAX_NICK];      /* Nickname tcm will use. */
char mynick[MAX_NICK];        /* tcm's current nickname */

/* kludge for ensuring no direct loops */
int  incoming_connnum;	      /* current connection number incoming */
/* KLUDGE  *grumble* */
/* allow for ':' ' ' etc. */

struct 
{
  char to_nick[MAX_NICK+1];
  char to_tcm[MAX_NICK+1];
  char from_nick[MAX_NICK+1];
  char from_tcm[MAX_NICK+1];
}route_entry;


void send_to_nick(char *,char *); /* - Dianora */
void prnt(int, char*);		/* - Hendrix */
void pong(int,char *);		/* - Hendrix */
void do_init(void);		/* - Hendrix */
int bindsocket(char *);		/* - Hendrix */
void toserv(char *);		/* - Hendrix */
void sendtoalldcc(char *,int ); /* - Hendrix */
void privmsg();
void onkick();
void onjoin();
void onnick();
void onnicktaken();
void ontraceuser();
void ontraceclass();
void gracefuldie(int);		/* - Hendrix */
void linkclosed(void);		/* - Hendrix */
char makeconn(char *,char *,char *); /* - Hendrix */
void serverproc();
void closeconn(int);		/* - Hendrix */
void dccproc(int);		/* - Hendrix, much modified now */
void proc(char *,char *,char *); /* - Hendrix */
void initlog();
void log_kline(struct tm *,char *,char *,char *,char *); /* - Dianora */
void reload_user_list(void);	/* - Dianora */
void clear_userlist();		/* - Dianora */
void check_services();		/* - Dianora */
void on_services_notice(char *);	/* - Dianora */
void on_ca_services_notice(char *);	/* - chris/Dianora */
void connect_remote_tcm(int);		/* - Dianora */
void init_remote_tcm_listen(void);	/* - Dianora */
void init_allow_nick();		/* - Dianora */
void setup_allow();		/* - Dianora */
void sendto_all_linkedbots();	/* - Dianora */
void do_a_kline(char *,char *,char *,char *);		/* - Dianora */
int  add_connection(int,int);		/* - Dianora */
int  test_ignore();		/* - Dianora */
char *type_show();		/* - Dianora */
int  already_have_tcm(char *);		/* - Dianora */

int reconnects = 0;        /* Times we have reconnected to the server */
int quit = NO;             /* When it is YES, we quit almost immediately */

FILE *outfile;             /* Debug output file handle
			      Now shared with writing pid file - Dianora */

FILE *logging_fp;	   /* If logging clones etc. - Dianora */
int remote_tcm_socket;	   /* listening socket */
fd_set readfds,nullfds;    /* Two file descriptor sets for use with select */


CONNECTION connections[MAXDCCCONNS+1];

/*  OH BLAH this is ugleee */
char source[80];
char fctn[80];
char body[512];
int  maxconns = 0;


/* For talking to services */
SERVICES_ENTRY services;

char allow_nick[MAX_ALLOW_SIZE][MAX_NICK+4];

/*
** bindsocket()
**   Sets up a socket and connects to the given host and port
*/
int bindsocket(char *hostport)
{
  int plug;
  struct sockaddr_in socketname;
  struct sockaddr_in localaddr;
  struct hostent *remote_host;
  /* virtual host support - dianora */
  struct hostent *local_host;
  char buf [BUFSIZ];
  char s [BUFSIZ];
  int portnum = 6667;
  char server[MAX_HOST],*hold;
  int optval;

  /* Parse serverhost to look for port number */
  strcpy (server,hostport);

  if ((hold = strchr(server,':')) != (char *)NULL)
    {
      *(hold++) = '\0';
      portnum = atoi(hold);
    }

  /* open an inet socket */
  if ((plug = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
      printf ("error: can't assign fd for socket\n");
      return (INVALID);
    }

  optval = 1;
  /* Let us pray.... */
  (void)setsockopt(plug,SOL_SOCKET,SO_REUSEADDR,(char *)&optval,
		   sizeof(optval));
  (void) bzero(&socketname, sizeof(socketname));

  /* virtual host support - Dianora */
  if(virtual_host_config[0] != '\0')
    {
      if ((local_host = gethostbyname (virtual_host_config))
	  != (struct hostent *) NULL)
	{
	  printf("virtual host [%s]\n",virtual_host_config);
	  printf("found official name [%s]\n",
		 local_host->h_name);

	  (void) bzero(&localaddr, sizeof(struct sockaddr_in));
	  (void) bcopy ((char *) local_host->h_addr,
			(char *) &localaddr.sin_addr,
			local_host->h_length);
	  localaddr.sin_family = AF_INET;
	  localaddr.sin_port = 0;

	  if(bind(plug,(struct sockaddr *)&localaddr,
	       sizeof(socketname)) < 0)
	    {
	      perror("unable to bind virtual host");
	    }
	  else
	    printf("bound to virtual host\n");
	}
    }

  /* kludge for DCC CHAT precalculated sin_addrs */
  if (*server == '#')
    {
#if defined(LINUX) || defined(FREEBSD) || defined(BSDI) || defined(SOLARIS) || defined(GNUWIN32) || defined(AIX) || defined(NEXT)
    socketname.sin_addr.s_addr = htonl(strtoul(server+1,(char **)NULL,10));
#else
    socketname.sin_addr.s_addr = htonl(strtol(server+1,(char **)NULL,10));
#endif
    }
  /* lookup host */
  else
    {
      if ((remote_host = gethostbyname (server)) == (struct hostent *) NULL)
	{
	  printf ("error: unknown host: %s\n", server);
	  return (INVALID);
	}
      (void) bcopy ((char *) remote_host->h_addr,
		    (char *) &socketname.sin_addr,
		    remote_host->h_length);
    }
  socketname.sin_family = AF_INET;
  socketname.sin_port = htons (portnum);

  /* connect socket */
 while (connect (plug, (struct sockaddr *) &socketname, sizeof socketname) < 0)
    if (errno != EINTR)
      {
	printf("Error: connect %i\n", errno);
	return (INVALID);
      }

  return (plug);
}

/*
** prnt()
**   Like toserv() but takes a socket number as a parameter.  This
**    makes it useful for use by both MrsBot and BabyBot.  Only
**    called from this file though.
*

inputs		- socket to reply on if local
output		- NONE
side effects	- input socket is ignored if its message meant
		  for user on another tcm, or its message
		  meant for a specific user on this tcm.

*/
void prnt(int sock,char *msg)
{
  char dccbuff[DCCBUFF_SIZE];

  if(route_entry.to_nick[0] != '\0')
    {
      printf("DEBUG: prnt route_entry non null\n");

      (void)sprintf(dccbuff,":%s@%s %s@%s %s",
		    route_entry.to_nick,
		    route_entry.to_tcm,
		    dfltnick,
		    dfltnick,
		    msg);

      printf("DEBUG: msg = [%s]",dccbuff);
      send(sock, dccbuff, strlen(dccbuff), 0);
    }
  else
    {
      send(sock, msg, strlen(msg), 0);
      printf("DEBUG: msg =  [%s]",msg);
    }
#ifdef DEBUGMODE
      (void)printf("%s",msg);	/* - zaph */
      (void)fprintf(outfile,"%s",msg);
#endif
}

/*
send_to_nick

inputs		- nick to send to
		  buffer to send to nick
output		- NONE
side effects	- NONE
*/

void send_to_nick(char *to_nick,char *buffer)
{
  int i;
  char dccbuff[DCCBUFF_SIZE];

  strncpy(dccbuff,buffer,DCCBUFF_SIZE-2);
  strcat(dccbuff,"\n");

  for( i = 1; i < maxconns; i++ )
    {
      if(CASECMP(to_nick,connections[i].nick) == 0)
	{
	  send(connections[i].socket, dccbuff, strlen(dccbuff), 0);
	}
    }
}

/*
toserv

inputs		- msg to send directly to server
output		- NONE
side effects	- server executes command.
*/

void toserv(char *msg)
{
  if (connections[0].socket != INVALID)
    send(connections[0].socket, msg, strlen(msg), 0);
}

/*
sendtoalldcc

inputs		- message to send
		- flag if message is to be sent only to all users or opers only
output		- NONE
side effects	- message is sent on /dcc link to all connected
		  users or to only opers on /dcc links

		  - Dianora

  I was going to include a flag "BOTS_ONLY", to have this routine
only send msg to linked bot, but I think its less clear doing it that way
and less efficient...

*/

void sendtoalldcc(char *msg,int type)
{
  char buff[MAX_BUFF];
  char bot_buff[MAX_BUFF];
  int i;
  int local_tcm = NO;	/* local tcm ? */

  /* what a hack...
     each tcm prefixes its messages sent to each user
     with "<nick@botnick>" unless its a clone report, link report
     etc. This is fine, unless its being sent to a bot

     if its an "o:" its gonna have a "o:<user@botnick>" so it goes
     straight through

     if its a '.' command, it goes straight through
     as '.' commands are not seen elsewhere, but are directly
     dealt with. (i.e. glines)

     if its another else and is missing the '<' it gets the bot nick
     prepended.
  */

  /* If opers only message, it goes straight through */
  if((msg[0] == 'o' || msg[0] == 'O')
	 && msg[1] == ':')
    {
      (void)sprintf(bot_buff,"%s\n",msg);
    }
  else
    {
      /* command prefix, goes straight through */

      if(msg[0] == '.')
	(void)sprintf(bot_buff,"%s\n",msg);

      /* Missing a leading '<', we prepend the botnick as <botnick> */
      else if(msg[0] != '<')
	{
	  (void)sprintf(bot_buff,"<%s> %s\n",dfltnick,msg);
	  local_tcm = YES;
	}

      /* anything else already has a leading "<" or "O:<" */
      else
	(void)sprintf(bot_buff,"%s\n",msg);
    }
  (void)sprintf(buff,"%s\n",msg);

  for ( i = 1; i < maxconns; i++)
    {
#ifdef NO_LOCAL_ECHO
      if( i != incoming_connnum )
#else
      if( ((connections[i].type & TYPE_BOT) && (i == incoming_connnum )) )
	/* NULL STATEMENT do nothing here */
	;
      else
#endif
	{
	  if (connections[i].socket != INVALID)
	    {
	      switch(type)
		{
		case SEND_OPERS_ONLY:
		  if(connections[i].type & TYPE_BOT)
		    prnt(connections[i].socket, bot_buff);
		  else if(connections[i].type & TYPE_OPER)
		    prnt(connections[i].socket, buff);
		  break;

		case SEND_ALL_USERS:
		  if(connections[i].type & TYPE_BOT)
		    prnt(connections[i].socket, bot_buff);
		  else
		    {
		      if(local_tcm)
			prnt(connections[i].socket, buff);
		      else
			{
			  if(!(connections[i].type & TYPE_IGNORE))
			    prnt(connections[i].socket, buff);
			}
		    }
		  break;

		default:
		  break;
		}
	    }
	}
    }
}

/*
** rdpt()
**   Readincoming data off one of the sockets and process it
*/
void rdpt(void)
{
  int select_result;
  int lnth, i;
  char dccbuff[DCCBUFF_SIZE];
  struct timeval server_time_out;

  for(;;)
    {
      FD_ZERO (&readfds);
      for (i=0; i<maxconns; ++i)
	if (connections[i].socket != INVALID)
	  FD_SET(connections[i].socket,&readfds);

      FD_SET(remote_tcm_socket,&readfds);

      server_time_out.tv_sec = SERVER_TIME_OUT;
      server_time_out.tv_usec = 0L;

      if( (select_result = select(FD_SETSIZE, &readfds, &nullfds, &nullfds,
				  &server_time_out)) > 0)
	{
	  if( FD_ISSET(remote_tcm_socket, &readfds) )
	    connect_remote_tcm(INVALID);

	  for (i=0; i<maxconns; ++i)
	    {
	      if (connections[i].socket != INVALID)
		{
		  if( (connections[i].type & TYPE_PENDING) &&
		      ((connections[i].last_message_time + TCM_REMOTE_TIMEOUT)
		       < time((time_t *)NULL)) )
		    {
		      closeconn(i);
		      continue;
		    }

		  if( FD_ISSET(connections[i].socket, &readfds))
		    {
		      incoming_connnum = i;

		      lnth =recv(connections[i].socket,connections[i].buffend,
				 1,0);

		      if (lnth == 0)
			{
			  if (i == 0)
			    linkclosed();
			  else
			    closeconn(i);
			  return;
			}
		      if (*connections[i].buffend == '\n' ||
			  *connections[i].buffend == '\r' ||
			  connections[i].buffend - connections[i].buffer
			  == BUFFERSIZE -1)
			{
			  *connections[i].buffend = '\0';
			  connections[i].buffend = connections[i].buffer;
			  if (*connections[i].buffer)
			    if (i == 0)
			      serverproc();
			    else
			      {
				if( (connections[i].type & TYPE_BOT) &&
				    (connections[i].type & TYPE_PENDING))
				  connect_remote_tcm(i);
				else
				  dccproc(i);
			      }
			  return;
			}
		      else
			++connections[i].buffend;
		    }
		}
	    }
	  check_services();
	}
      else
	{
	  if( select_result == 0)
	    linkclosed();		/* PING time out of server */
	}
    }
}

/*
check_services

inputs		- NONE
output		- NONE
side effects	-
*/

void check_services(void)
{
  time_t cur_time;
  char clone_msg[MAX_BUFF];

  cur_time = time((time_t *)NULL);

  if((services.last_checked_time + SERVICES_CHECK_TIME) < cur_time )
     {
       services.last_checked_time = cur_time;
       route_entry.to_nick[0] = '\0';
       route_entry.to_tcm[0] = '\0';
       route_entry.from_nick[0] = '\0';
       route_entry.from_tcm[0] = '\0';

       (void)sprintf(clone_msg,"clones %d\n", SERVICES_CLONE_THRESHOLD );
       msg(SERVICES_NICK,clone_msg);
       /* check if services.ca just signed on */
       toserv("ISON CA-SVCS\r\n");
     }
}

/*
on_services_notice

inputs		- body from message sent to us from server
output		- NONE
side effects	- reports of global cloners
*/

void on_services_notice(char *body)
{
  char *parm1;
  char *parm2;
  char *parm3;
  char dccbuff[DCCBUFF_SIZE];
  char kbuffer[DCCBUFF_SIZE];
  char userathost[MAX_HOST];
  int  identd;

  while(*body == ' ')
    body++;

  parm1 = strtok(body," ");
  if(parm1 == (char *)NULL)
    return;

  parm2 = strtok((char *)NULL," ");
  if(parm2 == (char *)NULL)
    return;

  parm3 = strtok((char *)NULL,"");
  if(parm3 == (char *)NULL)
    return;

  if( strstr(parm3,"users") )
    {
      strncpy(services.cloning_host,parm1,MAX_HOST-1);
      strncpy(services.user_count,parm3,SMALL_BUFF-1);
      services.kline_suggested = NO;
      return;
    }

  if((CASECMP("on",parm2) == 0) && (CASECMP(server_name,parm3) == 0))
    {
      (void)sprintf(dccbuff,"%s reports %s cloning %s nick %s",
		    SERVICES_NAME,
		    services.user_count,
		    services.cloning_host,
		    parm1);

      msg(mychannel,dccbuff);
      sendtoalldcc(dccbuff,SEND_ALL_USERS);

      if(services.kline_suggested == NO)
	{
	  char *user;
	  char *host;
	  char user_host[MAX_HOST+1];

	  strncpy(userathost,services.cloning_host,MAX_HOST);

	  /* strtok is going to destroy the original userathost,
	     so save a copy for our own uses */

	  strncpy(user_host,userathost,MAX_HOST);

	  user = strtok(userathost,"@");	
	  if(user == (char *)NULL)
	    return;

	  identd = YES;
	  if(*user == '~')
	    identd = NO;

	  host = strtok((char *)NULL,"");
	  if(host == (char *)NULL)
	    return;


#ifdef AUTOKSERVICES	   /* If doing AUTO anything on any services */
#ifdef AUTO_KLINE_SERVICES /* If doing AUTO kline on any services */
	  if( (okhost(user_host)) || (isoper(user_host)))return;

	  suggest_kline(YES,
			user,
			host,
			NO,
			identd,
			"clones/multiple servers");

#else			/* doing AUTO on services.us, but killing only */

	  if( (okhost(user_host)) || (isoper(user_host)))return;

	  (void)sprintf(kbuffer,"KILL %s :services.us clones\n",parm1);
	  toserv(kbuffer);

	  suggest_kline(NO,
			user,
			host,
			NO,
			identd,
			"clones/multiple servers");
#endif
#else			/* Not doing AUTO on services.us but reporting */

	  suggest_kline(NO,
			user,
			host,
			NO,
			identd,
			"clones/multiple servers");
#endif
	  services.kline_suggested = YES;
	}
    }
}

/*
on_ca_services_notice

inputs		- body from message sent to us from server
output		- NONE
side effects	- reports of global cloners
*/

void on_ca_services_notice(char *body)
{
  char *parm1, *parm2, *parm3, *parm4;
  char *user, *host, *tmp;
  char dccbuff[DCCBUFF_SIZE];
  char userathost[MAX_HOST];

	while(*body == ' ')
		body++;

	parm1 = strtok(body," ");
	if (parm1 == (char *)NULL)
		return;

	parm2 = strtok((char *)NULL," ");
	if(parm2 == (char *)NULL)
		return;

	parm3 = strtok((char *)NULL," ");
	if(parm3 == (char *)NULL)
		return;

	parm4 = strtok((char *)NULL," ");
	if(parm4 == (char *)NULL)
		return;

	/*
	 * If this is services.ca suggesting a kline, we'll do it
	 * ourselves, thanks very much.  Just return. 
	 */
	if (CASECMP("kline",parm2) == 0)
		return;

	if (CASECMP("Nickflood",parm1) == 0)
	  {
#ifdef AUTOKSERVICES
#ifdef AUTO_KILL_NICK_FLOODING
	    if(autopilot)
	      {
		sprintf(dccbuff, "KILL %s :CaSvcs-Reported Nick Flooding\n",
			parm3);
		toserv(dccbuff);
		sprintf(dccbuff,
			"AutoKilled %s %s for CaSvcs-Reported Nick Flooding",
			parm3, parm4);
		sendtoalldcc(dccbuff,SEND_ALL_USERS);
	      }
	    else
#endif
#endif
	      {
		sprintf(dccbuff,
			"CaSvcs-Reported Nick flooding %s %s",
			parm3, parm4);
		sendtoalldcc(dccbuff,SEND_ALL_USERS);
	      }

	}
	
	(void)sprintf(dccbuff,"%s: %s %s %s %s",
		CA_SERVICES_NAME, parm1, parm2, parm3, strtok(NULL, ""));

	msg(mychannel,dccbuff);
	sendtoalldcc(dccbuff,SEND_ALL_USERS);

	/*
	 * Nope, services.ca is reporting clones.  Determine the 
	 * user@host.
	 */

	tmp = strtok(dccbuff,"(");	/* Chop off up to the '(' */
	tmp = strtok((char *)NULL,")");	/* Chop off up to the ')' */

	user = strtok(tmp,"@");		/* Chop off up to the ')' */
	if (user == (char *)NULL)
		return;			/* Hrm, strtok couldn't parse. */

	host = strtok((char *)NULL, "");
	if (host == (char *)NULL)
		return;			/* Hrm, strtok couldn't parse. */

	if (CASECMP("user-clones",parm2) == 0) {
		/*
		 * Is this services.ca reporting multiple user@hosts?
		 */
		suggest_kline(NO,user,host,NO,(*user != '~'), "CaSvcs-Reported User Clones");
	} else if (CASECMP("site-clones",parm2) == 0) {
		/*
		 * Multiple site clones? 
		 */
		suggest_kline(NO,user,host,YES,(*user != '~'), "CaSvcs-Reported Site Clones");
	} else if (CASECMP("Nickflood",parm1) == 0) {
		/*
		 * Nick flooding twit.
		 */
		suggest_kline(NO,user,host,NO,(*user != '~'), "CaSvcs-Reported Nick Flooding");
	}
}

/*
serverproc()

inputs		- NONE
output		- NONE
side effects	- process server message
*/

void serverproc()
{
  char *buffer = connections[0].buffer;
  char *curr;
  char *rest;

  *source = *body = *fctn = '\0';
  rest = strchr(buffer,'.');
  if (!rest || strchr(buffer,' ') < rest)
    curr = buffer;
  else {
    curr = strchr(buffer,' ');
    if (curr)
      *(curr++) = '\0';
    strcpy(source, (*buffer == ':' ? buffer + 1 : buffer));
    }

  if (curr) {
    rest = strchr(curr, ' ');
    if (rest) {
      *(rest++) = '\0';
      strcpy(body,rest);
      }
    strcpy(fctn,curr);
    }
  printf(">%s %s %s\n",source, fctn, body);	/* - zaph */
#ifdef DEBUGMODE
  fprintf(outfile,">%s %s %s\n",source, fctn, body);
#endif
  proc(source,fctn,body);
}

/*
** signon()
**   Send a USER and a NICK string to the server to facilitate signon.
**   Parameters: None
**   Returns: void
**   PDL:
**     What it said 4 lines above. :)  Also, initialize the internal
**     variable holding tcm's current nickname.
*/
void signon()
{
    char buff[MAX_BUFF];

    connections[0].buffend = connections[0].buffer;
    if (!*mynick)
      strcpy (mynick,dfltnick);
    (void)sprintf(buff,"user %s %s %s :%s\nnick %s\n",
		  username_config,
		  ourhostname,
		  server_name,
		  ircname_config,
		  mynick);
    prnt(connections[0].socket, buff);
}

/*
** pong()
**   Send back a pong to a pinging server.
**   Parameters:
**     sock - Socket being pinged
**     hst - Server doing the pinging
**   Returns: void
**   PDL:
**    Ugly, but it sends a "PONG <servername>\n" message.
*/
void pong(int sock,char *hst)
{
  prnt(sock,"pong ");
  prnt(sock,hst);
  prnt(sock,"\n");
}

void do_init(void)
{
  join(defchannel); 
  initopers();
  oper();
  inithash();
}

/*
** linkclosed()
**   Called when an error has causes the server to close our link.
**   Parameters: None
**   Returns: void
**   PDL:
**     Close the old dead socket.  If we haven't already reconnected
**     5 times, wait 5 seconds, reconnect to the server, and re-signon.
*/
void linkclosed()
{
  (void)close(connections[0].socket);
  if(logging_fp)
    (void)fclose(logging_fp);	/* -Dianora */
  
  if (++reconnects > MAXRECONNECTS)
    quit = YES;
  else
    {
      sleep(30);
      connections[0].socket = bindsocket(serverhost);
      if (connections[0].socket == INVALID)
	{
	  quit = YES;
	  return;
	}
      clear_userlist();
      load_botlist();
      signon();
      freehash();
      amianoper = NO;
    }
}

/*
** makeconn()
**   Makes another connection
*/
char makeconn(char *hostport,char *nick,char *userhost)
{
  int i;
  char *type;
  char dccbuff[DCCBUFF_SIZE];

  for (i=1; i<MAXDCCCONNS+1; ++i)
    if (connections[i].socket == INVALID)
      {
	if (maxconns < i+1)
	  maxconns = i+1;
	break;
      }

  if (i > MAXDCCCONNS)
    return 0;

  connections[i].socket = bindsocket(hostport);

  if (connections[i].socket == INVALID)
    return 0;

  connections[i].buffer = (char *)malloc(BUFFERSIZE);
  if(connections[i].buffer == (char *)NULL)
    {
      sendtoalldcc("Ran out of memory in makeconn",SEND_ALL_USERS);
      gracefuldie(0);
    }

  connections[i].buffend = connections[i].buffer;
  strncpy(connections[i].nick,nick,MAX_NICK-1);
  connections[i].nick[MAX_NICK-1] = '\0';
  strncpy(connections[i].userhost,userhost,MAX_HOST-1);
  connections[i].userhost[MAX_HOST-1] = '\0';
  connections[i].type = 0;
  connections[i].type |= isoper(userhost);
  
/* I think the credit for this idea of OPERS_ONLY is from phisher */
/* my hack though. blame me. - Dianora */

#ifdef OPERS_ONLY
  if(!(connections[i].type & TYPE_OPER))
    {
      prnt(connections[i].socket,
	   "Sorry, only opers may use this service.\n");
      (void)close(connections[i].socket);
      connections[i].socket = INVALID;
      connections[i].nick[0] = '\0';
      connections[i].userhost[0] = '\0';
      connections[i].type = 0;
      (void)free(connections[i].buffer);
      return 0;
    }
#endif
  prnt(connections[i].socket,VERSION2);
  prnt(connections[i].socket,"\n");

  /* Print a message of the day - Dianora */
  print_motd(connections[i].socket);

#ifdef AUTOPILOT
  if( autopilot )
    prnt(connections[i].socket,"autopilot is ON\n");
  else
    prnt(connections[i].socket,"autopilot is OFF\n");
#endif

  type = "User";
  if(connections[i].type & TYPE_OPER)
    type = "Oper";
  if(connections[i].type & TYPE_BOT)
    type = "Bot";

  sprintf(dccbuff,"%s %s (%s) has connected",
	  type,
	  connections[i].nick,
	  connections[i].userhost);
  msg(mychannel,dccbuff);
  sendtoalldcc(dccbuff,SEND_ALL_USERS);
  prnt(connections[i].socket,"Connected.  Send '.help' for commands.\n");
  return 1;
}

/*
add_connection

inputs		- socket
		- bot_entry
output		-
side effects	-
*/

int add_connection(int socket,int bot_entry)
{
  int i;

  for( i=1; i<MAXDCCCONNS+1; ++i )
    {
      if(connections[i].socket == INVALID)
	{
	  if(maxconns < i+1)
	    maxconns = i+ 1;
	  break;
	}
    }
  if(i > MAXDCCCONNS)
    return(INVALID);

  connections[i].buffer = (char *)malloc(BUFFERSIZE);
  if(connections[i].buffer == (char *)NULL)
    {
      sendtoalldcc("Ran out of memory in add_connection",SEND_ALL_USERS);
      gracefuldie(0);
    }

  connections[i].buffend = connections[i].buffer;

  strncat(connections[i].nick,botlist[bot_entry].theirnick,MAX_NICK-1);
  
  connections[i].nick[MAX_NICK-1] = '\0';
  strncpy(connections[i].userhost,botlist[bot_entry].userathost,MAX_HOST-1);
  connections[i].userhost[MAX_HOST-1] = '\0';
  connections[i].socket = socket;
  connections[i].type = TYPE_BOT|TYPE_OPER|TYPE_REGISTERED;
  return(i);
}

/*
closeconn()

inputs		- connection number
outptut		- NONE
side effects	- connection on connection number connnum is closed.
*/

void closeconn(int connnum)
{
  int i;
  char dccbuff[DCCBUFF_SIZE];
  char *type;

  if (connections[connnum].socket != INVALID)
    close(connections[connnum].socket);

  if(connections[connnum].buffer)
    free (connections[connnum].buffer);

  connections[connnum].buffer = (char *)NULL;
  connections[connnum].socket = INVALID;

  if (connnum + 1 == maxconns)
    {
      for (i=maxconns;i>0;--i)
	if (connections[i].socket != INVALID)
	  break;
      maxconns = i+1;
    }

  type = "User";
  if(connections[connnum].type & TYPE_OPER)
    type = "Oper";
  if(connections[connnum].type & TYPE_BOT)
    type = "Bot";

  if(connections[connnum].type & TYPE_PENDING)
    {
      (void)sprintf(dccbuff,"Failed Bot connect from %s",
	      connections[connnum].userhost);
    }
  else
    {
      sprintf(dccbuff,"%s %s (%s) has disconnected",
	      type,
	      connections[connnum].nick,
	      connections[connnum].userhost);
    }

  msg(mychannel,dccbuff);
  sendtoalldcc(dccbuff,SEND_ALL_USERS);
  connections[connnum].userhost[0] = '\0';
  connections[connnum].nick[0] = '\0';
}

/*
** dccproc()
**   Handles processing of dcc chat commands
*/
void dccproc(int connnum)
{
  char *buffer = connections[connnum].buffer;
  char dccbuff[MAX_BUFF];
  char who_did_command[2*MAX_NICK];
  int i;
  int opers_only = SEND_ALL_USERS; 	/* Is it an oper only message ? */
  int ignore_bot = NO;
  int token;
  char *param1,*param2,*param3;

  /* wot a kludge (to rhyme with sludge) */

  route_entry.to_nick[0] = '\0';
  route_entry.to_tcm[0] = '\0';
  route_entry.from_nick[0] = '\0';
  route_entry.from_tcm[0] = '\0';
  who_did_command[0] = '\0';

  /* remote message, either to a tcm command parser,
     or from a user meant to be sent on to another remote tcm,
     or, its from a remote tcm to be passed onto another tcm
  */

  if(*buffer == ':')
    {
      char *to,*from;
      char *to_nick;
      char *to_tcm;
      char *from_nick;
      char *from_tcm;

      buffer++;	/* skip the ':' */

      if(connections[connnum].type & TYPE_BOT)
	{
	  to = strtok(buffer," ");
	  if(to == (char *)NULL)return;	/* duh, give up */
	  from = strtok((char *)NULL," "); 
	  if(from == (char *)NULL)return; /* duh, give up */

	  to_nick = to;
	  to_tcm = strchr(to,'@');
	  if(to_tcm == (char *)NULL)
	    to_tcm = to;
	  else
	    {
	      *to_tcm = '\0';
	      to_tcm++;
	    }

	  from_nick = from;
	  strncpy(who_did_command,from,2*MAX_NICK);
	  printf("DEBUG: from_nick = [%s]\n", from_nick);
	  from_tcm = strchr(from,'@');
	  if(from_tcm == (char *)NULL)return; /*shrug*/
	  *from_tcm = '\0';
	  from_tcm++;

	  buffer = strtok((char *)NULL,"");
	  if(buffer == (char *)NULL)
	    return;
	  while(*buffer == ' ')
	    buffer++;
	  printf("DEBUG: final buffer [%s]\n", buffer);

	  if(CASECMP(to_tcm,dfltnick) == 0)	
	    {
	      /* Directed to someone on this tcm */
	      if(CASECMP(to_nick,dfltnick) == 0)
		{
		  /* Directed to the tcm itself */
		  /* Set up to let prnt return to this address */
		  strncpy(route_entry.to_nick,from_nick,MAX_NICK);
		  strncpy(route_entry.to_tcm,from_tcm,MAX_NICK);
		  strncpy(route_entry.from_nick,dfltnick,MAX_NICK);
		  strncpy(route_entry.from_tcm,dfltnick,MAX_NICK);
		  (void)printf("DEBUG: message to this tcm...\n");
		}
	      else
		{
		  /* Directed to this nick on the tcm */
		  send_to_nick(to_nick,buffer);
		  return;
		}
	    }
	  else
	    {
	      /* Directed to someone on another tcm */
	      (void)sprintf(dccbuff,":%s@%s %s@%s %s\n",
			to_nick,
			to_tcm,
			from_nick,
			from_tcm,
			buffer);

	      sendto_all_linkedbots(dccbuff);
	      return;
	    }
	}
      else	/* Its user */
	{
	  /* :server .command */

	  printf("DEBUG: user buffer = [%s]\n", buffer);

	  to_nick = strtok(buffer," ");
	  if(to_nick == (char *)NULL)
	    return;
	  buffer = strtok((char *)NULL,"");
	  if(buffer == (char *)NULL)
	    return;
	  while(*buffer == ' ')
	    buffer++;

	  (void)sprintf(dccbuff,":%s@%s %s@%s %s\n",
			to_nick,
			to_nick,
			connections[connnum].nick,
			dfltnick,
			buffer);

	  sendto_all_linkedbots(dccbuff);
	  return;
	}
    }
  else
    {
      (void)sprintf(who_did_command,"%s@%s",
		    connections[connnum].nick,dfltnick);

    }

  printf("DEBUG: After routing parsing buffer = [%s]\n", buffer);

  /* added command character - Dianora */
  if(*buffer != '.')
    {	
      if((buffer[0] == 'o' || buffer[0] == 'O')
	 && buffer[1] == ':')
	{
	  opers_only = SEND_OPERS_ONLY;
	  if( (connections[connnum].type & TYPE_BOT))
	    {
	      strncpy(dccbuff,buffer,MAX_BUFF);
	    }
	  else
	    {
	      (void)sprintf(dccbuff,"o:<%s@%s> %s",
			    connections[connnum].nick,dfltnick,
			    buffer+2);
	    }
	}
      else
	{
	  if((connections[connnum].type & TYPE_BOT))
	    {
	      ignore_bot = test_ignore(buffer);
	      strncpy(dccbuff,buffer,MAX_BUFF);
	    }
	  else
	    {
	      (void)sprintf(dccbuff,"<%s@%s> %s",
			    connections[connnum].nick,dfltnick,buffer);
	    }
	}
      if(!ignore_bot)
	{
	  if(connections[connnum].type & TYPE_IGNORE )
	    {
	      if(opers_only == SEND_OPERS_ONLY)
		sendtoalldcc(dccbuff,opers_only);
	      else
		prnt(connections[connnum].socket,
		     "You have IGNORE on, not sending to chat line\n");
	    }
	  else
	    sendtoalldcc(dccbuff,opers_only); /* Thanks Garfr, Talen - Dianora */
	}
      return;
    }

  (void)printf("DEBUG:  buffer [%s]\n", buffer);

  if((connections[connnum].type & TYPE_BOT) &&
     (route_entry.to_nick[0] == '\0'))
    {

      (void)printf("DEBUG:type & TYPE_BOT &&\n");
      (void)printf("DEBUG:route_entry.to_nick[0] = '%c'\n", 
		   route_entry.to_nick[0]);


      if(buffer[1] != 'T')	/* You didn't see this and
				   I won't admit to it */
	{
	  if(connections[connnum].type & TYPE_REGISTERED)
	    {
	      strcat(buffer,"\n");
	      sendto_all_linkedbots(buffer);
	      toserv(buffer+1);
	    }
	  return;
	}
    }

  (void)printf("DEBUG: about to skip '.' buffer [%s]\n", buffer);

  buffer++;	/* skip the '.' */


  /*
    This parser should be re-written using a hash table lookup
    If someone doesn't beat me to it, I'll get around to doing it
    - Dianora
    */

  param1 = strtok(buffer," ");
  param2 = strtok((char *)NULL," ");

  if(!param1)
    return;

  printf("param1 = [%s]\n", param1);

  if(param2)
    printf("param2 = [%s]\n", param2);

  if(param2)
    {
      if(*param2 == '@')
	{
	  param3 = strtok((char *)NULL,"");

	  /* Directed to someone on another tcm */
	  if(param3)
	    {
	      (void)sprintf(dccbuff,":%s@%s %s@%s .%s %s\n",
			    param2+1,
			    param2+1,
			    connections[connnum].nick,
			    dfltnick,
			    param1,
			    param3);
	    }
	  else
	    {
	      (void)sprintf(dccbuff,":%s@%s %s@%s .%s\n",
			    param2+1,
			    param2+1,
			    connections[connnum].nick,
			    dfltnick,
			    param1);
	    }
  
	  sendto_all_linkedbots(dccbuff);
	  return;
	}
    }

  param3 = strtok((char *)NULL,"");
  if(param3)
    printf("param3 = [%s]\n", param3);

  /*
    This parser should be re-written using a hash table lookup
    If someone doesn't beat me to it, I'll get around to doing it
    - Dianora
    well, I haven't finished yet. 
    - Dianora
    */

  if (!CASECMP(param1,"clones"))
    token = K_CLONES;
  else if (!CASECMP(param1,"nflood"))
    token = K_NFLOOD;
  else if(!CASECMP(param1,"rehash"))
    token = K_REHASH;
  else if(!CASECMP(param1,"trace"))
    token = K_TRACE;
  else if (!CASECMP(param1,"failures"))
    token = K_FAILURES;
  else if (!CASECMP(param1,"domains"))
    token = K_DOMAINS;
  else if (!CASECMP(param1,"checkversion"))
    token = K_CHECKVERSION;
  else if (!CASECMP(param1,"multi"))
    token = K_BOTS;
  else if (!CASECMP(param1,"bots"))
    token = K_BOTS;
  else if (!CASECMP(param1,"nfind"))
    token = K_NFIND;
  else if (!CASECMP(param1,"list"))
    token = K_LIST;
  else if (!CASECMP(param1,"killlist"))
    token = K_KILLLIST;
  else if (!CASECMP(param1,"kl"))
    token = K_KILLLIST;
  else if (!CASECMP(param1,"gline"))
    token = K_GLINE;
  else if (!CASECMP(param1,"kline"))
    token = K_KLINE;
  else if (!CASECMP(param1,"kclone"))
    token = K_KCLONE;
  else if (!CASECMP(param1,"kflood"))
    token = K_KFLOOD;
  else if (!CASECMP(param1,"kperm"))
    token = K_KPERM;
  else if (!CASECMP(param1,"kbot"))
    token = K_KBOT;
  else if (!CASECMP(param1,"kill"))
    token = K_KILL;
  else if (!CASECMP(param1,"register"))
    token = K_REGISTER;
  else if (!CASECMP(param1,"opers"))
    token = K_OPERS;
  else if (!CASECMP(param1,"tcmlist"))
    token = K_TCMLIST;
  else if (!CASECMP(param1,"tcmconn"))
    token = K_TCMCONN;
  else if (!CASECMP(param1,"allow"))
    token = K_ALLOW;
  else if (!CASECMP(param1,"autopilot"))
    token = K_AUTOPILOT;
  else if (!CASECMP(param1,"connections"))
    token = K_CONNECTIONS;
  else if (!CASECMP(param1,"disconnect"))
    token = K_DISCONNECT;
  else if (!CASECMP(param1,"help"))
    token = K_HELP;
  else if (!CASECMP(param1,"close"))
    token = K_CLOSE;
  else if (!CASECMP(param1,"op"))
    token = K_OP;
  else if (!CASECMP(param1,"cycle"))
    token = K_CYCLE;
  else if (!CASECMP(param1,"die"))
    token = K_DIE;
  else if (!CASECMP(param1,"ignore"))
    token = K_IGNORE;
  else if (!CASECMP(param1,"tcmintro"))
    token = K_TCMINTRO;
  else if (!CASECMP(param1,"tcmident"))
    token = K_TCMIDENT;
  else token = 0;

  switch(token)
    {
    case K_CLONES:
      report_clones(connections[connnum].socket);
      break;

    case K_NFLOOD:
      report_nick_flooders(connections[connnum].socket);
      break;

    case K_REHASH:
      (void)sprintf(dccbuff,"rehash requested by %s",
		    who_did_command);
      sendtoalldcc(dccbuff,SEND_ALL_USERS);
      clear_userlist();
      load_botlist();
      initopers();
      break;

    case K_TRACE:
      (void)sprintf(dccbuff,"trace requested by %s",
		    who_did_command);
      sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
      freehash();
      inithash();
      break;

    case K_FAILURES:
      if (!param2)
	report_failures(connections[connnum].socket,10);
      else if (atoi(param2) < 1)
	prnt(connections[connnum].socket,"Usage: .failures [min failures]\n");
      else
	report_failures(connections[connnum].socket,atoi(param2));
      break;

    case K_DOMAINS:
      if (!param2)
        report_domains(connections[connnum].socket,5);
      else if (atoi(param2) < 1)
        prnt(connections[connnum].socket,"Usage: .domains [min users]\n");
      else
        report_domains(connections[connnum].socket,atoi(param2));
      break;

    case K_CHECKVERSION:
      if (connections[connnum].type & TYPE_OPER)
	massversion(connections[connnum].socket);
      else
	prnt(connections[connnum].socket,
	     "Only authorized opers may use this command\n");
      break;

    case K_BOTS:
      report_multi(connections[connnum].socket,2);
      break;
      
    case K_NFIND:
      if (connections[connnum].type & TYPE_OPER)
	{
	  if (!param2)
	    prnt(connections[connnum].socket,
	       "Usage: .nfind <wildcarded nick>\n");
	  else
	    list_nicks(connections[connnum].socket,param2);
	}
      else
	prnt(connections[connnum].socket,
	     "Only authorized opers may use this command\n");
      break;

    case K_LIST:
      if (connections[connnum].type & TYPE_OPER)
	{
	  if (!param2)
	    prnt(connections[connnum].socket,
		 "Usage: .list <wildcarded userhost>\n");
	  else
	    list_users(connections[connnum].socket,param2);
	}
      else
	prnt(connections[connnum].socket,
	     "Only authorized opers may use this command\n");
      break;


    case K_KILLLIST:	/* - Phisher */
      if (connections[connnum].type & TYPE_REGISTERED)
	{
	  if (!param2)
	    {
	      prnt(connections[connnum].socket,
		   "Usage: .killlist <wildcarded userhost> or\n");
	      prnt(connections[connnum].socket,
		   "Usage: .kl <wildcarded userhost>\n");
	    }
	  else
	    {
	      (void)sprintf(dccbuff,"killlist %s by %s",
			    param2,who_did_command);
	      sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
	      kill_list_users(connections[connnum].socket,param2);
	    }
	}
      else
	prnt(connections[connnum].socket,"You aren't registered\n");
      break;

/* - Phisher */
#ifdef REMOTE_KLINE

    case K_GLINE:
      {
	char cmd[MAX_BUFF];
	time_t current_time;
	struct tm *broken_up_time;
	char *pattern;	/* u@h or nick */
	char *reason;	/* reason to use */

	current_time = time((time_t *)NULL);
	broken_up_time = localtime(&current_time);

	if( connections[connnum].type & TYPE_GLINE )
	  {
	    if(param2)
	      {
		pattern = param2;
		if(pattern)
		  {
		    reason = param3;
		    if( reason )
		      {
			/* Removed *@ prefix from kline parameter -tlj */
			sprintf(dccbuff,"gline %s : %s added by oper %s",
				pattern,reason,
				who_did_command,
				dfltnick);
			sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
			
			cmd[0] = '.';

			log_kline(broken_up_time,
				  "GLINE",
				  pattern,
				  who_did_command,
				  reason);
			
			sprintf(cmd+1,"KLINE %s :%s by %s %02d/%02d/%02d\n",
				pattern,reason,
				who_did_command,
				(broken_up_time->tm_mon)+1,
				broken_up_time->tm_mday,
				broken_up_time->tm_year);
			toserv(cmd+1);
			sendto_all_linkedbots(cmd);
		      }
		    else
		      {
			prnt(connections[connnum].socket,
			     "missing reason \"kline [nick]|[user@host] reason\"\n");
		      }
		  }
		else
		  {
		    prnt(connections[connnum].socket,
			 "missing nick/user@host \".kline [nick]|[user@host] reason\"\n");
		  }
	      }
	  }
	else
	  prnt(connections[connnum].socket,"You don't have gline privilege\n");
      }
    break;

    case K_KLINE:
      if( connections[connnum].type & TYPE_REGISTERED )
	{
	  if(param2 == (char *)NULL)
	    {
	      prnt(connections[connnum].socket,
	   "missing nick/user@host \".kline [nick]|[user@host] reason\"\n");
	      return;
	    }
	  
	  if(param3 == (char *)NULL)
	    {
	      prnt(connections[connnum].socket,
		   "missing reason \"kline [nick]|[user@host] reason\"\n");
	      return;
	    }
	  do_a_kline("kline",param2,param3,who_did_command);
	}
      else
	prnt(connections[connnum].socket,"You aren't registered\n");
    break;

/* Toast */
    case K_KCLONE:
      if( connections[connnum].type & TYPE_REGISTERED )
	{
	  if(param2 == (char *)NULL)
	    {
	      prnt(connections[connnum].socket,
		   "missing nick/user@host \".kclone [nick]|[user@host]\"\n");
	      return;
	    }
	  do_a_kline("kclone",param2,"Clones are prohibited",who_did_command);
	}
      else
	prnt(connections[connnum].socket,"You aren't registered\n");
    break;

/* Toast */
    case K_KFLOOD:
      if( connections[connnum].type & TYPE_REGISTERED )
	{
	  if(param2 == (char *)NULL)
	    {
	      prnt(connections[connnum].socket,
		   "missing nick/user@host \".kflood [nick]|[user@host]\"\n");
	      return;
	    }
	  do_a_kline("kflood",param2,"Flooding",who_did_command);
	}
      else
	prnt(connections[connnum].socket,"You aren't registered\n");
    break;

    case K_KPERM:
      if( connections[connnum].type & TYPE_REGISTERED )
	{
	  if(param2==(char *)NULL)
	    {
	      prnt(connections[connnum].socket,
		   "missing nick/user@host \".kperm [nick]|[user@host]\"\n");
	    }
	  do_a_kline("kperm",param2,"PERMANENT",who_did_command);
	}
      else
	prnt(connections[connnum].socket,"You aren't registered\n");
    break;

    case K_KBOT:
      if( connections[connnum].type & TYPE_REGISTERED )
	{
	  if(param2==(char *)NULL)
	    {
	      prnt(connections[connnum].socket,
		   "missing nick/user@host \".kbot [nick]|[user@host]\"\n");
	      return;
	    }
	  do_a_kline("kbot",param2,"Bots are prohibited",who_did_command);
	}
      else
	prnt(connections[connnum].socket,"You aren't registered\n");
    break;

    case K_KILL:
      {
        char cmd[MAX_BUFF];
	time_t current_time;
	struct tm *broken_up_time;
        char *pattern;  /* u@h or nick */
	char *reason;
	reason = "NO REASON";

	current_time = time((time_t *)NULL);
	broken_up_time = localtime(&current_time);

	if( connections[connnum].type & TYPE_REGISTERED )
	  {
	    if(param2)
	      {
		pattern = param2;
		reason = param3;
		    
		if(pattern && reason)
		  {
		    log_kline(broken_up_time,
			      "KILL",
			      pattern,
			      who_did_command,
			      reason);

		    sprintf(dccbuff,"kill %s : by oper %s %s",
			    pattern,
			    who_did_command,
			    reason);

                    sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
                    sprintf(cmd,"KILL %s : requested by %s reason- %s\n",
			    pattern,
			    who_did_command,
			    reason);
                    toserv(cmd);
                  }
                else
                  {
                    prnt(connections[connnum].socket,
			 "missing nick/user@host reason \".kill [nick]|[user@host] reason\"\n");
                  }
              }
          }
	else
	  prnt(connections[connnum].socket,"You aren't registered\n");
      }
    break;
#endif	/* -- #ifdef REMOTE_KLINE */

    case K_REGISTER:
      {
        char cmd[MAX_BUFF];
	char *password;

	if( connections[connnum].type & TYPE_OPER )
          {
	    int type;

            if(param2)
              {
                password = param2;

                if(password)
                  {
		    if(type = 
		       islegal_pass(connections[connnum].userhost, password))
			{
			  connections[connnum].type |= type;
			  prnt(connections[connnum].socket,
			       "You are now registered\n");
			  (void)sprintf(dccbuff,"%s has registered",
					who_did_command);
			  sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
			}
		    else
		      {
			(void)sprintf(dccbuff,"illegal password from %s",
				      who_did_command);
			prnt(connections[connnum].socket,"illegal password\n");
			sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
		      }
                  }
                else
                  {
		    prnt(connections[connnum].socket,"missing password\n");
                  }
              }
          }
	else
	  prnt(connections[connnum].socket,
	       "Only authorized opers may use this command\n");
      }
    break;

    case K_OPERS:
      {
	int i;
	char *usernick;

	for(i=0;i<MAXUSERS;i++)
	  {
	    if(userlist[i].userathost == (char *)NULL)
	      break;

	    if(userlist[i].usernick == (char *)NULL)
	      usernick = "-DCC-";
	    else
	      usernick = userlist[i].usernick;

	    (void)sprintf(dccbuff,"%s [%s] %s\n",
			  userlist[i].userathost,
			  usernick,
			  type_show(userlist[i].type));
			  
	    prnt(connections[connnum].socket,dccbuff);
	  }
      }
    break;

    case K_TCMLIST:
      {
	int i;

	for(i=0;i<MAXBOTS;i++)
	  {
	    if(botlist[i].userathost == (char *)NULL)
	      break;
	    (void)sprintf(dccbuff,"%s@%s\n",
			  botlist[i].theirnick,
			  botlist[i].userathost);
	    prnt(connections[connnum].socket,dccbuff);
	  }
      }
    break;

    case K_TCMCONN:
      {
	if(connections[connnum].type & TYPE_OPER)
	  {
	    int i;
	    int match;
	    int new_connnum;

	    match = NO;

	    if(param2)
	      match = YES;

	    for(i=0;i<MAXBOTS;i++)
	      {
		int socket;
		
		if(botlist[i].userathost == (char *)NULL)
		  break;

		if(match && (CASECMP(botlist[i].theirnick, param2) != 0))
		  continue;

		if(botlist[i].port == 0)
		  (void)sprintf(dccbuff,"%s:%d",
				botlist[i].userathost,
				TCM_PORT);
		else
		  (void)sprintf(dccbuff,"%s:%d",
				botlist[i].userathost,
				botlist[i].port);
		
		if((socket = bindsocket(dccbuff)) > 0)
		  {
		    new_connnum = add_connection(socket,i);
		    if(new_connnum == INVALID)
		      (void)close(socket);
		    else
		      {
			int j;

			/* Extra paranoia doesn't hurt at all */
			if(botlist[i].theirnick && botlist[i].password)
			  {
			    (void)sprintf(dccbuff,"%s %s %s\n",
					  botlist[i].theirnick,
					  dfltnick,
					  botlist[i].password);
			    prnt(socket,dccbuff);
			    (void)sprintf(dccbuff,".TCMINTRO %s %s ",
					  dfltnick,
					  botlist[i].theirnick);

			    (void)printf("DEBUG: dccbuff = [%s]\n",
					 dccbuff);

			    for (j=1;j<maxconns;++j)
			      if (connections[i].socket != INVALID)
				{
				  if(connections[j].type & TYPE_BOT)
				    {
				      (void)strcat(dccbuff," ");
				      (void)strcat(dccbuff,
						   connections[i].nick);
				      (void)printf("DEBUG: dccbuff = [%s]\n",
						   dccbuff);
				    }
				}
			    strcat(dccbuff,"\n");
			    printf("DEBUG: tcminfo [%s]\n",dccbuff);
			    sendto_all_linkedbots(dccbuff);
			  }
		      }
		  }
	      }
	  }
	else
	  prnt(connections[connnum].socket,
	       "Only authorized opers may use this command\n");
      }
    break;


    case K_TCMINTRO:
      {
	/* param2 param3 are possibly already set up for tcm nicks... */
	/* I pick up any more after param3 using tcmnick */

	char *tcmnick;
	char *newtcm;

	if(param2)
	  {
	    printf("DEBUG: tcmnick [%s] ", param2);
	  }
	else
	  return;

	newtcm = strtok(param3," ");;
	if(newtcm)
	  {
	    printf("linking [%s]\n", newtcm);
	  }
	else
	  return;

	tcmnick = strtok((char *)NULL," ");
	while(tcmnick)
	  {
	    printf("DEBUG: introducing [%s]\n", tcmnick);
	    if( already_have_tcm(tcmnick) )
	      {
		printf("DEBUG: already have [%s]\n",  tcmnick);
		(void)sprintf(dccbuff,
     "!%s! Routing loop tcm [%s] linking in [%s] finding already present [%s]",
			   dfltnick,param2,newtcm,tcmnick);
		sendtoalldcc(dccbuff,SEND_ALL_USERS);
		(void)sprintf(dccbuff,":%s@%s -@%s .disconnect %s\n",
			    param2,
			    param2,
			    dfltnick,
			    newtcm);

		sendto_all_linkedbots(dccbuff);
	      }
	    tcmnick = strtok((char *)NULL," ");
	  }
      }
      break;

    case K_ALLOW:
#ifdef PARANOID_PRIVS
      if (connections[connnum].type & TYPE_REGISTERED)
#else
      if (connections[connnum].type & TYPE_OPER)
#endif
	{
	  int i;
	  int found_one;

	  if(param2)
	    {
	      if(*param2 == '-')
		(void)sprintf(dccbuff,"allow of %s turned off by %s",
			      param2+1,
			      who_did_command);
	      else
		(void)sprintf(dccbuff,"allow of %s turned on by %s",
			      param2,
			      who_did_command);
		
	      sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
	      setup_allow(param2);
	    }
	  else
	    {
	      for(i = 0; i < MAX_ALLOW_SIZE; i++ )
		{
		  if(allow_nick[i][0] != '-')
		    {
		      found_one = YES;
		      (void)sprintf(dccbuff,
				    "allowed: %s\n",allow_nick[i]);
		      prnt(connections[connnum].socket,dccbuff);
		    }
		}
	      
	      if(!found_one)
		{
		  (void)sprintf(dccbuff,
				"There are no tcm allows in place\n");
		  prnt(connections[connnum].socket,dccbuff);
		}
	    }
	}
      else
	{
#ifdef PARANOID_PRIVS
	  prnt(connections[connnum].socket,"You aren't registered\n");
#else
	  prnt(connections[connnum].socket,
	       "Only authorized opers may use this command.\n");
#endif
	}
      break;

#ifdef AUTOPILOT

      case K_AUTOPILOT:
	if (!param2)
	  {
	    if( autopilot )
	      prnt(connections[connnum].socket,"autopilot is ON\n");
	    else
	      prnt(connections[connnum].socket,"autopilot is OFF\n");
	  }
	else
	  {
	    if(connections[connnum].type & TYPE_OPER)
	      {
		if( !NCASECMP(param2,"ON",2))
		  {
		    autopilot = YES;
		    (void)sprintf(dccbuff,"autopilot turned ON by %s",
				  who_did_command);
		    sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
		  }
		else if( !NCASECMP(param2,"OFF",3))
		  {
		    autopilot = NO;
		    (void)sprintf(dccbuff,"autopilot turned OFF by %s",
				  who_did_command);
		    sendtoalldcc(dccbuff,SEND_OPERS_ONLY);
		  }
	      }
	    else
	      prnt(connections[connnum].socket,
		   "Only authorized opers may use this command\n");
	  }
	break;
#endif
	
      case K_CONNECTIONS:
	for (i=1;i<maxconns;++i)
	  if (connections[i].socket != INVALID)
	    {
	      sprintf(dccbuff,"%s %s (%s) is connected\n",
		      connections[i].nick,
		      type_show(connections[i].type),
		      connections[i].userhost
		      );
	      prnt(connections[connnum].socket,dccbuff);
	    }
	break;

      case K_DISCONNECT:
#ifdef PARANOID_PRIVS
	if (connections[connnum].type & TYPE_REGISTERED)
#else
	if (connections[connnum].type & TYPE_OPER)
#endif
	  {
	    if (!param2)
	      prnt(connections[connnum].socket,"Usage: disconnect <nickname>\n");
	    else
	      {
		char *type;

		for (i=1;i<maxconns;++i)
		  if (connections[i].socket != INVALID &&
		      !CASECMP(param2,connections[i].nick))
		    {
		      type = "user";
		      if(connections[i].type & TYPE_OPER)
			type = "oper";
		      if(connections[i].type & TYPE_BOT)
			type = "Bot";

		      sprintf(dccbuff,"Disconnecting %s %s\n",
			      type,
			      connections[i].nick);
		      prnt(connections[connnum].socket,dccbuff);
		      sprintf(dccbuff,"You have been disconnected by oper %s\n",
			      who_did_command);
		      prnt(connections[i].socket,dccbuff);
		      closeconn(i);
		    }
	      }
	  }
	else
#ifdef PARANOID_PRIVS
	  prnt(connections[connnum].socket,"You aren't registered\n");
#else
	  prnt(connections[connnum].socket,
	   "Only authorized opers may use this command.\n");
#endif
	  break;

      case K_HELP:
	print_help(connections[connnum].socket, param2);
	break;

      case K_CLOSE:
	prnt(connections[connnum].socket,"Closing connection\n");
	closeconn(connnum);
	break;

/* Added by ParaGod */
/* additional modifications by Dianora */

      case K_OP:
	{
#ifdef PARANOID_PRIVS
	  if (connections[connnum].type & TYPE_REGISTERED)
#else
	  if (connections[connnum].type & TYPE_OPER)
#endif
	    {
	      if (!param2)
		prnt(connections[connnum].socket,"Usage: op [nick]\n");
	      else
		op(defchannel,param3); 
	    }
	  else
#ifdef PARANOID_PRIVS
	    prnt(connections[connnum].socket,"You aren't registered\n");
#else
	    prnt(connections[connnum].socket,
	     "Only authorized opers may use this command.\n");
#endif
	}
      break;

    case K_CYCLE:
      {
#ifdef PARANOID_PRIVS
	if (connections[connnum].type & TYPE_REGISTERED)
#else
	if (connections[connnum].type & TYPE_OPER)
#endif
	  {
	    leave(defchannel);
	    sendtoalldcc("I'm cycling.  Be right back.", SEND_OPERS_ONLY);
	    sleep(1);
	    join(defchannel);
	  }
	else
#ifdef PARANOID_PRIVS
	  prnt(connections[connnum].socket,"You aren't registered\n");
#else
	  prnt(connections[connnum].socket,
	     "Only authorized opers may use this command.\n");
#endif
      }
    break;

    case K_DIE:
      {
	if(!(connections[connnum].type & TYPE_BOT))
	  {
	   if(connections[connnum].type & TYPE_OPER) 
	     {
	       sendtoalldcc("I've been ordered to quit irc, goodbye.", SEND_ALL_USERS);
	       prnt(connections[0].socket,"QUIT :Dead by request!\n");
	       exit(1);
	     }
	   else
	     prnt(connections[connnum].socket,
		  "Only authorized opers may use this command\n");
	  }
	else
	  prnt(connections[connnum].socket,
	       "Disabled for remote tcm's\n");
      }
    /* End of stuff added by ParaGod */
    break;

    case K_IGNORE:
      if(param2)
	{
	  if(*param2 == '-')
	    (void)sprintf(dccbuff,"ignore of chatline turned off by %s",
			  who_did_command);
	  connections[connnum].type &= ~TYPE_IGNORE;
	}
      else
	{
	  (void)sprintf(dccbuff,"ignore of chatline turned on by %s",
			who_did_command);
	  connections[connnum].type |= TYPE_IGNORE;
	}

      sendtoalldcc(dccbuff,SEND_ALL_USERS);
      break;

    case K_TCMIDENT:

      break;

    default:
      prnt(connections[connnum].socket,"Unknown command\n");
      break;
    }
}

/*
do_a_kline()

inputs		- command used i.e. ".kline", ".kclone" etc.
		- pattern (i.e. nick or user@host)
		- reason
		- who asked for this (oper)
output		- NONE
side effects	- someone gets k-lined


*/

void do_a_kline(char *command_name,char *pattern,
		char *reason,char *who_did_command)
{
    char cmd[MAX_BUFF];
    char dccbuff[MAX_BUFF];
    time_t current_time;
    struct tm *broken_up_time;

#ifdef RESTRICT_REMOTE_KLINE
    if(route_entry.to_nick[0] != '\0')
      return;
#endif

    current_time = time((time_t *)NULL);
    broken_up_time = localtime(&current_time);

    if(pattern == (char *)NULL)return;
    if(reason == (char *)NULL)return;

    /* Removed *@ prefix from kline parameter -tlj */
    sprintf(dccbuff,"%s %s : %s added by oper %s",
	    command_name,pattern,reason,
	    who_did_command);
    sendtoalldcc(dccbuff,SEND_OPERS_ONLY);

    /* If the kline doesn't come from the local tcm
       and tcm has been compiled to restrict remote klines
       then just ignore it */


    log_kline(broken_up_time,
	      "KLINE",
	      pattern,
	      who_did_command,
	      reason);

    sprintf(cmd,"KLINE %s :%s by %s %02d/%02d/%02d\n",
	    pattern,reason,
	    who_did_command,
	    (broken_up_time->tm_mon)+1,
	    broken_up_time->tm_mday,
	    broken_up_time->tm_year);
    toserv(cmd);

}


/*
already_have_tcm

inputs		- new nick being introduced
output		- YES if this tcm nick is already linked to me
		  NO if this tcm nick is not already linked to me
side effects	- NONE
*/
int already_have_tcm(char *tcmnick)
{
  int i;

  printf("already_have_tcm() tcmnick = [%s]\n", tcmnick );

    for (i=1;i<maxconns;++i)
      if (connections[i].socket != INVALID)
	{
	  if(connections[i].type & TYPE_BOT)
	    {
	      printf("connections[%d].nick [%s] tcmnick [%s]\n",
		     i,connections[i].nick, tcmnick );
	      if(CASECMP(connections[i].nick, tcmnick) == 0)
		{
		  printf("returning YES\n");
		  return(YES);
		}
	    }
	}

  return(NO);
}


/*
** proc()
**   Parse server messages based on the function and handle them.
**   Parameters:
**     source - nick!user@host or server host that sent the message
**     fctn - function for the server msgs (e.g. PRIVMSG, MODE, etc.)
**     param - The remainder of the server message
**   Returns: void
**   PDL:
**     If the source is in nick!user@host format, split the nickname off
**     from the userhost.  Split the body off from the parameter for the
**     message.  The parameter is generally either our nickname or the
**     nickname directly affected by this message.  You can kind of figure
**     the rest of the giant 'if' statement out.  Occasionally we need to
**     parse additional parameters out of the body.  To find out what all
**     the numeric messages are, check out 'numeric.h' that comes with the
**     server code.  ADDED: watch out for partial PRIVMSGs received from the
**     server... hold them up and make sure to stay synced with the timer
**     signals that may be ongoing.
*/
void proc(char *source,char *fctn,char *param)
{
    char *userhost, *body;
    char *modeparms;
    int i;

    if ((userhost = strchr(source,'!')) != NULL) {
      *(userhost++) = '\0';
      if (*userhost == '~')
        ++userhost;
      }

    if ((body = strchr(param,' ')) != NULL) {
      *(body++) = '\0';
      if (*body == ':')
        ++body;
      }
      
    if (!strcmp(fctn,"PRIVMSG")) {
        privmsg(source, userhost, body,
                ((*param == '#'  || *param == '&') ? param : source));
        }
    else if (!strcmp(fctn,"PING"))
        pong(connections[0].socket,param);
    else if (!strcmp(fctn,"ERROR")) {
	if (!wldcmp("*closing*",param)) {
	  if (!wldcmp("*nick coll*",body))
	    onnicktaken();
	  linkclosed();
	  }
        }
    else if (!strcmp(fctn,"KILL"))
        if (strchr(source,'.')) {
	    onnicktaken();
            linkclosed();
	    }
        else
	    quit = YES;
    else if (!strcmp(fctn,"JOIN"))
        onjoin(source, param);
    else if (!strcmp(fctn,"KICK")) {
        if (modeparms = strchr(body,' '))    /* 2.8 fix */
          *modeparms = '\0';
        onkick(body,param);
	}
    else if (!strcmp(fctn,"NICK"))
        onnick(source,param);
    else if (!strcmp(fctn,"433"))
	onnicktaken();
    else if (!strcmp(fctn,"451"))	/* -Dianora "You have not registered"*/
      linkclosed();
    else if (!strcmp(fctn,"474") || !strcmp(fctn,"471") ||
             !strcmp(fctn,"475") || !strcmp(fctn,"473")) {
	*(strchr(body,' ')) = '\0';
        cannotjoin(body);
    }
    else if (!strcmp(fctn,"001")) {	/* Thanks.. ThemBones */
      if (amianoper == NO)
	do_init();
    }
    else if (!strcmp(fctn,"204"))	/* RPL_TRACEOPERATOR */
      ontraceuser(body);
    else if (!strcmp(fctn,"205"))	/* RPL_TRACEUSER */
        ontraceuser(body);
    else if (!strcmp(fctn,"209"))	/* RPL_TRACECLASS */
        ontraceclass();
/* - Dianora */
    else if (!strcmp(fctn,"243"))
      on_stats_o(body);
    else if (!strcmp(fctn,"219"))
      /* do nothing */ ;
/* - Dianora */
    else if (!strcmp(fctn,"NOTICE"))
      {
	char dccbuff[DCCBUFF_SIZE];

	if( CASECMP(source,server_name) == 0 )
	  {
	    if(strncmp(body,"*** Notice -- ",14) == 0)
	      onservnotice(body+14);
	  }
	else if(CASECMP(source,SERVICES_NAME) == 0)
	  {
	    on_services_notice(body);
	  }
         else if(CASECMP(source,CA_SERVICES_NAME) == 0) {
           on_ca_services_notice(body);
         }
      } else if (!strcmp(fctn,"303")) { 
        /* 
         * ISON reply for services.ca/ca-svcs 
         */
        if (!strcmp(body,"CA-SVCS ")) {
          if (!GettingCASVCSReports) {
            /*
             * Okay, CA-SVCS is online.  It's just signed on, so we
             * need to subscribe to the clone list for our server.
             */
            toserv("PRIVMSG CA-SVCS :ADDNOTIFY HERE\r\n");
            GettingCASVCSReports = YES;
          }
        } else {
          /*
           * CA-SVCS isn't online.  Make sure we know about that, so
           * we can detect when it signs on.
           */
          GettingCASVCSReports = NO;
        }
      }
}

/*
** gracefuldie()
**   Called when we encounter a segmentation fault.
**   Parameters: None
**   Returns: void
**   PDL:
**     While debugging, I got so many seg faults that it pissed me off enough
**     to write this.  When dying from a seg fault, open files are not closed.
**     This means I lose the last 8K or so that was appended to the debug
**     logfile, including the thing that caused the seg fault.  This will
**     close the file before dying... Not too much more graceful, I agree.
*/
void gracefuldie(int sig)
{
#ifdef DEBUGMODE
  fclose(outfile);
#endif

  if(logging_fp != (FILE *)NULL)
    (void)fclose(logging_fp);

  prnt(connections[0].socket,"QUIT :Woo hoo!  Bot crash!\n");
  
/* At least, give me a core file to work with 
   - Dianora 
*/
  if(sig != SIGTERM )
    abort();
  exit(1);
}

/*
reload_user_list(void)

  Thanks for the idea garfr

inputs - NONE
output - NONE
side effects -
  	       reloads user list without having to restart mobot
	       Now just re does the trace/ and stats O

	       - Dianora
*/

void reload_user_list(void)
{
  clear_userlist();
  load_botlist();
  initopers();

  load_hostlist();
  freehash();
  inithash();
  sendtoalldcc("*** Caught SIGHUP ***",SEND_ALL_USERS);
}

/*
** main()
**   Duh, hey chief... What does a main do?
**   Parameters:
**     argc - Count of command line arguments
**     argv - List of command line arguments
**   Returns: When the program dies.
**   PDL:
**     Look for up to two command line arguments: first, a server name to
**     overrride the default SERV in config.h.  This is recognized by having
**     a '.' in the string, which a channel name cannot have.  Second, look
**     for a default channel to override INCH from server.h.  The channel name
**     must NOT contain a '#' (since # is tough to pass from the shell).  Set
**     up assorted things: random numbers, handlers for seg faults and timers,
**     initialize the internal stores for songs, insults, users, and helper
**     bots.  Attach tcm to the server, sign her on to IRC, join her up
**     to the channel, and loop through processing incoming server messages
**     until tcm is told to quit, is killed, or gives up reconnecting.
*/
main(argc,argv)
int argc;
char *argv[];
{
  int i;
  init_userlist();  services.last_checked_time = time((time_t *)NULL);
  if(argc < 2)
    load_config_file(CONFIG_FILE);
  else
    load_config_file(argv[1]);
  strcpy(serverhost,server_config);	/* Load up desired server */
/*
  I removed the command line options for server/channel
  do it in the tcm.cf file... - Dianora
*/
  for (i=0;i<MAXDCCCONNS+1;++i)
    connections[i].socket = INVALID;
  *mychannel = 0;
  srand(time(NULL));	/* -zaph */
  signal(SIGSEGV,gracefuldie);
  signal(SIGBUS,gracefuldie);
  signal(SIGTERM,gracefuldie);
  signal(SIGHUP,reload_user_list);
/* lets write our pid -Dianora */
  if((outfile = fopen("tcm.pid","w")) == (FILE *)NULL)
    {
      fprintf(stderr,"Cannot write tcm.pid\n");
      exit(1);
    }
  (void)fprintf(outfile,"%d", getpid());
  (void)fclose(outfile);
#ifdef DEBUGMODE
  if( (outfile = fopen(DEBUG_LOGFILE,"w")) == (FILE *)NULL)
    {
      (void)fprintf(stderr,"Cannot create %s\n",DEBUG_LOGFILE);
      exit(1);
    }
#endif
  load_hostlist();
  connections[0].socket = bindsocket(serverhost);
  if (connections[0].socket == INVALID)
    exit(1);
  connections[0].buffer = (char *)malloc(BUFFERSIZE);
  if(connections[0].buffer == (char *)NULL)
    {
      fprintf(stderr,"memory allocation error in main()\n");
      exit(1);
    }
  connections[0].type = 0;
  maxconns = 1;
  FD_ZERO (&nullfds);
  if(virtual_host_config[0] != '\0')
    {
      strncpy(ourhostname,virtual_host_config,MAX_HOST-1);
    }
  else
    {
      gethostname(ourhostname,MAX_HOST-1);
      i = strlen(ourhostname);
#if !defined BSDI && !defined __EMX__
      getdomainname(ourhostname+i+1,(MAX_HOST-2)-i);
#else
      /* getdomainname() doesn't exist on BSDI as best I can tell,
	 so here's some ugliness... and yes, I'm aware I could use
	 DNS lookups to do this, but I'm tired as hell and don't
	 feel like adding it right now */
      strcpy(ourhostname+i+1,DOMAIN);
#endif
      ourhostname[i] = '.';
    }
  *mynick = '\0';
  load_botlist();
  signon();
  amianoper = NO;
  init_allow_nick();
  init_nick_change_table();
  init_link_look_table();
  init_remote_tcm_listen();
  while(!quit)
    rdpt();
  prnt(connections[0].socket,
       "QUIT :Bot terminating normally\n");
#ifdef DEBUGMODE
  fclose(outfile);
#endif
}

/*
initlog()
inputs - NONE
output - NONE
side effects
	- Dianora
*/
void initlog(void)
{
  time_t current_time;
  struct tm *broken_up_time;
  char filename[MAX_BUFF];
  char command[MAX_BUFF];
  char last_filename[MAX_BUFF];
  char *p;
  FILE *last_log_fp;
  FILE *email_fp;
  FILE *log_to_email_fp;
  last_filename[0] = '\0';
  if((last_log_fp = fopen(LAST_LOG_NAME,"r")) != (FILE *)NULL)
    {
      (void)fgets(last_filename,MAX_BUFF-1,last_log_fp);
      p = strchr(last_filename,'\n');
      if(p)
	*p = '\0';
      printf("last_filename = [%s]\n", last_filename );
      (void)fclose(last_log_fp);
    }
  current_time = time((time_t *)NULL);
  broken_up_time = localtime(&current_time);
  (void)sprintf(filename,"%s_%02d_%02d_%02d",LOGFILE,
		(broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
		broken_up_time->tm_year);
  logging_fp = fopen(filename,"a");
  if(email_config[0] == '\0')
    return;
  if( last_filename[0] == '\0')
    {
      strcpy(last_filename,filename);
    }
  if( strcmp(last_filename,filename) != 0 )
    {
      (void)sprintf(command,"%s \"clone report\" %s",HOW_TO_MAIL,
		    email_config);
      if( (email_fp = popen(command,"w")) != (FILE *)NULL)
	{
	  if((log_to_email_fp = fopen(last_filename,"r")) != (FILE *)NULL)
	    {
	      while(fgets(command,MAX_BUFF-1,log_to_email_fp ))
		fputs(command,email_fp);
	    }
	}
      (void)fclose(log_to_email_fp);
      (void)fclose(email_fp);
      (void)unlink(last_filename);
    }
  if((last_log_fp = fopen(LAST_LOG_NAME,"w")) != (FILE *)NULL)
    {
      (void)fputs(filename,last_log_fp);
      (void)fclose(last_log_fp);
    }
}
/*
log_kline
input		- struct tm pointer
		- command_name "KLINE" "GLINE" etc.
		- who_did_command who did the command
		- reason
output		- none
side effects	- log entry made
*/
void log_kline(struct tm *broken_up_time,
	       char *command_name,
	       char *pattern,
	       char *who_did_command,char *reason)
{
FILE *fp_log;
  if((fp_log = fopen(KILL_KLINE_LOG,"a")) != (FILE *)NULL)
    {
      fprintf(fp_log,"%02d/%02d/%d %02d:%02d %s %s by %s for %s\n",
	      (broken_up_time->tm_mon)+1,
	      broken_up_time->tm_mday,
	      broken_up_time->tm_year,
	      broken_up_time->tm_hour,
	      broken_up_time->tm_min,
	      command_name,pattern,who_did_command,reason);
      (void)fclose(fp_log);
    }
}
/* 
init_remote_tcm_listen
inputs		- NONE
output		- NONE
side effects	- just listen on tcm_port port. nothing fancy.
*/
void init_remote_tcm_listen(void)
{
struct sockaddr_in socketname;
int blocking_mode;
  bzero(&socketname,sizeof(struct sockaddr));
  socketname.sin_family = AF_INET;
  socketname.sin_addr.s_addr = INADDR_ANY;
  socketname.sin_port = htons(tcm_port);
  if( (remote_tcm_socket = socket(PF_INET,SOCK_STREAM,6)) < 0)
    {
      fprintf(stderr,"Can't create socket for %d\n",tcm_port);
      exit(1);
    }
  if(bind(remote_tcm_socket,(struct sockaddr *)&socketname,
	  sizeof(socketname)) < 0)
    {
      fprintf(stderr,"Can't bind TCM_PORT %d\n",TCM_PORT);
      exit(1);
    }
  if ( listen(remote_tcm_socket,4) < 0 )
    {
      fprintf(stderr,"Can't listen on TCM_PORT\n");
      exit(1);
    }
}
/*
connect_remote_tcm()
inputs 		- INVALID or a connection number
output		- NONE
side effects	-
*/
void connect_remote_tcm(int connnum)
{
  int i;
  struct sockaddr_in incoming_addr;
  struct hostent *host_seen;
  int addrlen;
  int lnth;
  char dccbuff[DCCBUFF_SIZE];
  if(connnum == INVALID)
    {
      for (i=1; i<MAXDCCCONNS+1; ++i)
	{
	  if (connections[i].socket == INVALID)
	    {
	      if (maxconns < i+1)
		maxconns = i+1;
	      break;
	    }
	}
      if(i > MAXDCCCONNS)
	return;
      addrlen = sizeof(struct sockaddr);
      if((connections[i].socket = accept(remote_tcm_socket,
			 (struct sockaddr *)&incoming_addr,&addrlen)) < 0 )
	{
	  fprintf(stderr,"Error in remote tcm connect on accept\n");
	  return;
	}
      connections[i].buffer = (char *)malloc(BUFFERSIZE);
      if(connections[i].buffer == (char *)NULL)
	{
	  sendtoalldcc("Ran out of memory in ",SEND_ALL_USERS);
	  gracefuldie(0);
	}
      connections[i].buffend = connections[i].buffer;
      connections[i].type = TYPE_PENDING|TYPE_BOT;
      connections[i].last_message_time = time((time_t *)NULL);
      host_seen = gethostbyaddr((char *)&incoming_addr.sin_addr.s_addr,
				4,AF_INET);
      if(host_seen)
	strncpy(connections[i].userhost,(char *)host_seen->h_name,MAX_HOST-1);
      else
	strncpy(connections[i].userhost,
		inet_ntoa(incoming_addr.sin_addr),MAX_HOST-1);
      (void)sprintf(dccbuff,"Bot connection from %s",
	      connections[i].userhost);
      msg(mychannel,dccbuff);
      sendtoalldcc(dccbuff,SEND_ALL_USERS);
    }
  else
    {
      if(connections[connnum].type & TYPE_PENDING)
	{
	  char *myname;
	  char *botname;
	  char *password;
	  int  type;
	  myname = strtok(connections[connnum].buffend," ");
	  if(myname != (char *)NULL)
	    {
	      if( CASECMP(myname,dfltnick) != 0 )
		{
		  (void)sprintf(dccbuff,
				"illegal connection from %s wrong myname",
				connections[connnum].userhost);
		  sendtoalldcc(dccbuff,SEND_ALL_USERS);
		  closeconn(connnum);
		  return;
		}
	      botname = strtok((char *)NULL," ");
	      if(botname == (char *)NULL)
		{
		  (void)sprintf(dccbuff,
				"illegal connection from %s missing botname",
				connections[connnum].userhost);
		  sendtoalldcc(dccbuff,SEND_ALL_USERS);
		  closeconn(connnum);
		  return;
		}
	      password = strtok((char *)NULL,"");
	      if(password != (char *)NULL)
		{
		  if( type = islinkedbot(connnum,botname,password))
		    {
		      (void)sprintf(dccbuff,"%s@%s link bot has connected",
				    connections[connnum].nick,
				    connections[connnum].userhost);
		      sendtoalldcc(dccbuff,SEND_ALL_USERS);
		      connections[connnum].type = type;
		    }
		  else
		    {
		      (void)sprintf(dccbuff,
			    "illegal connection from %s wrong password",
			    connections[connnum].userhost);
		      sendtoalldcc(dccbuff,SEND_ALL_USERS);
		      closeconn(connnum);
		    }
		}
	    }
	  else
	    {
	      (void)sprintf(dccbuff,"illegal connection from %s",
			    connections[connnum].userhost);
	      sendtoalldcc(dccbuff,SEND_ALL_USERS);
	      closeconn(connnum);
	    }
	}
      else
	{
	  dccproc(connnum);
	}
    }
}
/*
sendto_all_linkedbots()
inputs		- command to relay as input
output		- NONE
side effects	-
*/
void sendto_all_linkedbots(buffer)
char *buffer;
{
  int i;
  for( i = 1; i< maxconns; i++)
    {
      if(connections[i].socket != INVALID)
	{
	  if(connections[i].type & TYPE_BOT)
	    {
	      /*
		Don't send something back to a bot it originated from
	       */
	      if ( i != incoming_connnum )
		{
		  prnt(connections[i].socket,buffer);
		}
	    }
	}
    }
}
/*
type_show()
inputs		- int type
output		- pointer to a static char * showing the char types
side effects	-
*/
char *type_show(type)
int type;
{
static char type_string[SMALL_BUFF];
char *p;
  p = type_string;
  if(type&TYPE_OPER)*p++ = 'O';
  if(type&TYPE_REGISTERED)*p++ = 'K';
  if(type&TYPE_GLINE)*p++ = 'G';
  if(type&TYPE_BOT)*p++ = 'B';
  *p = '\0';
  return(type_string);
}
/*
setup_allow()
input		- nick to allow
output		- NONE
side effects	- nick is added to botnick allow list
*/
void setup_allow(nick)
char *nick;
{
  char botnick[MAX_NICK+4];	/* Allow room for '<' and '>' */
  char *p;
  int i=0;
  int remove_allow = NO;
  int first_free = -1;
  while(*nick==' ')
    nick++;
  if(*nick == '-')
    {
      remove_allow = YES;
      nick++;
    }
  botnick[i++] = '<';
  while(*nick)
    {
      if(*nick == ' ')
	break;
      if(i >= MAX_NICK+1)
	break;
      botnick[i++] = *nick++;
    }
  botnick[i++] = '>';
  botnick[i] = '\0';
  first_free = -1;
  for( i = 0; i < MAX_ALLOW_SIZE ; i++ )
    {
      if( allow_nick[i][0] == '\0' )
	allow_nick[i][0] = '-';
      if( (allow_nick[i][0] == '-') && (first_free < 0))
	{
	  first_free = i;
	}
      if(CASECMP(allow_nick[i],botnick) == 0)
	{
	  if(remove_allow)	/* make it so it no longer matches */
	    allow_nick[i][0] = '-';
	  return;
	}
    }
  /* Not found insert if room */
  if(first_free >= 0)
    {
      strcpy(allow_nick[first_free],botnick);
    }
  /* whoops. if first_free < 0 then.. I'll just ignore with nothing said */
}
/*
init_allow_nick()
inputs		- NONE
output		- NONE
side effects	- The allow nick table is cleared out.
*/
void init_allow_nick()
{
  int i;
  for(i=0;i<MAX_ALLOW_SIZE;i++)
    {
      allow_nick[i][0] = '-';
      allow_nick[i][1] = '\0';
    }
}
/*
test_ignore()
inputs		- input from link
output		- YES if not to ignore NO if to ignore
side effects	- NONE
*/
int test_ignore(line)
char *line;
{
  char botnick[MAX_NICK+4];	/* Allow room for '<' and '>' */
  char *p;
  int i;
  if(line[0] != '<')
    return(NO);
  for(i=0;i<MAX_NICK+3;)
    {
      if(*line == '@')return(NO);	/* Not even just a botnick */
      botnick[i++] = *line++;
      if(*line == '>')
	break;
    }
  botnick[i++] = '>';
  botnick[i] = '\0';

  for(i=0;i<MAX_ALLOW_SIZE;i++)
    {
      if(CASECMP(allow_nick[i],botnick) == 0)
	return(NO);
    }
  return(YES);
}
