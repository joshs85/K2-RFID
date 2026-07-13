#include "k2_tag.h"

#include "k2_crypto.h"

#include <ctype.h>
#include <furi.h>
#include <furi_hal_rtc.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <stdlib.h>
#include <string.h>

static const MfClassicKey k2_default_key = {
    .data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
};

static const char* const k2_weights[] = {"1 KG", "750 G", "600 G", "500 G", "250 G"};
static const char* const k2_weight_codes[] = {"0330", "0247", "0198", "0165", "0082"};
static const char* const k2_printers[] = {"K2", "K1", "HI"};

const char* k2_tag_weight_label(uint8_t index) {
    if(index >= COUNT_OF(k2_weights)) return k2_weights[0];
    return k2_weights[index];
}

const char* k2_tag_weight_code(uint8_t index) {
    if(index >= COUNT_OF(k2_weight_codes)) return k2_weight_codes[0];
    return k2_weight_codes[index];
}

const char* k2_tag_printer_suffix(uint8_t index) {
    if(index >= COUNT_OF(k2_printers)) return k2_printers[0];
    return k2_printers[index];
}

uint8_t k2_tag_weight_count(void) {
    return COUNT_OF(k2_weights);
}

uint8_t k2_tag_printer_count(void) {
    return COUNT_OF(k2_printers);
}

void k2_tag_config_set_defaults(K2TagConfig* config) {
    furi_check(config);
    config->date[0] = '\0';
    snprintf(config->supplier_id, sizeof(config->supplier_id), "0276");
    snprintf(config->batch_id, sizeof(config->batch_id), "A2");
    snprintf(config->material_id, sizeof(config->material_id), "01001");
    snprintf(config->color_hex, sizeof(config->color_hex), "FFFFFF");
    config->weight_index = 3;
    config->printer_index = 0;
    config->serial = 1;
    snprintf(config->reserve, sizeof(config->reserve), "00000000000000");
}

/** Encode M/DD/YY date prefix (5 chars) matching upstream K2-RFID format. */
void k2_tag_encode_date(char out[K2_TAG_DATE_LEN + 1]) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    if(dt.month <= 9) {
        out[0] = (char)('0' + dt.month);
    } else {
        out[0] = (char)('A' + (dt.month - 10));
    }

    if(dt.day <= 9) {
        out[1] = 'A';
        out[2] = (char)('0' + dt.day);
    } else {
        out[1] = (char)('A' + (dt.day / 10));
        out[2] = (char)('0' + (dt.day % 10));
    }

    snprintf(out + 3, 3, "%02u", (unsigned)(dt.year % 100));
}

const char* k2_tag_result_str(K2TagResult result) {
    switch(result) {
    case K2TagOk:
        return "Success";
    case K2TagErrorNoTag:
        return "No MIFARE tag detected";
    case K2TagErrorAuth:
        return "Authentication failed";
    case K2TagErrorWrite:
        return "Write failed";
    case K2TagErrorRead:
        return "Read failed";
    case K2TagErrorInvalid:
        return "Invalid tag data";
    default:
        return "Unknown error";
    }
}

bool k2_tag_build_payload(const K2TagConfig* config, uint8_t payload[K2_TAG_DATA_SIZE]) {
    furi_check(config);
    furi_check(payload);

    char serial[7];
    snprintf(serial, sizeof(serial), "%06lu", (unsigned long)(config->serial % 1000000U));

    char filament_id[10];
    snprintf(filament_id, sizeof(filament_id), "1%s", config->material_id);

    char color[10];
    snprintf(color, sizeof(color), "0%s", config->color_hex);

    char date[K2_TAG_DATE_LEN + 1];
    if(config->date[0] != '\0') {
        strncpy(date, config->date, K2_TAG_DATE_LEN);
        date[K2_TAG_DATE_LEN] = '\0';
    } else {
        k2_tag_encode_date(date);
    }

    char line[128];
    snprintf(
        line,
        sizeof(line),
        "%s%s%s%s%s%s%s%s%s",
        date,
        config->supplier_id,
        config->batch_id,
        filament_id,
        color,
        k2_tag_weight_code(config->weight_index),
        serial,
        config->reserve,
        k2_tag_printer_suffix(config->printer_index));

    memset(payload, ' ', K2_TAG_DATA_SIZE);
    size_t len = strlen(line);
    if(len > K2_TAG_DATA_SIZE) return false;
    memcpy(payload, line, len);
    return true;
}

static int k2_tag_find_weight_index(const char* code) {
    for(uint8_t i = 0; i < COUNT_OF(k2_weight_codes); i++) {
        if(strncmp(code, k2_weight_codes[i], 4) == 0) return (int)i;
    }
    return -1;
}

static int k2_tag_find_printer_index(const char* suffix) {
    for(uint8_t i = 0; i < COUNT_OF(k2_printers); i++) {
        if(strncmp(suffix, k2_printers[i], 2) == 0) return (int)i;
    }
    return -1;
}

