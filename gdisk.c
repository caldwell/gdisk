//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/param.h> // PATH_MAX on both linux and OS X
#include <sys/stat.h>  // mkdir
#include <err.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "lengthof.h"
#include "round.h"
#include "guid.h"
#include "partition-type.h"
#include "device.h"
#include "gpt.h"
#include "mbr.h"
#include "autolist.h"
#include "csprintf.h"
#include "human.h"
#include "gdisk.h"

autolist_define(command);

static int run_command(char *line);
static struct partition_table read_table(struct device *dev);
static void free_table(struct partition_table t);
static char *command_completion(const char *text, int state);
static char *partition_type_completion(const char *text, int state);
static struct partition_table dup_table(struct partition_table t);
static unsigned long partition_sectors(struct partition_table t);
static void backup_table();
static void dump_dev(struct device *dev);
static void dump_header(struct gpt_header *header);
static void dump_partition(struct gpt_partition *p);
static void *xmalloc(size_t size);
static void *xcalloc(size_t count, size_t size);
static void *xrealloc(void *old, size_t count);
static char *xstrdup(char *s);
static void *memdup(void *mem, size_t size);
static size_t sncatprintf(char *buffer, size_t space, char *format, ...) __attribute__ ((format (printf, 3, 4)));

static void usage(char *me, int exit_code)
{
    fprintf(exit_code ? stderr : stdout,
            "Usage: \n"
            "   %s <device>\n"
            "%s", me, device_help());
    exit(exit_code);
}

struct partition_table g_table_orig;
struct partition_table g_table;

int main(int c, char **v)
{
    char *device_name = v[1];
    if (!device_name)
        usage(v[0], 1);

    struct device *dev = open_device(device_name);
    if (!dev)
        err(0, "Couldn't find device %s", v[1]);

    g_table_orig = read_table(dev);
    g_table = read_table(dev);

    char *line;
    int status = 0;
    do {
        rl_completion_entry_function = (void*)command_completion; // rl_completion_entry_function is defined to return an int??
        line = readline("gdisk> ");
        if (!line)
            break;
        add_history(line);
        status = run_command(line);
        free(line);
    } while (status != ECANCELED); // Special case meaning Quit!
    printf("\nQuitting without saving changes.\n");

    free_table(g_table);

    //if (!write_mbr(dev, mbr))
    //    warn("Couldn't write MBR sector");
}

char *next_word(char **line)
{
    return strsep(line, " \t\f\r\n");
}

static char **parse_command(char *line)
{
    char **v = xcalloc(1, sizeof(char*));
    int c = 0;

    char *rest = line;
    char *word;
    while (word = next_word(&rest)) {
        if (!*word) continue; // compact multiple consecutive separators
        v = xrealloc(v, sizeof(char *) * (c+2));
        v[c] = xstrdup(word);
        v[c+1] = NULL;
        c++;
    }
    return v;
}

static struct command *find_command(char *command)
{
    foreach_autolist(struct command *c, command)
        if (strcmp(command, c->name) == 0)
            return c;
    return NULL;
}

static int run_command(char *line)
{
    int status = 0;
    char **argv = parse_command(line);
    int argc;
    for (argc=0; argv[argc]; argc++) {}
    if (!argc) goto done; // Blank line

    struct command *c = find_command(argv[0]);
    if (!c) {
        printf("Command not found: '%s'\n", argv[0]);
        status = EINVAL;
        goto done;
    }

    int args;
    for (args=0; c->arg[args].name; args++) {}

    if (argc > args+1) {
        printf("Too many arguments for command '%s'\n", argv[0]);
        status = EINVAL;
        goto done;
    }

    argv = xrealloc(argv, (1+args+1) * sizeof(*argv));
    memset(&argv[argc], 0, (1+args+1 - argc) * sizeof(*argv));

    char **v = argv + 1; // start past command name.
    for (int a=0; a<args; a++, v++) {
        if (*v) continue; // Don't prompt for args entered on command line.
        if (c->arg[a].type & C_Optional)
            continue;
        char *prompt = NULL;
        asprintf(&prompt, "%s: Enter %s: ", c->name, c->arg[a].help);
        if (!prompt) err(ENOMEM, "No memory for argument prompt");

        if (C_Type(c->arg[a].type) == C_File)
            rl_completion_entry_function = (void*)rl_filename_completion_function;
        else if (C_Type(c->arg[a].type) == C_Partition_Type)
            rl_completion_entry_function = (void*)partition_type_completion;

        *v = readline(prompt);
        if (!*v) goto done;
        free(prompt);
    }

    status = c->handler(argv);

  done:
    if (argv)
        for (int a=0; argv[a]; a++)
            free(argv[a]);
    free(argv);
    return status;
}

