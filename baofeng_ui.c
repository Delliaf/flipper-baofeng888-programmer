#include "baofeng_ui.h"
#include "baofeng_radio.h"
#include "baofeng_storage.h"

// Standard CTCSS tones (39 EIA standard + some extra, 50 total usually, but let's use 50 common ones)
const uint16_t ctcss_tones[] = {
    670, 693, 719, 744, 770, 797, 825, 854, 885, 915, 948, 974,
    1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318, 1365, 1413, 1462,
    1514, 1567, 1622, 1679, 1738, 1799, 1862, 1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503, 2541
};
#define CTCSS_COUNT (sizeof(ctcss_tones)/sizeof(ctcss_tones[0]))

const uint16_t dcs_codes[] = {
    23, 25, 26, 31, 32, 43, 47, 51, 54, 65, 71, 72, 73, 74, 114, 115, 116, 125, 131, 132, 134, 143, 152, 155, 156, 162, 165, 172, 174, 205, 223, 226, 243, 244, 245, 251, 261, 263, 265, 271, 306, 311, 315, 331, 343, 346, 351, 364, 365, 371, 411, 412, 413, 423, 431, 432, 445, 464, 465, 466, 503, 506, 516, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, 662, 664, 703, 712, 723, 731, 732, 734, 743, 754
};
#define DCS_COUNT (sizeof(dcs_codes)/sizeof(dcs_codes[0]))

static uint8_t find_ctcss_idx(uint16_t val) {
    for(uint8_t i=0; i<CTCSS_COUNT; i++) {
        if(ctcss_tones[i] == val) return i;
    }
    return 0;
}

static uint8_t find_dcs_idx(uint16_t val) {
    for(uint8_t i=0; i<DCS_COUNT; i++) {
        if(dcs_codes[i] == val) return i;
    }
    return 0;
}

// --- Main Menu ---
static void baofeng_main_menu_callback(void* context, uint32_t index) {
    BaofengApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void baofeng_scene_main_menu_on_enter(void* context) {
    BaofengApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "BF-888s Programmer");
    submenu_add_item(app->submenu, "Read Radio", BaofengCustomEventMenuRead, baofeng_main_menu_callback, app);
    submenu_add_item(app->submenu, "Edit Channels", BaofengCustomEventMenuEdit, baofeng_main_menu_callback, app);
    submenu_add_item(app->submenu, "Radio settings", BaofengCustomEventMenuSettings, baofeng_main_menu_callback, app);
    submenu_add_item(app->submenu, "Write (Clone) Radio", BaofengCustomEventMenuWrite, baofeng_main_menu_callback, app);
    submenu_add_item(app->submenu, "Load from SD", BaofengCustomEventMenuLoad, baofeng_main_menu_callback, app);
    submenu_add_item(app->submenu, "Save to SD", BaofengCustomEventMenuSave, baofeng_main_menu_callback, app);
    submenu_add_item(app->submenu, "Wiring", BaofengCustomEventMenuWiring, baofeng_main_menu_callback, app);
    submenu_add_item(app->submenu, "About / Help", BaofengCustomEventMenuAbout, baofeng_main_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewSubmenu);
}