static size_t k2_tag_payload_len(const char* data) {
    size_t len = 0;
    while(len < K2_TAG_DATA_SIZE && data[len] != '\0') {
        len++;
    }
    return len;
}

bool k2_tag_parse_payload(const char* data, K2TagConfig* config) {
    furi_check(data);
    furi_check(config);

    if(data[11] != '1') return false;
    if(data[17] != '0') return false;

    size_t len = k2_tag_payload_len(data);

    strncpy(config->date, data, K2_TAG_DATE_LEN);
    config->date[K2_TAG_DATE_LEN] = '\0';

    strncpy(config->supplier_id, data + 5, K2_TAG_SUPPLIER_LEN);
    config->supplier_id[K2_TAG_SUPPLIER_LEN] = '\0';

    strncpy(config->batch_id, data + 9, K2_TAG_BATCH_LEN);
    config->batch_id[K2_TAG_BATCH_LEN] = '\0';

    strncpy(config->material_id, data + 12, 5);
    config->material_id[5] = '\0';

    strncpy(config->color_hex, data + 18, 6);
    config->color_hex[6] = '\0';
    for(size_t i = 0; i < 6; i++) {
        if(!isxdigit((unsigned char)config->color_hex[i])) return false;
    }

    int weight_idx = k2_tag_find_weight_index(data + 24);
    if(weight_idx < 0) return false;
    config->weight_index = (uint8_t)weight_idx;

    for(size_t i = 0; i < K2_TAG_SERIAL_LEN; i++) {
        if(!isdigit((unsigned char)data[K2_TAG_SERIAL_OFFSET + i])) return false;
    }
    char serial_str[K2_TAG_SERIAL_LEN + 1];
    memcpy(serial_str, data + K2_TAG_SERIAL_OFFSET, K2_TAG_SERIAL_LEN);
    serial_str[K2_TAG_SERIAL_LEN] = '\0';
    config->serial = (uint32_t)strtoul(serial_str, NULL, 10);
    if(config->serial < 1) config->serial = 1;

    if(len >= 48) {
        strncpy(config->reserve, data + 34, K2_TAG_RESERVE_LEN);
        config->reserve[K2_TAG_RESERVE_LEN] = '\0';
    } else {
        snprintf(config->reserve, sizeof(config->reserve), "00000000000000");
    }

    config->printer_index = 0;
    if(len >= 50) {
        int printer_idx = k2_tag_find_printer_index(data + 48);
        if(printer_idx >= 0) config->printer_index = (uint8_t)printer_idx;
    }

    return true;
}

bool k2_tag_wait_for_classic(Nfc* nfc, volatile bool* stop) {
    furi_check(nfc);

    while(!stop || !*stop) {
        MfClassicType type = MfClassicType1k;
        MfClassicError error = mf_classic_poller_sync_detect_type(nfc, &type);
        if(error == MfClassicErrorNone && (type == MfClassicType1k || type == MfClassicTypeMini)) {
            return true;
        }
        furi_delay_ms(200);
    }

    return false;
}

static K2TagResult k2_tag_read_uid(Nfc* nfc, uint8_t uid[4]) {
    MfClassicBlock block0;
    MfClassicError error = mf_classic_poller_sync_read_block(
        nfc, 0, (MfClassicKey*)&k2_default_key, MfClassicKeyTypeA, &block0);
    if(error != MfClassicErrorNone) return K2TagErrorNoTag;
    memcpy(uid, block0.data, 4);
    return K2TagOk;
}

static bool k2_tag_try_auth_read(
    Nfc* nfc,
    uint8_t block,
    const MfClassicKey* key,
    MfClassicBlock* block_out) {
    MfClassicError error = mf_classic_poller_sync_read_block(
        nfc, block, (MfClassicKey*)key, MfClassicKeyTypeA, block_out);
    return error == MfClassicErrorNone;
}

static bool k2_tag_write_block(
    Nfc* nfc,
    uint8_t block,
    const MfClassicKey* key,
    const uint8_t data[16]) {
    MfClassicBlock block_data;
    memcpy(block_data.data, data, MF_CLASSIC_BLOCK_SIZE);
    MfClassicError error = mf_classic_poller_sync_write_block(
        nfc, block, (MfClassicKey*)key, MfClassicKeyTypeA, &block_data);
    return error == MfClassicErrorNone;
}