static char *arg_name(struct command_arg_ *arg)
{
    return csprintf("%s%s%s",
                    arg->type == C_Flag ? "--" : "<",
                    arg->name,
                    arg->type == C_Flag ? ""   : ">");
}

static int compare_command_name(const void *a, const void *b)
{
    return strcmp((*(struct command **)a)->name, (*(struct command **)b)->name);
}

static int help(char **arg)
{
    if (arg[1]) {
        struct command *c = find_command(arg[1]);
        if (!c) {
            printf("No such command: '%s'\n", arg[1]);
            return 0;
        }
        printf("%s:\n  %s\n\nUsage:\n  %s", c->name, c->help, c->name);
        for (int a=0; c->arg[a].name; a++) {
            printf(" %s%s%s",
                   c->arg[a].type & C_Optional ? "[ " : "",
                   arg_name(&c->arg[a]),
                   c->arg[a].type & C_Optional ? " ]" : "");
        }
        printf("\n\nArguments:\n");
        for (int a=0; c->arg[a].name; a++) {
            printf("  %s: %s\n", arg_name(&c->arg[a]), c->arg[a].help);
        }
        return 0;
    }

    printf("Commands:\n");
    int width=0, count=0;
    foreach_autolist(struct command *c, command) {
        width=MAX(width, strlen(c->name));
        count++;
    }

    struct command *command[count], **c = command;
    foreach_autolist(struct command *cmd, command)
        *c++ = cmd;
    qsort(command, count, sizeof(*command), compare_command_name);

    for (c=command; c < &command[count]; c++)
        printf("%-*s %s\n", width+2, (*c)->name, (*c)->help);

    return 0;
}
command_add("help", help, "Show a list of commands",
            command_arg("command", C_String|C_Optional, "Command to get help with"));

static int quit(char **arg)
{
    return ECANCELED; // total special case. Weak.
}
command_add("quit", quit, "Quit, leaving the disk untouched.");

static char *command_completion(const char *text, int state)
{
    int i=0;
    foreach_autolist(struct command *c, command)
        if (strncmp(text, c->name, strlen(text)) == 0 &&
            i++ == state)
            return xstrdup(c->name);
    return NULL;
}

static char *partition_type_completion(const char *text, int state)
{
    for (int i=0, s=0; gpt_partition_type[i].name; i++)
        if (strncmp(text, gpt_partition_type[i].name, strlen(text)) == 0 &&
            s++ == state)
            return xstrdup(gpt_partition_type[i].name);
    return NULL;
}

static struct partition_table blank_table(struct device *dev)
{
    struct partition_table t = {};
    t.dev = dev;
    t.header = alloc_sectors(dev, 1);
    t.alt_header = alloc_sectors(dev, 1);
    const int partitions = 128;
    const int partition_sectors = divide_round_up(partitions * sizeof(struct gpt_partition), dev->sector_size);
    *t.header = (struct gpt_header) {
               .signature = "EFI PART",
               .revision = PARTITION_REVISION,
               .header_size = sizeof(struct gpt_header),
               .my_lba = 1,
               .alternate_lba = dev->sector_count-1,
               .first_usable_lba = 2                  + partition_sectors,
               .last_usable_lba = dev->sector_count-2 - partition_sectors,
               .disk_guid = guid_create(),
               .partition_entry_lba = 2,
               .partition_entries = partitions,
               .partition_entry_size = sizeof(struct gpt_partition),
    };

    *t.alt_header = *t.header;
    t.alt_header->alternate_lba       = t.header->my_lba;
    t.alt_header->my_lba              = t.header->alternate_lba;
    t.alt_header->partition_entry_lba = t.header->last_usable_lba + 1;

    t.partition = alloc_sectors(dev, partition_sectors);

    for (int i=0; i<lengthof(t.alias); i++)
        t.alias[i] = -1;

    // Even a blank MBR should preserve the boot code.
    struct mbr mbr = read_mbr(dev);
    memcpy(t.mbr.code, mbr.code, sizeof(mbr.code));
    return t;
}

static void update_gpt_crc(struct gpt_header *h, struct gpt_partition *p)
{
    h->partition_crc32 = gpt_partition_crc32(h, p);
    h->header_crc32 = gpt_header_crc32(h);
}

