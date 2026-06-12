#include "baofeng_ui.h"

#define EEPROM_VOICE_PROMPT 0x02B0
#define EEPROM_VOICE_LANGUAGE 0x02B1
#define EEPROM_SCAN 0x02B2
#define EEPROM_VOX 0x02B3
#define EEPROM_VOX_LEVEL 0x02B4
#define EEPROM_VOX_INHIBIT_RX 0x02B5
#define EEPROM_LOW_VOL_INHIBIT_TX 0x02B6
#define EEPROM_HIGH_VOL_INHIBIT_TX 0x02B7
#define EEPROM_ALARM 0x02B8
#define EEPROM_FM_RADIO 0x02B9

#define EEPROM_SETTINGS2_BEEP_BAT 0x03C0 // bit 0: beep, bit 1: battery saver
#define EEPROM_SQUELCH_LEVEL 0x03C1
#define EEPROM_SIDEKEY 0x03C2
#define EEPROM_TIMEOUT 0x03C3
#define EEPROM_SCANMODE 0x03C7 // bit 0: scan mode

static const char* on_off_names[] = {"Off", "On"};
static const char* language_names[] = {"English", "Chinese"};
static const char* sidekey_names[] = {"Off", "Monitor", "TX Power", "Alarm"};
static const char* scanmode_names[] = {"Carrier", "Time"};
static const char* timeout_names[] = {
    "Off", "30s", "60s", "90s", "120s", "150s", 
    "180s", "210s", "240s", "270s", "300s"
};

enum SettingsItem {
    ItemVoicePrompt,
    ItemVoiceLanguage,
    ItemScan,
    ItemVOX,
    ItemVOXLevel,
    ItemVOXInhibitRX,
    ItemLowVolInhibitTX,
    ItemHighVolInhibitTX,
    ItemAlarm,
    ItemFMRadio,
    ItemBeep,
    ItemBatterySaver,
    ItemSquelchLevel,
    ItemSideKey,
    ItemTimeout,
    ItemScanMode,
    ItemCount
};

static void update_bool_item(VariableItem* item, uint8_t val) {
    variable_item_set_current_value_index(item, val ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_names[val ? 1 : 0]);
}

static void settings_changed_callback(VariableItem* item) {
    BaofengApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t selected_item = variable_item_list_get_selected_item_index(app->variable_item_list);

    uint8_t* eeprom = app->full_eeprom;

    switch(selected_item) {
        case ItemVoicePrompt:
            eeprom[EEPROM_VOICE_PROMPT] = index;
            update_bool_item(item, index);
            break;
        case ItemVoiceLanguage:
            eeprom[EEPROM_VOICE_LANGUAGE] = index;
            variable_item_set_current_value_text(item, language_names[index]);
            break;
        case ItemScan:
            eeprom[EEPROM_SCAN] = index;
            update_bool_item(item, index);
            break;
        case ItemVOX:
            eeprom[EEPROM_VOX] = index;
            update_bool_item(item, index);
            break;
        case ItemVOXLevel:
            eeprom[EEPROM_VOX_LEVEL] = index; // 0-4
            char lvl_str[4];
            snprintf(lvl_str, sizeof(lvl_str), "%d", index + 1);
            variable_item_set_current_value_text(item, lvl_str);
            break;
        case ItemVOXInhibitRX:
            eeprom[EEPROM_VOX_INHIBIT_RX] = index;
            update_bool_item(item, index);
            break;
        case ItemLowVolInhibitTX:
            eeprom[EEPROM_LOW_VOL_INHIBIT_TX] = index;
            update_bool_item(item, index);
            break;
        case ItemHighVolInhibitTX:
            eeprom[EEPROM_HIGH_VOL_INHIBIT_TX] = index;
            update_bool_item(item, index);
            break;
        case ItemAlarm:
            eeprom[EEPROM_ALARM] = index;
            update_bool_item(item, index);
            break;
        case ItemFMRadio:
            eeprom[EEPROM_FM_RADIO] = index ? 0xFF : 0x00;
            update_bool_item(item, index);
            break;
        case ItemBeep:
            if(index) eeprom[EEPROM_SETTINGS2_BEEP_BAT] |= (1 << 0);
            else eeprom[EEPROM_SETTINGS2_BEEP_BAT] &= ~(1 << 0);
            update_bool_item(item, index);
            break;
        case ItemBatterySaver:
            if(index) eeprom[EEPROM_SETTINGS2_BEEP_BAT] |= (1 << 1);
            else eeprom[EEPROM_SETTINGS2_BEEP_BAT] &= ~(1 << 1);
            update_bool_item(item, index);
            break;
        case ItemSquelchLevel:
            eeprom[EEPROM_SQUELCH_LEVEL] = index;
            char sq_str[4];
            snprintf(sq_str, sizeof(sq_str), "%d", index);
            variable_item_set_current_value_text(item, sq_str);
            break;
        case ItemSideKey:
            eeprom[EEPROM_SIDEKEY] = index;
            variable_item_set_current_value_text(item, sidekey_names[index]);
            break;
        case ItemTimeout:
            eeprom[EEPROM_TIMEOUT] = index;
            variable_item_set_current_value_text(item, timeout_names[index]);
            break;
        case ItemScanMode:
            if(index) eeprom[EEPROM_SCANMODE] |= (1 << 0);
            else eeprom[EEPROM_SCANMODE] &= ~(1 << 0);
            variable_item_set_current_value_text(item, scanmode_names[index]);
            break;
    }
}

