#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <strings.h>

#include "esp_console.h"
#include "esp_err.h"

#include "config.h"
#include "config_keys.h"
#include "config_vars.h"

#include "network.h"

typedef struct
{
    const char *key;
    config_item_id_t item;
    config_value_type_t type;
    void *ptr;
    uint16_t len;
} ggkg_cfg_map_t;

static const ggkg_cfg_map_t s_cfg_map[CONFIG_ITEM_COUNT] = {
#define X(id, t, k, v, l) [CONFIG_ITEM_##id] = { .key = (k), .item = CONFIG_ITEM_##id, .type = CONFIG_VALUE_##t, .ptr = (void *)&(v), .len = (uint16_t)(l) },
    CONFIG_ITEM_TABLE(X)
#undef X
};

static void ggkg_print_usage(void)
{
    printf("Usage:\n");
    printf("  ggkg config load <key|all>\n");
    printf("  ggkg config save <key|all>\n");
    printf("  ggkg config reset\n");
    printf("  ggkg config get <key|all>\n");
    printf("  ggkg config set <key> <value> [--save]\n");
    printf("  ggkg net down\n");
    printf("  ggkg net up\n");
    printf("  ggkg net restart\n");
}

static void ggkg_print_key_list(void)
{
    printf("Available keys:\n");
    for (int i = 0; i < CONFIG_ITEM_COUNT; ++i)
    {
        printf("  %s\n", s_cfg_map[i].key);
    }
}

static const ggkg_cfg_map_t *ggkg_find_key(const char *key)
{
    if (key == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < CONFIG_ITEM_COUNT; ++i)
    {
        if (strcmp(key, s_cfg_map[i].key) == 0)
        {
            return &s_cfg_map[i];
        }
    }

    return NULL;
}

static void ggkg_print_value(const ggkg_cfg_map_t *entry)
{
    if (entry == NULL)
    {
        return;
    }

    switch (entry->type)
    {
        case CONFIG_VALUE_U8:
            printf("%s=%" PRIu8 "\n", entry->key, *(const uint8_t *)entry->ptr);
            break;
        case CONFIG_VALUE_U16:
            printf("%s=%" PRIu16 "\n", entry->key, *(const uint16_t *)entry->ptr);
            break;
        case CONFIG_VALUE_U32:
            printf("%s=%" PRIu32 "\n", entry->key, *(const uint32_t *)entry->ptr);
            break;
        case CONFIG_VALUE_S8:
            printf("%s=%" PRId8 "\n", entry->key, *(const int8_t *)entry->ptr);
            break;
        case CONFIG_VALUE_S16:
            printf("%s=%" PRId16 "\n", entry->key, *(const int16_t *)entry->ptr);
            break;
        case CONFIG_VALUE_S32:
            printf("%s=%" PRId32 "\n", entry->key, *(const int32_t *)entry->ptr);
            break;
        case CONFIG_VALUE_S64:
            printf("%s=%" PRId64 "\n", entry->key, *(const int64_t *)entry->ptr);
            break;
        case CONFIG_VALUE_BOOL:
            printf("%s=%s\n", entry->key, *(const bool *)entry->ptr ? "true" : "false");
            break;
        case CONFIG_VALUE_STR:
            printf("%s=%s\n", entry->key, (const char *)entry->ptr);
            break;
        case CONFIG_VALUE_BLOB:
            printf("%s=<blob,len=%u>\n", entry->key, entry->len);
            break;
        default:
            printf("%s=<unsupported>\n", entry->key);
            break;
    }
}

static bool ggkg_parse_bool(const char *text, bool *out)
{
    if (text == NULL || out == NULL)
    {
        return false;
    }

    if (strcmp(text, "1") == 0 || strcasecmp(text, "true") == 0 || strcasecmp(text, "on") == 0 || strcasecmp(text, "yes") == 0)
    {
        *out = true;
        return true;
    }

    if (strcmp(text, "0") == 0 || strcasecmp(text, "false") == 0 || strcasecmp(text, "off") == 0 || strcasecmp(text, "no") == 0)
    {
        *out = false;
        return true;
    }

    return false;
}

static bool ggkg_parse_unsigned(const char *text, uint64_t max_value, uint64_t *out)
{
    if (text == NULL || out == NULL)
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0')
    {
        return false;
    }
    if (v > max_value)
    {
        return false;
    }

    *out = (uint64_t)v;
    return true;
}