static void update_table_crc(struct partition_table *t)
{
    update_gpt_crc(t->header,     t->partition);
    update_gpt_crc(t->alt_header, t->partition);
}

static bool gpt_crc_valid(struct gpt_header *h, struct gpt_partition *p)
{
    return h->partition_crc32 == gpt_partition_crc32(h, p) && h->header_crc32 == gpt_header_crc32(h);
}

static void utf16_from_ascii(uint16_t *utf16le, char *ascii)
{
    do {
        *utf16le++ = *ascii;
    } while (*ascii++);
}

static struct partition_table gpt_table_from_mbr(struct device *dev)
{
    struct partition_table t = blank_table(dev);
    t.mbr = read_mbr(dev);
    for (int mp=0,gp=0; mp<lengthof(t.mbr.partition); mp++) {
        if (t.mbr.partition[mp].partition_type) {
            t.partition[gp].first_lba = t.mbr.partition[mp].first_sector_lba;
            t.partition[gp].last_lba  = t.mbr.partition[mp].first_sector_lba + t.mbr.partition[mp].sectors - 1;

            if (t.partition[gp].first_lba < t.header->first_usable_lba ||
                t.partition[gp].last_lba > t.header->last_usable_lba)
                printf("ouch, mbr partition %d [%"PRId64"d,%"PRId64"d] outside of usable gpt space [%"PRId64"d,%"PRId64"d]\n",
                       mp+1, t.partition[gp].first_lba, t.partition[gp].last_lba, t.header->first_usable_lba, t.header->last_usable_lba);

            utf16_from_ascii(t.partition[gp].name, csprintf("MBR %d\n", mp+1));

            for (int i=0; gpt_partition_type[i].name; i++)
                for (int j=0; gpt_partition_type[i].mbr_equivalent[j]; j++)
                    if (t.mbr.partition[mp].partition_type == gpt_partition_type[i].mbr_equivalent[j]) {
                        t.partition[gp].partition_type = gpt_partition_type[i].guid;
                        goto found;
                    }
            // Not found, use gdisk specific guid to mean "unknown".
            t.partition[gp].partition_type = GUID(b334117e,118d,11de,9b0f,001cc0952d53);

          found:
            t.partition[gp].partition_guid = guid_create();
            t.alias[mp] = gp++;
        }
    }
    t.options.mbr_sync = true;
    update_table_crc(&t);
    return t;
}
static int gpt_from_mbr(char **arg)
{
    free_table(g_table);
    g_table = gpt_table_from_mbr(g_table_orig.dev);
    return 0;
}
command_add("init-from-mbr", gpt_from_mbr, "Init a GPT from the MBR");

static bool partition_entry_is_representable_in_mbr(struct gpt_partition entry)
{
    // MBR only has 32 bits for the LBA. So if the partition is further up the disk than that then it can't be represented in the MBR.
    return !guid_eq(gpt_partition_type_empty, entry.partition_type) &&
           entry.first_lba < 0x100000000LL &&
           entry.last_lba - entry.first_lba < 0x100000000LL;
}

static void create_mbr_alias_table(struct partition_table *t)
{
    t->options.mbr_sync = true;
    for (int i=0; i<lengthof(t->alias); i++)
        t->alias[i] = -1;
    for (int mp=0; mp<lengthof(t->mbr.partition); mp++) {
        if (!t->mbr.partition[mp].partition_type)
            continue;
        for (int gp=0; gp<t->header->partition_entries; gp++)
            if (partition_entry_is_representable_in_mbr(t->partition[gp]) &&
                (t->mbr.partition[mp].first_sector_lba == t->partition[gp].first_lba ||
                 // first partition could be type EE which covers the GPT partition table and the optional EFI filesystem.
                 // The EFI filesystem in the GPT doesn't cover the EFI partition table, so the starts might not line up.
                 t->mbr.partition[mp].first_sector_lba == 1 && t->mbr.partition[mp].partition_type == 0xee) &&
                t->mbr.partition[mp].first_sector_lba + t->mbr.partition[mp].sectors == t->partition[gp].last_lba + 1) {
                t->alias[mp] = gp;
                break;
            }
        if (t->alias[mp] == -1)
            t->options.mbr_sync = false;
    }
}

static struct partition_table read_table(struct device *dev)
{
    struct partition_table t = {};
    t.dev = dev;

