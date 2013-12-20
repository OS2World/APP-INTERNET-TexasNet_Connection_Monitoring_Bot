/*
  Some changes made by Dianora

  - added clear_userlist()
  - make it actually use MAXUSERS defined in config.h
  - added config file for bot nick, channel, server, port etc.
  - rudimentary remote tcm linking added
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "config.h"
#include "tcm.h"

#ifdef __EMX__
#define CASECMP	stricmp
#else
#define CASECMP	strcasecmp
#endif


static char *version="$Id: userlist.c,v 1.16 1997/05/26 16:14:07 db Exp db $";

#ifdef NEXT
char *strdup(char *);
#endif

AUTH_FILE_ENTRY userlist[MAXUSERS];
BOT_FILE_ENTRY botlist[MAXBOTS];

char *hostlist[MAXHOSTS];

int  user_list_index;

char username_config[MAX_CONFIG];
char virtual_host_config[MAX_CONFIG];
char oper_nick_config[MAX_CONFIG];
char oper_pass_config[MAX_CONFIG];
char server_config[MAX_CONFIG];
char server_name[MAX_CONFIG];
char server_port[MAX_CONFIG];
char port_config[MAX_CONFIG];
char ircname_config[MAX_CONFIG];
char email_config[MAX_CONFIG];
int  tcm_port;

extern char defchannel[];
extern char dfltnick[];
extern CONNECTION connections[];

/*
load_config_file

inputs		- NONE
output		- NONE
side effects	- configuration items needed for tcm are loaded
		  from CONFIG_FILE
		  rudimentary error checking in config file are reported
		  and if any found, tcm is terminated...
*/

void load_config_file(file_name)
char *file_name;
{
  FILE *fp;
  char line[MAX_BUFF];
  char *key;
  char *value;
  char *p;
  int error_in_config;

  error_in_config = NO;

  username_config[0] = '\0';
  virtual_host_config[0] = '\0';
  oper_nick_config[0] = '\0';
  oper_pass_config[0] = '\0';
  server_config[0] = '\0';
  ircname_config[0] = '\0';
  defchannel[0] = '\0';
  dfltnick[0] = '\0';
  email_config[0] = '\0';

  if( (fp = fopen(file_name,"r")) == (FILE *)NULL)
    {
      fprintf(stderr,"GACK! I don't know who I am or anything!!\n");
      fprintf(stderr,"tcm-dianora can't find %s file\n",file_name);
      exit(1);
    }

  tcm_port = TCM_PORT;

  while(fgets(line, MAX_BUFF-1,fp))
    {
      if(line[0] == '#')
	continue;

      key = strtok(line,":");
      if(key == (char *)NULL)
	continue;
      value = strtok((char *)NULL,"\r\n");
      if(value == (char *)NULL)
	continue;

      switch(*key)
	{
	case 'e':case 'E':
          strncpy(email_config,value,MAX_CONFIG-1);
          (void)printf("email address = [%s]\n", value );
	break;

	case 'o':case 'O':
	  {
	    char *oper_nick;
	    char *oper_pass;

	    oper_nick = strtok(value,":\r\n");
	    oper_pass = strtok((char *)NULL,":\r\n");
	    if(oper_nick == (char *)NULL)
	      continue;
	    if(oper_pass == (char *)NULL)
	      continue;

	    printf("oper nick = [%s]\n", oper_nick );
	    printf("oper pass = [%s]\n", oper_pass );
	    strncpy(oper_nick_config,oper_nick,MAX_NICK);
	    strncpy(oper_pass_config,oper_pass,MAX_CONFIG-1);
	  }
	  break;

	case 'p':case 'P':
	  tcm_port = atoi(value);
	  break;

	case 'u':case 'U':
	  printf("user name = [%s]\n", value );
	  strncpy(username_config,value,MAX_CONFIG-1);
	  break;

	case 'v':case 'V':
	  printf("virtual host name = [%s]\n", value );
	  strncpy(virtual_host_config,value,MAX_CONFIG-1);
	  break;

	case 's':case 'S':
	  {
	    printf("server = [%s]\n", value);
	    strncpy(server_config,value,MAX_CONFIG-1);
	  }
	  break;

	case 'n':case 'N':
	  printf("nick for bot = [%s]\n", value );
	  strncpy(dfltnick,value,MAX_NICK-1);
	  break;

	case 'i':case 'I':
	  printf("IRCNAME = [%s]\n", value );
	  strncpy(ircname_config,value,MAX_CONFIG-1);
	  break;

	case 'c':case 'C':
	  printf("Channel = [%s]\n", value);
	  strncpy(defchannel,value,MAX_CHANNEL-1);
	  break;

	default:
	  break;
	}
    }

  if(username_config[0] == '\0')
    {
      fprintf(stderr,"I need a username (U:) in %s\n",CONFIG_FILE);
      error_in_config = YES;
    }

  if(oper_nick_config[0] == '\0')
    {
      fprintf(stderr,"I need an opernick (O:) in %s\n",CONFIG_FILE);
      error_in_config = YES;
    }

  if(oper_pass_config[0] == '\0')
    {
      fprintf(stderr,"I need an operpass (O:) in %s\n",CONFIG_FILE);
      error_in_config = YES;
    }

  if(server_config[0] == '\0')
    {
      fprintf(stderr,"I need a server (S:) in %s\n",CONFIG_FILE);
      error_in_config = YES;
    }

  strncpy(server_name,server_config,MAX_CONFIG);
  p = strchr(server_name,':');
  if(p)
    {
      *p = '\0';
      p++;
      strncpy(server_port,p,MAX_CONFIG);
    }

  if(ircname_config[0] == '\0')
    {
      fprintf(stderr,"I need an ircname (I:) in %s\n",CONFIG_FILE);
      error_in_config = YES;
    }

  if(dfltnick[0] == '\0')
    {
      fprintf(stderr,"I need a nick (N:) in %s\n", CONFIG_FILE);
      error_in_config = YES;
    }

  if(error_in_config)
    exit(1);
}