bool baofeng_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    BaofengApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BaofengCustomEventMenuRead) {
            bool success = baofeng_radio_read(app);
            snprintf(app->popup_text, sizeof(app->popup_text), success ? "Read Success!" : "Read Failed!\nCheck connection.");
            scene_manager_next_scene(app->scene_manager, BaofengScenePopup);
            consumed = true;
        } else if(event.event == BaofengCustomEventMenuEdit) {
            scene_manager_next_scene(app->scene_manager, BaofengSceneChannelList);
            consumed = true;
        } else if(event.event == BaofengCustomEventMenuWrite) {
            bool success = baofeng_radio_write(app);
            snprintf(app->popup_text, sizeof(app->popup_text), success ? "Write Success!" : "Write Failed!\nCheck connection.");
            scene_manager_next_scene(app->scene_manager, BaofengScenePopup);
            consumed = true;
        } else if(event.event == BaofengCustomEventMenuLoad) {
            bool success = baofeng_load_dump(app);
            snprintf(app->popup_text, sizeof(app->popup_text), success ? "Load Success!" : "Load Failed!");
            scene_manager_next_scene(app->scene_manager, BaofengScenePopup);
            consumed = true;
        } else if(event.event == BaofengCustomEventMenuAbout) {
            scene_manager_next_scene(app->scene_manager, BaofengSceneAbout);
            consumed = true;
        } else if(event.event == BaofengCustomEventMenuSave) {
            snprintf(app->text_store, sizeof(app->text_store), "radio");
            scene_manager_next_scene(app->scene_manager, BaofengSceneSaveName);
            consumed = true;
        } else if(event.event == BaofengCustomEventMenuWiring) {
            scene_manager_next_scene(app->scene_manager, BaofengSceneWiring);
            consumed = true;
        } else if(event.event == BaofengCustomEventMenuSettings) {
            scene_manager_next_scene(app->scene_manager, BaofengSceneSettings);
            consumed = true;
        }
    }
    return consumed;
}

void baofeng_scene_main_menu_on_exit(void* context) {
    BaofengApp* app = context;
    submenu_reset(app->submenu);
}

// --- Channel List ---
static void baofeng_channel_list_callback(void* context, uint32_t index) {
    BaofengApp* app = context;
    app->current_edit_channel = (uint8_t)index;
    view_dispatcher_send_custom_event(app->view_dispatcher, BaofengCustomEventChannelSelected);
}

void baofeng_scene_channel_list_on_enter(void* context) {
    BaofengApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Channel");
    for(uint8_t i = 0; i < BAOFENG_CHANNELS_COUNT; i++) {
        snprintf(app->channel_names[i], sizeof(app->channel_names[i]), "CH %d", i + 1);
        submenu_add_item(app->submenu, app->channel_names[i], i, baofeng_channel_list_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewSubmenu);
}

bool baofeng_scene_channel_list_on_event(void* context, SceneManagerEvent event) {
    BaofengApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BaofengCustomEventChannelSelected) {
            scene_manager_next_scene(app->scene_manager, BaofengSceneChannelEdit);
            return true;
        }
    }
    return false;
}

void baofeng_scene_channel_list_on_exit(void* context) {
    BaofengApp* app = context;
    submenu_reset(app->submenu);
}

// --- Channel Edit ---
static void update_freq_text(BaofengApp* app, bool is_tx) {
    uint32_t freq_10hz = is_tx ? app->current_tx_freq_10hz : app->current_rx_freq_10hz;
    char* text_buffer = is_tx ? app->item_text_tx : app->item_text_rx;
    
    uint32_t mhz = freq_10hz / 100000;
    uint32_t remainder = freq_10hz % 100000;
    
    snprintf(text_buffer, 32, "%lu.%05lu", mhz, remainder * 10);
    
    if(!is_tx && app->item_rx_freq) {
        variable_item_set_current_value_text(app->item_rx_freq, text_buffer);
    } else if(is_tx && app->item_tx_freq) {
        variable_item_set_current_value_text(app->item_tx_freq, text_buffer);
    }
    
    freq_10hz_to_bcd(freq_10hz, is_tx ? app->channels[app->current_edit_channel].txfreq : app->channels[app->current_edit_channel].rxfreq);
}

static void freq_enter_callback(void* context, uint32_t index) {
    BaofengApp* app = context;
    if (index == 0) { // RX Freq
        app->is_editing_tx = false;
        uint32_t freq_10hz = app->current_rx_freq_10hz;
        snprintf(app->text_store, sizeof(app->text_store), "%lu", freq_10hz);
        scene_manager_next_scene(app->scene_manager, BaofengSceneTextInput);
    } else if (index == 1) { // TX Freq
        app->is_editing_tx = true;
        uint32_t freq_10hz = app->current_tx_freq_10hz;
        snprintf(app->text_store, sizeof(app->text_store), "%lu", freq_10hz);
        scene_manager_next_scene(app->scene_manager, BaofengSceneTextInput);
    }
}

