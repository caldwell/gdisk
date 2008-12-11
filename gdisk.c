//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <err.h>
#include <readline/readline.h>
#include "lengthof.h"
#include "round.h"
#include "guid.h"
#include "device.h"
#include "gpt.h"
#include "mbr.h"
#include "autolist.h"
#include "gdisk.h"

autolist_define(command);

static struct partition_table read_table(struct device *dev);
static void free_table(struct partition_table t);
void dump_dev(struct device *dev);
void dump_header(struct gpt_header *header);
void dump_partition(struct gpt_partition *p);

static void usage(char *me, int exit_code)
{
    fprintf(exit_code ? stderr : stdout,
            "Usage: \n"
            "   %s <device>\n"
            "%s", me, device_help());
    exit(exit_code);
}

struct partition_table g_table;

int main(int c, char **v)
{
    char *device_name = v[1];
    if (!device_name)
        usage(v[1], 1);

    struct device *dev = open_device(device_name);
    if (!dev)
        err(0, "Couldn't find device %s", v[1]);

    g_table = read_table(dev);

    char *line;
    while (line = readline("gdisk> ")) {
        add_history(line);
        char *l = line;
        while (isspace(*l)) l++;
        foreach_autolist(struct command *c, command)
            if (strcasecmp(c->name, l) == 0) {
                int args;
                for (args=0; c->arg[args].name; args++) {}
                char **argv = calloc(args, sizeof(*argv));
                if (!argv) err(ENOMEM, "No memory for argument list");
                for (int a=0; a<args; a++) {
                    char *prompt = NULL;
                    asprintf(&prompt, "%s: %s> ", c->name, c->arg[a].name);
                    if (!prompt) err(ENOMEM, "No memory for argument prompt");
                    argv[a] = readline(prompt);
                    if (!argv[a]) goto done;
                    free(prompt);
                }

                int status = c->handler(argv);
                if (status == ECANCELED) // Special case meaning Quit!
                    goto quit;
                if (status)
                    warnc(status, "%s failed", c->name);
              done:
                if (argv)
                    for (int a=0; a<args; a++)
                        free(argv[a]);
                free(argv);
                goto found;
            }
        if (*l)
            printf("Command not found: %s\n", l);
      found:
        free(line);
    }
    printf("\nQuitting without saving changes.\n");
  quit:
    if (line)
        free(line);

    free_table(g_table);

    //if (!write_mbr(dev, mbr))
    //    warn("Couldn't write MBR sector");
}

static int help(char **arg)
{
    printf("Commands:\n");
    foreach_autolist(struct command *c, command)
        printf("%-20s %s\n", c->name, c->help);

    return 0;
}
command_add("help", help, "Show a list of commands");

static int quit(char **arg)
{
    printf("Quitting without saving changes.\n");
    return ECANCELED; // total special case. Weak.
}
command_add("quit", quit, "Quit, leaving the disk untouched.");

static struct partition_table read_table(struct device *dev)
{
    struct partition_table t;
    t.dev = dev;
    t.header = get_sectors(dev,1,1);
    t.alt_header = get_sectors(dev,t.header->alternate_lba,1);

    if (sizeof(struct gpt_partition) != t.header->partition_entry_size)
        err(0, "Size of partition entries are %d instead of %d", t.header->partition_entry_size, sizeof(struct gpt_partition));

    t.partition = get_sectors(dev, 2, divide_round_up(t.header->partition_entry_size * t.header->partition_entries,dev->sector_size));

    t.mbr = read_mbr(dev);

