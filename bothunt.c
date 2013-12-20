/*
** This code below is UGLY as sin and is not commented.  I would NOT use
** it for the basis of anything real, as it is the worst example of data
** structure misuse and abuse that I have ever SEEN much less written.
** If you're looking for how to implement hash tables, don't look here.
** If I had $100 for every time I looped thru every bucket of the hash
** tables to process a user command, I could retire.  Any way, it may be
** inefficient as hell when handling user commands, but it's fast and
** much cleaner when handling the server notice traffic.  Since the server
** notice traffic should outweigh commands to the bot by - oh like - 100
** to 1 or more, I didn't care too much about inefficiencies and ugliness
** in the stuff that processes user commands... I just wanted to throw it
** together quickly.
*/
/*

Original comments above by Hendrix...

- Dianora
*/

/*

  Some changes by -Dianora
*/

#include <stdio.h>
#include <string.h>

#ifdef __EMX__
#define NCASECMP	strnicmp
#define CASECMP	stricmp
#else
#define NCASECMP	strncasecmp
#define CASECMP	strcasecmp
#endif

#ifdef LINUX
#include <sys/time.h>
#else
#include <time.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include "config.h"
#include "tcm.h"

static char *version="$Id: bothunt.c,v 1.29 1997/06/01 18:04:31 db Exp db $";

#ifdef NEXT
char *strdup(char *);
#endif

#define CONNECT 0
#define EXITING 1
/* CSr notice */
#define CS_CLONES 2
#define BOT 3
#define TOOMANY 4
#define NICKCHANGE 5
/* CSr notice */
#define CS_NICKFLOODING 6
#define UNAUTHORIZED 7
/* CSr notice */
#define CS_CLONEBOT_KILLED 8
/* CSr notice */
#define CS_IDLER 9
#define LINK_LOOK 10
#define KLINE_ADD_REPORT 11	/* Toast */
#define STATS 12

#define YES 1
#define NO  0

#define HASHTABLESIZE 3001

#define CLONECONNECTCOUNT 3
#define CLONECONNECTFREQ  30

#define CLONEDETECTINC 15
#define MAXFROMHOST    50


extern char mynick[];
extern char defchannel[];
extern char oper_nick_config[];
extern char oper_pass_config[];
extern char dfltnick[];
extern struct connection connections[];

#ifdef AUTOPILOT
int autopilot=AUTOPILOT_DEFAULT;
#endif

char mychannel[MAX_CHANNEL];
extern FILE *logging_fp;
extern AUTH_FILE_ENTRY userlist[];
extern int user_list_index;

void list_nicks();	/* - Dianora as derived from list_users */
void kill_list_users();	/* - phisher as derived from list_users */
void list_users();	/* - hendrix */
void privmsg();
void onjoin();
void onkick();
void onnick();
void onnicktaken();
void cannotjoin();
void ontraceuser();
void onservnotice();	/* - Hendrix */
void onctcp();
void adduserhost();
void kill_spoof();	/* - Dianora */
void removeuserhost();
void updateuserhost();
void updatehash();	/* update a users nick */
void checkhostclones();
char *chopuh();			/* - Hendrix */
void report_clones();
void bot_reject();
void logfailure();
void check_nick_flood();	/* - Dianora */
void cs_nick_flood();		/* - Dianora */
void cs_clones();		/* - Dianora */
void link_look_notice();	/* - Dianora */
void init_nick_change_table();	/* - Dianora */
void add_to_nick_change_table(); /* - Dianora */
void report_nick_flooders();	/* - Dianora */
void suggest_kline();		/* - Dianora */
void kline_add_report(char *);		/* - Toast/Dianora */
void kill_add_report(char *);		/* - Dianora/ThemBones */
void stats_notice();		/* - Dianora */
void initopers();		/* - Dianora */
void add_oper();		/* - Dianora */

char *msgs_to_mon[] = {
  "Client connecting: ", 
  "Client exiting: ",
  "Unauthorized connection from ",
/* CSr notice */
  "Rejecting clonebot:",
  "Too many connections from ",
  "Nick change:",
/* CSr notice */
  "Nick flooding detected by:",
  "Rejecting ",
/* CSr notice */
  "Clonebot killed:",
/* CSr notice */
  "Idle time limit exceeded for ",
/* +th has "LINKS 'something usually blank' requested by " */
/* *SIGH* - Dianora */
  "LINKS ",
/* non +th " requested by ", */
  "KLINE ",	/* Just a place holder */
  "STATS ",	/* look at stats ... */
  NULL};

/*
Modifications by
	- Dianora (db) db@db.net

*/

typedef struct userentry {
  char nick[MAX_NICK];
  char user[11];
  char host[MAX_HOST];
  char domain[MAX_DOMAIN];
  char link_count;
  char isoper;
  time_t connecttime;
  time_t reporttime;
  }USERENTRY;

typedef struct hashrec
  {
    USERENTRY *info;
    struct hashrec *collision;
  }HASHREC;

HASHREC *hosttable[HASHTABLESIZE];
HASHREC *domaintable[HASHTABLESIZE];

typedef struct sortarray {
  struct userentry *domainrec;
  int count;
  }SORTARRAY;

char doingtrace = NO;

struct failrec {
  char user[MAX_NICK+1];
  char host[MAX_HOST];
  int botcount;
  int failcount;
  struct failrec *next;
  };

struct failrec *failures = NULL;

/* nick flood finding code - Dianora */

#define NICK_CHANGE_TABLE_SIZE 100

typedef struct nick_change_entry
{
  char *user_host;
  char last_nick[MAX_NICK];
  int  nick_change_count;
  time_t first_nick_change;
  time_t last_nick_change;
  int noticed;
}NICK_CHANGE_ENTRY;

NICK_CHANGE_ENTRY *nick_changes[NICK_CHANGE_TABLE_SIZE];

#define LINK_LOOK_TABLE_SIZE 10

typedef struct link_look_entry
{
  char *user_host;
  int  link_look_count;
  time_t last_link_look;
}LINK_LOOK_ENTRY;

LINK_LOOK_ENTRY link_look[LINK_LOOK_TABLE_SIZE];

/*
oper()

inputs		- NONE
output		- NONE
side effects	- With any luck, we oper this bot *sigh*
*/

void oper()
{
  char hold[MAX_BUFF];
  
  (void)sprintf(hold,"OPER %s %s\nMODE %s :+sckrn\n", 
	  oper_nick_config, oper_pass_config, mynick);
  toserv(hold);
  amianoper = 1;
}

/*
privmsg()

inputs		- nick to send message to
output		- NONE
side effects	-	
*/

void privmsg (nick, userhost, text, output)
     char *userhost, *nick, *text, *output;
{
  if (*text == '\001')
    {
      onctcp (nick, userhost, text);
      return;
    }
  if (*output == '#' || *output == '&')
    {
      if (!NCASECMP(text,"clones",6))
	report_clones(0);
    }
}

/*
onjoin()

inputs		- nick, channel, as char string pointers	
output		- NONE
side effects	-
*/

void onjoin(nick,channel)
char *nick,*channel;
{
  char hold[MAX_BUFF];

  if (*channel == ':') ++channel;      /* 2.8 fix */
  if (!strcmp(mynick,nick))
    {
      strncpy(mychannel,channel,MAX_CHANNEL-1);
      mychannel[MAX_CHANNEL-1] = 0;
      (void)sprintf(hold,"MODE %s +nt\n",mychannel);
      toserv(hold);
    }
}

void onkick(nick,channel)
char *nick,*channel;
{
  if (!strcmp(mynick,nick))
    {
      join(mychannel);
      *mychannel = 0;
    }
}

void onnick(oldnick,newnick)
char *oldnick,*newnick;
{
  if (*newnick == ':') ++newnick;      /* 2.8 fix */
  if (!strcmp(oldnick,mynick))
    strcpy(mynick,newnick);
}

void onnicktaken()
{
  char randnick[MAX_NICK];

  (void)sprintf(randnick,"%s%1d",dfltnick, rand() % 10);
  if (!*mychannel)
    {
      newnick(randnick);
      strcpy(mynick,randnick);
      join(defchannel); 
    }
  else if (strncmp(randnick,dfltnick,strlen(dfltnick)))
    {
      newnick(randnick);
      strcpy(mynick,randnick);
    }
}

void cannotjoin(channel)
char *channel;
{
  char newchan[MAX_CHANNEL];
  int i;

  if (!strcmp(channel,defchannel))
    (void)sprintf(newchan,"%.78s2",defchannel);
  else
    {
      channel += strlen(defchannel);
      i = atoi(channel);
      (void)sprintf(newchan,"%.78s%1d",defchannel,i+1);
    }
  join(newchan);
}

/*
ontraceuser()

inputs		- traceline from server
output		- NONE
side effects	- user is added to hash tables


texas went and modified the output of /trace in their irc server
so that it appears as "nick [user@host]" ontraceuser promptly
threw out the "[user@host]" part.. *sigh* I've changed the code
here to check for a '[' right after a space, and not blow away
the "[user@host]" part. - Dianora

*/

void ontraceuser(traceline)
char *traceline;
{
  char *nuh, *userhost;
  char *p;		/* used to clean up trailing garbage */
  int  isoper;

  isoper = NO;

  if (!doingtrace)
    /* Code for mass version correlation goes here */
    return;
  printf("traceline = [%s]\n", traceline );
  isoper = NO;
  if(*traceline == 'O')
    {
      isoper = YES;
      printf("oper found in trace\n");
    }

  nuh = strchr(traceline,' ');    /* Skip 'User' */
  if (nuh)
    {
      nuh = strchr(nuh+1,' ');      /* Skip class */
      if (nuh)
	{
	  ++nuh;
	  userhost = strchr(nuh,' ');
	  if (userhost)
	    {
	      printf("userhost = [%s]\n", userhost);
	      if(userhost[1] != '[')
		*userhost = '\0';
	      else	/* clean up garbage */
		{
		  p = strchr(userhost+1,' ');
		  if(p)
		    *p = '\0';
		}
	    }
	  userhost = chopuh(nuh);
	  printf("nuh = [%s] userhost = [%s] isoper=%d\n",
		 nuh, userhost,isoper);
	  adduserhost(nuh,userhost,YES,isoper);
	}
    }
}

void ontraceclass()
{
  if (doingtrace)
    {
      doingtrace = NO;
      join(defchannel);
    }
}

/* - Dianora */
/* 
on_stats_o()

inputs		- body of server message
output		- none
side effects	- user list of tcm is built up from stats O of tcm server

  Some servers have some "interesting" O lines... lets
try and filter some of the worst ones out.. I have seen 
*@* used in a servers O line.. (I will not say which, to protect
the guilty)


Thinking about this.. I think perhaps this code should just go away..
Certainly, if you have REMOTE_KLINE etc. defined... You will need
to add users to userlist.load anyway.

  - Dianora
*/