static void set_tone_ui(VariableItem* item, uint8_t index, char* text_buffer, BF888S_Channel* ch, bool is_tx) {
    uint8_t* tone_ptr = is_tx ? ch->txtone : ch->rxtone;
    if (index == 0) {
        snprintf(text_buffer, 32, "None");
        encode_tone(ToneTypeNone, 0, tone_ptr);
    } else if (index <= CTCSS_COUNT) {
        uint16_t val = ctcss_tones[index - 1];
        snprintf(text_buffer, 32, "%d.%d Hz", val/10, val%10);
        encode_tone(ToneTypeCTCSS, val, tone_ptr);
    } else {
        uint16_t val = dcs_codes[index - CTCSS_COUNT - 1];
        snprintf(text_buffer, 32, "D%03dN", val);
        encode_tone(ToneTypeDCS, val, tone_ptr);
    }
    variable_item_set_current_value_text(item, text_buffer);
}

static void rx_tone_changed(VariableItem* item) {
    BaofengApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    set_tone_ui(item, index, app->item_text_rx_tone, &app->channels[app->current_edit_channel], false);
}

static void tx_tone_changed(VariableItem* item) {
    BaofengApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    set_tone_ui(item, index, app->item_text_tx_tone, &app->channels[app->current_edit_channel], true);
}

static void tx_power_changed(VariableItem* item) {
    BaofengApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "High" : "Low");
    if(index) app->channels[app->current_edit_channel].flags |= 0x08; // highpower
    else app->channels[app->current_edit_channel].flags &= ~0x08;
}

static void narrow_changed(VariableItem* item) {
    BaofengApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "Narrow" : "Wide");
    if(index) app->channels[app->current_edit_channel].flags |= 0x04; // narrow
    else app->channels[app->current_edit_channel].flags &= ~0x04;
}

static void scan_add_changed(VariableItem* item) {
    BaofengApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "No" : "Yes");
    if(index) app->channels[app->current_edit_channel].flags |= 0x10; // skip=1
    else app->channels[app->current_edit_channel].flags &= ~0x10;
}

static void beat_shift_changed(VariableItem* item) {
    BaofengApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "Yes" : "No");
    // CHIRP: enabled = 0, disabled = 1
    if(index) app->channels[app->current_edit_channel].flags &= ~0x02; // beatshift
    else app->channels[app->current_edit_channel].flags |= 0x02;
}

static void bcl_changed(VariableItem* item) {
    BaofengApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, index ? "Yes" : "No");
    // CHIRP: enabled = 0, disabled = 1
    if(index) app->channels[app->current_edit_channel].flags &= ~0x01; // bcl
    else app->channels[app->current_edit_channel].flags |= 0x01;
}

