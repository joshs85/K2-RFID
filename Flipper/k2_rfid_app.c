#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <nfc/nfc.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "k2_materials.h"
#include "k2_tag.h"

typedef enum {
    K2ViewMenu = 0,
    K2ViewConfig,
    K2ViewMaterialPick,
    K2ViewColorPick,
    K2ViewSupplierPick,
    K2ViewBatchPick,
    K2ViewDatePick,
    K2ViewPopup,
    K2ViewText,
    K2ViewTextInput,
    K2ViewSerialInput,
} K2View;

typedef enum {
    K2MenuConfigure = 10,
    K2MenuWrite,
    K2MenuRead,
    K2MenuAbout,
    K2EventDone = 100,
} K2CustomEvent;

typedef enum {
    K2CfgDate = 0,
    K2CfgSupplier,
    K2CfgBatch,
    K2CfgMaterial,
    K2CfgWeight,
    K2CfgColor,
    K2CfgSerial,
    K2CfgReserve,
    K2CfgPrinter,
    K2CfgFieldCount,
} K2CfgField;

typedef enum {
    K2TextInputDate = 0,
    K2TextInputSupplier,
    K2TextInputBatch,
    K2TextInputMaterial,
    K2TextInputColor,
    K2TextInputReserve,
} K2TextInputTarget;

typedef struct {
    const char* label;
    const char* hex;
} K2ColorPreset;

typedef struct {
    const char* id;
    const char* label;
} K2BrandPreset;

static const K2ColorPreset k2_color_presets[] = {
    {"White", "FFFFFF"},
    {"Black", "000000"},
    {"Red", "FF0000"},
    {"Blue", "0000FF"},
    {"Green", "00FF00"},
    {"Yellow", "FFFF00"},
    {"Orange", "FFA500"},
    {"Purple", "800080"},
    {"Gray", "808080"},
    {"Light Gray", "D8D8D8"},
    {"Dark Gray", "4C4C4C"},
    {"Pink", "FFC0CB"},
    {"Cyan", "00FFFF"},
    {"Creality Red", "C12E1F"},
};

static const K2BrandPreset k2_supplier_presets[] = {
    {"0276", "Creality (0276)"},
};

static const K2BrandPreset k2_batch_presets[] = {
    {"A2", "Default (A2)"},
};

#define K2_COLOR_CUSTOM_INDEX COUNT_OF(k2_color_presets)
#define K2_SUPPLIER_CUSTOM_INDEX COUNT_OF(k2_supplier_presets)
#define K2_BATCH_CUSTOM_INDEX COUNT_OF(k2_batch_presets)
#define K2_MATERIAL_CUSTOM_INDEX k2_materials_count
#define K2_DATE_AUTO_INDEX 0
#define K2_DATE_CUSTOM_INDEX 1

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Submenu* material_menu;
    Submenu* color_menu;
    Submenu* supplier_menu;
    Submenu* batch_menu;
    Submenu* date_menu;
    VariableItemList* config;
    VariableItem* config_items[K2CfgFieldCount];
    Popup* popup;
    TextBox* text_box;
    TextInput* text_input;
    NumberInput* number_input;
    Nfc* nfc;
    FuriThread* worker;

    K2TagConfig tag_config;
    size_t material_index;
    size_t color_index;
    size_t supplier_index;
    size_t batch_index;
    K2View current_view;
    volatile bool worker_stop;
    K2TextInputTarget text_input_target;
    char text_input_buffer[16];
    char result_text[256];
} K2App;

static size_t k2_color_find_preset_index(const char* hex) {
    for(size_t i = 0; i < COUNT_OF(k2_color_presets); i++) {
        if(strcmp(hex, k2_color_presets[i].hex) == 0) return i;
    }
    return K2_COLOR_CUSTOM_INDEX;
}

static size_t k2_supplier_find_preset_index(const char* id) {
    for(size_t i = 0; i < COUNT_OF(k2_supplier_presets); i++) {
        if(strcmp(id, k2_supplier_presets[i].id) == 0) return i;
    }
    return K2_SUPPLIER_CUSTOM_INDEX;
}