/*
load_userlist

inputs		- NONE
output		- NONE
side effects	- first part of oper list is loaded from file
*/

void load_userlist()
{
  FILE *userfile;
  char line[MAX_BUFF];

  if ( (userfile = fopen(USERLIST_FILE,"r")) == (FILE *)NULL )
    {
      printf("Cannot read userlist.load\n");
      /* As I am going to read the rest from a stats O , this is ok */
      return;
    }

/*

  userlist.load looks like now
  u@h:nick:password:ok o for opers, k for remote kline

- or you can use a number 1 for opers, 2 for remote kline -
i.e.
  u@h:nick:password:3 for opers

  h:nick:password:okb o for opers, 2 for remote kline, b for bot

- 4 is for bot -
i.e.
  h:nick:password:7   for remote tcm's

*/

  while (fgets(line, MAX_BUFF-1, userfile) )
    {
      char *userathost;
      char *usernick;
      char *password;
      char *type;

      line[strlen(line)-1] = 0;

      if(line[0] == '#')
	continue;

      userathost = strtok(line,":");
      if( userathost == (char *)NULL)
	continue;

#ifdef DEBUGV4
      printf("userlist.c userathost = %s\n", userathost );
#endif

      userlist[user_list_index].usernick = (char *)NULL;
      userlist[user_list_index].password = (char *)NULL;

      userlist[user_list_index].userathost = strdup(userathost);

      if(userlist[user_list_index].userathost == (char *)NULL)
	{
	  fprintf(stderr,"memory allocation error in load_userlist()\n");
	  exit(1);
	}

      usernick = strtok((char *)NULL,":");

      if(usernick)
	{
	  userlist[user_list_index].usernick = strdup(usernick);
	  if(userlist[user_list_index].usernick == (char *)NULL)
	    {
	      fprintf(stderr,"memory allocation error in load_userlist()\n");
	      exit(1);
	    }
	}

      password = strtok((char *)NULL,":");

      if(password)
	{
	  userlist[user_list_index].password = strdup(password);
	  if(userlist[user_list_index].password == (char *)NULL)
	    {
	      fprintf(stderr,"memory allocation error in load_userlist()\n");
	      exit(1);
	    }
	}

      type = strtok((char *)NULL,":");

      if(type)
	{
	  int type_int;
	  char *p;
	  p = type;

	  type_int = 0;

	  /*
	    If you really want to use a decimal instead of the keys...
	   */

	  if(isdigit(*p))
	     {
	       type_int = atoi(p);
	     }
	  else
	    {
	      while(*p)
		{
		  switch(*p)
		    {
		    case 'o':
		    case 'O':
		      type_int |= TYPE_OPER;
		      break;

		    case 'g':
		    case 'G':
		      type_int |= (TYPE_GLINE|TYPE_REGISTERED);
		      break;

		    case 'k':
		    case 'K':
		      type_int |= TYPE_REGISTERED;
		      break;

		    case 'b':
		    case 'B':
		      type_int |= TYPE_BOT;
		      break;

		    default:
		      break;
		    }
		  p++;
		}
	    }
	  userlist[user_list_index].type = type_int;
#ifdef DEBUGV4
	  printf("userlist.c type = %s type_int = %d\n", type, type_int );
#endif
	}

      user_list_index++;
      if( user_list_index == (MAXUSERS - 1))
	break;
    }
  (void)fclose(userfile);

  userlist[user_list_index].userathost = (char *)NULL;
  userlist[user_list_index].usernick = (char *)NULL;
  userlist[user_list_index].password = (char *)NULL;
  userlist[user_list_index].type = 0;
}