void baofeng_scene_channel_edit_on_enter(void* context) {
    BaofengApp* app = context;
    variable_item_list_reset(app->variable_item_list);
    
    BF888S_Channel* ch = &app->channels[app->current_edit_channel];
    
    if (ch->rxfreq[0] == 0xFF && ch->rxfreq[1] == 0xFF && ch->rxfreq[2] == 0xFF && ch->rxfreq[3] == 0xFF) {
        freq_10hz_to_bcd(400000 * 100, ch->rxfreq);
        freq_10hz_to_bcd(400000 * 100, ch->txfreq);
        encode_tone(ToneTypeNone, 0, ch->rxtone);
        encode_tone(ToneTypeNone, 0, ch->txtone);
        ch->flags = 0x0B; // High Power, Wide, Scan Add=Yes, Beat=Disabled, BCL=Disabled
        app->current_rx_freq_10hz = 40000000;
        app->current_tx_freq_10hz = 40000000;
    } else {
        app->current_rx_freq_10hz = bcd_to_freq_10hz(ch->rxfreq);
        app->current_tx_freq_10hz = bcd_to_freq_10hz(ch->txfreq);
    }

    // Enter callback to trigger text input
    variable_item_list_set_enter_callback(app->variable_item_list, freq_enter_callback, app);

    // Freq RX
    app->item_rx_freq = variable_item_list_add(app->variable_item_list, "RX Freq", 1, NULL, app);
    update_freq_text(app, false);

    // Freq TX
    app->item_tx_freq = variable_item_list_add(app->variable_item_list, "TX Freq", 1, NULL, app);
    update_freq_text(app, true);

    // Tones
    uint16_t tone_val;
    ToneType tone_type;
    uint8_t num_tones = 1 + CTCSS_COUNT + DCS_COUNT; // None + CTCSS + DCS
    
    // RX Tone
    VariableItem* item_rx_tone = variable_item_list_add(app->variable_item_list, "RX Tone", num_tones, rx_tone_changed, app);
    tone_type = decode_tone(ch->rxtone, &tone_val);
    uint8_t rx_tone_idx = 0;
    if(tone_type == ToneTypeCTCSS) rx_tone_idx = 1 + find_ctcss_idx(tone_val);
    else if(tone_type == ToneTypeDCS) rx_tone_idx = 1 + CTCSS_COUNT + find_dcs_idx(tone_val);
    variable_item_set_current_value_index(item_rx_tone, rx_tone_idx);
    set_tone_ui(item_rx_tone, rx_tone_idx, app->item_text_rx_tone, ch, false);
    
    // TX Tone
    VariableItem* item_tx_tone = variable_item_list_add(app->variable_item_list, "TX Tone", num_tones, tx_tone_changed, app);
    tone_type = decode_tone(ch->txtone, &tone_val);
    uint8_t tx_tone_idx = 0;
    if(tone_type == ToneTypeCTCSS) tx_tone_idx = 1 + find_ctcss_idx(tone_val);
    else if(tone_type == ToneTypeDCS) tx_tone_idx = 1 + CTCSS_COUNT + find_dcs_idx(tone_val);
    variable_item_set_current_value_index(item_tx_tone, tx_tone_idx);
    set_tone_ui(item_tx_tone, tx_tone_idx, app->item_text_tx_tone, ch, true);
    
    // Flags
    VariableItem* item_pow = variable_item_list_add(app->variable_item_list, "TX Power", 2, tx_power_changed, app);
    uint8_t pow_idx = (ch->flags & 0x08) ? 1 : 0;
    variable_item_set_current_value_index(item_pow, pow_idx);
    variable_item_set_current_value_text(item_pow, pow_idx ? "High" : "Low");
    
    VariableItem* item_wn = variable_item_list_add(app->variable_item_list, "Bandwidth", 2, narrow_changed, app);
    uint8_t wn_idx = (ch->flags & 0x04) ? 1 : 0;
    variable_item_set_current_value_index(item_wn, wn_idx);
    variable_item_set_current_value_text(item_wn, wn_idx ? "Narrow" : "Wide");

    VariableItem* item_scan = variable_item_list_add(app->variable_item_list, "Scan Add", 2, scan_add_changed, app);
    uint8_t scan_idx = (ch->flags & 0x10) ? 1 : 0; // 1=Skip, so 0=Yes
    variable_item_set_current_value_index(item_scan, scan_idx);
    variable_item_set_current_value_text(item_scan, scan_idx ? "No" : "Yes");
    
    VariableItem* item_beat = variable_item_list_add(app->variable_item_list, "Beat Shift", 2, beat_shift_changed, app);
    uint8_t beat_idx = (ch->flags & 0x02) ? 0 : 1; // 0=No, 1=Yes
    variable_item_set_current_value_index(item_beat, beat_idx);
    variable_item_set_current_value_text(item_beat, beat_idx ? "Yes" : "No");
    
    VariableItem* item_bcl = variable_item_list_add(app->variable_item_list, "Busy Lockout", 2, bcl_changed, app);
    uint8_t bcl_idx = (ch->flags & 0x01) ? 0 : 1; // 0=No, 1=Yes
    variable_item_set_current_value_index(item_bcl, bcl_idx);
    variable_item_set_current_value_text(item_bcl, bcl_idx ? "Yes" : "No");

    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewVariableItemList);
}