static size_t k2_batch_find_preset_index(const char* id) {
    for(size_t i = 0; i < COUNT_OF(k2_batch_presets); i++) {
        if(strcmp(id, k2_batch_presets[i].id) == 0) return i;
    }
    return K2_BATCH_CUSTOM_INDEX;
}

static size_t k2_material_find_preset_index(const char* id) {
    for(size_t i = 0; i < k2_materials_count; i++) {
        if(strcmp(id, k2_materials[i].id) == 0) return i;
    }
    return K2_MATERIAL_CUSTOM_INDEX;
}

static void k2_color_sync_index(K2App* app) {
    app->color_index = k2_color_find_preset_index(app->tag_config.color_hex);
}

static void k2_supplier_sync_index(K2App* app) {
    app->supplier_index = k2_supplier_find_preset_index(app->tag_config.supplier_id);
}

static void k2_batch_sync_index(K2App* app) {
    app->batch_index = k2_batch_find_preset_index(app->tag_config.batch_id);
}

static void k2_material_sync_index(K2App* app) {
    app->material_index = k2_material_find_preset_index(app->tag_config.material_id);
}

static void k2_stop_worker(K2App* app) {
    if(!app->worker) return;
    app->worker_stop = true;
    furi_thread_join(app->worker);
    furi_thread_free(app->worker);
    app->worker = NULL;
}

static void k2_show_text(K2App* app) {
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->result_text);
    app->current_view = K2ViewText;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewText);
}

static void k2_config_set_material(K2App* app, size_t index) {
    if(index >= K2_MATERIAL_CUSTOM_INDEX) return;
    app->material_index = index;
    strncpy(
        app->tag_config.material_id,
        k2_materials[app->material_index].id,
        sizeof(app->tag_config.material_id) - 1);
    app->tag_config.material_id[sizeof(app->tag_config.material_id) - 1] = '\0';
}

static void k2_config_set_color_preset(K2App* app, size_t index) {
    if(index >= K2_COLOR_CUSTOM_INDEX) return;
    app->color_index = index;
    snprintf(
        app->tag_config.color_hex,
        sizeof(app->tag_config.color_hex),
        "%s",
        k2_color_presets[index].hex);
}

static void k2_config_set_supplier_preset(K2App* app, size_t index) {
    if(index >= K2_SUPPLIER_CUSTOM_INDEX) return;
    app->supplier_index = index;
    snprintf(
        app->tag_config.supplier_id,
        sizeof(app->tag_config.supplier_id),
        "%s",
        k2_supplier_presets[index].id);
}

static void k2_config_set_batch_preset(K2App* app, size_t index) {
    if(index >= K2_BATCH_CUSTOM_INDEX) return;
    app->batch_index = index;
    snprintf(
        app->tag_config.batch_id,
        sizeof(app->tag_config.batch_id),
        "%s",
        k2_batch_presets[index].id);
}

static void k2_color_set_display_text(K2App* app, char* text, size_t text_size) {
    if(app->color_index < K2_COLOR_CUSTOM_INDEX) {
        snprintf(text, text_size, "%s", k2_color_presets[app->color_index].label);
    } else {
        snprintf(text, text_size, "Custom %s", app->tag_config.color_hex);
    }
}

static void k2_supplier_set_display_text(K2App* app, char* text, size_t text_size) {
    if(app->supplier_index < K2_SUPPLIER_CUSTOM_INDEX) {
        snprintf(text, text_size, "%s", k2_supplier_presets[app->supplier_index].label);
    } else {
        snprintf(text, text_size, "Custom %s", app->tag_config.supplier_id);
    }
}

static void k2_batch_set_display_text(K2App* app, char* text, size_t text_size) {
    if(app->batch_index < K2_BATCH_CUSTOM_INDEX) {
        snprintf(text, text_size, "%s", k2_batch_presets[app->batch_index].label);
    } else {
        snprintf(text, text_size, "Custom %s", app->tag_config.batch_id);
    }
}