void on_stats_o(body)
char *body;
{
  char *user_at_host;
  int non_lame_user_o;	/* If its not a wildcarded user O line... */
  int non_lame_host_o;	/* If its not a wildcarded host O line... */
  char *p;		/* pointer used to scan for valid O line */

/* No point if I am maxed out going any further */
  if( user_list_index == (MAXUSERS - 1))
    return;

  user_at_host = strtok(body," ");	/* discard the first little bit */
  if(user_at_host == (char *)NULL)
    return;

  /* debugging cruft, i've just decided to leave in - Dianora */
  printf("on_stats_o user_at_host now = [%s]\n", user_at_host );

  user_at_host = strtok((char *)NULL," ");	/* NOW its u@h */
  if(user_at_host == (char *)NULL)
    return;

  printf("on_stats_o user_at_host now = [%s]\n", user_at_host );

  p = user_at_host;
  non_lame_user_o = NO;

  while(*p)
    {
      if(*p == '@')	/* Found the first part of "...@" ? */
	break;

      if(*p != '*')	/* A non wild card found in the username? */
	non_lame_user_o = YES;	/* GOOD a non lame user O line */
      /* can't just break. I am using this loop to find the '@' too */

      p++;
    }
  
  if(!non_lame_user_o)	/* LAME O line ignore it */
    return;

  p++;			/* Skip the '@' */
  non_lame_host_o = NO;

  while(*p)
    {
      if(*p != '*')	/* A non wild card found in the hostname? */
	non_lame_host_o = YES;	/* GOOD a non lame host O line */
      p++;
    }

  if(non_lame_host_o)
    {
      /*
	If this user is already loaded due to userlist.load
	don't load them again. - Dianora
      */
      if( !isoper(user_at_host) )
	{
	  userlist[user_list_index].userathost = strdup(user_at_host);
	  if(userlist[user_list_index].userathost == (char *)NULL)
	    {
	      fprintf(stderr,"memory allocation error in load_userlist()\n");
	      exit(1);
	    }
	  userlist[user_list_index].usernick = (char *)NULL;
	  userlist[user_list_index].password = (char *)NULL;
	  userlist[user_list_index].type = TYPE_OPER;
	  user_list_index++;
	}
    }
}

/* - Dianora */

/*
   Chop a string of form "nick [user@host]" or "nick[user@host]" into
   nick and userhost parts.  Return pointer to userhost part.  Nick
   is still pointed to by the original param.  Note that since [ is a
   valid char for both nicks and usernames, this is non-trivial. */
/* Also, for digi servers, added form of "nick (user@host)" */

/*
Due to the fact texas net irc servers changed the output of the /trace
command slightly, chopuh() was coring... I've made the code a bit
more robust - Dianora

A lot of this code would be simpler using strrchr()... I'll do it
sometime... - Dianora

*/

char *chopuh(nickuserhost)
char *nickuserhost;
{
  char *uh;
  char *p;
  char skip = NO;

  /* If it's the first format, we have no problems */
  uh = strchr(nickuserhost,' ');
  if (!uh)
    {
      uh = strchr(nickuserhost,'[');
      if( uh == (char *)NULL)	/* IT might be of form "nick (user@host)"
				   i.e. +th (digi) format */
	{
	  uh = strchr(nickuserhost,'(');	/* lets see... */
	  if(uh == (char *)NULL)
	    {					/* MESSED up GIVE UP */
	      (void)fprintf(stderr,
			    "You have VERY badly screwed up +c output!\n");
	      (void)fprintf(stderr,
			    "1st case nickuserhost = [%s]\n", nickuserhost);
	      return((char *)NULL);	/*screwy...prolly core in the caller*/
	    }
	  p = strrchr(uh,')');
	  if( p )
	    *p = '\0';
	  else
	    {
	      (void)fprintf(stderr,
			    "You have VERY badly screwed up +c output!\n");
	      (void)fprintf(stderr,
			    "No ending ')' nickuserhost = [%s]\n",
			    nickuserhost);
	      /* No ending ')' found, but lets try it anyway */
	    }
	  return(uh);
	}

      if (strchr(uh+1,'['))
	{
	  /*moron has a [ in the nickname or username.  Let's do some AI crap*/
	  uh = strchr(uh,'~');
	  if (!uh)
	    {
	      /* No tilde to guess off of... means the lamer checks out with
		 identd and has (more likely than not) a valid username.
		 Find the last [ in the string and assume this is the
		 divider, unless it creates an illegal length username
		 or nickname */
	      uh = nickuserhost + strlen(nickuserhost);
	      while (--uh != nickuserhost)
		if (*uh == '[' && *(uh+1) != '@' && uh - nickuserhost < 10)
		  break;
	    }
	  else
	    {
	      /* We have a ~ which is illegal in a nick, but also valid
		 in a faked username.  Assume it is the marker for the start
		 of a non-ident username, which means a [ should precede it. */
	      if (*(uh-1) == '[')
		--uh;
	      else
		/* Idiot put a ~ in his username AND faked identd.  Take the
		   first [ that precedes this, unless it creates an
		   illegal length username or nickname */
		while (--uh != nickuserhost)
		  if (*uh == '[' && uh - nickuserhost < 10)
		    break;
	    }
	}
    }
  else
    skip = YES;

  *(uh++) = 0;
  if (skip)
    ++uh;                 /* Skip [ */
  if (strchr(uh,' '))
    *(strchr(uh,' ')) = 0;
  if (uh[strlen(uh)-1] == '.')
    uh[strlen(uh)-2] = 0;   /* Chop ] */
  else
    uh[strlen(uh)-1] = 0;   /* Chop ] */
  return uh;
}

/*
onservnotice()

inputs		- message from server
output		- NONE
side effects	-
*/

void onservnotice(notice)
char *notice;
{
  int i = -1;
  char *userhost;

  while (msgs_to_mon[++i])
    if (!strncmp(notice,msgs_to_mon[i],strlen(msgs_to_mon[i])))
      {
	notice += strlen(msgs_to_mon[i]);
	break;
      }

  /* Kline notice requested by Toast */
  if (strstr(notice, "added K-Line for"))
    kline_add_report(notice);

  if (strstr(notice, "KILL message for"))
    kill_add_report(notice);

#ifdef NOT_TH
  if (strstr(notice, "is now operator"))
    add_oper(notice);
#endif

  switch (i)
    {
    case CONNECT:
      userhost = chopuh(notice);
      adduserhost(notice,userhost,NO,NO);
      break;
    case EXITING:
      userhost = chopuh(notice);
      removeuserhost(notice,userhost);
      break;
    case BOT:
      bot_reject(notice);
      break;
    case UNAUTHORIZED:
    case TOOMANY:
      logfailure(notice,0);
      break;
    case NICKCHANGE:	/* - Dianora */
      check_nick_flood(notice);
      break;
/* CS style of reporting nick flooding */
    case CS_NICKFLOODING:	/* - Dianora */
      cs_nick_flood(notice);
      break;
    case CS_CLONES:
    case CS_CLONEBOT_KILLED:
      cs_clones(notice);
      break;
    case LINK_LOOK:
      link_look_notice(notice);
      break;
#ifndef OTHERNET
    case STATS:
      stats_notice(notice);
      break;
#endif
    default:
      break;
    }
}

void onctcp(nick, userhost, text)
char *nick, *userhost, *text;
{
  char *hold;
  char dccbuff[DCCBUFF_SIZE];
  char notice_buff[MAX_BUFF];

  dccbuff[0] = '#';
  ++text;
  if (!NCASECMP(text,"PING",4))
    notice(nick,text-1);
  else if (!NCASECMP(text,"VERSION",7)) {
#ifdef __EMX__
    (void)sprintf(notice_buff,"\001VERSION %s\001",VERSION4);
    notice(nick,notice_buff);
#endif
    (void)sprintf(notice_buff,"\001VERSION %s\001",VERSION1);
    notice(nick,notice_buff);
    (void)sprintf(notice_buff,"\001VERSION %s\001",VERSION2);
    notice(nick,notice_buff);
#ifdef AUTO_KLINE
    (void)sprintf(notice_buff,"\001VERSION %s\001",VERSION3);
    notice(nick,notice_buff);
#endif
    }
  else if (!NCASECMP(text,"DCC CHAT",8)) {
    text += 9;
    hold = strchr(text,' ');  /* Skip word 'Chat' */
    if (hold) {
      text = hold+1;
      hold = strchr(text,' ');
      if (hold) {
        *(hold++) = ':';
        strncpy(dccbuff+1,text,119);
        if (atoi(hold) < 1024)
          notice(nick,"Invalid port specified for DCC CHAT.  Not funny.");
        else if (!makeconn(dccbuff,nick,userhost))
          notice(nick,"DCC CHAT connection failed");
        return;
        }
      }
    notice(nick,"Unable to DCC CHAT.  Invalid protocol.");
    }
}

int hash_func(string)
char *string;
{
  long i;

  i = *(string++);
  if (*string)
    i |= (*(string++) << 8);
    if (*string)
      i |= (*(string++) << 16);
      if (*string)
        i |= (*string << 24);
  return i % HASHTABLESIZE;
}

void addtohash(table,key,item)
HASHREC *table[];
char *key;
USERENTRY *item;
{
  int index;
  HASHREC *newhashrec;

  index = hash_func(key);
  newhashrec = (HASHREC *)malloc(sizeof(HASHREC));
  if(newhashrec == (HASHREC *)NULL)
    {
      prnt(connections[0].socket,"Ran out of memory in addtohash\n");
      sendtoalldcc("Ran out of memory in addtohash",SEND_ALL_USERS);
      gracefuldie();
    }

  newhashrec->info = item;
  newhashrec->collision = table[index];
  table[index] = newhashrec;
}


/*
removefromhash()


	fixed memory leak here...
	make sure don't free() an already free()'ed info struct
	- Dianora
*/

char removefromhash(table,key,hostmatch,usermatch,nickmatch)
HASHREC *table[];
char *key, *hostmatch, *usermatch, *nickmatch;
{
  int index;
  HASHREC *find, *prev;

  index = hash_func(key);
  find = table[index];
  prev = (HASHREC *)NULL;

  while (find)
    {
      if ((!hostmatch || !strcmp(find->info->host,hostmatch)) &&
	  (!usermatch || !strcmp(find->info->user,usermatch)) &&
	  (!nickmatch || !strcmp(find->info->nick,nickmatch)))
	{
	  if (prev)
	    prev->collision = find->collision;
	  else
	    table[index] = find->collision;

	  if(find->info->link_count > 0)
	    find->info->link_count--;

	  /* debug output that can be removed some time - Dianora */
	  printf("find->info->link_count = %d\n", find->info->link_count );

	  if(find->info->link_count == 0)
	    {
	      (void)free(find->info); /* shouldn't this be freed too? - Dianora */
	      /* deubg output that can be removed some time - Dianora */
	      printf("link_count is 0 free() now\n");
	    }
	  (void)free(find);
	  return 1;
	}
      prev = find;
      find = find->collision;
    }
  return 0;
}