    t.header = get_sectors(dev,1,1);

#define header_error(format, ...) ({                        \
            fprintf(stderr, format, ##__VA_ARGS__);         \
            fprintf(stderr, ". Assuming blank partiton\n"); \
            free_table(t);                                  \
            blank_table(dev);                               \
        })

    if (memcmp(t.header->signature, "EFI PART", sizeof(t.header->signature)) != 0)
        return header_error("Missing signature");

    gpt_header_to_host(t.header);

    t.alt_header = get_sectors(dev,t.header->alternate_lba,1);

    if (memcmp(t.alt_header->signature, "EFI PART", sizeof(t.alt_header->signature)) != 0)
        return header_error("Missing signature in altername GPT header");

    gpt_header_to_host(t.alt_header);

    if (t.header->partition_entry_lba != 2)
        return header_error("Partition table LBA is %"PRId64" and not 2", t.header->partition_entry_lba);

    if (t.header->header_size != sizeof(struct gpt_header))
        return header_error("Partition header is %d bytes long instead of %zd", t.header->header_size, sizeof(struct gpt_header));

    if (sizeof(struct gpt_partition) != t.header->partition_entry_size)
        return header_error("Size of partition entries are %d instead of %zd", t.header->partition_entry_size, sizeof(struct gpt_partition));

    t.partition = get_sectors(dev, 2, divide_round_up(t.header->partition_entry_size * t.header->partition_entries,dev->sector_size));
    gpt_partition_to_host(t.partition, t.header->partition_entries);

    if (!gpt_crc_valid(t.header, t.partition))
        printf("header CRC not valid\n");
    if (!gpt_crc_valid(t.alt_header, t.partition))
        printf("alt header CRC not valid\n");

    t.mbr = read_mbr(dev);

    create_mbr_alias_table(&t);

    return t;
}

static void free_table(struct partition_table t)
{
    free(t.header);
    free(t.alt_header);
    free(t.partition);
}

static struct partition_table dup_table(struct partition_table t)
{
    struct partition_table dup;
    dup = t;
    dup.header = memdup(t.header, t.dev->sector_size);
    dup.alt_header = memdup(t.alt_header, t.dev->sector_size);
    dup.partition = memdup(t.partition, partition_sectors(t) * t.dev->sector_size);
    return dup;
}

static int compare_partition_entries(const void *_a, const void *_b)
{
    const struct gpt_partition *a = _a, *b = _b;
    int a_empty = guid_eq(gpt_partition_type_empty, a->partition_type),
        b_empty = guid_eq(gpt_partition_type_empty, b->partition_type);
    if (a_empty || b_empty)
        return a_empty - b_empty;
    return (long long)a->first_lba - (long long)b->first_lba;
}

static void compact_and_sort(struct partition_table *t)
{
    qsort(t->partition, t->header->partition_entries, sizeof(*t->partition), compare_partition_entries);
    update_table_crc(&g_table);
    create_mbr_alias_table(t);
}

static int command_compact_and_sort(char **arg)
{
    compact_and_sort(&g_table);
    return 0;
}
command_add("compact-and-sort", command_compact_and_sort, "Remove \"holes\" from table and sort entries in ascending order");

static struct gpt_partition *find_unused_partition(struct partition_table t)
{
    for (int i=0; i<t.header->partition_entries; i++)
        if (guid_eq(gpt_partition_type_empty, g_table.partition[i].partition_type))
            return &g_table.partition[i];
    return NULL;
}

static uint64_t find_free_space(struct partition_table unsorted, uint64_t blocks)
{
    // To make this sane we sort the partition table first.
    struct partition_table t = dup_table(unsorted);
    compact_and_sort(&t);

    uint64_t free_start = t.header->first_usable_lba;
    for (int p=0; p<t.header->partition_entries; p++) {
        if (guid_eq(gpt_partition_type_empty, t.partition[p].partition_type))
            break;
        if (t.partition[p].first_lba - free_start >= blocks)
            goto done;
        free_start = t.partition[p].last_lba + 1;
    }
    // Check for it fitting in the end (probably the most common case)
    if (t.header->last_usable_lba+1 - free_start >= blocks)
        goto done;

    free_start = -1LL;    // If we got here the was no room.

  done:
    free_table(t);
    return free_start;
}

static GUID type_guid_from_string(char *s)
{
    for (int t=0; gpt_partition_type[t].name; t++)
        if (strcasecmp(s, gpt_partition_type[t].name) == 0)
            return gpt_partition_type[t].guid;
    return guid_from_string(s);
}