static void k2_material_set_display_text(K2App* app, char* text, size_t text_size) {
    if(app->material_index < K2_MATERIAL_CUSTOM_INDEX) {
        snprintf(text, text_size, "%s", k2_materials[app->material_index].label);
    } else {
        snprintf(text, text_size, "Custom %s", app->tag_config.material_id);
    }
}

static void k2_date_set_display_text(K2App* app, char* text, size_t text_size) {
    if(app->tag_config.date[0] == '\0') {
        snprintf(text, text_size, "Auto (RTC)");
    } else {
        snprintf(text, text_size, "%s", app->tag_config.date);
    }
}

static void k2_config_refresh_item(K2App* app, K2CfgField field) {
    VariableItem* item = app->config_items[field];
    if(!item) return;

    switch(field) {
    case K2CfgDate: {
        char date_text[16];
        k2_date_set_display_text(app, date_text, sizeof(date_text));
        variable_item_set_current_value_text(item, date_text);
        break;
    }
    case K2CfgSupplier: {
        char supplier_text[24];
        k2_supplier_set_display_text(app, supplier_text, sizeof(supplier_text));
        variable_item_set_current_value_text(item, supplier_text);
        break;
    }
    case K2CfgBatch: {
        char batch_text[24];
        k2_batch_set_display_text(app, batch_text, sizeof(batch_text));
        variable_item_set_current_value_text(item, batch_text);
        break;
    }
    case K2CfgMaterial: {
        char material_text[32];
        k2_material_set_display_text(app, material_text, sizeof(material_text));
        variable_item_set_current_value_text(item, material_text);
        break;
    }
    case K2CfgWeight:
        variable_item_set_current_value_index(item, app->tag_config.weight_index);
        variable_item_set_current_value_text(
            item, k2_tag_weight_label(app->tag_config.weight_index));
        break;
    case K2CfgColor: {
        char color_text[24];
        k2_color_set_display_text(app, color_text, sizeof(color_text));
        variable_item_set_current_value_text(item, color_text);
        break;
    }
    case K2CfgPrinter:
        variable_item_set_current_value_index(item, app->tag_config.printer_index);
        variable_item_set_current_value_text(
            item, k2_tag_printer_suffix(app->tag_config.printer_index));
        break;
    case K2CfgSerial: {
        char serial_text[16];
        snprintf(serial_text, sizeof(serial_text), "%06lu", (unsigned long)app->tag_config.serial);
        variable_item_set_current_value_text(item, serial_text);
        break;
    }
    case K2CfgReserve:
        variable_item_set_current_value_text(item, app->tag_config.reserve);
        break;
    default:
        break;
    }
}

static void k2_config_refresh(K2App* app) {
    k2_material_sync_index(app);
    k2_color_sync_index(app);
    k2_supplier_sync_index(app);
    k2_batch_sync_index(app);
    for(uint8_t field = 0; field < K2CfgFieldCount; field++) {
        k2_config_refresh_item(app, field);
    }
}

static void k2_config_weight_changed(VariableItem* item) {
    K2App* app = variable_item_get_context(item);
    app->tag_config.weight_index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, k2_tag_weight_label(app->tag_config.weight_index));
}

static void k2_config_printer_changed(VariableItem* item) {
    K2App* app = variable_item_get_context(item);
    app->tag_config.printer_index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, k2_tag_printer_suffix(app->tag_config.printer_index));
}

static bool k2_validate_fixed_len(
    const char* text,
    size_t len,
    bool hex_only,
    FuriString* error,
    const char* err_msg) {
    if(strlen(text) != len) {
        furi_string_set_str(error, err_msg);
        return false;
    }
    if(hex_only) {
        for(size_t i = 0; i < len; i++) {
            if(!isxdigit((unsigned char)text[i])) {
                furi_string_set_str(error, "Hex only");
                return false;
            }
        }
    }
    return true;
}

static bool k2_date_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    return k2_validate_fixed_len(text, K2_TAG_DATE_LEN, false, error, "Need 5 chars");
}

static bool k2_supplier_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    return k2_validate_fixed_len(text, K2_TAG_SUPPLIER_LEN, false, error, "Need 4 chars");
}

static bool k2_batch_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    return k2_validate_fixed_len(text, K2_TAG_BATCH_LEN, false, error, "Need 2 chars");
}