#ifdef NOT_TH
/*
add_oper

input		- nick
output		- NONE
side effects	- none
*/
void add_oper(notice)
char *notice;
{
  int i;
  char *nick;
  HASHREC *userptr;

  nick = strtok(notice," ");
  if(nick == (char *)NULL)
    return;

  for (i=0;i<HASHTABLESIZE;++i)
    {
      userptr = domaintable[i];
      while (userptr)
	{
	  if( CASECMP(userptr->info->nick, nick) == 0 )
	    {
	      userptr->info->isoper = YES;
	      return;
	    }
	  userptr = userptr->collision;
	}
    }
}
#endif

/*
updateuserhost()

inputs -
output - NONE
side effects 
	     - A user has changed nicks. update the nick
	       as seen by the hosttable. This way, list command
	       will show the updated nick.
*/

void updateuserhost(nick1,nick2,userhost)
char *nick1,*nick2,*userhost;
{
  char *tmp, iphold[20];
  char *domain;
  char founddot = 0;
  int i = 0;

  tmp = strchr(userhost,'@');
  if (!tmp)
    return;
  *(tmp++) = 0;

  /* Determine the domain name */
  domain = tmp + strlen(tmp) - 1;
  if (isdigit(*domain))
    {
      /* IP */
      domain = tmp;
      while (*domain)
	{
	  iphold[i++] = *domain;
	  if( *(domain++) == '.' )
	    founddot++;
	  if(founddot == 3 )
	    break;

	/*
	if (*(domain++) == '.' && founddot++)
	  break;
	  */
	}
      iphold[i] = '\0';
      domain = iphold;
    }
  else
    /* FQDN */
    while (--domain != tmp)
      if (*domain == '.' && founddot++)
        break;

  updatehash(hosttable,tmp,nick1,nick2);
}

/*
updatehash

inputs	- has table to update
	- key to use
	- nick1, nick2 nick changes
output	- NONE
side effects -
	  user entry nick is updated if found
*/

void updatehash(table,key,nick1,nick2)
HASHREC *table[];
char *key, *nick1, *nick2;
{
  int index;
  HASHREC *find, *prev;

  index = hash_func(key);
  find = table[index];

  while (find)
    {
      if( strcmp(find->info->nick,nick1) == 0 )
	{
	  strncpy(find->info->nick,nick2,MAX_NICK);
	}
      find = find->collision;
    }
}

void removeuserhost(nick,userhost)
char *nick,*userhost;
{
  char *tmp, iphold[20];
  char *domain;
  char founddot = 0;
  int i = 0;

  tmp = strchr(userhost,'@');
  if (!tmp)
    return;
  *(tmp++) = 0;

  /* Determine the domain name */
  domain = tmp + strlen(tmp) - 1;
  if (isdigit(*domain))
    {
      /* IP */
      domain = tmp;
      while (*domain)
	{
	  iphold[i++] = *domain;
	  if( *(domain++) == '.' )
	    founddot++;
	  if(founddot == 3 )
	    break;

	  /*
	  if (*(domain++) == '.' && founddot++)
	    break;
	    */

	}
      iphold[i] = '\0';
      domain = iphold;
    }
  else
    /* FQDN */
    while (--domain != tmp)
      if (*domain == '.' && founddot++)
        break;

  if (!removefromhash(hosttable,tmp,tmp,userhost,nick))
    if (!removefromhash(hosttable,tmp,tmp,userhost,(char *)NULL))
      printf("*** Error removing %s!%s@%s from host table!\n",nick,userhost,tmp);

  if (!removefromhash(domaintable,domain+1,tmp,userhost,nick))
    if (!removefromhash(domaintable,domain+1,tmp,userhost,(char *)NULL))
      printf("*** Error removing %s!%s@%s from domain table!\n",nick,userhost,tmp);
}


/*
adduserhost()

inputs		- nick
		- user@host
		- from a trace YES or NO
		- is this user an oper YES or NO
output		- NONE
side effects	-

These days, its better to show host IP's as class C
- Dianora
*/

void adduserhost(nick,userhost,fromtrace,isoper)
char *nick,*userhost;
int fromtrace;
int isoper;
{
  USERENTRY *newuser;
  char *host, iphold[20];
  char *domain;
  char founddot = 0;
  int i = 0;
  char *p;
  char *user = userhost;

  host = strrchr(userhost,'@');
  if (!host)
    return;
  *(host++) = '\0';

/*
 *sigh* catch some obvious dns spoofs, I won't be able
 to do much more for now, until later 
 basically, at least throw off users with a top level domain
 with more than 3 characters in it, throw off users with a '*' or '@'
 in hostpart.

 - Dianora 
*/

  p = strrchr(host,'.');
  if(p)
    {
      int len;

      p++;
      len = strlen(p);
      if(len > 3)
	{
	  kill_spoof(nick);
	  return;
	}

      if(len == 3)
	{
	  int legal_top_level=NO;

	  if(CASECMP(p,"net")==0)legal_top_level = YES;
	  if(CASECMP(p,"com")==0)legal_top_level = YES;
	  if(CASECMP(p,"org")==0)legal_top_level = YES;
	  if(CASECMP(p,"gov")==0)legal_top_level = YES;
	  if(CASECMP(p,"edu")==0)legal_top_level = YES;
	  if(CASECMP(p,"mil")==0)legal_top_level = YES;
	  if(CASECMP(p,"int")==0)legal_top_level = YES;

	  if(isdigit(*p) && isdigit(*(p+1)) && isdigit(*(p+2)) )
	     legal_top_level = YES;

	  if(!legal_top_level)
	    {
	      kill_spoof(nick);
	      return;
	    }
	}
    }

  if(strchr(host,'@'))
    {
      kill_spoof(nick);
      return;
    }

  if(strchr(host,'*'))
    {
      kill_spoof(nick);
      return;
    }

  if(strchr(host,'?'))
    {
      kill_spoof(nick);
      return;
    }

  newuser = (USERENTRY *)malloc(sizeof(USERENTRY));
  if( newuser == (USERENTRY *)NULL)
    {
      prnt(connections[0].socket,"Ran out of memory in adduserhost\n");
      sendtoalldcc("Ran out of memory in adduserhost",SEND_ALL_USERS);
      gracefuldie();
    }

  strncpy(newuser->nick,nick,MAX_NICK);
  newuser->nick[MAX_NICK-1] = '\0';
  strncpy(newuser->user,user,11);
  newuser->user[MAX_NICK] = '\0';
  strncpy(newuser->host,host,MAX_HOST);
  newuser->host[MAX_HOST-1] = '\0';
  newuser->connecttime = (fromtrace ? 0 : time(NULL));
  newuser->reporttime = 0;
  newuser->link_count = 2;
  newuser->isoper = isoper;

  /* Determine the domain name */
  domain = host + strlen(host) - 1;
  if (isdigit(*domain))
    {
      /* IP */
      domain = host;
      while (*domain)
	{
	  iphold[i++] = *domain;
	  if( *(domain++) == '.' )
	    founddot++;
	  if(founddot == 3 )
	    break;
	  /*
	  if (*(domain++) == '.' && founddot++)
	    break;
	    */
	}
      iphold[i] = '\0';
      domain = iphold;
    }
  else
    /* FQDN */
    while (--domain != host)
      if (*domain == '.' && founddot++)
        break;
  strncpy(newuser->domain,(*domain=='.' ? domain+1 : domain),MAX_DOMAIN);
  newuser->domain[MAX_DOMAIN-1] = '\0';

  /* Add it to the hash tables */
  addtohash(hosttable, host, newuser);
  addtohash(domaintable, domain+1, newuser);

  /* Clonebot check */
  if (!fromtrace)
    checkhostclones(host);
}

/*
kill_spoof

input	- nick
output	- none
side effects - lamer is killed
*/

void kill_spoof(nick)
char *nick;
{
  char hold[MAX_BUFF];
  char notice[MAX_BUFF];

  (void)sprintf(hold,"KILL %s :dns spoof\n",
		nick);
  toserv(hold);


  (void)sprintf(notice,"dns spoofer %s",nick);
  sendtoalldcc(notice,SEND_ALL_USERS);
}

void inithash()
{
  int i;

  for (i=0;i<HASHTABLESIZE;++i)
    hosttable[i] = domaintable[i] = (HASHREC *)NULL;
  doingtrace = YES;
  toserv("TRACE\n");
}

/* - Dianora */
/*
initopers()

inputs		- NONE
output		- NONE
side effects	-

*/

void initopers()
{
  clear_userlist();
  load_userlist();
  toserv("STATS O\n");
}

/* - Dianora */

/*
*/

