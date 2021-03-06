/************************************************************
* MrsBot by Hendrix <jimi@texas.net>                        *
* wild.c                                                    *
*   Contains my wildcard matching routines for *'s and ?'s  *
* Includes routines:                                        *
*   int wldcmp                                              *
*   int wldwld                                              *
*   int bancmp                                              *
*   int starcmp                                             *
************************************************************/

#include <ctype.h>
#include <string.h>

int starcmp();

static char *version="$Id: wild.c,v 1.1 1996/12/09 02:39:18 db Exp $";

/*
** wldcmp()
**   My very own wildcard matching routine for one wildcarded string
**    and one non-wildcarded string.  Wildcards ? and * recognized.
**   Parameters:
**     wildexp - A string possibly containing wildcard expressions
**     regstr - A string DEFINITELY containing NO wildcard expressions
**   Returns:
**     Like strcmp()... Returns 1 on no match and 0 if they do match.
**   PDL:
**     Go thru each character.  Match is still intact if a * is found,
**     a ? is found, or both characters in the two strings match (excluding
**     case).  If a * is encountered, recursively call wldcmp() with the
**     remainder of the wildcarded string and all possible remaining pieces
**     of the regular string.  If any of those match, then the whole compare
**     is a match.  If the character encountered is a null, then we have
**     reached end of string and the match is a success.  However, an effort
**     to match a ? with the end of the string is a failure.
*/
int wldcmp (wildexp,regstr)
char *wildexp,*regstr;
{
  while (*wildexp == '*' || tolower(*wildexp) == tolower(*regstr) || *wildexp == '?')
    if (*wildexp == '*') {
      /* This will stop idiots who ban *?*?*?*?*?*?* from crashing the bot */
      while (*(++wildexp) == '*' || (*wildexp == '?' && *(wildexp+1) == '*'));
      if (*wildexp) {
        while (*regstr)
          if (!wldcmp(wildexp,regstr++))
            return 0;
        return 1;
        }
      else
        return 0;
      }
    else if (!*wildexp)
      return 0;
    else if (!*regstr) /* Only true if nothing to match ? with */
      return 1;
    else {
      ++wildexp;
      ++regstr;
      }
  return 1;
}

/*
** wldwld()
**   My very own wildcard matching routine for TWO wildcarded strings.
**    Wildcards ? and * recognized.  Note: this is MUCH less efficient
**    than wldcmp(), so use it only where necessary.  There are very few
**    cases that you should truly need to compare 2 wildcarded strings.
**   Parameters:
**     wild1 - A string hopefully containing wildcard expressions
**     wild2 - A string hopefully containing wildcard expressions
**   Returns:
**     Like strcmp()... Returns 1 on no match and 0 if they do match.
**   PDL:
**     As in wldcmp() but check for *'s and ?'s in both strings.  If a
**     * is found in EITHER string, recursively call wldwld() with the
**     remainder of the other string.  You can see how this can get into
**     deep recursion, so use wldcmp() when you can.
*/
int wldwld (wild1,wild2)
char *wild1,*wild2;
{
  while (tolower(*wild1) == tolower(*wild2) || *wild1 == '*' || *wild2 == '*' ||
         *wild1 == '?' || *wild2 == '?') {
    if (*wild1 == '*') {
      /* This will stop idiots who ban *?*?*?*?*?*?* from crashing the bot */
      while (*(++wild1) == '*' || (*wild1 == '?' && *(wild1+1) == '*'));
      if (*wild1) {
        while (*wild2)
          if (!wldwld(wild1,wild2++))
            return 0;
        return 1;
        }
      else
        return 0;
      }
    else if (*wild2 == '*') {
      /* This will stop idiots who ban *?*?*?*?*?*?* from crashing the bot */
      while (*(++wild2) == '*' || (*wild2 == '?' && *(wild2+1) == '*'));
      if (*wild2) {
        while (*wild1)
          if (!wldwld(wild1++,wild2))
            return 0;
        return 1;
        }
      else
        return 0;
      }
    else if (!*wild1)
      return (*wild2 == '?');
    else if (!*wild2)   /* wild1 must be a ? in this case */
      return 1;
    else {
      ++wild1;
      ++wild2;
      }
    }
    return 1;
}

/*
** bancmp()
**   Upgraded method for comparing bans as two wildcarded strings.  It's
**    even more inefficient than wldwld() but it removes some bad
**    tendencies in wldwld, such as matching *!jnc@*.ufl to *!*@*blah*
**   Parameters:
**     banmask - A USERHOST ONLY part of a banmask coming in
**     protmask - A USERHOST ONLY part of a protected user mask
**   Returns:
**     Like strcmp()... Returns 1 on no match and 0 if they do match.
**   PDL:
**     Locate the username part of both bans by finding the @.  If we
**     can't find a @, something is wrong so just compare the whole
**     strings.  If we can find one, compare the username parts to each
**     other.  If they match, compare the hostname parts to each other.
**     The whole match is a success only if both parts match.  In any
**     case, make sure to replace the @'s back into the string.
*/
int bancmp(banmask,protmask)
char *banmask,*protmask;
{
  char *tmpptr,*tmpptr2;

  tmpptr = strchr(banmask,'@');
  tmpptr2 = strchr(protmask,'@');
  if (tmpptr && tmpptr2) {
    *tmpptr = *tmpptr2 = '\0';
    if (!starcmp(banmask,protmask)) {
      *(tmpptr++) = *(tmpptr2++) = '@';
      return starcmp(tmpptr,tmpptr2);
      }
    else {
      *(tmpptr++) = *(tmpptr2++) = '@';
      return 1;
      }
    }
  else
    return starcmp(banmask,protmask);
}

/*
** starcmp()
**   A kludge to get around wldwld()'s tendency to match *blah* to ANY
**    string with a wildcard in it.
**   Parameters:
**     banitem - A part of or a whole banmask
**     protitem - A part of or a whoel protected user mask
**   Returns:
**     Like strcmp()... Returns 1 on no match and 0 if they do match.
**   PDL:
**     Manually check for a string of the form *blah*.  If it is not of
**     that format, wldwld() will work fine, so call it.  If it does
**     start and end with a *, instead check that ALL non-wild characters
**     in the banmask are contained in the protected user mask, and that
**     they are in the correct order.  Note that we need to do two
**     checks, one with uppercase, and one with lowercase.  If all chars
**     are found in the proper order, it is considered a match.  Note
**     that this is not perfect, but it's much better than what wldwld()
**     would do with it.
*/
int starcmp(banitem,protitem)
char *banitem,*protitem;
{
  char *tmpptr,*tmpptr2;

  if ((*banitem == '*' && banitem[strlen(banitem)-1] == '*') ||
      (*banitem == '*' && protitem[strlen(protitem)-1] == '*'))
    ++banitem;
  else if (*protitem == '*' && banitem[strlen(banitem)-1] == '*')
    ++protitem;
  else
    return wldwld(banitem,protitem);

  while (*banitem) {
    if (*banitem != '*' && *banitem != '?') {
      tmpptr = strchr(protitem,tolower(*banitem));
      if (!tmpptr)
        tmpptr = strchr(protitem,toupper(*banitem));
      if (tmpptr)
        protitem = tmpptr;
      else
        return 1;
      }
    ++banitem;
    }
  return 0;
}