static bool k2_material_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    if(strlen(text) != 5) {
        furi_string_set_str(error, "Need 5 chars");
        return false;
    }
    for(size_t i = 0; i < 5; i++) {
        if(!isalnum((unsigned char)text[i])) {
            furi_string_set_str(error, "Alphanumeric");
            return false;
        }
    }
    return true;
}

static bool k2_color_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    return k2_validate_fixed_len(text, 6, true, error, "Need 6 hex digits");
}

static bool k2_reserve_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    return k2_validate_fixed_len(text, K2_TAG_RESERVE_LEN, false, error, "Need 14 chars");
}

static void k2_copy_field(char* dst, size_t dst_size, const char* src) {
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void k2_text_input_apply(K2App* app) {
    switch(app->text_input_target) {
    case K2TextInputDate:
        k2_copy_field(app->tag_config.date, sizeof(app->tag_config.date), app->text_input_buffer);
        k2_config_refresh_item(app, K2CfgDate);
        break;
    case K2TextInputSupplier:
        k2_copy_field(
            app->tag_config.supplier_id, sizeof(app->tag_config.supplier_id), app->text_input_buffer);
        k2_supplier_sync_index(app);
        k2_config_refresh_item(app, K2CfgSupplier);
        break;
    case K2TextInputBatch:
        k2_copy_field(
            app->tag_config.batch_id, sizeof(app->tag_config.batch_id), app->text_input_buffer);
        k2_batch_sync_index(app);
        k2_config_refresh_item(app, K2CfgBatch);
        break;
    case K2TextInputMaterial:
        k2_copy_field(
            app->tag_config.material_id, sizeof(app->tag_config.material_id), app->text_input_buffer);
        k2_material_sync_index(app);
        k2_config_refresh_item(app, K2CfgMaterial);
        break;
    case K2TextInputColor:
        k2_copy_field(
            app->tag_config.color_hex, sizeof(app->tag_config.color_hex), app->text_input_buffer);
        k2_color_sync_index(app);
        k2_config_refresh_item(app, K2CfgColor);
        break;
    case K2TextInputReserve:
        k2_copy_field(
            app->tag_config.reserve, sizeof(app->tag_config.reserve), app->text_input_buffer);
        k2_config_refresh_item(app, K2CfgReserve);
        break;
    default:
        break;
    }
}

static void k2_text_input_done(void* context) {
    K2App* app = context;
    k2_text_input_apply(app);
    app->current_view = K2ViewConfig;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
}

static void
    k2_open_text_input(K2App* app, K2TextInputTarget target, const char* header, const char* value, TextInputValidatorCallback validator, size_t min_len) {
    app->text_input_target = target;
    snprintf(app->text_input_buffer, sizeof(app->text_input_buffer), "%s", value);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_validator(app->text_input, validator, app);
    text_input_set_result_callback(
        app->text_input,
        k2_text_input_done,
        app,
        app->text_input_buffer,
        sizeof(app->text_input_buffer),
        false);
    text_input_set_minimum_length(app->text_input, min_len);
    app->current_view = K2ViewTextInput;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewTextInput);
}

static void k2_date_menu_callback(void* context, uint32_t index) {
    K2App* app = context;
    if(index == K2_DATE_AUTO_INDEX) {
        app->tag_config.date[0] = '\0';
        k2_config_refresh_item(app, K2CfgDate);
        app->current_view = K2ViewConfig;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
        return;
    }
    const char* seed = app->tag_config.date[0] ? app->tag_config.date : "AB124";
    k2_open_text_input(
        app, K2TextInputDate, "Date (M/DD/YY)", seed, k2_date_validator, K2_TAG_DATE_LEN);
}

static void k2_date_menu_build(K2App* app) {
    submenu_reset(app->date_menu);
    submenu_set_header(app->date_menu, "Write Date");
    submenu_add_item(app->date_menu, "Auto (RTC)", K2_DATE_AUTO_INDEX, k2_date_menu_callback, app);
    submenu_add_item(app->date_menu, "Custom", K2_DATE_CUSTOM_INDEX, k2_date_menu_callback, app);
}