void checkhostclones(host)
char *host;
{
  HASHREC *find;
  int clonecount = 0;
  int reportedclones = 0;
  char *last_user="";
  int last_identd,current_identd;
  int different;
  time_t now, lastreport, oldest;
  char notice1[MAX_BUFF];
  char notice[MAX_BUFF];
  struct tm *tmrec;
  int index;

  oldest = now = time(NULL);
  lastreport = 0;
  index = hash_func(host);
  find = hosttable[index];

  while (find)
    {
      if (!strcmp(find->info->host,host) &&
	  (now - find->info->connecttime < CLONECONNECTFREQ + 1))
	if (find->info->reporttime > 0)
	  {
	    ++reportedclones;
	    if (lastreport < find->info->reporttime)
	      lastreport = find->info->reporttime;
	  }
	else
	  {
	    ++clonecount;
	    if (find->info->connecttime < oldest)
	      oldest = find->info->connecttime;
	  }
      find = find->collision;
    }

  if ((reportedclones == 0 && clonecount < CLONECONNECTCOUNT) ||
      now - lastreport < 10)
    return;

  initlog();

  if (reportedclones)
    {
      (void)sprintf(notice,"%d more possible clones (%d total) from %s:",
            clonecount, clonecount+reportedclones, host);

      if(logging_fp)
	{
	  fprintf(logging_fp,"%d more possible clones (%d total) from %s:\n",
            clonecount, clonecount+reportedclones, host);
	}

    }
  else
    {
      (void)sprintf(notice,
	      "Possible clones from %s detected: %d connects in %d seconds",
	      host, clonecount, now - oldest);

      if(logging_fp)
	fprintf(logging_fp,
	       "Possible clones from %s detected: %d connects in %d seconds\n",
		host, clonecount, now - oldest);
    }

  msg(mychannel,notice);
  sendtoalldcc(notice,SEND_ALL_USERS);

  clonecount = 0;
  find = hosttable[index];

  while (find)
    {
    if (!strcmp(find->info->host,host) &&
        (now - find->info->connecttime < CLONECONNECTFREQ + 1) &&
        find->info->reporttime == 0)
      {
	++clonecount;
	tmrec = localtime(&find->info->connecttime);

	if(clonecount == 1)
	  {
	    (void)sprintf(notice1,"  %s is %s@%s (%2.2d:%2.2d:%2.2d)",
		    find->info->nick, find->info->user, find->info->host,
		    tmrec->tm_hour, tmrec->tm_min, tmrec->tm_sec);
	  }
	else
	  {
	    (void)sprintf(notice,"  %s is %s@%s (%2.2d:%2.2d:%2.2d)",
		    find->info->nick, find->info->user, find->info->host,
		    tmrec->tm_hour, tmrec->tm_min, tmrec->tm_sec);
	  }

	last_identd = current_identd = YES;
        different = NO;

	if(clonecount == 1)
	  last_user = find->info->user;
	else if(clonecount == 2)
	  {
	    char *current_user;

	    if( *last_user == '~' )
	      {
		last_user++;
		last_identd = NO;
	      }

	    current_user = find->info->user;
	    if( *current_user == '~' )
	      {
		current_user++;
		current_identd = NO;
	      }

	    if(strcmp(last_user,current_user) != 0)
	      different = YES;

	    suggest_kline(YES,
			  find->info->user,
			  find->info->host,
			  different,
			  last_identd|current_identd,
			  "clones");

	  }

	find->info->reporttime = now;
	if(clonecount == 1)
	  ;
	else if(clonecount == 2)
	  {
	    msg(mychannel,notice1);
	    sendtoalldcc(notice1,SEND_ALL_USERS);

	    if(logging_fp)
	      {
		fputs(notice1,logging_fp);
		fputc('\n',logging_fp);
	      }

	    msg(mychannel,notice);
	    sendtoalldcc(notice,SEND_ALL_USERS);

	    if(logging_fp)
	      {
		fputs(notice,logging_fp);
		fputc('\n',logging_fp);
	      }
	  }
	else if (clonecount < 5)
	  {
	    msg(mychannel,notice);
	    sendtoalldcc(notice,SEND_ALL_USERS);

	    if(logging_fp)
	      {
		fputs(notice,logging_fp);
		fputc('\n',logging_fp);
	      }
	  }
	else if (clonecount == 5)
	  {
	    msg(mychannel,"  [etc.]");
	    sendtoalldcc(notice,SEND_ALL_USERS);
	    if(logging_fp)
	      {
		fputs("  [etc.]\n",logging_fp);
	      }
	  }
      }
    find = find->collision;
    }

  if(logging_fp)
    {
      (void)fclose(logging_fp);
      logging_fp = (FILE *)NULL;
    }
}

/*
suggest_kline

  Suggest a kline for an oper to use
inputs	- user user name
	  host host name
	  different whether last username matches first users name
	  reason for kline
output	- none
side effects
	connected opers are dcc'ed a suggested kline

  I have to reassemble user and host back into a u@h, in order
to do matching of users not to KILL or KLINE. urgh. This seems
silly as I have had to split them elsewhere. 

	- Dianora 
*/

