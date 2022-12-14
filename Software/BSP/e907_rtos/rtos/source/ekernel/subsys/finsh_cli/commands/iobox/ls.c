#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <aw_dirent.h>

#include <rtthread.h>
#include "wildcard.h"

#define LONGLIST    (1 << 0)
#define SZ_KB       (1 << 1)
#define NEW_LINE    (1 << 2)

#ifdef DFS_USING_WORKDIR
extern char working_directory[];
#endif

static void show_help(void)
{
    printf("Usage: ls [-l] [-h] [-k] <file>...\n");
    printf("  -l: use long list\n");
    printf("  -h: show this help message and exit");
    printf("  -k: use 1024-byte blocks");
}

static const char *relative_path(const char *path, const char *dir)
{
    const char *p = path + strlen(dir);

    while (*p == '/')
    {
        p++;
    }
    return p < path + strlen(path) ? p : NULL;
}

static void ls_show(const char *file, int flags, struct stat s)
{
    struct tm tm;

    if (!(flags & LONGLIST))
    {
        if (flags & NEW_LINE)
        {
            printf("%s\n", file);
        }
        else
        {
            printf("%s ", file);
        }
        return;
    }

    switch (s.st_mode & S_IFMT)
    {
        case S_IFBLK:
            printf("%c", 'b');
            break;
        case S_IFCHR:
            printf("%c", 'c');
            break;
        case S_IFDIR:
            printf("%c", 'd');
            break;
        case S_IFLNK:
            printf("%c", 'l');
            break;
        case S_IFREG:
            printf("%c", '-');
            break;
        default:
            printf("%c", '?');
            break;
    }

    localtime_r(&s.st_mtime, &tm);

    printf("%c%c%c%c%c%c%c%c%c root root %04d-%02d-%02d %02d:%02d:%02d %010lu%sB %12s\n",
           s.st_mode & S_IRUSR ? 'r' : '-',
           s.st_mode & S_IWUSR ? 'w' : '-',
           s.st_mode & S_IXUSR ? 'x' : '-',
           s.st_mode & S_IRGRP ? 'r' : '-',
           s.st_mode & S_IWGRP ? 'w' : '-',
           s.st_mode & S_IXGRP ? 'x' : '-',
           s.st_mode & S_IROTH ? 'r' : '-',
           s.st_mode & S_IWOTH ? 'w' : '-',
           s.st_mode & S_IXOTH ? 'x' : '-',
           tm.tm_year + 1900,
           tm.tm_mon + 1,
           tm.tm_mday,
           tm.tm_hour,
           tm.tm_min,
           tm.tm_sec,
           flags & SZ_KB ? s.st_size / 1024 : s.st_size,
           flags & SZ_KB ? "K" : "", file);
}

static void dir_list_wildcard(char *path, int flags)
{
    char *dir_path;
    char *pattern;
    struct dirent *entry;
    struct stat s;
    DIR *dir;
    int rc;
    int cnt = 1;

    if (!path)
    {
        return;
    }

#ifdef DFS_USING_WORKDIR
    dir_path = (char *)&working_directory;
#else
    dir_path = "/";
#endif

    dir = opendir(dir_path);
    if (dir == NULL)
    {
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        rc = wild_char_match(entry->d_name, path, 0);
        if (rc == 1)
        {
            char fpath[128];
            int len;

            memset(fpath, 0, 128);
            len = strlen(dir_path);
#ifdef COMP_SPIFFS
            /*
            * fix for spiffs
            * readdir/opendir on spiffs will traverse all
            * files in all directories
            */
            snprintf(fpath, 128, "/data/%s", entry->d_name);
            if (strncmp(dir, fpath, len))
            {
                continue;
            }
#else
            if (dir_path[len] == '/')
            {
                snprintf(fpath, 128, "%s%s", dir_path, entry->d_name);
            }
            else
            {
                snprintf(fpath, 128, "%s/%s", dir_path, entry->d_name);
            }
#endif
            memset(&s, 0, sizeof(struct stat));
            if (stat(fpath, &s))
            {
                printf("stat %s failed - %s\n", fpath, strerror(errno));
                continue;
            }

            if (!(cnt % 5))
            {
                flags |= NEW_LINE;
            }
            else
            {
                flags &= ~NEW_LINE;
            }
            cnt++;

            ls_show(relative_path(fpath, dir_path), flags, s);
        }
        else if (rc == 0)
        {
            continue ;
        }
        else
        {
            printf("error %d file=%s\n", rc, entry->d_name);
        }
    }

    closedir(dir);
    if (!(flags & LONGLIST) && !(flags & NEW_LINE))
    {
        printf("\n");
    }
}