static void k2_supplier_menu_callback(void* context, uint32_t index) {
    K2App* app = context;
    if(index == K2_SUPPLIER_CUSTOM_INDEX) {
        k2_open_text_input(
            app,
            K2TextInputSupplier,
            "Supplier ID (4)",
            app->tag_config.supplier_id,
            k2_supplier_validator,
            K2_TAG_SUPPLIER_LEN);
        return;
    }
    k2_config_set_supplier_preset(app, index);
    k2_config_refresh_item(app, K2CfgSupplier);
    app->current_view = K2ViewConfig;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
}

static void k2_supplier_menu_build(K2App* app) {
    submenu_reset(app->supplier_menu);
    submenu_set_header(app->supplier_menu, "Select Supplier");
    for(size_t i = 0; i < COUNT_OF(k2_supplier_presets); i++) {
        submenu_add_item(
            app->supplier_menu, k2_supplier_presets[i].label, i, k2_supplier_menu_callback, app);
    }
    submenu_add_item(
        app->supplier_menu, "Custom", K2_SUPPLIER_CUSTOM_INDEX, k2_supplier_menu_callback, app);
}

static void k2_batch_menu_callback(void* context, uint32_t index) {
    K2App* app = context;
    if(index == K2_BATCH_CUSTOM_INDEX) {
        k2_open_text_input(
            app,
            K2TextInputBatch,
            "Batch ID (2)",
            app->tag_config.batch_id,
            k2_batch_validator,
            K2_TAG_BATCH_LEN);
        return;
    }
    k2_config_set_batch_preset(app, index);
    k2_config_refresh_item(app, K2CfgBatch);
    app->current_view = K2ViewConfig;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
}

static void k2_batch_menu_build(K2App* app) {
    submenu_reset(app->batch_menu);
    submenu_set_header(app->batch_menu, "Select Batch");
    for(size_t i = 0; i < COUNT_OF(k2_batch_presets); i++) {
        submenu_add_item(
            app->batch_menu, k2_batch_presets[i].label, i, k2_batch_menu_callback, app);
    }
    submenu_add_item(
        app->batch_menu, "Custom", K2_BATCH_CUSTOM_INDEX, k2_batch_menu_callback, app);
}

static void k2_color_menu_callback(void* context, uint32_t index) {
    K2App* app = context;
    if(index == K2_COLOR_CUSTOM_INDEX) {
        k2_open_text_input(
            app,
            K2TextInputColor,
            "Color RRGGBB",
            app->tag_config.color_hex,
            k2_color_validator,
            6);
        return;
    }
    k2_config_set_color_preset(app, index);
    k2_config_refresh_item(app, K2CfgColor);
    app->current_view = K2ViewConfig;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
}

static void k2_color_menu_build(K2App* app) {
    submenu_reset(app->color_menu);
    submenu_set_header(app->color_menu, "Select Color");
    for(size_t i = 0; i < COUNT_OF(k2_color_presets); i++) {
        submenu_add_item(app->color_menu, k2_color_presets[i].label, i, k2_color_menu_callback, app);
    }
    submenu_add_item(app->color_menu, "Custom", K2_COLOR_CUSTOM_INDEX, k2_color_menu_callback, app);
}

static void k2_material_menu_callback(void* context, uint32_t index) {
    K2App* app = context;
    if(index == K2_MATERIAL_CUSTOM_INDEX) {
        k2_open_text_input(
            app,
            K2TextInputMaterial,
            "Material ID (5)",
            app->tag_config.material_id,
            k2_material_validator,
            5);
        return;
    }
    k2_config_set_material(app, index);
    k2_config_refresh_item(app, K2CfgMaterial);
    app->current_view = K2ViewConfig;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
}

static void k2_material_menu_build(K2App* app) {
    submenu_reset(app->material_menu);
    submenu_set_header(app->material_menu, "Select Material");
    for(size_t i = 0; i < k2_materials_count; i++) {
        submenu_add_item(app->material_menu, k2_materials[i].label, i, k2_material_menu_callback, app);
    }
    submenu_add_item(
        app->material_menu, "Custom", K2_MATERIAL_CUSTOM_INDEX, k2_material_menu_callback, app);
}