bool baofeng_scene_channel_edit_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void baofeng_scene_channel_edit_on_exit(void* context) {
    BaofengApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}

// --- Popup ---
static void baofeng_popup_callback(void* context) {
    BaofengApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

void baofeng_scene_popup_on_enter(void* context) {
    BaofengApp* app = context;
    popup_reset(app->popup);
    popup_set_header(app->popup, "Info", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, app->popup_text, 64, 30, AlignCenter, AlignCenter);
    popup_set_callback(app->popup, baofeng_popup_callback);
    popup_set_context(app->popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewPopup);
}

bool baofeng_scene_popup_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void baofeng_scene_popup_on_exit(void* context) {
    BaofengApp* app = context;
    popup_reset(app->popup);
}

// --- About Scene ---
void baofeng_scene_about_on_enter(void* context) {
    BaofengApp* app = context;
    widget_reset(app->widget);
    
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, 
        "Baofeng 888 Programmer\n"
        "By Delliaf\n\n"
        "Supported Radios:\n"
        "- Baofeng BF-888S\n"
        "- Retevis H-777\n\n"
        "Ranges:\n"
        "390-399.9: Gray Zone (Lower)\n"
        "400-470.0: Green (Official)\n"
        "470-479.9: Gray Zone (Upper)\n"
        "Outside: May break radio!\n\n"
        "Warning: Use at your own risk!\n"
        "We are not responsible for\n"
        "any damage to your radio.\n\n"
        "Connection:\n"
        "Flipper TX(13) -> Radio RX\n"
        "Flipper RX(14) -> Radio TX\n"
        "Flipper GND(8) -> Radio GND\n\n"
        "Use a 2.5mm and 3.5mm jack\n"
        "to connect to the radio.\n"
        "Warning: 3.3V logic used.\n"
    );
    
    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewWidget);
}

bool baofeng_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void baofeng_scene_about_on_exit(void* context) {
    BaofengApp* app = context;
    widget_reset(app->widget);
}