/*
clear_userlist

input	- NONE
output	- NONE
side effects -
	  user list is cleared out prepatory to a userlist reload

	- Dianora
*/

void clear_userlist()
{
  int cnt;

  user_list_index = 0;

  for(cnt = 0; cnt < MAXUSERS; cnt++)
    {
      if(userlist[cnt].userathost == (char *)NULL)
	return;
      else
	{
	  (void)free(userlist[cnt].userathost);
	  userlist[cnt].userathost = (char *)NULL;
	  
	  if(userlist[cnt].usernick)
	    {
	      (void)free(userlist[cnt].usernick);
	      userlist[cnt].usernick = (char *)NULL;
	    }

	  if(userlist[cnt].password)
	    {
	      (void)free(userlist[cnt].password);
	      userlist[cnt].password = (char *)NULL;
	    }
	  userlist[cnt].type = 0;
	}
    }

  for(cnt = 0; cnt < MAXBOTS; cnt++)
    {
      if(botlist[cnt].userathost == (char *)NULL)
	return;
      else
	{
	  (void)free(botlist[cnt].userathost);
	  botlist[cnt].userathost = (char *)NULL;

	  if(botlist[cnt].theirnick)
	    {
	      (void)free(botlist[cnt].theirnick);
	      botlist[cnt].theirnick = (char *)NULL;
	    }

	  if(botlist[cnt].password)
	    {
	      (void)free(botlist[cnt].password);
	      botlist[cnt].password = (char *)NULL;
	    }
	  botlist[cnt].port = 0;
	}
    }
}

/*
load_botlist

inputs		- NONE
output		- NONE
side effects	- bot list is loaded from botlist.load file
*/

void load_botlist()
{
  FILE *botfile;
  int  bot_list_index;
  char line[MAX_BUFF];

  if ( (botfile = fopen(BOTLIST_FILE,"r")) == (FILE *)NULL)
    {
      printf("Cannot read botlist.load\n");
      return;
    }

/*

  botlist.load looks like now
  host:theirnick:password:port   for remote tcm's
*/

  bot_list_index = 0;

  while (fgets(line, MAX_BUFF-1, botfile) )
    {
      char *userathost;
      char *theirnick;
      char *password;
      char *port_string;
      int  port;

      line[strlen(line)-1] = 0;
      if(line[0] == '#')
	continue;

      userathost = strtok(line,":");
      if( userathost == (char *)NULL)
	continue;

#ifdef DEBUGV4
      printf("userlist.c loadbotlist userathost = %s\n", userathost );
#endif

      botlist[bot_list_index].theirnick = (char *)NULL;
      botlist[bot_list_index].password = (char *)NULL;

      botlist[bot_list_index].userathost = strdup(userathost);

      if(botlist[bot_list_index].userathost == (char *)NULL)
	{
	  fprintf(stderr,"memory allocation error in load_botlist()\n");
	  exit(1);
	}

      theirnick = strtok((char *)NULL,":");

      if(theirnick)
	{
	  botlist[bot_list_index].theirnick = strdup(theirnick);
	  if(botlist[bot_list_index].theirnick == (char *)NULL)
	    {
	      fprintf(stderr,"memory allocation error in load_botlist()\n");
	      exit(1);
	    }
	}

      password = strtok((char *)NULL,":");

      if(password)
	{
	  botlist[bot_list_index].password = strdup(password);
	  if(botlist[bot_list_index].password == (char *)NULL)
	    {
	      fprintf(stderr,"memory allocation error in load_botlist()\n");
	      exit(1);
	    }
	}

      port_string = strtok((char *)NULL,":");
      port = TCM_PORT;

      if(port_string)
	{
	  if(isdigit(*port_string))
	     {
	       port = atoi(port_string);
	     }

	}

      botlist[bot_list_index].port = port;
#ifdef DEBUGV4
      printf("userlist.c port = %d\n", port );
#endif

      bot_list_index++;
      if( bot_list_index == (MAXBOTS - 1))
	break;
    }
  (void)fclose(botfile);

  botlist[bot_list_index].userathost = (char *)NULL;
  botlist[bot_list_index].theirnick = (char *)NULL;
  botlist[bot_list_index].password = (char *)NULL;
  botlist[bot_list_index].port = 0;
}

