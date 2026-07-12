#pragma once

#include <nfc/nfc.h>
#include <stdbool.h>
#include <stdint.h>

#define K2_TAG_DATA_SIZE 96

#define K2_TAG_DATE_LEN 5
#define K2_TAG_SUPPLIER_LEN 4
#define K2_TAG_BATCH_LEN 2
#define K2_TAG_RESERVE_LEN 14

typedef struct {
    /** Empty string = use RTC at write time. */
    char date[K2_TAG_DATE_LEN + 1];
    char supplier_id[K2_TAG_SUPPLIER_LEN + 1];
    char batch_id[K2_TAG_BATCH_LEN + 1];
    char material_id[8];
    char color_hex[8];
    uint8_t weight_index;
    uint8_t printer_index;
    uint32_t serial;
    char reserve[K2_TAG_RESERVE_LEN + 1];
} K2TagConfig;

typedef enum {
    K2TagOk = 0,
    K2TagErrorNoTag,
    K2TagErrorAuth,
    K2TagErrorWrite,
    K2TagErrorRead,
    K2TagErrorInvalid,
} K2TagResult;

void k2_tag_config_set_defaults(K2TagConfig* config);

/** Encode current RTC date into 5-char K2 prefix. */
void k2_tag_encode_date(char out[K2_TAG_DATE_LEN + 1]);

bool k2_tag_build_payload(const K2TagConfig* config, uint8_t payload[K2_TAG_DATA_SIZE]);

/** Parse read tag string into config fields. */
bool k2_tag_parse_payload(const char* data, K2TagConfig* config);

/** Poll until a MIFARE Classic 1K/Mini tag is present, or stop is set. */
bool k2_tag_wait_for_classic(Nfc* nfc, volatile bool* stop);

K2TagResult k2_tag_write(Nfc* nfc, const K2TagConfig* config);

K2TagResult k2_tag_read(Nfc* nfc, char* out, size_t out_size);

const char* k2_tag_result_str(K2TagResult result);

const char* k2_tag_weight_label(uint8_t index);

const char* k2_tag_weight_code(uint8_t index);

const char* k2_tag_printer_suffix(uint8_t index);

uint8_t k2_tag_weight_count(void);

uint8_t k2_tag_printer_count(void);
