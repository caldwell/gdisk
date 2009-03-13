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
#include "device.h"
#include "gpt.h"
#include "mbr.h"
#include "autolist.h"
#include "csprintf.h"
#include "gdisk.h"

autolist_define(command);

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
    while (1) {
        rl_completion_entry_function = (void*)command_completion; // rl_completion_entry_function is defined to return an int??
        line = readline("gdisk> ");
        if (!line)
            break;
        add_history(line);
        char *l = line;
        while (isspace(*l)) l++;
        foreach_autolist(struct command *c, command)
            if (strcasecmp(c->name, l) == 0) {
                int args;
                for (args=0; c->arg[args].name; args++) {}
                char **argv = xcalloc(args+1, sizeof(*argv));
                char **v = argv;
                *v++ = xstrdup(c->name);
                for (int a=0; a<args; a++, v++) {
                    char *prompt = NULL;
                    asprintf(&prompt, "%s: %s ", c->name, c->arg[a].help);
                    if (!prompt) err(ENOMEM, "No memory for argument prompt");

                    if (C_Type(c->arg[a].type) == C_File)
                        rl_completion_entry_function = (void*)rl_filename_completion_function;
                    else if (C_Type(c->arg[a].type) == C_Partition_Type)
                        rl_completion_entry_function = (void*)partition_type_completion;

                    *v = readline(prompt);
                    if (!*v) goto done;
                    free(prompt);

                    if (C_Type(c->arg[a].type) == C_Flag &&
                        strcasecmp(*v, "y")   != 0 &&
                        strcasecmp(*v, "yes") != 0) {
                        free(*v);
                        *v = NULL;
                    }
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
    for (int i=0; gpt_partition_type[i].name; i++)
        if (strncmp(text, gpt_partition_type[i].name, strlen(text)) == 0 &&
            i++ == state)
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
    memcpy(t.header,
           &(struct gpt_header) {
               .signature = "EFI PART",
               .revision = PARTITION_REVISION,
               .header_size = sizeof(struct gpt_header),
               .my_lba = 1,
               .alternate_lba = dev->sector_count-1,
               .first_usable_lba = 2                  + partition_sectors,
               .last_usable_lba = dev->sector_count-1 - partition_sectors,
#warning "Implement new_guid()"
//               .disk_guid = new_guid(),
               .partition_entry_lba = 2,
               .partition_entries = partitions,
               .partition_entry_size = sizeof(struct gpt_partition),
           },
           sizeof(struct gpt_header));
    memcpy(t.alt_header, t.header, sizeof(struct gpt_header));
    uint32_t alt = t.alt_header->alternate_lba;
    t.alt_header->alternate_lba = t.alt_header->my_lba;
    t.alt_header->my_lba = alt;
    t.partition = alloc_sectors(dev, partition_sectors);
    // Even a blank MBR should preserve the boot code.
    struct mbr mbr = read_mbr(dev);
    memcpy(t.mbr.code, mbr.code, sizeof(mbr.code));
    return t;
}

static struct partition_table read_table(struct device *dev)
{
    struct partition_table t = {};
    t.dev = dev;
    t.header = get_sectors(dev,1,1);
    t.alt_header = get_sectors(dev,t.header->alternate_lba,1);
    gpt_header_to_host(t.header);
    gpt_header_to_host(t.alt_header);

#warning "assert(t.header->partition_entry_lba == 2)"

    if (memcmp(t.header->signature, "EFI PART", sizeof(t.header->signature)) != 0) {
        fprintf(stderr, "Missing signature in GPT header. Assuming blank partiton\n");
        free_table(t);
        return blank_table(dev);
    }

    if (sizeof(struct gpt_partition) != t.header->partition_entry_size)
        err(EINVAL, "Size of partition entries are %d instead of %td", t.header->partition_entry_size, sizeof(struct gpt_partition));

    t.partition = get_sectors(dev, 2, divide_round_up(t.header->partition_entry_size * t.header->partition_entries,dev->sector_size));
    gpt_partition_to_host(t.partition, t.header->partition_entries);

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

static struct partition_table dup_table(struct partition_table t)
{
    struct partition_table dup;
    dup = t;
    dup.header = memdup(t.header, t.dev->sector_size);
    dup.alt_header = memdup(t.alt_header, t.dev->sector_size);
    dup.partition = memdup(t.partition, partition_sectors(t) * t.dev->sector_size);
    return dup;
}

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
    if (info)  fclose(front);
    return err;
}

int command_export(char **arg)
{
    return export_table(g_table, arg[1]);
}
command_add("export", command_export, "Save table to a file (not to a device)",
            command_arg("filename", C_File, "Enter filename:"));


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

static float human_number(long long x)
{
    float n = x;
    while (n > 1024)
        n /= 1024;
    return n;
}
static char *human_units(long long x)
{
    char *units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
    for (char **u = units; ; x /= 1024, u++)
        if (x < 1024)
            return *u;
}
#define human_format(x) human_number(x), human_units(x)

static int command_print(char **arg)
{
    printf("%s:\n", g_table.dev->name);
    printf("  Disk GUID: %s\n", guid_str(g_table.header->disk_guid));
    printf("  %lld %ld byte sectors for %lld total bytes (%.2f %s capacity)\n",
           g_table.dev->sector_count,  g_table.dev->sector_size,
           g_table.dev->sector_count * g_table.dev->sector_size,
           human_format(g_table.dev->sector_count * g_table.dev->sector_size));
    printf("  MBR partition table is %s synced to the GPT table\n", g_table.options.mbr_sync ? "currently" : "not");
    printf("\n    %3s) %14s %14s %26s %s\n", "###", "Start LBA", "End LBA", "Size", "GUID");
    printf("    %.98s\n", "----------------------------------------------------------------------------------------------------------------");
    for (int i=0; i<g_table.header->partition_entries; i++) {
        if (memcmp(gpt_partition_type_empty, g_table.partition[i].partition_type, sizeof(gpt_partition_type_empty)) == 0)
            continue;
        printf("    %3d) %14"PRId64" %14"PRId64" %14"PRId64" (%6.2f %2s) %s\n", i,
               g_table.partition[i].first_lba, g_table.partition[i].last_lba,
               (g_table.partition[i].last_lba - g_table.partition[i].first_lba) * g_table.dev->sector_size,
               human_format((g_table.partition[i].last_lba - g_table.partition[i].first_lba) * g_table.dev->sector_size),
               guid_str(g_table.partition[i].partition_guid));
    }
    printf("\n    %3s) %-20s %s %-3s %-26s\n", "###", "Flags", "B", "MBR", "GPT Type");
    printf("    %.98s\n", "----------------------------------------------------------------------------------------------------------------");
    for (int i=0; i<g_table.header->partition_entries; i++) {
        if (memcmp(gpt_partition_type_empty, g_table.partition[i].partition_type, sizeof(gpt_partition_type_empty)) == 0)
            continue;
        printf("    %3d) %-20s ", i, "none");
        if (g_table.options.mbr_sync) {
            for (int m=0; m<lengthof(g_table.alias); m++)
                if (g_table.alias[m] == i) {
                    printf("%1s %02x  ", g_table.mbr.partition[m].status & MBR_STATUS_BOOTABLE ? "*" : "",
                           g_table.mbr.partition[m].partition_type);
                    goto mbr_printed;
                }
        }
        printf("%1s %3s ","","");
      mbr_printed:;
        int found=0;
        for (int t=0; gpt_partition_type[t].name; t++)
            if (memcmp(gpt_partition_type[t].guid, g_table.partition[i].partition_type, sizeof(g_table.partition[i].partition_type)) == 0) {
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
    printf("partition_crc        = %08x\n", header->partition_crc);
}

static int command_dump_header(char **arg)
{
    dump_header(arg[1] ? g_table.alt_header : g_table.header);
    return 0;
}
command_add("debug-dump-gpt-header", command_dump_header, "Dump GPT header structure",
            command_arg("alt", C_Flag, "Display the alternate partition header?"));

static void dump_partition(struct gpt_partition *p)
{
    printf("partition_type = %s\n",   guid_str(p->partition_type));
    for (int i=0; gpt_partition_type[i].name; i++)
        if (memcmp(gpt_partition_type[i].guid, p->partition_type, sizeof(p->partition_type)) == 0)
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
