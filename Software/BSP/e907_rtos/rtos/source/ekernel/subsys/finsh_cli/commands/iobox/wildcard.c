#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <finsh.h>
#include <unistd.h>
#include <string.h>

/* Defines and Macros */
#define MATCH      1
#define NOT_MATCH  0

#define MATCH_CHAR(c1,c2,ignore_case)  ( (c1==c2) || ((ignore_case==1) &&(tolower(c1)==tolower(c2))))

int wildcard_check(char *dir)
{
    if (dir == NULL)
    {
        return 0;
    }

    return strchr(dir, '*') == NULL ? 0 : 1;
}

int wild_char_match(char *src, char *pattern, int ignore_case)
{
    int result;

    while (*src)
    {
        if (*pattern == '*')
        {
            while ((*pattern == '*') || (*pattern == '?'))
            {
                pattern++;
            }

            if (!*pattern)
            {
                return MATCH;
            }

            while (*src && (!MATCH_CHAR(*src, *pattern, ignore_case)))
            {
                src++;
            }

            if (!*src)
            {
                return NOT_MATCH;
            }

            result = wild_char_match(src, pattern, ignore_case);
            while ((!result) && (*(src + 1)) && MATCH_CHAR(*(src + 1), *pattern, ignore_case))
            {
                result = wild_char_match(++src, pattern, ignore_case);
            }

            return result;

        }
        else
        {
            if (MATCH_CHAR(*src, *pattern, ignore_case) || ('?' == *pattern))
            {
                return wild_char_match(++src, ++pattern, ignore_case);
            }
            else
            {
                return NOT_MATCH;
            }
        }
    }

    if (*pattern)
    {
        if ((*pattern == '*') && (*(pattern + 1) == 0))
        {
            return MATCH;
        }
        else
        {
            return NOT_MATCH;
        }
    }
    else
    {
        return MATCH;
    }
}