static void k2_serial_input_done(void* context, int32_t number) {
    K2App* app = context;
    if(number < 1) number = 1;
    if(number > 999999) number = 999999;
    app->tag_config.serial = (uint32_t)number;
    k2_config_refresh_item(app, K2CfgSerial);
    app->current_view = K2ViewConfig;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
}

static void k2_config_enter_callback(void* context, uint32_t index) {
    K2App* app = context;
    if(index == K2CfgDate) {
        k2_date_menu_build(app);
        app->current_view = K2ViewDatePick;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewDatePick);
    } else if(index == K2CfgSupplier) {
        k2_supplier_menu_build(app);
        app->current_view = K2ViewSupplierPick;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewSupplierPick);
    } else if(index == K2CfgBatch) {
        k2_batch_menu_build(app);
        app->current_view = K2ViewBatchPick;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewBatchPick);
    } else if(index == K2CfgMaterial) {
        k2_material_menu_build(app);
        app->current_view = K2ViewMaterialPick;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewMaterialPick);
    } else if(index == K2CfgColor) {
        k2_color_menu_build(app);
        app->current_view = K2ViewColorPick;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewColorPick);
    } else if(index == K2CfgSerial) {
        number_input_set_header_text(app->number_input, "Serial Number");
        number_input_set_result_callback(
            app->number_input,
            k2_serial_input_done,
            app,
            (int32_t)app->tag_config.serial,
            1,
            999999);
        app->current_view = K2ViewSerialInput;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewSerialInput);
    } else if(index == K2CfgReserve) {
        k2_open_text_input(
            app,
            K2TextInputReserve,
            "Reserve (14 chars)",
            app->tag_config.reserve,
            k2_reserve_validator,
            K2_TAG_RESERVE_LEN);
    }
}

static int32_t k2_write_worker(void* context) {
    K2App* app = context;
    if(!k2_tag_wait_for_classic(app->nfc, &app->worker_stop)) return 0;

    K2TagResult result = k2_tag_write(app->nfc, &app->tag_config);
    snprintf(
        app->result_text,
        sizeof(app->result_text),
        "Write: %s\n\nWrite another tag with the\nsame settings for spool side 2.",
        k2_tag_result_str(result));
    view_dispatcher_send_custom_event(app->view_dispatcher, K2EventDone);
    return 0;
}

static int32_t k2_read_worker(void* context) {
    K2App* app = context;
    if(!k2_tag_wait_for_classic(app->nfc, &app->worker_stop)) return 0;

    char tag_data[128];
    K2TagResult result = k2_tag_read(app->nfc, tag_data, sizeof(tag_data));
    if(result == K2TagOk) {
        if(k2_tag_parse_payload(tag_data, &app->tag_config)) {
            k2_config_refresh(app);
            snprintf(
                app->result_text,
                sizeof(app->result_text),
                "Read OK\nConfig loaded.\n\nTag data:\n%s",
                tag_data);
        } else {
            snprintf(
                app->result_text,
                sizeof(app->result_text),
                "Read OK (parse failed)\n\nTag data:\n%s",
                tag_data);
        }
    } else {
        snprintf(app->result_text, sizeof(app->result_text), "Read: %s", k2_tag_result_str(result));
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, K2EventDone);
    return 0;
}

static void k2_start_worker(K2App* app, FuriThreadCallback callback, const char* header) {
    k2_stop_worker(app);
    popup_reset(app->popup);
    popup_set_header(app->popup, header, 64, 6, AlignCenter, AlignTop);
    popup_set_text(
        app->popup,
        "Hold tag near back\n\nWaiting for tag...",
        64,
        22,
        AlignCenter,
        AlignTop);
    app->current_view = K2ViewPopup;
    app->worker_stop = false;
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewPopup);
    app->worker = furi_thread_alloc_ex("K2Worker", 4096, callback, app);
    furi_thread_start(app->worker);
}

