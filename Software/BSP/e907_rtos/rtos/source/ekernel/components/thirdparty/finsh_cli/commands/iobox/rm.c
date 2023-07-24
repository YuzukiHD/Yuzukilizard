#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <aw_dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <rtthread.h>
#include "wildcard.h"

#ifdef DFS_USING_WORKDIR
extern char working_directory[];
#endif

#define RM_FLAG_RECURSION (1 << 0)

static void show_help(void)
{
    printf("Usage: rm [-r] <file1> [<file2>...]\n");
}

static int rrmdir(const char *path)
{
    struct stat s;
    DIR *pdir = NULL;
    struct dirent *entry = NULL;
    int ret = -1;
    char *dir, *p;

    if (!path)
    {
        return -EINVAL;
    }

    dir = strdup(path);
    if(dir == NULL)
    {
        return -EINVAL;
    }

    p = dir + strlen(dir) - 1;
    while (*p == '/' && p > dir)
    {
        *p = '\0';
        p--;
    }

    if (stat(dir, &s) || !S_ISDIR(s.st_mode))
    {
        goto out;
    }

    pdir = opendir(dir);
    if (!pdir)
    {
        goto out;
    }

    ret = 0;
    while (ret == 0 && (entry = readdir(pdir)))
    {
        char fpath[128] = {0};

        snprintf(fpath, 128, "%s/%s", dir, entry->d_name);

        ret = stat(fpath, &s);
        if (ret)
        {
            break;
        }

        if (!strcmp(entry->d_name, "."))
        {
            continue;
        }
        if (!strcmp(entry->d_name, ".."))
        {
            continue;
        }

        if (S_ISDIR(s.st_mode))
        {
            ret = rrmdir(fpath);
        }
        else
        {
            ret = unlink(fpath);
        }
    }

    closedir(pdir);
    if (ret == 0)
    {
        ret = rmdir(dir);
        if (ret)
        {
            printf("rmdir %s failed\n", dir);
        }
    }
out:
    free(dir);
    return ret;
}

static int do_remove(const char *path, int flags)
{
    struct stat s;
    int ret = -1;

    if (stat(path, &s))
    {
        goto err;
    }

    if (S_ISREG(s.st_mode))
    {
        ret = unlink(path);
    }
    else if (flags & RM_FLAG_RECURSION)
    {
        ret = rrmdir(path);
    }
err:
    if (ret)
    {
        printf("%s is not existed or directory\n", path);
    }
    return ret;
}

static void dir_rm_wildcard(char *path, int flags)
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
            do_remove(entry->d_name, flags);
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
}

static int cmd_rm(int argc, char **argv)
{
    int opts = 0, flags = 0, index, ret = 0;

    optind = 0;
    while ((opts = getopt(argc, argv, ":r")) != EOF)
    {
        switch (opts)
        {
            case 'r':
                flags |= RM_FLAG_RECURSION;
                break;
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

    if (argc < 1)
    {
        show_help();
        return -1;
    }

    for (index = 0; index < argc; index++)
    {
        char *path = argv[index];

        if (wildcard_check(argv[index]))
        {
            dir_rm_wildcard(argv[index], flags);
            continue;
        }

        if (do_remove(path, flags))
        {
            ret |= -ENOENT;
        }
    }

    return ret;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_rm, __cmd_rm, Remove(unlink) the FILE(s).);