static bool sync_partition_to_mbr(struct partition_table *t, int gpt_index)
{
    int mbr_type;
    struct gpt_partition *p = &t->partition[gpt_index];
    if (!partition_entry_is_representable_in_mbr(*p) ||
        !(mbr_type = find_mbr_equivalent(p->partition_type)))
        return false;

    for (int i=0; i < lengthof(t->mbr.partition); i++)
        if (t->mbr.partition[i].partition_type == 0) {
            t->mbr.partition[i] = (struct mbr_partition) {
                .status = 0,
                .first_sector = {}, // Need disk geometry to do this properly
                .last_sector  = {}, // Need disk geometry to do this properly
                .partition_type = mbr_type,
                .first_sector_lba = p->first_lba,
                .sectors = p->last_lba - p->first_lba + 1,
            };
            t->alias[i] = gpt_index;
            return true;
        }
    return false;
}

static int command_create_partition(char **arg)
{
    struct gpt_partition part = {};
    part.partition_type = type_guid_from_string(arg[1]);
    if (guid_eq(bad_guid, part.partition_type)) {
        fprintf(stderr, "Unknown GUID format: \"%s\"\n", arg[1]);
        return EINVAL;
    }

    part.partition_guid = arg[7] ? guid_from_string(arg[7]) : guid_create();
    if (guid_eq(bad_guid, part.partition_guid)) {
        fprintf(stderr, "Unknown GUID format: \"%s\"\n", arg[7]);
        return EINVAL;
    }

    uint64_t size = human_size(arg[2]);
    uint64_t blocks = divide_round_up(size, g_table.dev->sector_size);
    part.first_lba = arg[4] ? strtoull(arg[4], NULL, 0) : find_free_space(g_table, blocks);
    if (part.first_lba == -1LL) {
        fprintf(stderr, "Couldn't find %"PRId64" blocks (%"PRId64", %s) of free space.\n", blocks, size, human_string(size));
        return ENOSPC;
    }
    part.last_lba = arg[5] ? strtoull(arg[5], NULL, 0) : part.first_lba + blocks;
    part.attributes = 0 | (arg[6] ? PA_SYSTEM_PARTITION : 0);
    utf16_from_ascii(part.name, arg[3] ? arg[3] : "");

    dump_partition(&part);

    struct gpt_partition *p = find_unused_partition(g_table);
    if (!p) {
        fprintf(stderr, "Partition table is full.\n");
        return ENOSPC;
    }

    *p = part;

    if (g_table.options.mbr_sync)
        sync_partition_to_mbr(&g_table, p - g_table.partition);

    return 0;
}

command_add("new", command_create_partition, "Create a new partition entry in the table",
            command_arg("type",      C_Partition_Type, "Type of new partition"),
            command_arg("size",      C_Number, "Size of the new partition"),
            command_arg("label",     C_String|C_Optional, "The name of the new partition"),
            command_arg("first_lba", C_String|C_Optional, "The first block of the new partition"),
            command_arg("last_lba",  C_String|C_Optional, "The last block of the new partition (this overrides the size argument)"),
            command_arg("system",    C_Flag, "Set the \"System Partition\" attribute"),
            command_arg("guid",      C_String|C_Optional, "The GUID of the new partition")
            );

static int get_mbr_alias(struct partition_table t, int index)
{
    for (int m=0; m<lengthof(t.alias); m++)
        if (g_table.alias[m] == index)
            return m;
    return -1;
}

static void delete_mbr_partition(struct partition_table *t, int mbr_index)
{
    memset(&t->mbr.partition[mbr_index], 0, sizeof(*t->mbr.partition));
    t->alias[mbr_index] = -1;
}

static int command_delete_partition(char **arg)
{
    int index = strtol(arg[1], NULL, 0);
    if (index < 0 || index >= g_table.header->partition_entries) {
        fprintf(stderr, "Bad index '%d'. Should be between 0 and %d (inclusive)\n", index, g_table.header->partition_entries-1);
        return EINVAL;
    }
    memset(&g_table.partition[index], 0, sizeof(g_table.partition[index]));
    update_table_crc(&g_table);
    int mbr_alias = get_mbr_alias(g_table, index);
    if (g_table.options.mbr_sync && mbr_alias != -1)
        delete_mbr_partition(&g_table, mbr_alias);
    return 0;
}