// --- Wiring Scene ---
static const uint8_t image__19c72ef4_e23c_4f27_9f77_af5629d58baa_0_bits[] = {0x7f,0xc6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x63,0x6e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x63,0x6c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x63,0x38,0xf8,0xff,0xff,0xff,0xff,0xff,0xff,0x7f,0x00,0x00,0x00,0x00,0x00,0x7f,0x38,0xf8,0xff,0xff,0xff,0xff,0xff,0xff,0x7f,0x80,0xff,0x0f,0x00,0x00,0x33,0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x80,0xf8,0xff,0x1f,0x00,0x73,0x6c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x40,0xf0,0xff,0x7f,0x00,0x63,0xe6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0xe0,0xf3,0xff,0xff,0x00,0xe3,0xc6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x90,0xf7,0xff,0xff,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x90,0xf7,0xff,0xff,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0x3f,0xf7,0xff,0xff,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0x18,0x06,0x40,0xf7,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x18,0x06,0x40,0xef,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x18,0x06,0x40,0xef,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x18,0x06,0x40,0xef,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0x18,0x06,0x40,0xef,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0xff,0xff,0x7f,0xe7,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0xe7,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x90,0xe7,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x90,0xe3,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0xe1,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0xe0,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0xe0,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0xe0,0xff,0xff,0x03,0xff,0xc6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0xe0,0xff,0xff,0x03,0xff,0xe6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0xe0,0xff,0xff,0x07,0x18,0x6e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0xe0,0xff,0xff,0x07,0x18,0x7c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0xe0,0xff,0xff,0x07,0x18,0x38,0xf8,0xff,0xff,0xff,0xff,0xff,0xff,0x0f,0x40,0xe0,0xff,0xff,0x07,0x18,0x38,0xf8,0xff,0xff,0xff,0xff,0xff,0xff,0x0f,0x40,0xe0,0xff,0xff,0x07,0x18,0x7c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x40,0xe0,0xff,0xff,0x07,0x18,0x6c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0xe0,0xe7,0xff,0xff,0x07,0x18,0xe6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x20,0xe7,0xff,0xff,0x07,0x18,0xc7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0xe7,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfe,0xff,0x7f,0xee,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x83,0x61,0x80,0xee,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x82,0x61,0x80,0xee,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x82,0x61,0x80,0xee,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x83,0x61,0x80,0xee,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfe,0xff,0x7f,0xee,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0xe7,0xff,0xff,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x26,0xe7,0xff,0xff,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc6,0xe7,0xff,0xff,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x86,0xf0,0xff,0xff,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x86,0xf0,0xff,0xff,0x0f,0x7c,0x1c,0xf3,0x01,0x3e,0x00,0x00,0x00,0x00,0x00,0x06,0xf9,0xff,0xff,0x0f,0xfe,0x1c,0xf3,0x07,0x7e,0x00,0x0c,0x00,0x00,0x00,0x06,0xfe,0xff,0xff,0x07,0xc6,0x3c,0x33,0x06,0xc6,0x38,0x9e,0x07,0x00,0x00,0x06,0xf0,0xff,0xff,0x07,0x03,0x3c,0x33,0x06,0xc6,0x7c,0xde,0x0f,0x00,0x00,0x06,0xe0,0xff,0xff,0x07,0x03,0x6c,0x33,0x0e,0xc6,0xc0,0x0c,0x8c,0xff,0xff,0x07,0xc0,0xff,0xff,0x07,0xf3,0xec,0x33,0x0e,0xc6,0xf8,0x8c,0x8f,0xff,0xff,0x07,0xc0,0xff,0xff,0x07,0xc7,0xcc,0x33,0x06,0xc6,0xcc,0xcc,0x0c,0x00,0x00,0x00,0xc0,0xff,0xff,0x07,0xc6,0x8c,0x33,0x06,0xe6,0xc6,0xcc,0x0c,0x00,0x00,0x00,0x80,0xff,0xff,0x03,0xfe,0x8c,0xf3,0x03,0x7e,0xfc,0xdc,0x0f,0x00,0x00,0x00,0x80,0xff,0xff,0x03,0x78,0x0c,0xf3,0x00,0x1e,0xd8,0x9c,0x0d,0x00,0x00,0x00,0x80,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0x03};

void baofeng_wiring_view_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_set_bitmap_mode(canvas, true);
    canvas_draw_xbm(canvas, 8, 3, 116, 58, image__19c72ef4_e23c_4f27_9f77_af5629d58baa_0_bits);
}

void baofeng_scene_wiring_on_enter(void* context) {
    BaofengApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewWiring);
}

bool baofeng_scene_wiring_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void baofeng_scene_wiring_on_exit(void* context) {
    UNUSED(context);
}

// --- TextInput Scene ---
static void text_input_callback(void* context) {
    BaofengApp* app = context;
    // Parse app->text_store
    // Format expected: 433.92000 (MHz)
    uint32_t freq_10hz = 0;
    char* dot = strchr(app->text_store, '.');
    if (dot) {
        *dot = '\0'; // Split string
        uint32_t mhz = atoi(app->text_store);
        
        uint32_t remainder = 0;
        uint32_t multiplier = 10000;
        char* p = dot + 1;
        for(int i=0; i<5; i++) {
            if (*p >= '0' && *p <= '9') {
                remainder += (*p - '0') * multiplier;
                p++;
            }
            multiplier /= 10;
        }
        freq_10hz = mhz * 100000 + remainder;
    } else {
        // No dot, try parsing as direct 10Hz units if it's very large, or MHz if small
        uint32_t raw = atoi(app->text_store);
        if (raw >= 40000000) {
            freq_10hz = raw; // They entered raw 10Hz units
        } else if (raw >= 400000) {
            freq_10hz = raw * 100; // They entered kHz
        } else {
            freq_10hz = raw * 100000; // They entered MHz without decimals
        }
    }
    
    if (app->is_editing_tx) {
        app->current_tx_freq_10hz = freq_10hz;
    } else {
        app->current_rx_freq_10hz = freq_10hz;
    }
    
    scene_manager_previous_scene(app->scene_manager);

    if (freq_10hz < 39000000 || freq_10hz > 47999500) {
        snprintf(app->popup_text, sizeof(app->popup_text), "\n\nOUT OF BOUNDS!\nRadio WILL NOT WORK.\nWARNING: RADIO MAY BREAK");
        scene_manager_next_scene(app->scene_manager, BaofengScenePopup);
    } else if (freq_10hz < 40000000 || freq_10hz > 47000000) {
        snprintf(app->popup_text, sizeof(app->popup_text), "\n\nGRAY ZONE!\nMay not work.\nAt your own risk.");
        scene_manager_next_scene(app->scene_manager, BaofengScenePopup);
    }
}