static void k2_menu_callback(void* context, uint32_t index) {
    K2App* app = context;
    if(index == K2MenuConfigure) {
        k2_config_refresh(app);
        app->current_view = K2ViewConfig;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
    } else if(index == K2MenuWrite) {
        k2_start_worker(app, k2_write_worker, "Writing...");
    } else if(index == K2MenuRead) {
        k2_start_worker(app, k2_read_worker, "Reading...");
    } else if(index == K2MenuAbout) {
        snprintf(
            app->result_text,
            sizeof(app->result_text),
            "K2 RFID Writer\n"
            "Based on DnG-Crafts/K2-RFID\n\n"
            "Configure:\n"
            " Left/Right = cycle options\n"
            " OK = pick date/supplier/batch/\n"
            " material/color/reserve\n"
            " Custom = manual ID entry\n\n"
            "Requires blank MIFARE\n"
            "Classic 1K tags.");
        k2_show_text(app);
    }
}

static bool k2_navigation_callback(void* context) {
    K2App* app = context;
    if(app->current_view == K2ViewPopup) {
        k2_stop_worker(app);
        app->current_view = K2ViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewMenu);
        return true;
    }
    if(app->current_view == K2ViewMaterialPick || app->current_view == K2ViewColorPick ||
       app->current_view == K2ViewSupplierPick || app->current_view == K2ViewBatchPick ||
       app->current_view == K2ViewDatePick || app->current_view == K2ViewTextInput ||
       app->current_view == K2ViewSerialInput) {
        app->current_view = K2ViewConfig;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewConfig);
        return true;
    }
    if(app->current_view == K2ViewConfig || app->current_view == K2ViewText) {
        app->current_view = K2ViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewMenu);
        return true;
    }
    return false;
}

static bool k2_custom_event_callback(void* context, uint32_t event) {
    K2App* app = context;
    if(event == K2EventDone) {
        k2_stop_worker(app);
        k2_show_text(app);
        return true;
    }
    return false;
}

static void k2_config_build(K2App* app) {
    variable_item_list_reset(app->config);

    app->config_items[K2CfgDate] = variable_item_list_add(app->config, "Date", 0, NULL, app);
    char date_text[16];
    k2_date_set_display_text(app, date_text, sizeof(date_text));
    variable_item_set_current_value_text(app->config_items[K2CfgDate], date_text);

    app->config_items[K2CfgSupplier] =
        variable_item_list_add(app->config, "Supplier", 0, NULL, app);
    char supplier_text[24];
    k2_supplier_set_display_text(app, supplier_text, sizeof(supplier_text));
    variable_item_set_current_value_text(app->config_items[K2CfgSupplier], supplier_text);

    app->config_items[K2CfgBatch] = variable_item_list_add(app->config, "Batch", 0, NULL, app);
    char batch_text[24];
    k2_batch_set_display_text(app, batch_text, sizeof(batch_text));
    variable_item_set_current_value_text(app->config_items[K2CfgBatch], batch_text);

    app->config_items[K2CfgMaterial] =
        variable_item_list_add(app->config, "Material", 0, NULL, app);
    char material_text[32];
    k2_material_set_display_text(app, material_text, sizeof(material_text));
    variable_item_set_current_value_text(app->config_items[K2CfgMaterial], material_text);

    app->config_items[K2CfgWeight] = variable_item_list_add(
        app->config, "Weight", k2_tag_weight_count(), k2_config_weight_changed, app);
    variable_item_set_current_value_index(
        app->config_items[K2CfgWeight], app->tag_config.weight_index);
    variable_item_set_current_value_text(
        app->config_items[K2CfgWeight], k2_tag_weight_label(app->tag_config.weight_index));

    app->config_items[K2CfgColor] = variable_item_list_add(app->config, "Color", 0, NULL, app);
    char color_text[24];
    k2_color_set_display_text(app, color_text, sizeof(color_text));
    variable_item_set_current_value_text(app->config_items[K2CfgColor], color_text);

    app->config_items[K2CfgSerial] = variable_item_list_add(app->config, "Serial", 0, NULL, app);
    char serial_text[16];
    snprintf(serial_text, sizeof(serial_text), "%06lu", (unsigned long)app->tag_config.serial);
    variable_item_set_current_value_text(app->config_items[K2CfgSerial], serial_text);

    app->config_items[K2CfgReserve] =
        variable_item_list_add(app->config, "Reserve", 0, NULL, app);
    variable_item_set_current_value_text(
        app->config_items[K2CfgReserve], app->tag_config.reserve);

    app->config_items[K2CfgPrinter] = variable_item_list_add(
        app->config, "Printer", k2_tag_printer_count(), k2_config_printer_changed, app);
    variable_item_set_current_value_index(
        app->config_items[K2CfgPrinter], app->tag_config.printer_index);
    variable_item_set_current_value_text(
        app->config_items[K2CfgPrinter],
        k2_tag_printer_suffix(app->tag_config.printer_index));
}