/*
init_userlist

input	- NONE
output	- NONE
side effects -
	  user list is cleared 

	- Dianora
*/

void init_userlist()
{
  int cnt;
  user_list_index = 0;

  for(cnt = 0; cnt < MAXUSERS; cnt++)
    {
      userlist[cnt].userathost = (char *)NULL;
      userlist[cnt].usernick = (char *)NULL;
      userlist[cnt].password = (char *)NULL;
      userlist[cnt].type = 0;
    }

    for(cnt = 0; cnt < MAXBOTS; cnt++)
    {
      botlist[cnt].userathost = (char *)NULL;
      botlist[cnt].theirnick = (char *)NULL;
      botlist[cnt].password = (char *)NULL;
      botlist[cnt].port = 0;
    }
}

/*
isoper()

inputs		- user@host name
output		- TYPE_OPER if oper, 0 if not
side effects	- NONE
*/

int isoper(userhost)
char *userhost;
{
  int i = -1;

  while (userlist[++i].userathost)
    {
      if((userlist[i].type & TYPE_BOT) == 0)
	if (!wldcmp(userlist[i].userathost,userhost))
	  return(TYPE_OPER);
    }
  return(0);
}

/*
islegal_pass()

inputs		- user@host password
output		- YES if legal NO if not
side effects	- NONE
*/

int islegal_pass(userhost,password)
char *userhost;
char *password;
{
  int i = 0;

  while (userlist[i].userathost)
    {
      if(!(userlist[i].type & TYPE_BOT))
	{
	  if (wldcmp(userlist[i].userathost,userhost) == 0)
	    {
	      if(userlist[i].password)
		{
		  if(strcmp(userlist[i].password,password) == 0)
		    return(userlist[i].type);
		  else
		    return(0);
		}
	    }
	}
      i++;
    }
  return(0);
}

/*
islinkedbot()

inputs		- connection number, botname, password
output		- privs if linkedbot 0 if not
side effects	- NONE
*/

int islinkedbot(connnum,botname,password)
int connnum;
char *botname;
char *password;
{
  int i = 0;
  int j;

  while ((userlist[i].userathost) && (i < (MAXDCCCONNS -1)) )
    {
      if ( (userlist[i].type & TYPE_BOT) && 
	   (wldcmp(userlist[i].userathost,
		       connections[connnum].userhost) == 0) )
	{
	  if(userlist[i].usernick == (char *)NULL)
	    {
	      continue;
	    }

	  if(userlist[i].password == (char *)NULL)
	    continue;

	  if( (CASECMP(botname,userlist[i].usernick) == 0 ) &&
	     (strcmp(password,userlist[i].password) == 0 ) )
	    {
	      /* 
		 Close any other duplicate connections using same botnick
	       */

	      for(j = 0; j < MAXDCCCONNS+1; j++)
		{
		  if(connections[j].userhost)
		     {
		       if(!CASECMP(connections[j].nick,botname))
			 {
			   closeconn(j);
			 }
		     }
		}

	      strcpy(connections[connnum].nick,botname);
	      return(userlist[i].type);
	    }
	}
      i++;
    }
  return(0);
}

/*
  I've added Phishers auto kline code as an option - Dianora
*/

/* Added Allowable hostlist for autokline. Monitor checks allowed hosts
   before adding kline, if it matches, it just returns dianoras
   suggested kline.  Phisher dkemp@frontiernet.net


okhost() is now called on nick floods etc. to mark whether a user
should be reported or not.. hence its not just for AUTO_KLINE now

*/

void load_hostlist()
{
  FILE *hostfile;
  char line[MAX_BUFF];
  int cnt = 0;

  if( (hostfile = fopen("hostlist.load","r")) == (FILE *)NULL )
    {
      printf("Cannot read hostlist.load\n");
      exit(1);
    }
    
  while (fgets(line, MAX_BUFF-1, hostfile) != NULL)
    {
      line[strlen(line)-1] = 0;

      hostlist[cnt] = strdup(line);
      if(hostlist[cnt] == (char *)NULL)
        {
          fprintf(stderr,"memory allocation error in load_hostlist()\n");
          exit(1);
        }
      cnt++;
    }
  fclose(hostfile);
  hostlist[cnt] = (char *)NULL;
}

/* Checks for ok hosts to block auto-kline - Phisher */

int okhost(userhost)
char *userhost;
{
  int i = -1;

  while (hostlist[++i])
    if (!wldcmp(hostlist[i],userhost))
      return(YES);
  return(NO);
}