static int ls_do(int argc, char **argv, int flags)
{
    int index;

    for (index = 0; index < argc; index++)
    {
        char *dir = argv[index];
        DIR *pdir = NULL;
        struct dirent *entry = NULL;
        struct stat s;
        int cnt = 1;

        if (wildcard_check(dir))
        {
            dir_list_wildcard(dir, flags);
            continue;
        }

        memset(&s, 0, sizeof(struct stat));

        if (stat(dir, &s))
        {
            printf("%s not existed\n", dir);
            continue;
        }
        if (!S_ISDIR(s.st_mode))
        {
            flags |= NEW_LINE;
            ls_show(dir, flags, s);
            if (argc > 1 && index + 1 < argc)
            {
                printf("\n");
            }
            continue;
        }
        else
        {
            if (argc > 1)
            {
                printf("%s:\n", dir);
            }

            pdir = opendir(dir);
            if (!pdir)
            {
                printf("opendir %s failed - %s\n", argv[index], strerror(errno));
                continue;
            }

            while ((entry = readdir(pdir)))
            {
                char fpath[128];
                int len;

                memset(fpath, 0, 128);
                len = strlen(dir);
#ifdef COMP_SPIFFS
                /*
                 * fix for spiffs
                 * readdir/opendir on spiffs will traverse all
                 * files in all directories
                 */
                snprintf(fpath, 128, "/data/%s", entry->d_name);
                if (strncmp(dir, fpath, len))
                {
                    continue;
                }
#else
                if (dir[len] == '/')
                {
                    snprintf(fpath, 128, "%s%s", dir, entry->d_name);
                }
                else
                {
                    snprintf(fpath, 128, "%s/%s", dir, entry->d_name);
                }
#endif
                memset(&s, 0, sizeof(struct stat));
                if (stat(fpath, &s))
                {
                    printf("stat %s failed - %s\n", fpath, strerror(errno));
                    continue;
                }

                if (!(cnt % 5))
                {
                    flags |= NEW_LINE;
                }
                else
                {
                    flags &= ~NEW_LINE;
                }
                cnt++;

                ls_show(relative_path(fpath, dir), flags, s);
            }
            closedir(pdir);
            if (!(flags & LONGLIST) && !(flags & NEW_LINE))
            {
                printf("\n");
            }
            if (argc > 1 && index + 1 < argc)
            {
                printf("\n");
            }
        }
    }
    return 0;
}

static int ls_main(int argc, char **argv)
{
    int opts = 0, flags = 0;

    optind = 0;
    while ((opts = getopt(argc, argv, ":lhk")) != EOF)
    {
        switch (opts)
        {
            case 'l':
                flags |= LONGLIST;
                break;
            case 'k':
                flags |= SZ_KB;
                break;
            case 'h':
                show_help();
                return 0;
            case '?':
                printf("invalid option %c\n", optopt);
                return -1;
            case ':':
                printf("option -%c requires an argument\n", optopt);
                show_help();
                return -1;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 0)
    {
        char *argv_new[1];
#ifdef DFS_USING_WORKDIR
        argv_new[0] = (char *)&working_directory;
#else
        argv_new[0] = "/";
#endif
        return ls_do(1, argv_new, flags);
    }
    else
    {
        return ls_do(argc, argv, flags);
    }
}
FINSH_FUNCTION_EXPORT_ALIAS(ls_main, __cmd_ls, list file or directory);

static int ll_main(int argc, char **argv)
{
    argc -= 1;
    argv += 1;

    if (argc == 0)
    {
        char *argv_new[1];
#ifdef DFS_USING_WORKDIR
        argv_new[0] = (char *)&working_directory;
#else
        argv_new[0] = "/";
#endif
        return ls_do(1, argv_new, LONGLIST | SZ_KB);
    }
    else
    {
        return ls_do(argc, argv, LONGLIST | SZ_KB);
    }
}
FINSH_FUNCTION_EXPORT_ALIAS(ll_main, __cmd_ll, the same as 'ls -kl');