command_add("delete", command_delete_partition, "Delete a partition from the table",
            command_arg("index",     C_Number, "The index number of the partition. The first partitiion is partition zero"));

static int command_sync_mbr(char **arg)
{
    if (g_table.options.mbr_sync && !arg[1]) {
        fprintf(stderr, "MBR is already synced to GPT. Use --force flag to force a re-sync.\n");
        return EEXIST;
    }
    for (int i=0; i < lengthof(g_table.mbr.partition); i++)
        delete_mbr_partition(&g_table, i);

    for (int i=0; i < g_table.header->partition_entries; i++)
        sync_partition_to_mbr(&g_table, i);

    g_table.options.mbr_sync = true;
    return 0;
}
command_add("sync-mbr", command_sync_mbr, "Create a new MBR partition table with data from the GPT partition table",
            command_arg("force",     C_Flag, "Force a re-sync if the MBR already looks synced"));

static unsigned long partition_sectors(struct partition_table t)
{
    return divide_round_up(t.header->partition_entry_size * t.header->partition_entries, t.dev->sector_size);
}

static int export_table(struct partition_table t, char *filename)
{
    FILE *info = NULL, *front = NULL, *back = NULL;
    int err = 0;
    if ((front = fopen(csprintf("%s.front", filename), "wb")) == NULL) { err = errno; warn("Couldn't open %s.front", filename); goto done; }
    if ((back  = fopen(csprintf("%s.back",  filename), "wb")) == NULL) { err = errno; warn("Couldn't open %s.back",  filename); goto done; }
    if ((info  = fopen(csprintf("%s.info",  filename), "w"))  == NULL) { err = errno; warn("Couldn't open %s.info",  filename); goto done; }

    // *.front: mbr, then gpt header, then partitions
    void *sector = sector_from_mbr(t.dev, t.mbr);
    if (fwrite(sector, t.dev->sector_size, 1, front) != 1) { err = errno; warn("Couldn't write mbr to %s.front", filename); goto done; }
    free(sector);

    gpt_header_from_host(t.header);
    int wrote = fwrite(t.header, t.dev->sector_size, 1, front);
    if (wrote != 1) err = errno;
    gpt_header_to_host(t.header);
    if (wrote != 1)  { warn("Couldn't write GPT header to %s.front", filename); goto done; }

    gpt_partition_from_host(t.partition, t.header->partition_entries);
    wrote = fwrite(t.partition, t.dev->sector_size, partition_sectors(t), front);
    if (wrote != partition_sectors(t)) err = errno;
    gpt_partition_to_host(t.partition, t.header->partition_entries);
    if (wrote != partition_sectors(t))  { warn("Couldn't write partitions to %s.front", filename); goto done; }

    // *.back: gpt partitions, then gpt_header
    gpt_partition_from_host(t.partition, t.header->partition_entries);
    wrote = fwrite(t.partition, t.dev->sector_size, partition_sectors(t), back);
    if (wrote != partition_sectors(t)) err = errno;
    gpt_partition_to_host(t.partition, t.header->partition_entries);
    if (wrote != partition_sectors(t))  { warn("Couldn't write partitions to %s.back", filename); goto done; }

    gpt_header_from_host(t.alt_header);
    wrote = fwrite(t.alt_header, t.dev->sector_size, 1, back);
    if (wrote != 1) err = errno;
    gpt_header_to_host(t.alt_header);
    if (wrote != 1)  { warn("Couldn't write Alternate GPT header to %s.back", filename); goto done; }

    fprintf(info, "sector_size: %ld\n", t.dev->sector_size);
    fprintf(info, "sector_count: %lld\n", t.dev->sector_count);
    fprintf(info, "# device: %s\n", t.dev->name);
    fprintf(info, "# dd if=%s of=%s bs=%ld count=%ld\n",
            csprintf("%s.front", filename), t.dev->name, t.dev->sector_size, 1+1+partition_sectors(t));
    fprintf(info, "# dd if=%s of=%s seek=%"PRId64" bs=%ld count=%ld\n",
            csprintf("%s.back", filename), t.dev->name, t.alt_header->partition_entry_lba, t.dev->sector_size, 1+partition_sectors(t));

  done:
    if (front) fclose(front);
    if (back)  fclose(back);
    if (info)  fclose(info);
    return err;
}

int command_export(char **arg)
{
    return export_table(g_table, arg[1]);
}
command_add("export", command_export, "Save table to a file (not to a device)",
            command_arg("filename", C_File, "Filename to as a base use for exported files"));