void suggest_kline(kline,user,host,different,identd,reason)
int kline;	/* kline or not */
char *user;
char *host;
int different;
int identd;
char *reason;
{
  char notice[MAX_BUFF];
#ifdef AUTO_KLINE
  char userathost[2*MAX_HOST];	/* ARGH, now I have to reassemble u@h */
  char hold[MAX_BUFF];
#endif
  char *work_host;	/* copy of the host name */
  char *suggested_host;	/* what will be suggested as host to kline */
  char *p;
  char *q;
  int number_of_dots=0;	/* how many dots in the name ? */
  int ip_number = YES;
  time_t current_time;
  struct tm *broken_up_time;

#ifdef AUTO_KLINE
  hold[0] = '\0';	

  /* urgh.. dumb dumb dumb ... - Dianora */


  strncpy(userathost,user,MAX_HOST-2); /* to ensure no buffer overruns -sigh-*/
  strcat(userathost,"@");
  strncat(userathost,host,MAX_HOST); 
				     
#endif

  notice[0] = '\0';

/* 
  Ugh. with the advent of dns spoofing, I'll have to be a little
  more careful with auto kline code. one lamer with a host name
  of "*.*" came on irc2.magic.ca and nearly klined the entire server...
  - Dianora
*/

  p = strchr(host,'*');
  if(p)
    {
      (void)sprintf(notice,"Bogus dns spoofed host %@%s",user,host);
      msg(mychannel,notice);
      sendtoalldcc(notice,SEND_ALL_USERS);
      return;
    }

  p = strchr(host,'?');
  if(p)
    {
      (void)sprintf(notice,"Bogus dns spoofed host %@%s",user,host);
      msg(mychannel,notice);
      sendtoalldcc(notice,SEND_ALL_USERS);
      return;
    }

  work_host = strdup(host);
  if(work_host == (char *)NULL)
    {
      (void)sprintf(notice,"Ran out of memory in suggest_kline");
      msg(mychannel,notice);
      sendtoalldcc(notice,SEND_ALL_USERS);
      gracefuldie();
    }

  p = q = work_host;

  while(*p)
    {
      if(*p == '.')
	number_of_dots++;
      else if( !isdigit(*p) )
	ip_number = NO;
      p++;
    }    

  if(number_of_dots != 3)
    ip_number = NO;

  if(ip_number)
    {
      while(*p != '.')
        {
	  p--;
	  if( p == q )	/* JUST in case */
	    break;
	}
      *p++ = '.';	/* should already be a '.' right here, but... */
      *p++ = '*';
      *p = '\0';
    }
  else
    {
/*
  
*/
      if(number_of_dots > 1)	/* Is it "some.host.dom" or just
				   "somehost.dom" ? if only one dot
				   it's the second form */
	{
	  while(*q != '.')
	    {
	      q++;
	      if( *q == '\0' ) /* JUST in case */
		break;
	    }

	  p = q;

	  while(*p)
	    p++;
	  while(*p != '.')
	    p--;
	  p++;
/*
  I am now at the end of the hostname. the last little bit is the
  top level domain. if its only two letters, then its a country
  domain, and I have to rescan
*/
	  if(strlen(p) != 3)	/* *sigh* try again */
	    {
	      q = work_host;
	      if(number_of_dots > 2)
		{
		  while(*q != '.')
		    {
		      q++;
		      if( *q == '\0' ) /* JUST in case */
			break;
		    }
		  q--;
		  *q = '*';
		}
	    }
	  else
	    {
	      q--;
	      *q = '*';
	    }
	}
    }

  suggested_host = q;

  current_time = time((time_t *)NULL);
  broken_up_time = localtime(&current_time);

  if(different)
    {
      if(identd)
	{

  /* added Auto k-line 9/6/96 Phisher dkemp@frontiernet.net */
/* After adding Auto k-line, I thought it might be nice to compare 
   first to a hosttable so we aren't klining our own users etc. - Phisher

   Wouldn't hurt to make sure they aren't an oper too
   - Dianora
*/
         if ( (!okhost(userathost)) && (!isoper(userathost)))  
	    {
	      (void)sprintf(notice,
			    "/quote kline *@%s :%s %02d/%02d/%02d",
			    host,reason,
			    (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
			    broken_up_time->tm_year);
	    }
	  else
	    {
#ifndef DONT_WARN_OUR_CLONES
	      (void)sprintf(notice,
			    "/quote kline *@%s :%s %02d/%02d/%02d",
			    host,reason,
			    (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
			    broken_up_time->tm_year);
#endif
	    }

	 /* Just always log 'em for now */

	  if(logging_fp)
	    fprintf(logging_fp,"/quote kline *@%s :%s %02d/%02d/%02d\n",
		    host,reason,
		    (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
		    broken_up_time->tm_year);
	}
      else
	{
          if ( (!okhost(userathost)) && (!isoper(userathost)) )  
	    {
#ifdef AUTO_KLINE
	      if(autopilot && kline)
		{
		  (void)sprintf(notice,
			  "Adding Auto Kline for ~*@%s :%s %02d/%02d/%02d",
			  suggested_host,reason,
			  (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
			  broken_up_time->tm_year);

		  (void)sprintf(hold,
			  "KLINE ~*@%s :Auto-kline, %s %02d/%02d/%02d\n",
			  suggested_host,reason,
			  (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
			  broken_up_time->tm_year);
		}
	      else
#endif
		{
		  (void)sprintf(notice,
			  "/quote kline ~*@%s :%s %02d/%02d/%02d",
			  suggested_host,reason,
			  (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
			  broken_up_time->tm_year);
		}
	    }
	  else
	    {
#ifndef DONT_WARN_OUR_CLONES
	      (void)sprintf(notice,
		      "/quote kline ~*@%s :%s %02d/%02d/%02d",
		      suggested_host,reason,
		      (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
		      broken_up_time->tm_year);
#endif
	    }

/* Just log 'em for now */

	  if(logging_fp)
	    fprintf(logging_fp,"/quote kline ~*@%s :%s %02d/%02d/%02d\n",
		    suggested_host,reason,
		    (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
		    broken_up_time->tm_year);
	}
    }
  else
    {
      if(*user == '~') /* see if failed ident */
	user++;

      if ( (!okhost(userathost)) && (!isoper(userathost)) )  
	 {
	   (void)sprintf(notice,
			 "/quote kline *%s@%s :%s %02d/%02d/%02d",
			 user,suggested_host,reason,
			 (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
			 broken_up_time->tm_year);
	 }
      else
	{
#ifndef DONT_WARN_OUR_CLONES
	  (void)sprintf(notice,
		  "/quote kline *%s@%s :%s %02d/%02d/%02d",
		  user,suggested_host,reason,
		  (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
		  broken_up_time->tm_year);
#endif
	}

/* Just log 'em for now */

      if(logging_fp)
	fprintf(logging_fp,"/quote kline *%s@%s :%s %02d/%02d/%02d\n",
	       user,suggested_host,reason,
	       (broken_up_time->tm_mon)+1,broken_up_time->tm_mday,
	       broken_up_time->tm_year);
    }

  if(notice[0] != '\0')
    {
      msg(mychannel,notice);
      sendtoalldcc(notice,SEND_ALL_USERS);
    }

#ifdef AUTO_KLINE
/* Don't send anything to server if nothing */
  if(hold[0] != '\0')
    toserv(hold);
#endif

  (void)free(work_host);
}

/*
report_nick_flooders

inputs - socket to use
output - NONE
side effects -
	list of current nick flooders is reported


  Read the comment in add_to_nick_change_table as well.

	- Dianora
*/

void report_nick_flooders(socket)
int socket;
{
  int i;
  NICK_CHANGE_ENTRY *ncp;
  char outmsg[MAX_BUFF];
  int reported_nick_flooder= NO;
  time_t current_time;

  current_time = time((time_t *)NULL);

  for(i = 0; i < NICK_CHANGE_TABLE_SIZE; i++)
    {
      if(ncp = nick_changes[i])
	{
	  time_t time_difference;
	  int time_ticks;

	  time_difference = current_time - ncp->last_nick_change;

	  /* is it stale ? */
	  if( time_difference >= NICK_CHANGE_T2_TIME )
	    {
	      (void)free(ncp->user_host);
	      (void)free(nick_changes[i]);
	      nick_changes[i] = (NICK_CHANGE_ENTRY *)NULL;
	    }
	  else
	    {
	      /* how many 10 second intervals do we have? */
	      time_ticks = time_difference / NICK_CHANGE_T1_TIME;

	      /* is it stale? */
	      if(time_ticks >= ncp->nick_change_count)
		{
		  (void)free(ncp->user_host);
		  (void)free(nick_changes[i]);
		  nick_changes[i] = (NICK_CHANGE_ENTRY *)NULL;
		}
	      else
		{
		  /* just decrement 10 second units of nick changes */
		  ncp->nick_change_count -= time_ticks;
		  if(ncp->nick_change_count > 1)
		    {
		      (void)sprintf(outmsg,
				    "user: %s (%s) %d in %d\n",
				    ncp->user_host,
				    ncp->last_nick,
				    ncp->nick_change_count,
				    ncp->last_nick_change-ncp->first_nick_change);
		      reported_nick_flooder = YES;
		      if(socket)
			prnt(socket,outmsg);
		    }
		}
	    }
	}
    }

  if(!reported_nick_flooder)
    {
      (void)sprintf(outmsg,"No nick flooders found\n");
      if(socket)
	prnt(socket,outmsg);
    }
}

/*
report_domains
intput 		- socket
		- num
output		- NONE
side effects	-
*/


/* allocating this much onn the stack makes me queasy
   so for now, I'll define it in the data segment even
   though it really should be malloc() 
   - Dianora
*/

struct sortarray sort[MAXDOMAINS];

void report_domains(socket,num)
int socket;
int num;
{
  struct hashrec *userptr;

  int inuse = 0;
  int i,j,maxx,found,foundany = 0;
  char outmsg[MAX_BUFF];

  for (i=0;i<HASHTABLESIZE;++i) {
    userptr = hosttable[i];
    while (userptr) {
      for (j=0;j<inuse;++j)
        if (!CASECMP(userptr->info->domain,sort[j].domainrec->domain))
          break;
      if (j == inuse && inuse < MAXDOMAINS) {
        sort[inuse].domainrec = userptr->info;
        sort[inuse++].count = 1;
        }
      else
        ++sort[j].count;
      userptr = userptr->collision;
      }
    }
  /* Print 'em out from highest to lowest */
  for (;;) {
    maxx = num-1;
    found = -1;
    for (i=0;i<inuse;++i)
      if (sort[i].count > maxx) {
        found = i;
        maxx = sort[i].count;
        }
    if (found == -1)
      break;
    if (!foundany++)
      prnt(socket,"Domains with most users on the server:\n");
    (void)sprintf(outmsg,
		  "  %-40s %3d users\n",sort[found].domainrec->domain,maxx);
    prnt(socket,outmsg);
    sort[found].count = 0;
    }
  if (!foundany)
    {
      (void)sprintf(outmsg,
		    "No domains have %d or more users.\n",num);
      prnt(socket,outmsg);
    }
}

void massversion(socket)
int socket;
{
  prnt(socket,"Cannot currently mass version\n");
}

void report_multi(socket)
int socket;
{
  struct hashrec *userptr,*top,*temp;
  int numfound,i;
  char notip, foundany = 0;
  char outmsg[MAX_BUFF];

  for (i=0;i<HASHTABLESIZE;++i) {
    top = userptr = domaintable[i];
    while (userptr) {
      numfound = 0;
      /* Ensure we haven't already checked this user & domain */
      temp = top;
      while (temp != userptr)
        if (!strcmp(temp->info->user,userptr->info->user) &&
            !strcmp(temp->info->domain,userptr->info->domain))
          break;
        else
          temp = temp->collision;
      if (temp == userptr) {
        temp = temp->collision;
        while (temp)
	  {
	    if (!strcmp(temp->info->user,userptr->info->user) &&
		!strcmp(temp->info->domain,userptr->info->domain))
	      numfound++;	/* - zaph & Dianora :-) */
	    temp = temp->collision;
          }
        if (numfound)
	  {
	    if (!foundany++)
	      prnt(socket,"Multiple clients from the following userhosts:\n");
	    notip = strncmp(userptr->info->domain,userptr->info->host,
			    strlen(userptr->info->domain)) ||
	      (strlen(userptr->info->domain) == strlen(userptr->info->host));
	    numfound++;	/* - zaph and next line*/
	    (void)sprintf(outmsg,
		    " %s %2d connections -- %s@%s%s\n",
		    (numfound > 2) ? "==>" : "   ",numfound,userptr->info->user,
		    notip ? "*" : userptr->info->domain,
		    notip ? userptr->info->domain : "*", numfound);
	    prnt(socket,outmsg);
          }
        }
      userptr = userptr->collision;
      }
    }
  if (!foundany)    
    prnt(socket,"No multiple logins found.\n");
}
/*
list_nicks()

inputs		- socket to reply on, nicks to search for
output		- NONE
side effects	-
*/

void list_nicks(socket,nick)
int socket;
char *nick;
{
  HASHREC *userptr;
  int i;
  int numfound=0;
  char outmsg[MAX_BUFF];
  char fulluh[MAX_HOST+MAX_DOMAIN];

  for (i=0;i<HASHTABLESIZE;++i)
    {
      userptr = domaintable[i];
      while (userptr)
	{
	  if (!wldcmp(nick,userptr->info->nick))
	    {
	      if(!numfound++)
		{
		  (void)sprintf(outmsg,
				"The following clients match %.150s:\n",nick);
		  prnt(socket,outmsg);
		}
	      (void)sprintf(fulluh,
			    "%s@%s",userptr->info->user,userptr->info->host);
	      (void)sprintf(outmsg,
			    "  %s (%s)\n",userptr->info->nick,fulluh);
	      prnt(socket,outmsg);
	    }
	  userptr = userptr->collision;
        }
    }

  if (numfound > 0)
    (void)sprintf(outmsg,
		  "%d matches for %s found\n",numfound,nick);
  else
    (void)sprintf(outmsg,
		  "No matches for %s found\n",nick);
  prnt(socket,outmsg);
}

/*
list_users()

inputs		- socket to reply on
output		- NONE
side effects	-
*/

void list_users(socket,userhost)
int socket;
char *userhost;
{
  HASHREC *userptr;
  char fulluh[MAX_HOST+MAX_DOMAIN];
  int i,numfound = 0;
  char outmsg[MAX_BUFF];

  if (!strcmp(userhost,"*") || !strcmp(userhost,"*@*"))
    prnt(socket,"Listing all users is not recommended.  To do it anyway, use 'list ?*@*'.\n");
  else {
    for (i=0;i<HASHTABLESIZE;++i) {
      userptr = domaintable[i];
      while (userptr)
	{
        (void)sprintf(fulluh,
		      "%s@%s",userptr->info->user,userptr->info->host);
        if (!wldcmp(userhost,fulluh))
	  {
	    if (!numfound++)
	      {
		(void)sprintf(outmsg,
			     "The following clients match %.150s:\n",userhost);
		prnt(socket,outmsg);
	      }
	    (void)sprintf(outmsg,
			  "  %s (%s)\n",userptr->info->nick,fulluh);
	    prnt(socket,outmsg);
          }
        userptr = userptr->collision;
        }
      }
    if (numfound > 0)
      (void)sprintf(outmsg,
	      "%d matches for %s found\n",numfound,userhost);
    else
      (void)sprintf(outmsg,
		    "No matches for %s found\n",userhost);
    prnt(socket,outmsg);
  }
}

/*  This code from Phisher, Phisher comments ... ( - Dianora )


   added listkill command, instead of reporting the nick@uh from a list command
   the bot just kills em. POWER!!!  Phisher dkemp@frontiernet.net note:
   obviously most of this code is just hendrixes routine, why re-write the 
   bible, its the idea that counts :)


*/

void kill_list_users(socket,userhost)
int socket;
char *userhost;
{
  struct hashrec *userptr;
  char fulluh[100];
  int i,numfound = 0;
  char outmsg[MAX_BUFF];

  if (!strcmp(userhost,"*") || !strcmp(userhost,"*@*"))
    prnt(socket,"Killing all users is not recommended.  To do it anyway, use 'list ?*@*'.\n");
  else {
    for (i=0;i<HASHTABLESIZE;++i) {
      userptr = domaintable[i];
      while (userptr) {
        sprintf(fulluh,"%s@%s",userptr->info->user,userptr->info->host);
        if (!wldcmp(userhost,fulluh)) {
          if (!numfound++) {
            /* sprintf(outmsg,"The following clients match %.150s:\n",userhost);
            prnt(socket,outmsg); */
     }
          sprintf(outmsg,"KILL %s :Too many connections, read MOTD\n",userptr->info->nick);
          toserv(outmsg);
          if(logging_fp)
	    fprintf(logging_fp,"%s listkilled %s\n",
               socket,fulluh);
          }
        userptr = userptr->collision;
        }
      }
    if (numfound > 0)
      sprintf(outmsg,"%d matches for %s found\n",numfound,userhost);
    else
      sprintf(outmsg,"No matches for %s found\n",userhost);
    prnt(socket,outmsg);
    }
}

/*
print_help()

inputs		- socket, help_text to use
output		- none
side effects	- prints help file to user
*/

void print_help(socket,text)
int socket;
char *text;
{
  FILE *userfile;
  char line[MAX_BUFF];
  char help_file[MAX_BUFF];
  int cnt = 0;

  if(text == (char *)NULL)
    {
      if( (userfile = fopen(HELP_FILE,"r")) == (FILE *)NULL)
	{
	  prnt(socket,"Help is not currently available\n");
	  return;
	}
    }
  else
    {
      while(*text == ' ')
	text++;

      (void)sprintf(help_file,"%s.%s",HELP_FILE,text);
      if( (userfile = fopen(help_file,"r")) == (FILE *)NULL)
	{
	  (void)sprintf(line,"Help for %s is not currently available\n",text);
	  prnt(socket,line);
	  return;
	}
    }

  while (fgets(line, MAX_BUFF-1, userfile) != NULL)
    {
      prnt(socket,line);
    }
  fclose(userfile);
}

/*
print_motd()

inputs		- socket
output		- none
side effects	- prints a message of the day to the connecting client

Larz asked for this one. a message of the day on connect
I just stole the code from print_help
- Dianora
*/

void print_motd(socket)
int socket;
{
  FILE *userfile;
  char line[MAX_BUFF];
  int cnt = 0;

  if( (userfile = fopen(MOTD_FILE,"r")) == (FILE *)NULL)
    {
      prnt(socket,"No MOTD\n");
      return;
    }

  while (fgets(line, MAX_BUFF-1, userfile) != NULL)
    {
      prnt(socket,line);
    }
  fclose(userfile);
}

void report_failures(socket,num)
int socket,num;
{
  int i,j,maxx,foundany = 0;
  char outmsg[MAX_BUFF];
  struct failrec *tmp,*found;

  /* Print 'em out from highest to lowest */
  for (;;) {
    maxx = num-1;
    found = NULL;
    tmp = failures;
    while (tmp) {
      if (tmp->failcount > maxx) {
        found = tmp;
        maxx = tmp->failcount;
        }
      tmp = tmp->next;
      }
    if (!found)
      break;
    if (!foundany++)
      prnt(socket,"Userhosts with most connect rejections:\n");
    sprintf(outmsg," %5d rejections: %s@%s%s\n", found->failcount,
            (*found->user ? found->user : "<UNKNOWN>"), found->host,
            (found->botcount ? " <BOT>" : ""));
    prnt(socket,outmsg);
    found->failcount = -found->failcount;   /* Yes, this is horrible */
    }
  if (!foundany) {
    sprintf(outmsg,"No userhosts have %d or more rejections.\n",num);
    prnt(socket,outmsg);
    }
  tmp = failures;
  while (tmp) {
    if (tmp->failcount < 0)
      tmp->failcount = -tmp->failcount;   /* Ugly, but it works. */
    tmp = tmp->next;
    }
}

void report_clones(socket)
int socket;
{
  struct hashrec *userptr,*top,*temp;
  int numfound,i,j,k;
  char notip, foundany = 0;
  char outmsg[MAX_BUFF];
  time_t connfromhost[MAXFROMHOST];

  for (i=0;i<HASHTABLESIZE;++i) {
    top = userptr = hosttable[i];
    while (userptr) {
      numfound = 0;
      /* Ensure we haven't already checked this host */
      temp = top;
      while (temp != userptr)
        if (!strcmp(temp->info->host,userptr->info->host))
          break;
        else
          temp = temp->collision;
      if (temp == userptr) {
        connfromhost[numfound++] = temp->info->connecttime;
        temp = temp->collision;
        while (temp) {
          if (!strcmp(temp->info->host,userptr->info->host) &&
              numfound < MAXFROMHOST)
            connfromhost[numfound++] = temp->info->connecttime;
          temp = temp->collision;
          }
        if (numfound > 2) {
          for (k=numfound-1;k>1;--k)
            for (j=0;j<numfound-k;++j) {
              if (connfromhost[j] &&
                  connfromhost[j] - connfromhost[j+k] <= (k+1) * CLONEDETECTINC)
                goto getout;  /* goto rules! */
              }
          getout:
          if (k > 1) {
            if (!foundany++)
              if (socket)
                prnt(socket,"Possible clonebots from the following hosts:\n");
              else
                msg(mychannel,"Possible clonebots from the following hosts:");
            sprintf(outmsg,
                    "  %2d connections in %3d seconds (%2d total) from %s\n",
                    k+1, connfromhost[j] - connfromhost[j+k], numfound+1,
                    userptr->info->host);
            if (socket)
              prnt(socket,outmsg);
            else if (foundany == 5)
              msg(mychannel,"  [rest deleted... use DCC CHAT for full list]");
            else if (foundany < 5) {
              outmsg[strlen(outmsg)-1] = 0;
              msg(mychannel,outmsg);
              }
            }
          }
        }
      userptr = userptr->collision;
      }
    }
  if (!foundany)
    if (socket)
      prnt(socket,"No potential clonebots found.\n");
    else
      msg(mychannel,"No potential clonebots found.");
}

void logfailure(nickuh,botreject)
char *nickuh, botreject;
{
  char *uh, *host;
  struct failrec *tmp, *hold = NULL;

  uh = chopuh(nickuh);
  host = strchr(uh,'@');
  if (!host)
    return;
  *(host++) = 0;
  tmp = failures;
  while (tmp) {
    if (!CASECMP(tmp->user,uh) && !CASECMP(tmp->host,host)) {
      /* For performance, move the most recent to the head of the queue */
      if (hold) {
        hold->next = tmp->next;
        tmp->next = failures;
        failures = tmp;
        }
      break;
      }
    hold = tmp;
    tmp = tmp->next;
    }

  if (!tmp)
    {
    tmp = (struct failrec *)malloc(sizeof(struct failrec));
    if(tmp == (struct failrec *)NULL)
       {
         prnt(connections[0].socket,"Ran out of memory in logfailure\n");
	 sendtoalldcc("Ran out of memory in logfailure",SEND_ALL_USERS);
	 gracefuldie();
       }

    strncpy(tmp->user,uh,11);
    tmp->user[10] = 0;
    strncpy(tmp->host,host,MAX_HOST);
    tmp->host[79] = 0;
    tmp->failcount = tmp->botcount = 0;
    tmp->next = failures;
    failures = tmp;
    }
  if (botreject)
    ++tmp->botcount;
  ++tmp->failcount;
}

/*
link_look_notice

inputs	- rest of notice from server
output	- NONE
side effects

  What happens here: There is a fixed sized table of MAX_LINK_LOOKS
each with a LINK_LOOK_ENTRY struct. Both the expiry of old old link
entries is made, plus the search for an empty slot to stick a possible
new entry into. If the user@host entry is NOT found in the table
then an entry is made for this user@host, and is time stamped.

	- Dianora

ARGGHHHHH

  +th ircd has "LINKS '...' requested by "
  where ... is usualy blank or a server name etc.
  LT and CS do not. sorry guys for missing that. :-(
  Jan 1 1997  - Dianora
*/
void link_look_notice(server_notice)
char *server_notice;
{
  char *nick_reported;
  char *user_host;
  char *user;
  char *host;
  char *p;
  char notice[MAX_BUFF];
  char hold[MAX_BUFF];
  char copy_of_server_notice[MAX_BUFF];
  time_t current_time;
  int first_empty_entry = -1;
  int found_entry = NO;
  int i;

  current_time = time((time_t *)NULL);

  p = server_notice;

  (void)printf("DEBUG: link_look server_notice = [%s]\n", server_notice );

  if(*p == '\'')		/* Is this a +th server ?*/
    {
      /* This is a +th server, skip the '...' part */
      p++;
      p = strchr(p,'\'');
      if(p == (char*)NULL)return; 	/* just ignore it *sigh* */
      p++;
      strncpy(copy_of_server_notice,p,MAX_BUFF);
    }
  else
    strncpy(copy_of_server_notice,p,MAX_BUFF);

  p = strtok(copy_of_server_notice," ");
  if(p == (char *)NULL) return;	/* just ignore it *sigh* */
  (void)printf("DEBUG: link_look p = [%s]\n", p);

  if(CASECMP(p,"requested") != 0)return;	/* just ignore it *sigh* */

  p = strtok((char *)NULL," ");
  if(p == (char *)NULL) return;		/* just ignore it *sigh* */
  if(CASECMP(p,"by") != 0)return;	/* just ignore it *sigh* */

  nick_reported = strtok((char *)NULL," ");
  if(nick_reported == (char *)NULL)return;

  user_host = strtok((char *)NULL," ");
  if(user_host == (char *)NULL)return;

  (void)printf("DEBUG: link_look user_host now = [%s]\n", user_host );

/*
  Lets try and get it right folks... [user@host] or (user@host)
*/

  if(*user_host == '[')
    {
      user_host++;
      p = strrchr(user_host,']');
      if(p)
	*p = '\0';
    }
  else if(*user_host == '(')
    {
      user_host++;
      p = strrchr(user_host,')');
      if(p)
	*p = '\0';
    }

  (void)printf("DEBUG: link_look user_host now = [%s]\n", user_host );

  /* Don't even complain about opers */

  if ( isoper(user_host) )  
    {
      (void)printf("DEBUG: is oper\n");
      return;
    }

  (void)sprintf(notice,"[LINKS %s]", server_notice); /* - zaph */
  sendtoalldcc(notice,SEND_ALL_USERS);

  for(i = 0; i < MAX_LINK_LOOKS; i++ )
    {
      if(link_look[i].user_host)
	{
	  if(CASECMP(link_look[i].user_host,user_host) == 0)
	    {
	      char *user;
	      char *host;

	      found_entry = YES;
	      (void)printf("DEBUG: found_entry = YES [%s]\n", user_host );
	  
	      /* if its an old old entry, let it drop to 0, then start counting
		 (this should be very unlikely case)
		 */
	      if((link_look[i].last_link_look + MAX_LINK_TIME) < current_time)
		{
		  link_look[i].link_look_count = 0;
		}

	      link_look[i].link_look_count++;
	      (void)printf("DEBUG: link_look[%d].link_look_count =  [%d]\n",
			   i,link_look[i].link_look_count );
	      
	      if(link_look[i].link_look_count >= MAX_LINK_LOOKS)
		{
		  (void)sprintf(notice,"possible LINK LOOKER nick [%s]", 
				nick_reported,user_host);
		  sendtoalldcc(notice,SEND_ALL_USERS);

		  initlog();
		  if(logging_fp)
		    fprintf(logging_fp,
			    "possible LINK LOOKER  = %s [%s]\n",
			    nick_reported,user_host);

/*
   The code as it is, doesn't AUTO_KLINE link lookers ever, but will
   AUTO_KILL link lookers IFF autopilot is on. HOWEVER,
   Shadowfax still wants to see a suggested kline to use.

   - Dianora
*/
		  if ( !okhost(user_host) )
		    {
#ifdef AUTO_KILL_LINK_LOOKERS
		      if(autopilot)
			{
			  (void)sprintf(hold,"KILL %s :link looker\n",
					nick_reported );
			  toserv(hold);
			  (void)free(link_look[i].user_host);
			  link_look[i].user_host = (char *)NULL;
			}
#endif
		      user = strtok(user_host,"@");
		      if(user == (char *)NULL)return; /* OOOPSIES! */
		      host = strtok((char *)NULL,"");
		      if(host == (char *)NULL)return; /* OOPSIES! */
		      
		      if(*user_host == '~')
			suggest_kline(NO,user,host,NO,NO,"link looker");
		      else
			suggest_kline(NO,user,host,NO,YES,"link_looker");
		    }

#ifndef AUTO_KILL_LINK_LOOKERS
		  link_look[i].last_link_look = current_time;
#endif
		  if(logging_fp)
		    {
		      (void)fclose(logging_fp);
		      logging_fp = (FILE *)NULL;
		    }
		}
	      else
		{
		  link_look[i].last_link_look = current_time;
		}
	    }
	  else
	    {
	      if((link_look[i].last_link_look + MAX_LINK_TIME) < current_time)
		{
		  (void)free(link_look[i].user_host);
		  link_look[i].user_host = (char *)NULL;
		}
	    }
	}
      else
	{
	  if(first_empty_entry < 0)
	    first_empty_entry = i;
	}
    }

/*
   If this is a new entry, then found_entry will still be NO
*/

  if(!found_entry)
    {
      if(first_empty_entry >= 0)
	{
	  (void)printf("DEBUG: new entry user_host [%s]\n", user_host );
	  link_look[first_empty_entry].user_host = strdup(user_host);
	  link_look[first_empty_entry].last_link_look = current_time;
	  link_look[first_empty_entry].link_look_count = 0;
	}
    }
}

/*
cs_nick_flood

inputs	- rest of notice from server
output	- NONE
side effects

	For clones CS uses [user@host] for nick flooding CS uses (user@host)
	go figure.

	- Dianora
*/
void cs_nick_flood(server_notice)
char *server_notice;
{
  char *nick_reported;
  char *user_host;
  char *user;
  char *host;
  char *p;
  char notice[MAX_BUFF];

  nick_reported = strtok(server_notice," ");
  if(nick_reported == (char *)NULL)return;

  user_host = strtok((char *)NULL," ");
  if(user_host == (char *)NULL)return;

/*
  Lets try and get it right folks... [user@host] or (user@host)
*/

  if(*user_host == '[')
    {
      user_host++;
      p = strrchr(user_host,']');
      if(p)
	*p = '\0';
    }
  else if(*user_host == '(')
    {
      user_host++;
      p = strrchr(user_host,')');
      if(p)
	*p = '\0';
    }

    (void)sprintf(notice,"CS nick flood user_host = [%s]", user_host);
    sendtoalldcc(notice,SEND_ALL_USERS);

  initlog();
  if(logging_fp)
    fprintf(logging_fp,"CS nick flood user_host = [%s]\n", user_host);

  if ( (!okhost(user_host)) && (!isoper(user_host)) )  
    {
#ifdef AUTO_KILL_NICK_FLOODING
      if(autopilot)
	{
	  (void)sprintf(notice,"KILL %s :flooding\n", nick_reported );
	  toserv(notice);
	}
#endif
      user = strtok(user_host,"@");
      if(user == (char *)NULL)return; /* OOOPSIES! */
      host = strtok((char *)NULL,"");
      if(host == (char *)NULL)return; /* OOPSIES! */
      
#ifdef AUTO_KILL_NICK_FLOODING
/*
   IF AUTO killing nick flooders, I will not be k-lining
   nick flooders
*/
      if(*user_host == '~')
	suggest_kline(NO,user,host,NO,NO,"flooding");
      else
	suggest_kline(NO,user,host,NO,YES,"flooding");
#else
/*
   IF not AUTO killing nick flooders, might still be AUTO k-lining
   nick flooders
*/
      if(*user_host == '~')
	suggest_kline(YES,user,host,NO,NO,"flooding");
      else
	suggest_kline(YES,user,host,NO,YES,"flooding");
#endif
    }

  if(logging_fp)
    {
      (void)fclose(logging_fp);
      logging_fp = (FILE *)NULL;
    }
}

/*
cs_clones

inputs	- notice
output	- none
side effects
	connected opers are dcc'ed a suggested kline

	- Dianora 
*/
void cs_clones(server_notice)
char *server_notice;
{
  int identd = YES;
  char *user;
  char *host;
  char *p;
  char *nick_reported;
  char *user_host;
  char notice[MAX_BUFF];

  nick_reported = strtok(server_notice," ");
  if(nick_reported == (char *)NULL)return;

  user_host = strtok((char *)NULL," ");
  if(user_host == (char *)NULL)return;

/*
  Lets try and get it right folks... [user@host] or (user@host)
*/

  if(*user_host == '[')
    {
      user_host++;
      p = strrchr(user_host,']');
      if(p)
	*p = '\0';
    }
  else if(*user_host == '(')
    {
      user_host++;
      p = strrchr(user_host,')');
      if(p)
	*p = '\0';
    }

  (void)sprintf(notice,"CS clones user_host = [%s]\n", user_host);
  sendtoalldcc(notice,SEND_ALL_USERS);

  p = strdup(user_host);
  user = strtok(user_host,"@");
  if(*user == '~')
    {
      user++;
      identd = NO;
    }

  host = strtok((char*)NULL,"");

  initlog();

  if(logging_fp)
    fprintf(logging_fp,"CS clones = [%s]\n", user_host);
  suggest_kline(NO,user,host,NO,identd,"clones");

  if(logging_fp)
    {
      (void)fclose(logging_fp);
      logging_fp = (FILE *)NULL;
    }
  (void)free(p);
}

/*
check_nick_flood()

inputs	- rest of notice from server
output	- NONE
side effects

	- Dianora
*/

void check_nick_flood(server_notice)
char *server_notice;
{
  char *p;
  char *nick1;
  char *nick2;
  char *user_host;

#ifdef TELETYPE_NIH
  nick1 = strtok(server_notice," ");	/* This _should_ be nick1 */
  if(nick1 == (char *)NULL)return;
#else
  p = strtok(server_notice," ");	/* Throw away the "From" */
  if(p == (char *)NULL) return;

  if(CASECMP(p,"From") != 0)	/* This isn't an LT notice */
    {
      nick1 = p;	/* This _should_ be nick1 */

      user_host = strtok((char *)NULL," ");	/* (user@host) */
      if(user_host == (char *)NULL)return;
      if(*user_host == '(')
	user_host++;

      p = strrchr(user_host,')');
      if(p)
	*p = '\0';

      p = strtok((char *)NULL," ");
      if(p == (char *)NULL)
	return;

      if(strcmp(p,"now") != 0 )
	return;

      p = strtok((char *)NULL," ");
      if(p == (char *)NULL)
	return;

      if(strcmp(p,"known") != 0 )
	return;

      p = strtok((char *)NULL," ");
      if(p == (char *)NULL)
	return;

      if(strcmp(p,"as") != 0 )
	return;

      nick2 = strtok((char *)NULL," ");/* This _should_ be nick2 */
      if(nick2 == (char *)NULL)return;

      add_to_nick_change_table(user_host,nick2);
      updateuserhost(nick1,nick2,user_host);

      return;
    }

  nick1 = strtok((char *)NULL," ");	/* This _should_ be nick1 */
  if(nick1 == (char *)NULL)return;
#endif

  p = strtok((char *)NULL," ");		/* Throw away the "to" */
  if(p == (char *)NULL)return;

  nick2 = strtok((char *)NULL," ");	/* This _should_ be nick2 */
  if(nick2 == (char *)NULL)return;

  user_host = strtok((char *)NULL," ");	/* and this _should_ be the u@h part */
  if(user_host == (char *)NULL)return;

  if(*user_host == '[')
    user_host++;

  p = strrchr(user_host,']');
  if(p)
    *p = '\0';

/* N.B.
   hendrix's original code munges the user_host variable
   so, add_to_nick_change must occur BEFORE
   updateuserhost is called. grrrrrrrrrrrr
   I hate order dependencies of calls.. but there you are.
   This caused a bug in v0.1

   -Dianora
*/

  add_to_nick_change_table(user_host,nick2);
  updateuserhost(nick1,nick2,user_host);
}
/*
init_nick_change_table()
inputs - NONE
output - NONE
side effects -
	clears out the nick change table
	- Dianora
*/
void init_nick_change_table()
{
  int i;
  for(i = 0; i < NICK_CHANGE_TABLE_SIZE; i++)
    nick_changes[i] = (NICK_CHANGE_ENTRY *)NULL;
}
/*
init_link_look_table()
inputs - NONE
output - NONE
side effects -
	clears out the link looker change table
	This is very similar to the NICK_CHANGE code in many respects
	- Dianora
*/
void init_link_look_table()
{
  int i;
  for(i = 0; i < LINK_LOOK_TABLE_SIZE; i++)
    link_look[i].user_host = (char *)NULL;
}
/*
add_to_nick_change_table()
inputs	- user_host i.e. user@host
	- last_nick last nick change
output	- NONE
side effects -
	add to list of current nick changers
  What happens here is that a new nick is introduced for
an already existing user, or a possible nick flooder entry is made.
When a new possible nick flooder entry is made, the entry
is time stamped with its creation. Already present entries
get updated with the current time "last_nick_change"
  Expires of already existing nick entries was combined in this
loop and in the loop in report_nick_flooders() (i.e. no more
expire nick_table.. as in previous versions)
at the suggestion of Shadowfax, (mpearce@varner.com)
  What happens is that add_to_nick_change_table() is called
at the whim of nick change notices, i.e. not from a timer.
(similar applies to report_nick_flooders(), when expires are done)
  Every NICK_CHANGE_T1_TIME, (defaulted to 10 seconds in config.h)
one nick change count is decremented from the nick change count
for each user in list. Since this function is called asynchronously,
I have to calculate how many "time_ticks" i.e. how many 10
second intervals have passed by since the entry was last examined.
  If an entry is really stale, i.e. nothing has changed in it in
NICK_CHANGE_T2_TIME it is just completely thrown out.
This code is possibly, uneeded. I am paranoid. The idea here
is that if someone racks up a lot of nick changes in a brief
amount of time, but stop (i.e. get killed, flooded off, klined :-) )
Their entry doesn't persist longer than five minutes.
	- Dianora
*/
void add_to_nick_change_table(user_host,last_nick)
char *user_host;
char *last_nick;
{
  char *user;
  char *host;
  char notice[MAX_BUFF];
#ifdef AUTO_KILL_NICK_FLOODING
  char hold[MAX_BUFF];
#endif
  int i;
  int found_entry=NO;
  int found_empty_entry=INVALID;
  NICK_CHANGE_ENTRY *ncp;
  time_t current_time;
  struct tm *tmrec;
  current_time = time((time_t *)NULL);
  for(i = 0; i < NICK_CHANGE_TABLE_SIZE; i++)
    {
      if(ncp = nick_changes[i])
	{
	  time_t time_difference;
	  int time_ticks;
	  time_difference = current_time - ncp->last_nick_change;
	  /* is it stale ? */
	  if( time_difference >= NICK_CHANGE_T2_TIME )
	    {
	      (void)free(ncp->user_host);
	      (void)free(nick_changes[i]);
	      nick_changes[i] = (NICK_CHANGE_ENTRY *)NULL;
	    }
	  else
	    {
	      /* how many 10 second intervals do we have? */
	      time_ticks = time_difference / NICK_CHANGE_T1_TIME;
	      /* is it stale? */
	      if(time_ticks >= ncp->nick_change_count)
		{
		  (void)free(ncp->user_host);
		  (void)free(nick_changes[i]);
		  nick_changes[i] = (NICK_CHANGE_ENTRY *)NULL;
		}
	      else
		{
		  /* just decrement 10 second units of nick changes */
		  ncp->nick_change_count -= time_ticks;
		  if(CASECMP(ncp->user_host,user_host) == 0)
		    {
		      ncp->last_nick_change = current_time;
		      (void)strncpy(ncp->last_nick,last_nick,MAX_NICK);
		      ncp->nick_change_count++;
		      found_entry = YES;
		    }
		  /* now, check for a nick flooder */
	  
		  if((ncp->nick_change_count >= NICK_CHANGE_MAX_COUNT)
		     && !ncp->noticed)
		    {
		      tmrec = localtime(&ncp->last_nick_change);
		      initlog();
		      (void)sprintf(notice,
	    "nick flood %s (%s) %d in %d seconds (%2.2d:%2.2d:%2.2d)",
				    ncp->user_host,
				    ncp->last_nick,
				    ncp->nick_change_count,
				    ncp->last_nick_change-
				      ncp->first_nick_change,
				    tmrec->tm_hour,
				    tmrec->tm_min,
				    tmrec->tm_sec);
/*
   A little convoluted here *sigh*
   but if I want to auto kill nick flooders, I compile
   in a KILL message here, then I still want to suggest a kline to use
  I trust opers will not be nick flooding, but just in case
  I won't AUTO kill an oper..
  - Dianora
*/
#ifdef AUTO_KILL_NICK_FLOODING
		      if(autopilot)
			{
			  if ( (!okhost(user_host)) && (!isoper(user_host)))  
			    {
			      (void)sprintf(hold,
				      "KILL %s :nick flooding\n", last_nick );
			      toserv(hold);
			    }
			}
#endif
/*
   get ready to suggest a kline
*/
		      user = strtok(user_host,"@");
		      if(user == (char *)NULL)return; /* OOOPSIES! */
		      host = strtok((char *)NULL,"");
		      if(host == (char *)NULL)return; /* OOPSIES! */
		      
/*
   If auto killing nick flooders, suggest the kline but
   tell "suggest_kline()" NOT to kline the nick
*/
#ifdef AUTO_KILL_NICK_FLOODING
		      if(*user_host == '~')
			suggest_kline(NO,user,host,NO,NO,"flooding");
		      else
			suggest_kline(NO,user,host,NO,YES,"flooding");
#else
/*
   If NOT auto killing nick flooders, suggest the kline but
   tell "suggest_kline()" TO kline the nick IFF autopilot is ON
   AND AUTO_KLINE is on
*/
		      if(*user_host == '~')
			suggest_kline(YES,user,host,NO,NO,"flooding");
		      else
			suggest_kline(YES,user,host,NO,YES,"flooding");
#endif
		      if(logging_fp)
			(void)fprintf(logging_fp,
 "nick flood %s (%s) %d in %d seconds (%02d/%02d/%02d %2.2d:%2.2d:%2.2d)\n",
				    ncp->user_host,
				    ncp->last_nick,
				    ncp->nick_change_count,
				    ncp->last_nick_change-
				      ncp->first_nick_change,
				    tmrec->tm_mon+1,
				    tmrec->tm_mday,
				    tmrec->tm_year,
				    tmrec->tm_hour,
				    tmrec->tm_min,
				    tmrec->tm_sec);

		      ncp->noticed = YES;
		      sendtoalldcc(notice,SEND_ALL_USERS);
		      if(logging_fp)
			{
			  (void)fclose(logging_fp);
			  logging_fp = (FILE *)NULL;
			}
		    }
		}
	    }
	}
      else
	{
	  if( found_empty_entry == INVALID )
	    found_empty_entry = i;
	}
    }
  if(found_entry)
    return;
  ncp = (NICK_CHANGE_ENTRY *)malloc(sizeof(NICK_CHANGE_ENTRY));
  if( ncp == (NICK_CHANGE_ENTRY *)NULL)
    {
      prnt(connections[0].socket,"Ran out of memory in add_to_nick_change_table\n");
      sendtoalldcc("Ran out of memory in add_to_nick_change_table",SEND_ALL_USERS);
      gracefuldie();
    }
  if( (ncp->user_host = strdup(user_host)) == (char *)NULL)
    {
      (void)sprintf(notice,"Ran out of memory in add_to_nick_change_table");
      sendtoalldcc(notice,SEND_ALL_USERS);
      gracefuldie();
    }
/* If the table is full, don't worry about this nick change for now
   if this nick change is part of a flood, it will show up
   soon enough anyway... -db
*/
  if(found_empty_entry != INVALID)
    {
      ncp->first_nick_change = current_time;
      ncp->last_nick_change = current_time;
      ncp->nick_change_count = 1;
      ncp->noticed = NO;
      nick_changes[found_empty_entry] = ncp;
    }
}

void bot_reject(text)
char *text;
{
  char generic = 0;
  if (text) {
    if (!strncmp("bot:",text,4))
      ++generic;
    text = strchr(text,' ');
    if (!text)
      return;
    if (!generic) {
      text = strchr(text+1,' ');
      if (!text)
        return;
      }
    logfailure(text+1,1);
    }
}

/*
freehash()
inputs	- NONE
output	- NONE
side effects -
	clear all allocated memory hash tables
	- rewritten by Dianora
	memory leak found...
*/
void freehash()
{
  int i;
  HASHREC *hp;
  HASHREC *next_hp;
  HASHREC *dp;
  HASHREC *next_dp;
  for (i=0;i<HASHTABLESIZE;++i)
    {
      hp = hosttable[i];
      while(hp)
	{
	  next_hp = hp->collision;
	  printf("hp->info->link_count = %d\n", hp->info->link_count);
	  if(hp->info->link_count > 0)
	    hp->info->link_count--;
	  if(hp->info->link_count == 0)
	    {
	      printf("free(hp->info);\n");
	      (void)free(hp->info); 
	    }
	  (void)free(hp);
	  hp = next_hp;
	}
      hosttable[i] = (HASHREC *)NULL;
      dp = domaintable[i];
      while(dp)
	{
	  next_dp = dp->collision;
	  printf("dp->info->link_count = %d\n", dp->info->link_count);
	  if(dp->info->link_count > 0)
	    dp->info->link_count--;
	  if(dp->info->link_count == 0)
	    {
	      printf("free(dp->info);\n");
	      (void)free(dp->info);
	    }
	  (void)free(dp);
	  dp = next_dp;
	}
      domaintable[i] = (HASHREC *)NULL;
    }
}
/*
kline_add_report
inputs	- rest of notice from server
output	- NONE
side effects
     Reports klines when added.
>irc2.blackened.com NOTICE ToastMON :*** Notice -- ToastMON added K-Line for
[fake@another.test.kline]: remove me too by Toast 02/21/97
- Toast
*/
void kline_add_report(char *server_notice)
{
  FILE *fp_log;
  time_t current_time;
  struct tm *broken_up_time;
  char notice[MAX_BUFF];
  current_time = time((time_t *)NULL);
  broken_up_time = localtime(&current_time);
  
  (void)sprintf(notice,"*** %s", server_notice);
  sendtoalldcc(notice,SEND_ALL_USERS);
/* Probably don't need to log klines. --- Toast */
/* I think we need to log everything JIC - Dianora */
  if((fp_log = fopen(KILL_KLINE_LOG,"a")) != (FILE *)NULL)
    {
      fprintf(fp_log,"%02d/%02d/%d %02d:%02d %s\n",
	      (broken_up_time->tm_mon)+1,
	      broken_up_time->tm_mday,
	      broken_up_time->tm_year,
	      broken_up_time->tm_hour,
	      broken_up_time->tm_min,
	      server_notice);
      (void)fclose(fp_log);
    }
}
/*
kill_add_report
input		- server notice
output		- none
side effects	- local kills are logged (hopefully)
  Log only local kills though....
*** Notice -- Received KILL message for Newbie2. From Dianora_ Path:
  ts1-4.ottawa.net!Dianora_ (clone)
Thanks Thembones for bug fix (Brian Kraemer kraemer@u.washington.edu)
*/
void kill_add_report(char *server_notice)
{
char *path;
char *p, *from;
int number_of_bangs = 0;
  from = strstr(server_notice,"From ");		/* We do it like this */
  if(path == (char *)NULL)			/* because the nick killed */
    return;					/* Could match *From* */
  /* Now check the killer's name for a . */
  for (p = (from += 7); ((*p) && (*p != ' ')); p++)
    if (*p == '.')			/* Ignore Server kills */
      return;
  path = strstr(server_notice,"Path:");
  if (path == (char *)NULL)
    return;
  p = path;
  while(*p)
    {
      if(*p == '!')
        {
          number_of_bangs++;
          if( number_of_bangs > 1)return;
        }
      p++;
    }
  kline_add_report(server_notice);
}
#ifndef OTHERNET
/*
stats_notice
intputs		- notice
output		- none
side effects 	-
*/
void stats_notice(server_notice)
char *server_notice;
{
char *requested;
char *by;
char *nick;
  printf("stats_notice server_notice = [%s]\n",server_notice);
  if((*server_notice == 'g') || (*server_notice == 'G'))
    {
      server_notice += 2;
      requested = strtok(server_notice," ");
      if(requested == (char *)NULL)
	return;
      if(CASECMP(requested,"requested"))
	return;
      by = strtok((char *)NULL," ");
      if(by == (char *)NULL)
	return;
      if(CASECMP(by,"by"))
	return;
      nick = strtok((char *)NULL," ");
      notice(nick,"There are no G Lines on Efnet");
    }
#ifdef NOT_TH
  if((*server_notice == 'p') || (*server_notice == 'P'))
    {
      HASHREC *userptr;
      int i;
      char notice_buff[NOTICE_SIZE];
      char fulluh[MAX_HOST+MAX_DOMAIN];
      server_notice += 2;
      requested = strtok(server_notice," ");
      if(requested == (char *)NULL)
	return;
      if(CASECMP(requested,"requested"))
	return;
      by = strtok((char *)NULL," ");
      if(by == (char *)NULL)
	return;
      if(CASECMP(by,"by"))
	return;
      nick = strtok((char *)NULL," ");
      for( i=0; i < HASHTABLESIZE; ++i)
	{
	  userptr = domaintable[i];
	  while(userptr)
	    {
	      (void)sprintf(notice_buff,"seen nick %s isoper %d",
			    userptr->info->nick,userptr->info->isoper);
	      if(userptr->info->isoper)
		{
		  (void)sprintf(fulluh,
				"%s@%s",
				userptr->info->user,userptr->info->host);
		  (void)sprintf(notice_buff,"Oper %s (%s)",
				userptr->info->nick,fulluh);
		  notice(nick,notice_buff);
		}
	      userptr = userptr->collision;
	    }
	}
    }
#endif
#ifdef NOT_A_HUB
  if((*server_notice == 'h') || (*server_notice == 'H'))
    {
      server_notice += 2;
      requested = strtok(server_notice," ");
      if(requested == (char *)NULL)
	return;
      if(CASECMP(requested,"requested"))
	return;
      by = strtok((char *)NULL," ");
      if(by == (char *)NULL)
	return;
      if(CASECMP(by,"by"))
	return;
      nick = strtok((char *)NULL," ");
      notice(nick,"No, we aren't a HUB, No we cannot link your server");
    }
#endif
}
#endif

