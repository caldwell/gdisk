//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
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
#include "xmem.h"
#include "dalloc.h"
#include "gdisk.h"

autolist_define(command);

static int run_command(char *line, char **final_line);
static struct partition_table read_table(struct device *dev);
static void free_table(struct partition_table t);
static char *command_completion(const char *text, int state);
static char *partition_type_completion(const char *text, int state);
static char *partition_size_completion(const char *text, int state);
struct free_space {
    uint64_t first_lba;
    uint64_t blocks;
};
static struct free_space *find_free_spaces(struct partition_table unsorted);
static struct free_space largest_free_space(struct partition_table unsorted);
static void dump_dev(struct device *dev);
static void dump_header(struct gpt_header *header);
static void dump_partition(struct gpt_partition *p);
static size_t sncatprintf(char *buffer, size_t space, char *format, ...) __attribute__ ((format (printf, 3, 4)));
static char *tr(char *in, char *from, char *to);
static char *trdup(char *in, char *from, char *to);
static char *ctr(char *in, char *from, char *to);
static char *trim(char *s);

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
        usage(v[0], 1);

    struct device *dev = open_device(device_name);
    if (!dev)
        err(0, "Couldn't find device %s", v[1]);
    if (dev->sector_size < 512)
        err(0, "Disk has a sector size of %lu which is not big enough to support an MBR which I don't support yet.", dev->sector_size);
    g_table = read_table(dev);

    char *line, *final_line;
    int status = 0;
    do {
        rl_completion_entry_function = (void*)command_completion; // rl_completion_entry_function is defined to return an int??
        rl_completion_append_character = ' ';
        line = readline("gdisk> ");
        if (!line)
            break;
        status = run_command(line, &final_line);
        add_history(final_line);
        free(line);
        free(final_line);
    } while (status != ECANCELED); // Special case meaning Quit!
    printf("\nQuitting without saving changes.\n");

    free_table(g_table);

    //if (!write_mbr(dev, mbr))
    //    warn("Couldn't write MBR sector");
}

char *next_word(char **line)
{
    if (!*line) return NULL;
    if (**line == '"') {
        char *s = ++*line; // skip quote
        while (**line != '"' && **line != '\0')
            ++*line;
        if (**line)
            *(*line)++ = '\0';
        if (**line == '\0')
            *line = NULL;
        return s;
    }
    return strsep(line, " \t\f\r\n");
}