static void backup_table()
{
    char *HOME = getenv("HOME");
    if (!HOME) return;

    char *dev_name = xstrdup(g_table_orig.dev->name);

    for (int i=0; dev_name[i]; i++)
        if (!isalnum(dev_name[i]))
            dev_name[i] = '_';

    char backup_path[PATH_MAX];
    snprintf(backup_path, sizeof(backup_path), "%s/.gdisk", HOME);
    mkdir(backup_path, 0777);
    sncatprintf(backup_path, sizeof(backup_path), "/backups");
    mkdir(backup_path, 0777);
    sncatprintf(backup_path, sizeof(backup_path), "/%s", dev_name);
    time_t now = time(NULL);
    strftime(backup_path + strlen(backup_path), sizeof(backup_path) - strlen(backup_path),
             "-%G-%m-%d-%H-%M-%S", localtime(&now));

    fprintf(stderr, "backing up current partition table to %s\n", backup_path);
    export_table(g_table_orig, backup_path);
}

static int command_print(char **arg)
{
    printf("%s:\n", g_table.dev->name);
    printf("  Disk GUID: %s\n", guid_str(g_table.header->disk_guid));
    printf("  %lld %ld byte sectors for %lld total bytes (%s capacity)\n",
           g_table.dev->sector_count,  g_table.dev->sector_size,
           g_table.dev->sector_count * g_table.dev->sector_size,
           human_string(g_table.dev->sector_count * g_table.dev->sector_size));
    printf("  MBR partition table is %s synced to the GPT table\n", g_table.options.mbr_sync ? "currently" : "not");
    printf("\n    %3s) %14s %14s %26s %s\n", "###", "Start LBA", "End LBA", "Size", "GUID");
    printf("    %.98s\n", "----------------------------------------------------------------------------------------------------------------");
    for (int i=0; i<g_table.header->partition_entries; i++) {
        if (guid_eq(gpt_partition_type_empty, g_table.partition[i].partition_type))
            continue;
        printf("    %3d) %14"PRId64" %14"PRId64" %14"PRId64" (%9s) %s\n", i,
               g_table.partition[i].first_lba, g_table.partition[i].last_lba,
               (g_table.partition[i].last_lba - g_table.partition[i].first_lba) * g_table.dev->sector_size,
               human_string((g_table.partition[i].last_lba - g_table.partition[i].first_lba) * g_table.dev->sector_size),
               guid_str(g_table.partition[i].partition_guid));
    }
    printf("\n    %3s) %-20s %s %-3s %-26s\n", "###", "Flags", "B", "MBR", "GPT Type");
    printf("    %.98s\n", "----------------------------------------------------------------------------------------------------------------");
    for (int i=0; i<g_table.header->partition_entries; i++) {
        if (guid_eq(gpt_partition_type_empty, g_table.partition[i].partition_type))
            continue;
        printf("    %3d) %-20s ", i, "none");
        int m = get_mbr_alias(g_table, i);
        if (g_table.options.mbr_sync && m != -1) {
            printf("%1s %02x  ", g_table.mbr.partition[m].status & MBR_STATUS_BOOTABLE ? "*" : "",
                   g_table.mbr.partition[m].partition_type);
        } else
            printf("%1s %3s ","","");
        int found=0;
        for (int t=0; gpt_partition_type[t].name; t++)
            if (guid_eq(gpt_partition_type[t].guid, g_table.partition[i].partition_type)) {
                printf("%s%s", found ? " or " : "", gpt_partition_type[t].name);
                found = 1;
            }
        if (!found)
            printf("%s", guid_str(g_table.partition[i].partition_type));
        printf("\n");
    }
    return 0;
}
command_add("print", command_print, "Print the partition table.");