static bool ggkg_parse_signed(const char *text, int64_t min_value, int64_t max_value, int64_t *out)
{
    if (text == NULL || out == NULL)
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    long long v = strtoll(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0')
    {
        return false;
    }
    if (v < min_value || v > max_value)
    {
        return false;
    }

    *out = (int64_t)v;
    return true;
}

static esp_err_t ggkg_set_memory_value(const ggkg_cfg_map_t *entry, const char *value)
{
    if (entry == NULL || value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (entry->type)
    {
        case CONFIG_VALUE_U8:
        {
            uint64_t v = 0;
            if (!ggkg_parse_unsigned(value, UINT8_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(uint8_t *)entry->ptr = (uint8_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_U16:
        {
            uint64_t v = 0;
            if (!ggkg_parse_unsigned(value, UINT16_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(uint16_t *)entry->ptr = (uint16_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_U32:
        {
            uint64_t v = 0;
            if (!ggkg_parse_unsigned(value, UINT32_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(uint32_t *)entry->ptr = (uint32_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_S8:
        {
            int64_t v = 0;
            if (!ggkg_parse_signed(value, INT8_MIN, INT8_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(int8_t *)entry->ptr = (int8_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_S16:
        {
            int64_t v = 0;
            if (!ggkg_parse_signed(value, INT16_MIN, INT16_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(int16_t *)entry->ptr = (int16_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_S32:
        {
            int64_t v = 0;
            if (!ggkg_parse_signed(value, INT32_MIN, INT32_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(int32_t *)entry->ptr = (int32_t)v;
            return ESP_OK;
        }
        case CONFIG_VALUE_S64:
        {
            int64_t v = 0;
            if (!ggkg_parse_signed(value, INT64_MIN, INT64_MAX, &v))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(int64_t *)entry->ptr = v;
            return ESP_OK;
        }
        case CONFIG_VALUE_BOOL:
        {
            bool b = false;
            if (!ggkg_parse_bool(value, &b))
            {
                return ESP_ERR_INVALID_ARG;
            }
            *(bool *)entry->ptr = b;
            return ESP_OK;
        }
        case CONFIG_VALUE_STR:
        {
            size_t n = strlen(value);
            if (entry->len == 0 || n >= entry->len)
            {
                return ESP_ERR_INVALID_SIZE;
            }
            memcpy(entry->ptr, value, n + 1U);
            return ESP_OK;
        }
        case CONFIG_VALUE_BLOB:
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

static int ggkg_do_config_load(const char *key)
{
    if (strcmp(key, "all") == 0)
    {
        for (int i = 0; i < CONFIG_ITEM_COUNT; ++i)
        {
            const esp_err_t err = config_read_item(s_cfg_map[i].item);
            if (err != ESP_OK)
            {
                printf("load %s failed: %s\n", s_cfg_map[i].key, esp_err_to_name(err));
            }
        }
        printf("load all done\n");
        return 0;
    }

    const ggkg_cfg_map_t *entry = ggkg_find_key(key);
    if (entry == NULL)
    {
        printf("unknown key: %s\n", key);
        ggkg_print_key_list();
        return 1;
    }

    const esp_err_t err = config_read_item(entry->item);
    if (err != ESP_OK)
    {
        printf("load %s failed: %s\n", key, esp_err_to_name(err));
        return 1;
    }

    ggkg_print_value(entry);
    return 0;
}

static int ggkg_do_config_save(const char *key)
{
    if (strcmp(key, "all") == 0)
    {
        const esp_err_t err = config_saveall();
        if (err != ESP_OK)
        {
            printf("save all failed: %s\n", esp_err_to_name(err));
            return 1;
        }

        printf("save all done\n");
        return 0;
    }

    const ggkg_cfg_map_t *entry = ggkg_find_key(key);
    if (entry == NULL)
    {
        printf("unknown key: %s\n", key);
        ggkg_print_key_list();
        return 1;
    }

    esp_err_t err = config_write_item(entry->item);
    if (err != ESP_OK)
    {
        printf("save %s failed: %s\n", key, esp_err_to_name(err));
        return 1;
    }

    err = config_commit();
    if (err != ESP_OK)
    {
        printf("commit %s failed: %s\n", key, esp_err_to_name(err));
        return 1;
    }

    printf("save %s done\n", key);
    return 0;
}

static int ggkg_do_get(const char *key)
{
    if (strcmp(key, "all") == 0)
    {
        for (int i = 0; i < CONFIG_ITEM_COUNT; ++i)
        {
            ggkg_print_value(&s_cfg_map[i]);
        }
        return 0;
    }

    const ggkg_cfg_map_t *entry = ggkg_find_key(key);
    if (entry == NULL)
    {
        printf("unknown key: %s\n", key);
        ggkg_print_key_list();
        return 1;
    }

    ggkg_print_value(entry);
    return 0;
}

static int ggkg_do_set(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: ggkg set <key> <value> [--save]\n");
        return 1;
    }

    const char *key = argv[0];
    if (strcmp(key, "all") == 0)
    {
        printf("set does not support key=all\n");
        return 1;
    }

    const ggkg_cfg_map_t *entry = ggkg_find_key(key);
    if (entry == NULL)
    {
        printf("unknown key: %s\n", key);
        ggkg_print_key_list();
        return 1;
    }

    const char *value = NULL;
    bool save = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--save") == 0)
        {
            save = true;
        }
        else if (value == NULL)
        {
            value = argv[i];
        }
        else
        {
            printf("unexpected argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (value == NULL)
    {
        printf("usage: ggkg set <key> <value> [--save]\n");
        return 1;
    }

    esp_err_t err = ggkg_set_memory_value(entry, value);
    if (err != ESP_OK)
    {
        printf("set %s failed: %s\n", key, esp_err_to_name(err));
        return 1;
    }

    if (save)
    {
        err = config_write_item(entry->item);
        if (err != ESP_OK)
        {
            printf("save %s failed: %s\n", key, esp_err_to_name(err));
            return 1;
        }

        err = config_commit();
        if (err != ESP_OK)
        {
            printf("commit %s failed: %s\n", key, esp_err_to_name(err));
            return 1;
        }
    }

    ggkg_print_value(entry);
    return 0;
}

static int ggkg_do_net(const char *action)
{
    esp_err_t err;

    if (strcmp(action, "down") == 0)
    {
        err = network_down();
    }
    else if (strcmp(action, "up") == 0)
    {
        err = network_up();
    }
    else if (strcmp(action, "restart") == 0)
    {
        err = network_restart();
    }
    else
    {
        printf("usage: ggkg net <down|up|restart>\n");
        return 1;
    }

    if (err != ESP_OK)
    {
        printf("net %s failed: %s\n", action, esp_err_to_name(err));
        return 1;
    }

    printf("net %s done\n", action);
    return 0;
}

static int cmd_ggkg(int argc, char **argv)
{
    if (argc < 2)
    {
        ggkg_print_usage();
        return 1;
    }

    if (strcmp(argv[1], "net") == 0)
    {
        if (argc != 3)
        {
            printf("usage: ggkg net <down|up|restart>\n");
            return 1;
        }
        return ggkg_do_net(argv[2]);
    }

    if (strcmp(argv[1], "config") != 0)
    {
        ggkg_print_usage();
        return 1;
    }

    if (argc < 3)
    {
        ggkg_print_usage();
        return 1;
    }

    if (strcmp(argv[2], "load") == 0)
    {
        if (argc != 4)
        {
            printf("usage: ggkg config load <key|all>\n");
            return 1;
        }
        return ggkg_do_config_load(argv[3]);
    }

    if (strcmp(argv[2], "save") == 0)
    {
        if (argc != 4)
        {
            printf("usage: ggkg config save <key|all>\n");
            return 1;
        }
        return ggkg_do_config_save(argv[3]);
    }

    if (strcmp(argv[2], "get") == 0)
    {
        if (argc != 4)
        {
            printf("usage: ggkg config get <key|all>\n");
            return 1;
        }
        return ggkg_do_get(argv[3]);
    }

    if (strcmp(argv[2], "set") == 0)
    {
        if (argc < 5)
        {
            printf("usage: ggkg config set <key> <value> [--save]\n");
            return 1;
        }
        return ggkg_do_set(argc - 3, &argv[3]);
    }

    if (strcmp(argv[2], "reset") == 0)
    {
        if (argc != 3)
        {
            printf("usage: ggkg config reset\n");
            return 1;
        }

        const esp_err_t err = config_factory_reset();
        if (err != ESP_OK)
        {
            printf("factory reset failed: %s\n", esp_err_to_name(err));
            return 1;
        }

        return 0;
    }

    ggkg_print_usage();
    return 1;
}

esp_err_t register_ggkg(void)
{
    const esp_console_cmd_t cmd = {
        .command = "ggkg",
        .help = "GGKG runtime config commands",
        .hint = NULL,
        .func = &cmd_ggkg,
    };

    return esp_console_cmd_register(&cmd);
}