static char **parse_command(char *line)
{
    char **v = dcalloc(1, sizeof(char*));
    int c = 0;

    char *rest = line;
    char *word;
    while (word = next_word(&rest)) {
        if (!*word) continue; // compact multiple consecutive separators
        v = drealloc(v, sizeof(char *) * (c+2));
        v[c] = word;
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

static int run_command(char *line, char **final_line)
{
    if (final_line) *final_line = xstrdup(line);
    dalloc_start();
    int status = 0;
    char **cmdv = NULL;
    char **argv = parse_command(line);
    int argc;
    for (argc=0; argv[argc]; argc++) {}
    if (!argc) goto done; // Blank line

    for (char **v = argv; *v; v++)
        if (strchr(*v, '"')) {
            fprintf(stderr, "Sorry, embedded quotes don't work (yet?). If you are trying to do\n"
                    "'--arg=\"a b\"' then do '--arg \"a b\"' or '\"--arg=a b\"' instead.\n"
                    "Yeah, that sucks. Send me a patch if it bugs you that much.\n");
            status = EINVAL;
            goto done;
        }

    struct command *c = find_command(argv[0]);
    if (!c) {
        printf("Command not found: '%s'\n", argv[0]);
        status = EINVAL;
        goto done;
    }

    int args;
    for (args=0; c->arg[args].name; args++) {}

    cmdv = dcalloc(1+args+1, sizeof(*argv));
    cmdv[0] = argv[0];
    for (int i=1; i<argc; i++) {
        if (argv[i][0] == '-') {
            char *rest = argv[i];
            char *name = strsep(&rest, "=");
            while (*name == '-') name++;
            for (int a=0; a<args; a++)
                if (strcmp(c->arg[a].name, name) == 0) {
                    char **arg = &cmdv[a+1];
                    if (c->arg[a].type == C_Flag)
                        *arg = argv[i];
                    else if (!rest  || !*rest)
                        if (i+1 >= argc) {
                            fprintf(stderr, "Missing parameter for argument \"%s\"\n", name);
                            status = EINVAL;
                            goto done;
                        } else
                            *arg = argv[++i];
                    else
                        *arg = rest;
                    goto found;
                }
            fprintf(stderr, "Command \"%s\" has no such argument \"%s\". Try 'help %s'\n", c->name, name, c->name);
            status = EINVAL;
            goto done;
        } else {
            for (int o=1; o<args+1; o++)
                if (!cmdv[o] && c->arg[o-1].type != C_Flag) {
                    cmdv[o] = argv[i];
                    goto found;
                }
            fprintf(stderr, "Too many arguments for command '%s'\n", argv[0]);
            status = EINVAL;
            goto done;
        }
      found:;
    }

    char **v = cmdv + 1; // start past command name.
    for (int a=0; a<args; a++, v++) {
        if (*v) continue; // Don't prompt for args entered on command line.
        if (c->arg[a].type & C_Optional)
            continue;
        char *prompt = dsprintf("%s: Enter %s: ", c->name, c->arg[a].help);
        if (!prompt) err(ENOMEM, "No memory for argument prompt");

        if (C_Type(c->arg[a].type) == C_File)
            rl_completion_entry_function = (void*)rl_filename_completion_function;
        else if (C_Type(c->arg[a].type) == C_Partition_Type)
            #warning "BUG: partition type completion doesn't do spaces right"
            rl_completion_entry_function = (void*)partition_type_completion;
        else if (C_Type(c->arg[a].type) == C_FreeSpace)
            rl_completion_entry_function = (void*)partition_size_completion;
        // rl_completer_quote_characters = "\"'";
        // rl_basic_word_break_characters = "";
        rl_completion_append_character = '\0';

        *v = dalloc_remember(readline(prompt));
        if (!*v) goto done;
        *v = trim(*v);

        if (final_line) {
            *final_line = xstrcat(*final_line, " ");
            *final_line = xstrcat(*final_line, *v);
        }
    }

    status = c->handler(cmdv);

  done:
    dalloc_free();
    return status;
}

static char *arg_name(struct command_arg_ *arg)
{
    return dsprintf("%s%s%s",
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

static char *partition_size_completion(const char *text, int state)
{
    struct free_space *space = find_free_spaces(g_table);
    char *size = NULL;
    for (int i=0, s=0; i<space[i].blocks; i++) {
        char *_size = NULL; asprintf(&_size, "%"PRId64, space[i].blocks * g_table.dev->sector_size);
        if (strncasecmp(text, _size, strlen(text)) == 0 && s++ == state) {
            size = _size;
            break;
        }
        free(_size);
    }
    free(space);
    return size;
}

static char *partition_type_completion(const char *text, int state)
{
    for (int i=0, s=0; gpt_partition_type[i].name; i++) {
        char *_name = trdup(gpt_partition_type[i].name, " ", "_");
        if (strncasecmp(text, _name, strlen(text)) == 0 &&
            s++ == state)
            return _name;
        free(_name);
    }
    return NULL;
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

static void utf16_from_ascii(uint16_t *utf16le, char *ascii, int n)
{
    if (!n) return;
    while (--n && (*utf16le++ = *ascii++)) {}
    *utf16le = '\0';
}

static bool partition_entry_is_representable_in_mbr(struct gpt_partition entry)
{
    // MBR only has 32 bits for the LBA. So if the partition is further up the disk than that then it can't be represented in the MBR.
    return !guid_eq(gpt_partition_type_empty, entry.partition_type) &&
           entry.first_lba < 0x100000000LL &&
           entry.last_lba - entry.first_lba < 0x100000000LL;
}

static void create_mbr_alias_table(struct partition_table *t)
{
    int synced = -1;
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
                if (synced < 0) synced = 1;
                break;
            }
        if (t->alias[mp] == -1)
            synced = 0;
    }
    t->options.mbr_sync = synced == 1;
}

static unsigned long partition_sectors(struct partition_table t)
{
    return divide_round_up(t.header->partition_entry_size * t.header->partition_entries, t.dev->sector_size);
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

    return t;
}

static int command_clear_table(char **arg)
{
    struct mbr mbr = g_table.mbr;
    g_table = blank_table(g_table.dev);
    g_table.mbr = mbr;
    return 0;
}
command_add("clear-table", command_clear_table, "Clear out GPT partition table for a nice fresh start.");

static struct mbr blank_mbr(struct device *dev)
{
    struct mbr mbr = { .mbr_signature = MBR_SIGNATURE };
    // Even a blank MBR should preserve the boot code.
    struct mbr code = read_mbr(dev);
    memcpy(mbr.code, code.code, sizeof(mbr.code));
    return mbr;
}

static int command_clear_mbr(char **arg)
{
    g_table.mbr = blank_mbr(g_table.dev);
    create_mbr_alias_table(&g_table);
    return 0;
}
command_add("clear-mbr", command_clear_mbr, "Clear out MBR partition table and start anew.");

static int command_create_protective_mbr(char **arg)
{
    g_table.mbr = blank_mbr(g_table.dev);
    g_table.mbr.partition[0] = (struct mbr_partition) {
        .first_sector_lba = 1,
        .sectors = g_table.dev->sector_count-1,
        .partition_type = 0xee,
    };
    create_mbr_alias_table(&g_table);
    return 0;
}
command_add("create-protective-mbr", command_create_protective_mbr, "Replace MBR entries with a protective MBR as per the EFI spec.");

static int command_add_protective_mbr_partition(char **arg)
{
    struct partition_table *t = &g_table;
    for (int i=0; i < lengthof(t->mbr.partition); i++)
        if (t->mbr.partition[i].partition_type == 0) {
            t->mbr.partition[i] = (struct mbr_partition) {
                .first_sector_lba = 1,
                .sectors = g_table.header->first_usable_lba-1-1/*mbr*/,
                .partition_type = 0xee,
            };
            return 0;
        }
    fprintf(stderr, "No free MBR partitions found.\n");
    return ENOSPC;
}
command_add("add-protective-mbr-partition", command_add_protective_mbr_partition, "Add a protective MBR entry that just covers the EFI partitioning sectors.");

static struct partition_table gpt_table_from_mbr(struct mbr mbr, struct device *dev)
{
    struct partition_table t = blank_table(dev);
    t.mbr = mbr;
    for (int mp=0,gp=0; mp<lengthof(t.mbr.partition); mp++) {
        if (t.mbr.partition[mp].partition_type) {
            if (t.mbr.partition[mp].partition_type == 0xee)
                continue;

            t.partition[gp].first_lba = t.mbr.partition[mp].first_sector_lba;
            t.partition[gp].last_lba  = t.mbr.partition[mp].first_sector_lba + t.mbr.partition[mp].sectors - 1;

            if (t.partition[gp].first_lba < t.header->first_usable_lba ||
                t.partition[gp].last_lba > t.header->last_usable_lba)
                printf("ouch, mbr partition %d [%"PRId64"d,%"PRId64"d] outside of usable gpt space [%"PRId64"d,%"PRId64"d]\n",
                       mp+1, t.partition[gp].first_lba, t.partition[gp].last_lba, t.header->first_usable_lba, t.header->last_usable_lba);

            utf16_from_ascii(t.partition[gp].name, csprintf("MBR %d\n", mp+1), lengthof(t.partition[gp].name));

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
    struct device *dev = g_table.dev;
    struct mbr mbr = g_table.mbr;
    free_table(g_table);
    g_table = gpt_table_from_mbr(mbr, dev);
    return 0;
}
command_add("init-gpt-from-mbr", gpt_from_mbr, "(Re)create GPT partition table using data from the MBR partition table");

static int recreate_gpt(char **arg)
{
    struct mbr mbr = g_table.mbr;
    struct partition_table new_table = blank_table(g_table.dev);
    new_table.header->disk_guid = new_table.alt_header->disk_guid = g_table.header->disk_guid;
    assert(new_table.header->partition_entries == g_table.header->partition_entries);
    struct partition_table gt = g_table;
    g_table.header = new_table.header;
    g_table.alt_header = new_table.alt_header;
    new_table.header = gt.header;
    new_table.alt_header = gt.alt_header;
    free_table(new_table);
    update_table_crc(&g_table);
    return 0;
}
command_add("recreate-gpt", recreate_gpt, "Recreate GPT partition table using partitions in current GPT. Useful for resizing disks.");

static struct partition_table read_gpt_table(struct device *dev)
{
    struct partition_table t = {};
    t.dev = dev;

    bool primary_valid = true, alternate_valid = true;

#define header_error(format, ...) ({                        \
            fprintf(stderr, format, ##__VA_ARGS__);         \
            fprintf(stderr, ". Assuming blank partition...\n");   \
            free_table(t);                                  \
            blank_table(dev);                               \
        })

#define header_warning(format, ...) ({              \
            fprintf(stderr, "Warning: ");           \
            fprintf(stderr, format, ##__VA_ARGS__); \
            fprintf(stderr, ".\n");                 \
        })

#define header_corrupt(which, format, ...) ({                             \
        header_warning(format, ##__VA_ARGS__);                            \
        which ## _valid = false;                                          \
        if (!primary_valid && !alternate_valid)                           \
            return header_error("There were no valid GPT headers found"); \
        })

    t.header = get_sectors(dev,1,1);

    if (memcmp(t.header->signature, "EFI PART", sizeof(t.header->signature)) != 0)
        header_corrupt(primary, "Missing signature in primary GPT header");
    else
        gpt_header_to_host(t.header);

    t.alt_header = get_sectors(dev, primary_valid ? t.header->alternate_lba : dev->sector_count-1, 1);

    if (memcmp(t.alt_header->signature, "EFI PART", sizeof(t.alt_header->signature)) != 0)
        header_corrupt(alternate, "Missing signature in altername GPT header");
    else
        gpt_header_to_host(t.alt_header);

    if (primary_valid && t.header->header_size != sizeof(struct gpt_header))
        header_corrupt(primary, "Partition header is %d bytes long instead of %zd", t.header->header_size, sizeof(struct gpt_header));

    if (alternate_valid && t.alt_header->header_size != sizeof(struct gpt_header))
        header_corrupt(alternate, "Partition header is %d bytes long instead of %zd", t.header->header_size, sizeof(struct gpt_header));

    if (primary_valid && t.header->partition_entry_size != sizeof(struct gpt_partition))
        header_corrupt(primary, "Size of partition entries are %d instead of %zd", t.header->partition_entry_size, sizeof(struct gpt_partition));

    if (alternate_valid && t.alt_header->partition_entry_size != sizeof(struct gpt_partition))
        header_corrupt(alternate, "Size of partition entries are %d instead of %zd", t.alt_header->partition_entry_size, sizeof(struct gpt_partition));

    uint64_t primary_lba,alternate_lba;
    if (primary_valid && alternate_valid && t.header->my_lba != t.alt_header->alternate_lba) {
        header_warning("Header LBA is %"PRId64" but alternate header claims it is %"PRId64"", t.header->my_lba, t.header->alternate_lba);
        if (t.header->my_lba == 1 || t.alt_header->alternate_lba == 1)
            primary_lba = 1; // It *really* should be one.
        else
            primary_lba = t.header->my_lba; // In any other screwball case, trust the primary header.
    } else
        primary_lba = primary_valid ? t.header->my_lba : t.alt_header->alternate_lba;

    if (primary_lba != 1)
        header_warning("Header LBA is %"PRId64" and not 1", primary_lba);

    if (primary_valid && alternate_valid && t.header->alternate_lba != t.alt_header->my_lba) {
        header_warning("Primary header says Alternate header LBA is %"PRId64" but alternate header claims it is %"PRId64"", t.header->alternate_lba, t.header->my_lba);
        if (t.header->my_lba == dev->sector_count-1 || t.alt_header->alternate_lba == dev->sector_count-1)
            alternate_lba = dev->sector_count-1;
        else
            alternate_lba = t.header->alternate_lba; // In any other screwball case, trust the primary header.
    } else
        alternate_lba = primary_valid ? t.header->alternate_lba : t.alt_header->my_lba;

    if (alternate_lba != dev->sector_count-1)
        header_warning("Alternate header LBA is %"PRId64" and not at the end of the disk (LBA: %llu)", alternate_lba, dev->sector_count-1);

    // Technically we can guess the start lba and check the validity of the opposite table if the one we're looking at doesn't CRC..
    if (primary_valid) {
        if (t.header->partition_entries * t.header->partition_entry_size / dev->sector_size > dev->sector_count/2)
            header_corrupt(primary, "The number of partition_entries is ludicrous: %d", t.header->partition_entries);
        else {
            t.partition = get_sectors(dev, t.header->partition_entry_lba, divide_round_up(t.header->partition_entry_size * t.header->partition_entries,dev->sector_size));
            gpt_partition_to_host(t.partition, t.header->partition_entries);

            if (!gpt_crc_valid(t.header, t.partition)) {
                header_warning("Header CRC is not valid. Fixing.");
                update_table_crc(&t);
            }
        }
    } else if (alternate_valid) {
        if (t.alt_header->partition_entries * t.alt_header->partition_entry_size / dev->sector_size > dev->sector_count/2)
            header_corrupt(alternate, "The number of partition_entries is ludicrous: %d", t.alt_header->partition_entries);
        else {
            t.partition = get_sectors(dev, t.alt_header->partition_entry_lba, divide_round_up(t.alt_header->partition_entry_size * t.alt_header->partition_entries,dev->sector_size));
            gpt_partition_to_host(t.partition, t.alt_header->partition_entries);

            if (!gpt_crc_valid(t.alt_header, t.partition)) {
                header_warning("Alt Header CRC is not valid. Fixing.");
                update_table_crc(&t);
            }
        }
    }

    #warning "TODO: Sanity check first_usable_lba and last_usable_lba."

    // If one is bad, fix it from the other one...
    if (primary_valid && !alternate_valid) {
        *t.alt_header = *t.header;
        uint64_t temp = t.alt_header->my_lba;
        t.alt_header->my_lba = t.alt_header->alternate_lba;
        t.alt_header->alternate_lba = temp;
        t.alt_header->partition_entry_lba = t.alt_header->last_usable_lba + 1;
    } else if (!primary_valid && alternate_valid) {
        *t.header = *t.alt_header;
        uint64_t temp = t.header->my_lba;
        t.header->my_lba = t.header->alternate_lba;
        t.header->alternate_lba = temp;
        t.header->partition_entry_lba = t.header->my_lba + 1;
    }

    #warning "TODO: Capture both sets of partition tables in case on has a bad crc."

    return t;
}

static struct partition_table read_table(struct device *dev)
{
    struct partition_table t = read_gpt_table(dev);

    t.mbr = read_mbr(dev);

    create_mbr_alias_table(&t);

    return t;
}

#warning "TODO: Add 'fix' command that moves alternate partition and header to end of disk"
#warning "TODO: Add 'new-guids' command (with better name) that recreates all the guids in the table (run it after an image copy)"

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
    dup.header = xmemdup(t.header, t.dev->sector_size);
    dup.alt_header = xmemdup(t.alt_header, t.dev->sector_size);
    dup.partition = xmemdup(t.partition, partition_sectors(t) * t.dev->sector_size);
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

static struct free_space *find_free_spaces(struct partition_table unsorted)
{
    // To make this sane we sort the partition table first.
    struct partition_table t = dup_table(unsorted);
    compact_and_sort(&t);

    uint64_t free_start = t.header->first_usable_lba;
    struct free_space *space = malloc(sizeof(*space)*(t.header->partition_entries+1+1/*NULL*/));
    int s=0;
    for (int p=0; p<t.header->partition_entries; p++) {
        if (guid_eq(gpt_partition_type_empty, t.partition[p].partition_type))
            break;
        if (t.partition[p].first_lba - free_start > 0)
            space[s++] = (struct free_space) { .blocks = t.partition[p].first_lba - free_start,
                                               .first_lba = free_start };
        free_start = t.partition[p].last_lba + 1;
    }
    // Check for it fitting in the end (probably the most common case)
    if (t.header->last_usable_lba+1 - free_start > 0)
        space[s++] = (struct free_space) { .blocks = t.header->last_usable_lba+1 - free_start,
                                           .first_lba = free_start };
    space[s++] = (struct free_space) { };

    free_table(t);
    return space;
}

static uint64_t find_free_space(struct partition_table unsorted, uint64_t blocks)
{
    uint64_t start = -1LL;
    struct free_space *space = find_free_spaces(unsorted);
    for (int i=0; i<space[i].blocks; i++)
        if (space[i].blocks >= blocks) {
            start = space[i].first_lba;
            break;
        }
    free(space);
    return start;
}

static struct free_space largest_free_space(struct partition_table unsorted)
{
    struct free_space found = { .blocks = 0 };
    struct free_space *space = find_free_spaces(unsorted);
    for (int i=0; i<space[i].blocks; i++)
        if (space[i].blocks >= found.blocks)
            found = space[i];
    free(space);
    return found;
}


static GUID type_guid_from_string(char *s)
{
    for (int t=0; gpt_partition_type[t].name; t++) {
        if (strcasecmp(s, ctr(gpt_partition_type[t].name, " ", "_")) == 0)
            return gpt_partition_type[t].guid;
    }
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
    if (!size) {
        struct free_space largest = largest_free_space(g_table);
        if (!largest.blocks) {
            fprintf(stderr, "There is no free space left!\n");
            return ENOSPC;
        }
        blocks = largest.blocks;
        size = blocks * g_table.dev->sector_size;
        part.first_lba = largest.first_lba;
        printf("Largest unused space: %"PRId64" blocks (%"PRId64", %s) at block %"PRId64".\n",
               blocks, size, human_string(size), part.first_lba);
    } else
        part.first_lba = arg[4] ? strtoull(arg[4], NULL, 0) : find_free_space(g_table, blocks);
    if (part.first_lba == -1LL) {
        fprintf(stderr, "Couldn't find %"PRId64" blocks (%"PRId64", %s) of free space.\n", blocks, size, human_string(size));
        return ENOSPC;
    }
    part.last_lba = arg[5] ? strtoull(arg[5], NULL, 0) : part.first_lba + blocks - 1;
    part.attributes = 0 | (arg[6] ? PA_SYSTEM_PARTITION : 0);
    utf16_from_ascii(part.name, arg[3] ? arg[3] : "", lengthof(part.name));

    dump_partition(&part);

    struct gpt_partition *p = find_unused_partition(g_table);
    if (!p) {
        fprintf(stderr, "Partition table is full.\n");
        return ENOSPC;
    }

    *p = part;

    update_table_crc(&g_table);

    if (g_table.options.mbr_sync)
        sync_partition_to_mbr(&g_table, p - g_table.partition);

    return 0;
}

command_add("new", command_create_partition, "Create a new partition entry in the table",
            command_arg("type",      C_Partition_Type,    "Type of new partition"),
            command_arg("size",      C_FreeSpace,         "Size of the new partition"),
            command_arg("label",     C_String|C_Optional, "The name of the new partition"),
            command_arg("first_lba", C_String|C_Optional, "The first block of the new partition"),
            command_arg("last_lba",  C_String|C_Optional, "The last block of the new partition (this overrides the size argument)"),
            command_arg("system",    C_Flag,              "Set the \"System Partition\" attribute"),
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

static int choose_partition(char *string)
{
    int index = strtol(string, NULL, 0);
    if (index < 0 || index >= g_table.header->partition_entries) {
        fprintf(stderr, "Bad index '%d'. Should be between 0 and %d (inclusive).\n", index, g_table.header->partition_entries-1);
        return -1;
    }
    if (guid_eq(g_table.partition[index].partition_type, gpt_partition_type_empty)) {
        fprintf(stderr, "Partition '%d' is empty.\n", index);
        return -1;
    }
    return index;
}

static int command_delete_partition(char **arg)
{
    int index = choose_partition(arg[1]);
    if (index < 0) return EINVAL;

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
command_add("init-mbr-from-gpt", command_sync_mbr, "(Re)create MBR partition table using data from the GPT partition table",
            command_arg("force",     C_Flag, "Force a re-sync if the MBR already looks synced"));


static int command_edit(char **arg)
{
    int index = choose_partition(arg[1]);
    if (index < 0) return EINVAL;

    if (arg[2]) {
        GUID type = type_guid_from_string(arg[2]);
        if (guid_eq(bad_guid, type)) {
            fprintf(stderr, "Not a valid type string or unknown GUID format: \"%s\"\n", arg[2]);
            return EINVAL;
        }
        g_table.partition[index].partition_type = type;

        int mbr_alias = get_mbr_alias(g_table, index);
        int mbr_type = find_mbr_equivalent(type);
        if (g_table.options.mbr_sync && mbr_alias != -1 && mbr_type)
            g_table.mbr.partition[mbr_alias].partition_type = mbr_type;
        update_table_crc(&g_table);
    }

    if (arg[3]) {
        utf16_from_ascii(g_table.partition[index].name, arg[3], lengthof(g_table.partition[index].name));
        update_table_crc(&g_table);
    }

    if (arg[4]) {
        GUID guid = guid_from_string(arg[4]);
        if (guid_eq(bad_guid, guid)) {
            fprintf(stderr, "Bad GUID: \"%s\"\n", arg[4]);
            return EINVAL;
        }
        g_table.partition[index].partition_guid = guid;
        update_table_crc(&g_table);
    }

    return 0;
}
command_add("edit", command_edit, "Change parts of a partition",
            command_arg("index",     C_Number,                    "The index number of the partition. The first partitiion is partition zero"),
            command_arg("type",      C_Partition_Type|C_Optional, "Type of partition"),
            command_arg("label",     C_String|C_Optional,         "The name of the new partition"),
            command_arg("guid",      C_String|C_Optional,         "The GUID of the new partition"));

static int command_edit_attributes(char **arg)
{
    int index = choose_partition(arg[1]);
    if (index < 0) return EINVAL;

    uint64_t val = strcmp(arg[3], "system") == 0 ? PA_SYSTEM_PARTITION : strtoull(arg[3], NULL, 0);

    if      (strcmp(arg[2], "set") == 0)
        g_table.partition[index].attributes |=  val;
    else if (strcmp(arg[2], "clear") == 0)
        g_table.partition[index].attributes &= ~val;
    else {
        fprintf(stderr, "command \"%s\" is not \"set\" or \"clear\".\n", arg[2]);
        return EINVAL;
    }
    printf("Attributes is now %"PRIx64" after %s %016"PRIx64"\n",
           g_table.partition[index].attributes, strcmp(arg[2], "set") == 0 ? "setting" : "clearing", val);

    return 0;
}
command_add("edit-attributes", command_edit_attributes, "Change parts of a partition",
            command_arg("index",      C_Number, "The index number of the partition. The first partitiion is partition zero"),
            command_arg("command",    C_String, "\"set\" or \"clear\" -- what you want to do to the bits"),
            command_arg("attributes", C_String, "The bits you want to set or clear as a number (or \"system\" for \"System Partition\" attribute)"));


struct write_vec {
    void *buffer;
    unsigned long long block;
    unsigned long long blocks;
    char *name;
};

struct write_image {
    struct write_vec vec[10];
    int count;
};

struct write_image image_from_table(struct partition_table t)
{
    struct write_image image = { .count = 5 };

    // *.front: mbr, then gpt header, then partitions
    image.vec[0] = (struct write_vec) {
        .buffer = sector_from_mbr(t.dev, t.mbr),
        .block  = 0,
        .blocks = 1,
        .name = xstrdup("mbr"),
    };

    void *buffer = xcalloc(1, t.dev->sector_size);
    gpt_header_from_host(t.header);
    memcpy(buffer, t.header, sizeof(*t.header));
    gpt_header_to_host(t.header);

    image.vec[1] = (struct write_vec) {
        .buffer = buffer,
        .block  = t.header->my_lba,
        .blocks = 1,
        .name = xstrdup("gpt_header"),
    };

    buffer = xcalloc(t.header->partition_entries, t.dev->sector_size);
    gpt_partition_from_host(t.partition, t.header->partition_entries);
    memcpy(buffer, t.partition, sizeof(*t.partition) * t.header->partition_entries);
    gpt_partition_to_host(t.partition, t.header->partition_entries);

    image.vec[2] = (struct write_vec) {
        .buffer = buffer,
        .block  = t.header->partition_entry_lba,
        .blocks = partition_sectors(t),
        .name = xstrdup("gpt_partitions"),
    };

    image.vec[3] = (struct write_vec) {
        .buffer = xmemdup(buffer, t.header->partition_entries * t.dev->sector_size),
        .block  = t.alt_header->partition_entry_lba,
        .blocks = partition_sectors(t),
        .name = xstrdup("alt_gpt_partitions"),
    };

    buffer = xcalloc(1, t.dev->sector_size);
    gpt_header_from_host(t.alt_header);
    memcpy(buffer, t.alt_header, sizeof(*t.alt_header));
    gpt_header_to_host(t.alt_header);

    image.vec[4] = (struct write_vec) {
        .buffer = buffer,
        .block  = t.alt_header->my_lba,
        .blocks = 1,
        .name = xstrdup("alt_gpt_header"),
    };

    return image;
}

static struct write_vec *find_vec_in_image(struct write_image *image, char *name)
{
    for (int i=0; i<image->count; i++)
        if (strcmp(image->vec[i].name, name) == 0)
            return &image->vec[i];
    return NULL;
}

static struct partition_table table_from_image(struct write_image image, struct device *dev)
{
    struct partition_table t = blank_table(dev);

#define find_vec(vec_name, vec_blocks) ({                               \
            struct write_vec *vec = find_vec_in_image(&image, vec_name); \
            if (!vec) {                                                 \
                fprintf(stderr, "Image is missing %s section.\n", vec_name); \
                free_table(t);                                          \
                return blank_table(dev);                                \
            }                                                           \
            if (vec->blocks != vec_blocks) {                            \
                fprintf(stderr, "Section %s is %lu blocks instead of %"PRIu64".\n", vec_name, (unsigned long) vec_blocks, vec->blocks); \
                free_table(t);                                          \
                return blank_table(dev);                                \
            }                                                           \
            vec;                                                        \
        })

    struct write_vec *mbr                = find_vec("mbr",                1);
    struct write_vec *gpt_header         = find_vec("gpt_header",         1);
    struct write_vec *alt_gpt_header     = find_vec("alt_gpt_header",     1);

    t.mbr = mbr_from_sector(mbr->buffer);

    memcpy(t.header, gpt_header->buffer, sizeof(t.header));
    gpt_header_to_host(t.header);

    memcpy(t.alt_header, alt_gpt_header->buffer, sizeof(t.alt_header));
    gpt_header_to_host(t.alt_header);

    struct write_vec *gpt_partitions     = find_vec("gpt_partitions",     partition_sectors(t));
    //struct write_vec *alt_gpt_partitions = find_vec("alt_gpt_partitions", partition_sectors(t));

    free(t.partition); // May be a different length, so reallocate it.
    t.partition = xmemdup(gpt_partitions->buffer, sizeof(*t.partition) * t.header->partition_entries);
    gpt_partition_to_host(t.partition, t.header->partition_entries);

    create_mbr_alias_table(&t);

    return t;
}

void free_image(struct write_image image)
{
    for (int i=0; i < image.count; i++) {
        free(image.vec[i].buffer);
        free(image.vec[i].name);
    }
}

static int export_image(struct write_image image, struct device *dev, char *filename)
{
    FILE *info = NULL, *data = NULL;
    int err = 0;
    if ((data = fopen(csprintf("%s.data", filename), "wb")) == NULL) { err = errno; warn("Couldn't open %s.data", filename); goto done; }
    if ((info = fopen(csprintf("%s.info", filename), "w"))  == NULL) { err = errno; warn("Couldn't open %s.info", filename); goto done; }

    fprintf(info, "# sector_size: %ld\n", dev->sector_size);
    fprintf(info, "# sector_count: %lld\n", dev->sector_count);
    fprintf(info, "# device: %s\n", dev->name);

    unsigned skip=0;
    for (int i=0; i < image.count; i++) {
        int wrote = fwrite(image.vec[i].buffer, dev->sector_size, image.vec[i].blocks, data);
        if (wrote != image.vec[i].blocks) {
            err = errno;
            warn("Couldn't write %s to %s.data", image.vec[i].name, filename);
            goto done;
        }
        fprintf(info, "# %s:\ndd if=\"%s.data\" of=\"%s\" bs=%ld skip=%d seek=%lld count=%lld\n",
                image.vec[i].name,
                filename, dev->name, dev->sector_size,
                skip, image.vec[i].block, image.vec[i].blocks);
        skip += image.vec[i].blocks;
    }

  done:
    if (data) fclose(data);
    if (info) fclose(info);
    return err;
}

static int export_table(struct partition_table t, char *filename)
{
    struct write_image image = image_from_table(t);
    int status = export_image(image, t.dev, filename);
    free_image(image);
    return status;
}

int command_export(char **arg)
{
    return export_table(g_table, arg[1]);
}
command_add("export", command_export, "Save table to a file (not to a device)",
            command_arg("filename", C_File, "Filename to as a base use for exported files"));

static int import_image(struct write_image *image_out, struct device *dev, char *filename)
{
    struct write_image image = { .count = 0 };
    FILE *info = NULL, *data = NULL;
    int err = 0;
    if ((data = fopen(csprintf("%s.data", filename), "rb")) == NULL) { err = errno; warn("Couldn't open %s.data", filename); goto done; }
    if ((info = fopen(csprintf("%s.info", filename), "r"))  == NULL) { err = errno; warn("Couldn't open %s.info", filename); goto done; }

    unsigned long sector_size=0;
    unsigned long long sector_count=0;
    char device[1000]="";

    char line[1000];
    char section[20];

    while(fgets(line, sizeof(line), info)) {
        if (!sector_size && sscanf(line, "# sector_size: %ld", &sector_size) == 1 && sector_size != dev->sector_size) {
            fprintf(stderr, "image file has a sector size of %ld but device %s wants %ld\n", sector_size, dev->name, dev->sector_size);
            err = EINVAL;
            goto done;
        }
        if (!sector_count && sscanf(line, "# sector_count: %lld", &sector_count) == 1 && sector_count != dev->sector_count) {
            fprintf(stderr, "image file has %lld LBAs but device %s has %lld\n", sector_count, dev->name, dev->sector_count);
            err = EINVAL;
            goto done;
        }
        if (!device[0] && sscanf(line, "# device: %1000s", device) == 1 && strcmp(device,dev->name) != 0)
            fprintf(stderr, "Warning: image file was created from %s but we're operating on %s.\n", device, dev->name);
        if (sscanf(line, "# %20s:", section) == 1)
            if (section[0] && section[strlen(section)-1] == ':')
                section[strlen(section)-1] = '\0';
        unsigned long long skip, seek;
        unsigned long count;
        if (sscanf(line, "dd if=\"%*s of=\"%*s bs=%*d skip=%llu seek=%llu count=%lu", &skip, &seek, &count) == 3) {
            if (image.count == lengthof(image.vec)) {
                err = ENOSPC;
                printf("Too many sections in %s.info\n", filename);
                goto done;
            }
            void *buf = alloc_sectors(dev, count);
            if (fseek(data, skip*dev->sector_size, SEEK_SET)) {
                warn("Couldn't seek to %lld in %s.data", skip*dev->sector_size, filename);
                goto done;
            }
            if (fread(buf, dev->sector_size, count, data) != count) {
                warn("Couldn't read %s from %s.data", section, filename);
                goto done;
            }
            image.vec[image.count++] = (struct write_vec) {
                .buffer = buf,
                .block  = seek,
                .blocks = count,
                .name = xstrdup(section),
            };
        }
    }

  done:
    if (data) fclose(data);
    if (info) fclose(info);
    if (!err) *image_out = image;
    else free_image(image);
    return err;
}

int command_import(char **arg)
{
    struct write_image image = {};
    int status = import_image(&image, g_table.dev, arg[1]);
    for (int i=0; i<image.count; i++)
        printf(" %d) %20s: %llu @ %llu\n", i, image.vec[i].name, image.vec[i].blocks, image.vec[i].block);
    struct device *dev = g_table.dev;
    free_table(g_table);
    g_table = table_from_image(image, dev);
    free_image(image);
    return status;
}
command_add("import", command_import, "Load table from a previously exported file",
            command_arg("filename", C_File, "Base filename to import (don't include .info or .data)"));

static struct write_image image_from_image(struct write_image image, struct device *dev)
{
    struct write_image on_disk = image;
    for (int i=0; i < on_disk.count; i++) {
        on_disk.vec[i].buffer = get_sectors(dev, on_disk.vec[i].block, on_disk.vec[i].blocks);
        on_disk.vec[i].name = xstrdup(on_disk.vec[i].name);
    }
    return on_disk;
}

static void dump_data(void *data, size_t length)
{
    const unsigned char zero[6/*lines*/ * 16] = { 0 };
    unsigned char *d = data;
    for (int i=0; i<length; i+=16) {
        int chunk=MIN(length,i+16);

        if (length-i >= sizeof(zero) && memcmp(zero, &d[i], sizeof(zero)) == 0) {
            int zeros=0;
            for (; i < round_down(length,16) && memcmp(zero, &d[i], 16) == 0; i+=16)
                zeros += 16;
            printf("    * Skipped %d zeros\n", zeros);
            i -= 16;
            continue;
        }
        
        printf("  ");
        for (int j=i; j<chunk; j++)
            printf("%02x ", d[j]);
        if (chunk%16)
            printf("%*s", (16-chunk%16)*3, "");
        for (int j=i; j<chunk; j++)
            printf("%c",  isprint(d[j]) ? d[j] : '.');
        printf("\n");
    }
}

static int write_table(struct partition_table t, bool force, bool dry_run, bool verbose)
{
    char *HOME = getenv("HOME");
    if (!HOME) {
        fprintf(stderr, "Couldn't find $HOME environment variable. Punting.\n");
        if (!force) return ECANCELED;
    }

    char *dev_name = xstrdup(t.dev->name);

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
    free(dev_name);

    struct write_image image = image_from_table(t);
    struct write_image backup = image_from_image(image, t.dev);
    int status = export_image(backup, t.dev, backup_path);
    if (status) {
        warn("Error writing backup of table");
        if (!force) return ECANCELED;
    }
    free_image(backup);

    int err = 0;
    for (int i=0; i<image.count; i++) {
        if (dry_run || verbose)
            printf("Writing %"PRIu64" blocks to LBA %"PRIu64"...\n", image.vec[i].blocks, image.vec[i].block);
        if (verbose)
            dump_data(image.vec[i].buffer, image.vec[i].blocks * t.dev->sector_size);
        if (!dry_run && !device_write(t.dev, image.vec[i].buffer, image.vec[i].block, image.vec[i].blocks)) {
            err = errno;
            warn("Error while writing %s to %s", image.vec[i].name, t.dev->name);
            if (i > 0)
                fprintf(stderr, "The partition table on your disk is now most likely corrupt.\n");
            break;
        }
    }
    free_image(image);
    return err;
}

int command_write(char **arg)
{
    int status = write_table(g_table, !!arg[1], !!arg[2], !!arg[3]);
    if (status == ECANCELED) {
        status = ENOENT; // ECANCELED will quit the program if we return it.
        fprintf(stderr, "Table not written because a backup of the existing data could not be made.\n"
                "Re-run with the --force option to save without a backup.\n");
    }
    return status;
}
command_add("write", command_write, "Write the partition table back to the disk",
            command_arg("force", C_Flag, "Force table to be written even if backup cannot be saved"),
            command_arg("dry-run", C_Flag, "Don't write the table, just print what we would do"),
            command_arg("verbose", C_Flag, "Dump all the data to the screen before writing"));

static char *_p_first_lba(struct gpt_partition *p, struct mbr_partition *m, struct device *dev) { return dsprintf("%14"PRId64"", p->first_lba); }
static char *_p_last_lba (struct gpt_partition *p, struct mbr_partition *m, struct device *dev) { return dsprintf("%14"PRId64"", p->last_lba); }
static char *_p_size     (struct gpt_partition *p, struct mbr_partition *m, struct device *dev) { return dsprintf("%14"PRId64" (%9s)", (p->last_lba - p->first_lba + 1) * dev->sector_size,
                                                                                                                          human_string((p->last_lba - p->first_lba + 1) * dev->sector_size)); }
static char *_p_guid     (struct gpt_partition *p, struct mbr_partition *m, struct device *dev) { return dstrdup(guid_str(p->partition_guid)); }
static char *_p_flags    (struct gpt_partition *p, struct mbr_partition *m, struct device *dev) { return dsprintf("implement_me"); }
static char *_p_boot     (struct gpt_partition *p, struct mbr_partition *m, struct device *dev) { return !m ? "" : dsprintf("%s", m->status & MBR_STATUS_BOOTABLE ? "*" : ""); }
static char *_p_mbr_type (struct gpt_partition *p, struct mbr_partition *m, struct device *dev) { return !m ? "" : dsprintf("%02x", m->partition_type); }
static char *_p_gpt_type (struct gpt_partition *p, struct mbr_partition *m, struct device *dev)
{
    char *string = NULL;
    for (int t=0; gpt_partition_type[t].name; t++)
        if (guid_eq(gpt_partition_type[t].guid, p->partition_type))
            string = xstrcat(string, dsprintf("%s%s", string ? " or " : "", gpt_partition_type[t].name));
    if (!string)
        return guid_str(p->partition_type);
    return dalloc_remember(string);
}
static char *_p_gpt_label(struct gpt_partition *p, struct mbr_partition *m, struct device *dev)
{
    wchar_t name[36];
    for (int i=0; i<lengthof(name); i++)
        name[i] = p->name[i];
    return dsprintf("%.36ls", name);
}

static int command_print(char **arg)
{
    bool verbose = !!arg[1];
    dalloc_start();
    printf("%s:\n", g_table.dev->name);
    printf("  Disk GUID: %s\n", guid_str(g_table.header->disk_guid));
    printf("  %lld %ld byte sectors for %lld total bytes (%s capacity)\n",
           g_table.dev->sector_count,  g_table.dev->sector_size,
           g_table.dev->sector_count * g_table.dev->sector_size,
           human_string(g_table.dev->sector_count * g_table.dev->sector_size));
    printf("  MBR partition table is %s synced to the GPT table\n", g_table.options.mbr_sync ? "currently" : "not");
    printf("\n");

    struct printer {
        char *title;
        bool right;
        bool verbose;
        char *(*print)(struct gpt_partition *p, struct mbr_partition *m, struct device *dev);
    };
    struct printer set[] = {
        { .title="Start LBA", .print= _p_first_lba, .verbose=true, .right = true, },
        { .title="End LBA",   .print= _p_last_lba,  .verbose=true, .right = true, },
        { .title="Size",      .print= _p_size,                     .right = true, },
        { .title="GUID",      .print= _p_guid,      .verbose=true, },
        { .title="Flags",     .print= _p_flags,     },
        { .title="B",         .print= _p_boot,      },
        { .title="MBR",       .print= _p_mbr_type,  },
        { .title="GPT Type",  .print= _p_gpt_type,  },
        { .title="Label",     .print= _p_gpt_label, },
    };

    struct {
        int width;
        char *data[g_table.header->partition_entries];
    } column[lengthof(set)];
    memset(column, 0, sizeof(column));

    for (int p=0; p<g_table.header->partition_entries; p++) {
        if (guid_eq(gpt_partition_type_empty, g_table.partition[p].partition_type))
            continue;
        for (int c=0; c<lengthof(set); c++) {
            if (set[c].verbose && !verbose) continue;
            int m = get_mbr_alias(g_table, p);
            column[c].data[p] = set[c].print(&g_table.partition[p],
                                             g_table.options.mbr_sync && m != -1 ? &g_table.mbr.partition[m] : NULL,
                                             g_table.dev);
            if (!column[c].width) column[c].width = strlen(set[c].title);
            column[c].width = MAX(column[c].width, strlen(column[c].data[p]));
        }
    }

#define format(x) ( set[x].right ? "%*s  " : "%-*s  " )

    printf("  ###  ");
    for (int c=0; c<lengthof(set); c++)
        if (!set[c].verbose || verbose)
            printf(format(c), column[c].width, set[c].title);
    printf("\n");
    printf("  -----");
    for (int c=0; c<lengthof(set); c++)
        if (!set[c].verbose || verbose)
            printf("%.*s--", column[c].width, "----------------------------------------------------------------------------------------------------------------");
    printf("\n");
    for (int p=0; p<g_table.header->partition_entries; p++) {
        if (!column[2].data[p]) continue;
        printf("  %3d) ", p);
        for (int c=0; c<lengthof(set); c++)
            if (!set[c].verbose || verbose)
                printf(format(c), column[c].width, column[c].data[p]);
        printf("\n");
    }
    dalloc_free();
    return 0;
}
command_add("print", command_print, "Print the partition table.",
            command_arg("verbose", C_Flag, "Print more details"));

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
            printf(" %4u:%03u:%02u %4u:%03u:%02u",
                   g_table.mbr.partition[i].first_sector.cylinder, g_table.mbr.partition[i].first_sector.head, g_table.mbr.partition[i].first_sector.sector,
                   g_table.mbr.partition[i].last_sector.cylinder,  g_table.mbr.partition[i].last_sector.head,  g_table.mbr.partition[i].last_sector.sector);
        printf(" %10u %10u (%9s) %02x (%s)\n",
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

static size_t sncatprintf(char *buffer, size_t space, char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    size_t length = strlen(buffer);
    int count = vsnprintf(buffer + length, space - length, format, ap);
    va_end(ap);
    return count;
}

static char *tr(char *in, char *from, char *to)
{
    char *out = in;
    for (int i=0; out[i]; i++) {
        char *c = strchr(from, out[i]);
        if (c) out[i] = to[c-from];
    }
    return out;
}

static char *trdup(char *in, char *from, char *to)
{
    return tr(xstrdup(in), from, to);
}

static char *ctr(char *in, char *from, char *to)
{
    return tr(csprintf("%s", in), from, to);
}

static char *trim(char *s)
{
    while (isspace(*s)) s++;
    char *e = s+strlen(s) - 1;
    while (e >= s && isspace(*e))
        *e-- = '\0';
    return s;
}