static int command_print_mbr(char **arg)
{
    int verbose = !!arg[1];
    printf("    #) %-9s", "Flags");
    if (verbose) printf(" %-11s %-11s", "Start C:H:S", "End C:H:S");
    printf(" %10s %22s %s\n", "Start LBA", "Size", "Type");
    printf("    %.*s\n", verbose ? 98 : 78, "----------------------------------------------------------------------------------------------------------------");
    for (int i=0; i<lengthof(g_table.mbr.partition); i++) {
        if (!g_table.mbr.partition[i].partition_type) {
            printf("    %d) Unused entry\n", i);
            continue;
        }

        printf("    %d) %02x %6s",
                   i, g_table.mbr.partition[i].status, g_table.mbr.partition[i].status & MBR_STATUS_BOOTABLE ? "(boot)" : "");
        if (verbose)
            printf(" %4d:%03d:%02d %4d:%03d:%02d",
                   g_table.mbr.partition[i].first_sector.cylinder, g_table.mbr.partition[i].first_sector.head, g_table.mbr.partition[i].first_sector.sector,
                   g_table.mbr.partition[i].last_sector.cylinder,  g_table.mbr.partition[i].last_sector.head,  g_table.mbr.partition[i].last_sector.sector);
        printf(" %10d %10d (%9s) %02x (%s)\n",
                   g_table.mbr.partition[i].first_sector_lba,
                   g_table.mbr.partition[i].sectors, human_string((long long)g_table.mbr.partition[i].sectors * g_table.dev->sector_size),
                   g_table.mbr.partition[i].partition_type,
                   mbr_partition_type[g_table.mbr.partition[i].partition_type] ? mbr_partition_type[g_table.mbr.partition[i].partition_type] : "???");
    }
    return 0;
}
command_add("print-mbr", command_print_mbr, "Print the MBR partition table.",
            command_arg("verbose", C_Flag, "Include extra cylinder/head/sector information in output"));

static void dump_dev(struct device *dev)
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

static void dump_header(struct gpt_header *header)
{
    printf("signature[8]         = %.8s\n", header->signature);
    printf("revision             = %08x\n", header->revision);
    printf("header_size          = %d\n",   header->header_size);
    printf("header_crc32         = %08x\n", header->header_crc32);
    printf("reserved             = %08x\n", header->reserved);
    printf("my_lba               = %"PRId64"\n", header->my_lba);
    printf("alternate_lba        = %"PRId64"\n", header->alternate_lba);
    printf("first_usable_lba     = %"PRId64"\n", header->first_usable_lba);
    printf("last_usable_lba      = %"PRId64"\n", header->last_usable_lba);
    printf("disk_guid            = %s\n",   guid_str(header->disk_guid));
    printf("partition_entry_lba  = %"PRId64"\n", header->partition_entry_lba);
    printf("partition_entries    = %d\n",   header->partition_entries);
    printf("partition_entry_size = %d\n",   header->partition_entry_size);
    printf("partition_crc32      = %08x\n", header->partition_crc32);
}

static int command_dump_header(char **arg)
{
    dump_header(arg[1] ? g_table.alt_header : g_table.header);
    return 0;
}
command_add("debug-dump-gpt-header", command_dump_header, "Dump GPT header structure",
            command_arg("alt", C_Flag, "Display the alternate partition header"));

static void dump_partition(struct gpt_partition *p)
{
    printf("partition_type = %s\n",   guid_str(p->partition_type));
    for (int i=0; gpt_partition_type[i].name; i++)
        if (guid_eq(gpt_partition_type[i].guid, p->partition_type))
            printf("      * %s\n", gpt_partition_type[i].name);
    printf("partition_guid = %s\n",   guid_str(p->partition_guid));
    printf("first_lba      = %"PRId64"\n", p->first_lba);
    printf("last_lba       = %"PRId64"\n",  p->last_lba);
    printf("attributes     = %016"PRIx64"\n", p->attributes);
    wchar_t name[36];
    for (int i=0; i<lengthof(name); i++)
        name[i] = p->name[i];
    printf("name[36]       = %.36ls\n",   name);
}

static int command_dump_partition(char **arg)
{
    for (int i=0; i<g_table.header->partition_entries; i++) {
        if (guid_eq(gpt_partition_type_empty, g_table.partition[i].partition_type))
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

// Some useful library routines. Should maybe go in another file at some point.
static void *xmalloc(size_t size)
{
    void *mem = malloc(size);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
static void *xcalloc(size_t count, size_t size)
{
    void *mem = calloc(count, size);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
static void *xrealloc(void *old, size_t count)
{
    void *mem = realloc(old, count);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
static char *xstrdup(char *s)
{
    char *dup = strdup(s);
    if (!dup) err(errno, "Out of memory");
    return dup;
}
static void *memdup(void *mem, size_t size)
{
    void *dup = xmalloc(size);
    memcpy(dup, mem, size);
    return dup;
}

static size_t sncatprintf(char *buffer, size_t space, char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    size_t length = strlen(buffer);
    int count = vsnprintf(buffer + length, space - length, format, ap);
    va_end(ap);
    return count;
}