K2TagResult k2_tag_write(Nfc* nfc, const K2TagConfig* config) {
    furi_check(nfc);
    furi_check(config);

    MfClassicType type = MfClassicType1k;
    MfClassicError detect_error = mf_classic_poller_sync_detect_type(nfc, &type);
    if(detect_error != MfClassicErrorNone) return K2TagErrorNoTag;
    if(type != MfClassicType1k && type != MfClassicTypeMini) return K2TagErrorNoTag;

    uint8_t uid[4];
    K2TagResult uid_result = k2_tag_read_uid(nfc, uid);
    if(uid_result != K2TagOk) return uid_result;

    uint8_t enc_key[6];
    k2_crypto_create_key(uid, enc_key);
    MfClassicKey derived_key;
    memcpy(derived_key.data, enc_key, 6);

    uint8_t payload[K2_TAG_DATA_SIZE];
    if(!k2_tag_build_payload(config, payload)) return K2TagErrorInvalid;

    bool encrypted = false;
    MfClassicBlock probe;
    if(!k2_tag_try_auth_read(nfc, 4, &k2_default_key, &probe)) {
        if(!k2_tag_try_auth_read(nfc, 4, &derived_key, &probe)) {
            return K2TagErrorAuth;
        }
        encrypted = true;
    }

    const MfClassicKey* sector1_key = encrypted ? &derived_key : &k2_default_key;

    uint8_t sector1_plain[48];
    memcpy(sector1_plain, payload, 48);
    uint8_t sector1_cipher[48];
    for(uint8_t i = 0; i < 3; i++) {
        k2_crypto_cipher_block(1, sector1_plain + i * 16, sector1_cipher + i * 16);
    }

    for(uint8_t i = 0; i < 3; i++) {
        if(!k2_tag_write_block(nfc, 4 + i, sector1_key, sector1_cipher + i * 16)) {
            return K2TagErrorWrite;
        }
    }

    if(!encrypted) {
        MfClassicBlock trailer;
        if(!k2_tag_try_auth_read(nfc, 7, &k2_default_key, &trailer)) {
            return K2TagErrorAuth;
        }
        memcpy(trailer.data, enc_key, 6);
        memcpy(trailer.data + 10, enc_key, 6);
        if(!k2_tag_write_block(nfc, 7, &k2_default_key, trailer.data)) {
            return K2TagErrorWrite;
        }
        sector1_key = &derived_key;
    }

    for(uint8_t i = 0; i < 3; i++) {
        if(!k2_tag_write_block(nfc, 8 + i, &k2_default_key, payload + 48 + i * 16)) {
            return K2TagErrorWrite;
        }
    }

    return K2TagOk;
}

K2TagResult k2_tag_read(Nfc* nfc, char* out, size_t out_size) {
    furi_check(nfc);
    furi_check(out);
    if(out_size < 48) return K2TagErrorInvalid;

    MfClassicType type = MfClassicType1k;
    if(mf_classic_poller_sync_detect_type(nfc, &type) != MfClassicErrorNone) {
        return K2TagErrorNoTag;
    }

    uint8_t uid[4];
    K2TagResult uid_result = k2_tag_read_uid(nfc, uid);
    if(uid_result != K2TagOk) return uid_result;

    uint8_t enc_key[6];
    k2_crypto_create_key(uid, enc_key);
    MfClassicKey derived_key;
    memcpy(derived_key.data, enc_key, 6);

    const MfClassicKey* key = &k2_default_key;
    MfClassicBlock block;
    if(!k2_tag_try_auth_read(nfc, 4, &k2_default_key, &block)) {
        if(!k2_tag_try_auth_read(nfc, 4, &derived_key, &block)) {
            return K2TagErrorAuth;
        }
        key = &derived_key;
    }

    uint8_t sector1_cipher[48];
    for(uint8_t i = 0; i < 3; i++) {
        if(!k2_tag_try_auth_read(nfc, 4 + i, key, &block)) return K2TagErrorRead;
        memcpy(sector1_cipher + i * 16, block.data, 16);
    }

    uint8_t sector1_plain[48];
    for(uint8_t i = 0; i < 3; i++) {
        k2_crypto_cipher_block(0, sector1_cipher + i * 16, sector1_plain + i * 16);
    }

    uint8_t buffer[K2_TAG_DATA_SIZE];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, sector1_plain, 48);

    if(k2_tag_try_auth_read(nfc, 8, &k2_default_key, &block)) {
        memcpy(buffer + 48, block.data, 16);
        if(k2_tag_try_auth_read(nfc, 9, &k2_default_key, &block)) {
            memcpy(buffer + 64, block.data, 16);
            if(k2_tag_try_auth_read(nfc, 10, &k2_default_key, &block)) {
                memcpy(buffer + 80, block.data, 16);
            }
        }
    }

    for(size_t i = 0; i < K2_TAG_DATA_SIZE; i++) {
        if(buffer[i] == '\0') buffer[i] = ' ';
    }

    size_t copy_len = K2_TAG_DATA_SIZE;
    if(copy_len >= out_size) copy_len = out_size - 1;
    memcpy(out, buffer, copy_len);
    out[copy_len] = '\0';

    size_t end = copy_len;
    while(end > 0 && out[end - 1] == ' ') {
        out[--end] = '\0';
    }

    if(end < 40) return K2TagErrorInvalid;
    return K2TagOk;
}