static K2App* k2_app_alloc(void) {
    K2App* app = malloc(sizeof(K2App));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->material_menu = submenu_alloc();
    app->color_menu = submenu_alloc();
    app->supplier_menu = submenu_alloc();
    app->batch_menu = submenu_alloc();
    app->date_menu = submenu_alloc();
    app->config = variable_item_list_alloc();
    app->popup = popup_alloc();
    app->text_box = text_box_alloc();
    app->text_input = text_input_alloc();
    app->number_input = number_input_alloc();
    app->nfc = nfc_alloc();
    app->worker = NULL;
    app->current_view = K2ViewMenu;
    app->worker_stop = false;
    app->material_index = 0;
    app->color_index = 0;
    app->supplier_index = 0;
    app->batch_index = 0;
    app->text_input_target = K2TextInputDate;

    k2_tag_config_set_defaults(&app->tag_config);
    k2_material_sync_index(app);
    k2_color_sync_index(app);
    k2_supplier_sync_index(app);
    k2_batch_sync_index(app);

    submenu_set_header(app->submenu, "K2 RFID Writer");
    submenu_add_item(app->submenu, "Configure Tag", K2MenuConfigure, k2_menu_callback, app);
    submenu_add_item(app->submenu, "Write Tag", K2MenuWrite, k2_menu_callback, app);
    submenu_add_item(app->submenu, "Read Tag", K2MenuRead, k2_menu_callback, app);
    submenu_add_item(app->submenu, "About", K2MenuAbout, k2_menu_callback, app);

    k2_config_build(app);
    variable_item_list_set_enter_callback(app->config, k2_config_enter_callback, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, k2_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, k2_custom_event_callback);

    view_dispatcher_add_view(app->view_dispatcher, K2ViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, K2ViewConfig, variable_item_list_get_view(app->config));
    view_dispatcher_add_view(
        app->view_dispatcher, K2ViewMaterialPick, submenu_get_view(app->material_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, K2ViewColorPick, submenu_get_view(app->color_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, K2ViewSupplierPick, submenu_get_view(app->supplier_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, K2ViewBatchPick, submenu_get_view(app->batch_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, K2ViewDatePick, submenu_get_view(app->date_menu));
    view_dispatcher_add_view(app->view_dispatcher, K2ViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(app->view_dispatcher, K2ViewText, text_box_get_view(app->text_box));
    view_dispatcher_add_view(
        app->view_dispatcher, K2ViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, K2ViewSerialInput, number_input_get_view(app->number_input));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void k2_app_free(K2App* app) {
    if(!app) return;
    k2_stop_worker(app);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewMaterialPick);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewColorPick);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewSupplierPick);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewBatchPick);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewDatePick);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewText);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, K2ViewSerialInput);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    submenu_free(app->material_menu);
    submenu_free(app->color_menu);
    submenu_free(app->supplier_menu);
    submenu_free(app->batch_menu);
    submenu_free(app->date_menu);
    variable_item_list_free(app->config);
    popup_free(app->popup);
    text_box_free(app->text_box);
    text_input_free(app->text_input);
    number_input_free(app->number_input);
    nfc_free(app->nfc);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t k2_rfid_app(void* p) {
    UNUSED(p);
    K2App* app = k2_app_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewMenu);
    view_dispatcher_run(app->view_dispatcher);
    k2_app_free(app);
    return 0;
}