void baofeng_scene_text_input_on_enter(void* context) {
    BaofengApp* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Freq (10Hz) e.g. 43392000");
    text_input_set_result_callback(
        app->text_input,
        text_input_callback,
        app,
        app->text_store,
        sizeof(app->text_store),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewTextInput);
}

bool baofeng_scene_text_input_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void baofeng_scene_text_input_on_exit(void* context) {
    BaofengApp* app = context;
    text_input_reset(app->text_input);
}

// --- Save Name Scene ---
static void save_name_callback(void* context) {
    BaofengApp* app = context;
    char filename[64];
    snprintf(filename, sizeof(filename), "%s.img", app->text_store);
    
    bool success = baofeng_save_dump(app, filename);
    snprintf(app->popup_text, sizeof(app->popup_text), success ? "Saved to\n/ext/baofeng/%s" : "Save Failed!", filename);
    
    scene_manager_previous_scene(app->scene_manager); // pop SaveName, back to main menu
    scene_manager_next_scene(app->scene_manager, BaofengScenePopup); // push popup
}

void baofeng_scene_save_name_on_enter(void* context) {
    BaofengApp* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter File Name (no ext)");
    text_input_set_result_callback(
        app->text_input,
        save_name_callback,
        app,
        app->text_store,
        sizeof(app->text_store),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, BaofengViewTextInput);
}

bool baofeng_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void baofeng_scene_save_name_on_exit(void* context) {
    BaofengApp* app = context;
    text_input_reset(app->text_input);
}

// Scene Handlers array
void (*const baofeng_on_enter_handlers[])(void*) = {
    baofeng_scene_main_menu_on_enter,
    baofeng_scene_channel_list_on_enter,
    baofeng_scene_channel_edit_on_enter,
    baofeng_scene_popup_on_enter,
    baofeng_scene_about_on_enter,
    baofeng_scene_wiring_on_enter,
    baofeng_scene_text_input_on_enter,
    baofeng_scene_save_name_on_enter,
    baofeng_scene_settings_on_enter,
};

bool (*const baofeng_on_event_handlers[])(void*, SceneManagerEvent) = {
    baofeng_scene_main_menu_on_event,
    baofeng_scene_channel_list_on_event,
    baofeng_scene_channel_edit_on_event,
    baofeng_scene_popup_on_event,
    baofeng_scene_about_on_event,
    baofeng_scene_wiring_on_event,
    baofeng_scene_text_input_on_event,
    baofeng_scene_save_name_on_event,
    baofeng_scene_settings_on_event,
};

void (*const baofeng_on_exit_handlers[])(void*) = {
    baofeng_scene_main_menu_on_exit,
    baofeng_scene_channel_list_on_exit,
    baofeng_scene_channel_edit_on_exit,
    baofeng_scene_popup_on_exit,
    baofeng_scene_about_on_exit,
    baofeng_scene_wiring_on_exit,
    baofeng_scene_text_input_on_exit,
    baofeng_scene_save_name_on_exit,
    baofeng_scene_settings_on_exit,
};

const SceneManagerHandlers baofeng_scene_handlers = {
    .on_enter_handlers = baofeng_on_enter_handlers,
    .on_event_handlers = baofeng_on_event_handlers,
    .on_exit_handlers = baofeng_on_exit_handlers,
    .scene_num = BaofengSceneCount,
};