    t.options.mbr_sync = true;
    for (int i=0; i<lengthof(t.mbr.partition); i++)
        t.alias[i] = -1;
    for (int mp=0; mp<lengthof(t.mbr.partition); mp++) {
        for (int gp=0; gp<t.header->partition_entries; gp++)
            if (t.mbr.partition[mp].partition_type &&
                t.partition[gp].first_lba < 0x100000000LL &&
                t.partition[gp].last_lba  < 0x100000000LL &&
                (t.mbr.partition[mp].first_sector_lba == t.partition[gp].first_lba ||
                 // first partition could be type EE which covers the GPT partition table and the optional EFI filesystem.
                 // The EFI filesystem in the GPT doesn't cover the EFI partition table, so the starts might not line up.
                 t.mbr.partition[mp].first_sector_lba == 1 && t.mbr.partition[mp].partition_type == 0xee) &&
                t.mbr.partition[mp].first_sector_lba + t.mbr.partition[mp].sectors == t.partition[gp].last_lba + 1)
                t.alias[mp] = gp;
        if (t.alias[mp] == -1)
            t.options.mbr_sync = false;
    }

    return t;
}

static void free_table(struct partition_table t)
{
    free(t.header);
    free(t.alt_header);
    free(t.partition);
}

void dump_dev(struct device *dev)
{
    printf("dev.sector_size: %ld\n", dev->sector_size);
    printf("dev.sector_count: %lld\n", dev->sector_count);
}

static int command_dump_dev(char **arg)
{
    dump_dev(g_table.dev);
    return 0;
}
command_add("debug-dump-dev", command_dump_dev, "Dump device structure");

void dump_header(struct gpt_header *header)
{
    printf("signature[8]         = %.8s\n", header->signature);
    printf("revision             = %08x\n", header->revision);
    printf("header_size          = %d\n",   header->header_size);
    printf("header_crc32         = %08x\n", header->header_crc32);
    printf("reserved             = %08x\n", header->reserved);
    printf("my_lba               = %lld\n", header->my_lba);
    printf("alternate_lba        = %lld\n", header->alternate_lba);
    printf("first_usable_lba     = %lld\n", header->first_usable_lba);
    printf("last_usable_lba      = %lld\n", header->last_usable_lba);
    printf("disk_guid            = %s\n",   guid_str(header->disk_guid));
    printf("partition_entry_lba  = %lld\n", header->partition_entry_lba);
    printf("partition_entries    = %d\n",   header->partition_entries);
    printf("partition_entry_size = %d\n",   header->partition_entry_size);
    printf("partition_crc        = %08x\n", header->partition_crc);
}

static int command_dump_header(char **arg)
{
    dump_header(arg[0] ? g_table.alt_header : g_table.header);
    return 0;
}
command_add("debug-dump-gpt-header", command_dump_header, "Dump GPT header structure",
            "alt", C_Flag);

void dump_partition(struct gpt_partition *p)
{
    printf("partition_type = %s\n",   guid_str(p->partition_type));
    for (int i=0; gpt_partition_type[i].name; i++)
        if (memcmp(gpt_partition_type[i].guid, p->partition_type, sizeof(p->partition_type)) == 0)
            printf("      * %s\n", gpt_partition_type[i].name);
    printf("partition_guid = %s\n",   guid_str(p->partition_guid));
    printf("first_lba      = %lld\n", p->first_lba);
    printf("last_lba       = %lld\n",  p->last_lba);
    printf("attributes     = %016llx\n", p->attributes);
    wchar_t name[36];
    for (int i=0; i<lengthof(name); i++)
        name[i] = p->name[i];
    printf("name[36]       = %.36ls\n",   name);
}

static int command_dump_partition(char **arg)
{
    for (int i=0; i<g_table.header->partition_entries; i++) {
        if (memcmp(gpt_partition_type_empty, g_table.partition[i].partition_type, sizeof(gpt_partition_type_empty)) == 0)
            continue;
        printf("Partition %d of %d\n", i, g_table.header->partition_entries);
        dump_partition(&g_table.partition[i]);
    }
    return 0;
}
command_add("debug-dump-partition", command_dump_partition, "Dump partitions structure");

static int command_dump_mbr(char **arg)
{
    dump_mbr(g_table.mbr);
    return 0;
}
command_add("debug-dump-mbr", command_dump_mbr, "Dump MBR structure");