void baofeng_scene_settings_on_enter(void* context) {
    BaofengApp* app = context;
    VariableItemList* vil = app->variable_item_list;
    variable_item_list_reset(vil);

    uint8_t* eeprom = app->full_eeprom;
    VariableItem* item;

    // ItemVoicePrompt
    item = variable_item_list_add(vil, "Voice Prompt", 2, settings_changed_callback, app);
    update_bool_item(item, eeprom[EEPROM_VOICE_PROMPT]);

    // ItemVoiceLanguage
    item = variable_item_list_add(vil, "Voice Language", 2, settings_changed_callback, app);
    uint8_t lang = eeprom[EEPROM_VOICE_LANGUAGE] > 1 ? 0 : eeprom[EEPROM_VOICE_LANGUAGE];
    variable_item_set_current_value_index(item, lang);
    variable_item_set_current_value_text(item, language_names[lang]);

    // ItemScan
    item = variable_item_list_add(vil, "Scan", 2, settings_changed_callback, app);
    update_bool_item(item, eeprom[EEPROM_SCAN]);

    // ItemVOX
    item = variable_item_list_add(vil, "VOX", 2, settings_changed_callback, app);
    update_bool_item(item, eeprom[EEPROM_VOX]);

    // ItemVOXLevel (1-5)
    item = variable_item_list_add(vil, "VOX Level", 5, settings_changed_callback, app);
    uint8_t vox_lvl = eeprom[EEPROM_VOX_LEVEL];
    if(vox_lvl > 4) vox_lvl = 0;
    variable_item_set_current_value_index(item, vox_lvl);
    char lvl_str[4];
    snprintf(lvl_str, sizeof(lvl_str), "%d", vox_lvl + 1);
    variable_item_set_current_value_text(item, lvl_str);

    // ItemVOXInhibitRX
    item = variable_item_list_add(vil, "Inhibit VOX on RX", 2, settings_changed_callback, app);
    update_bool_item(item, eeprom[EEPROM_VOX_INHIBIT_RX]);

    // ItemLowVolInhibitTX
    item = variable_item_list_add(vil, "Low Vol Inhibit TX", 2, settings_changed_callback, app);
    update_bool_item(item, eeprom[EEPROM_LOW_VOL_INHIBIT_TX]);

    // ItemHighVolInhibitTX
    item = variable_item_list_add(vil, "High Vol Inhibit TX", 2, settings_changed_callback, app);
    update_bool_item(item, eeprom[EEPROM_HIGH_VOL_INHIBIT_TX]);

    // ItemAlarm
    item = variable_item_list_add(vil, "Alarm", 2, settings_changed_callback, app);
    update_bool_item(item, eeprom[EEPROM_ALARM]);

    // ItemFMRadio
    item = variable_item_list_add(vil, "FM Radio", 2, settings_changed_callback, app);
    update_bool_item(item, eeprom[EEPROM_FM_RADIO] != 0);

    // ItemBeep
    item = variable_item_list_add(vil, "Beep", 2, settings_changed_callback, app);
    update_bool_item(item, (eeprom[EEPROM_SETTINGS2_BEEP_BAT] & (1 << 0)) != 0);

    // ItemBatterySaver
    item = variable_item_list_add(vil, "Battery Saver", 2, settings_changed_callback, app);
    update_bool_item(item, (eeprom[EEPROM_SETTINGS2_BEEP_BAT] & (1 << 1)) != 0);

    // ItemSquelchLevel
    item = variable_item_list_add(vil, "Squelch Level", 10, settings_changed_callback, app);
    uint8_t sq = eeprom[EEPROM_SQUELCH_LEVEL];
    if(sq > 9) sq = 3;
    variable_item_set_current_value_index(item, sq);
    char sq_str[4];
    snprintf(sq_str, sizeof(sq_str), "%d", sq);
    variable_item_set_current_value_text(item, sq_str);

    // ItemSideKey
    item = variable_item_list_add(vil, "Side Key Function", 4, settings_changed_callback, app);
    uint8_t sk = eeprom[EEPROM_SIDEKEY];
    if(sk > 3) sk = 1;
    variable_item_set_current_value_index(item, sk);
    variable_item_set_current_value_text(item, sidekey_names[sk]);

    // ItemTimeout
    item = variable_item_list_add(vil, "Timeout Timer", 11, settings_changed_callback, app);
    uint8_t tt = eeprom[EEPROM_TIMEOUT];
    if(tt > 10) tt = 0;
    variable_item_set_current_value_index(item, tt);
    variable_item_set_current_value_text(item, timeout_names[tt]);

    // ItemScanMode
    item = variable_item_list_add(vil, "Scan Mode", 2, settings_changed_callback, app);
    uint8_t sm = (eeprom[EEPROM_SCANMODE] & (1 << 0)) != 0;
    variable_item_set_current_value_index(item, sm);
    variable_item_set_current_value_text(item, scanmode_names[sm]);

    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewVariableItemList);
}

bool baofeng_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void baofeng_scene_settings_on_exit(void* context) {
    BaofengApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
