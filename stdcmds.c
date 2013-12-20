/************************************************************
* MrsBot by Hendrix <jimi@texas.net>                        *
* stdcmds.c                                                 *
*   Simple interfaces to send out most types of IRC messages*
*   Contains interface to msg an entire file to a user      *
* Includes routines:                                        *
*   void toserv                                             *
*   void op                                                 *
*   void kick                                               *
*   void who                                                *
*   void whois                                              *
*   void names                                              *
*   void join                                               *
*   void leave                                              *
*   void notice                                             *
*   void msg                                                *
*   void say                                                *
*   void action                                             *
*   void newnick                                            *
*   void invite                                             *
*   void get_userhost                                       *
*   void msg                                                *
************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include "tcm.h"
#include "config.h"

static char *version="$Id: stdcmds.c,v 1.4 1997/04/25 20:53:13 db Exp $";

char buff[BUFFERSIZE];           /* Used to hold msgs to server */

extern FILE *outfile;      /* Debug output file handle */

/* The following are primitives that send messages to the server to perform
   certain things.  The names are quite self explanatory, so I am not going
   to document each.  By no means are they complex. */
void op(chan,nick)
char *chan,*nick;
{
  sprintf(buff,"MODE %s +oooo %s\n", chan, nick);
  toserv(buff);
}

void kick(chan,nick,comment)
char *chan,*nick,*comment;
{
  sprintf(buff,"KICK %s %s :%s\n", chan, nick, comment);
  toserv(buff);
}

void who(nick)
char *nick;
{
  sprintf(buff,"WHO %s\n", nick);
  toserv(buff);
}

void whois(nick)
char *nick;
{
  sprintf(buff,"WHOIS %s\n", nick);
  toserv(buff);
}

void names(chan)
char *chan;
{
  sprintf(buff,"NAMES %s\n", chan);
  toserv(buff);
}

void join(chan)
char *chan;
{
  sprintf(buff,"JOIN %s\n", chan);
  toserv(buff);
}

void leave(chan)
char *chan;
{
  sprintf(buff,"PART %s\n", chan);
  toserv(buff);
}

void notice(nick,msg)
char *nick, *msg;
{
  sprintf(buff,"NOTICE %s :%s\n", nick, msg);
  toserv(buff);
}

void msg(nick,msg)
char *nick, *msg;
{
  sprintf(buff,"PRIVMSG %s :%s\n", nick, msg);
  toserv(buff);
}

void say(chan,msg)
char *chan,*msg;
{
  sprintf(buff,"PRIVMSG %s :%s\n", chan, msg);
  toserv(buff);
}

void action(chan,msg)
char *chan,*msg;
{
  sprintf(buff,"PRIVMSG %s :\001ACTION %s\001\n", chan, msg);
  toserv(buff);
}

void newnick(nick)
char *nick;
{
  sprintf(buff,"NICK %s\n", nick);
  toserv(buff);
}

void invite(nick,chan)
char *nick,*chan;
{
  sprintf(buff,"INVITE %s %s\n", nick, chan);
  toserv(buff);
}
