#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/modules/text_input.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#define BAOFENG_CHANNELS_COUNT 16
#define BAOFENG_UART_CH FuriHalSerialIdUsart
#define BAOFENG_BAUDRATE 9600

#pragma pack(push, 1)
typedef struct {
    uint8_t rxfreq[4];  // Little-endian BCD
    uint8_t txfreq[4];  // Little-endian BCD
    uint8_t rxtone[2];  // Subtone logic
    uint8_t txtone[2];  // Subtone logic
    uint8_t flags;      // Bit flags (tx power, narrow, etc.)
    uint8_t padding[3]; // Unused bytes
} BF888S_Channel;
#pragma pack(pop)

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;
    Storage* storage;

    Submenu* submenu;
    VariableItemList* variable_item_list;
    Popup* popup;
    Widget* widget;
    View* wiring_view;
    TextInput* text_input;

    uint8_t full_eeprom[992];
    BF888S_Channel channels[BAOFENG_CHANNELS_COUNT];
    
    uint8_t current_edit_channel;
    char popup_text[64];
    
    // UI state
    uint32_t current_rx_freq_10hz;
    uint32_t current_tx_freq_10hz;
    
    char channel_names[BAOFENG_CHANNELS_COUNT][16];
    char text_store[16]; // For text input
    bool is_editing_tx;
    
    char item_text_rx[32];
    char item_text_tx[32];
    char item_text_rx_tone[32];
    char item_text_tx_tone[32];
    
    VariableItem* item_rx_freq;
    VariableItem* item_tx_freq;
    VariableItem* item_rx_pol;
    VariableItem* item_tx_pol;
    
    bool debug_mode;
    uint8_t about_ok_clicks;
} BaofengApp;

typedef enum {
    BaofengViewSubmenu,
    BaofengViewVariableItemList,
    BaofengViewPopup,
    BaofengViewWidget,
    BaofengViewWiring,
    BaofengViewTextInput,
} BaofengView;

typedef enum {
    BaofengSceneMainMenu,
    BaofengSceneChannelList,
    BaofengSceneChannelEdit,
    BaofengScenePopup,
    BaofengSceneAbout,
    BaofengSceneWiring,
    BaofengSceneTextInput,
    BaofengSceneSaveName,
    BaofengSceneSettings,
    BaofengSceneCount,
} BaofengScene;

// Custom Event for custom view transitions
enum {
    BaofengCustomEventMenuRead,
    BaofengCustomEventMenuEdit,
    BaofengCustomEventMenuWrite,
    BaofengCustomEventMenuLoad,
    BaofengCustomEventMenuSave,
    BaofengCustomEventMenuWiring,
    BaofengCustomEventMenuSettings,
    BaofengCustomEventMenuAbout,
    BaofengCustomEventChannelSelected,
    BaofengCustomEventSaveChannel,
};
