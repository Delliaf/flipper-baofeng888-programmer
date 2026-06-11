#include "baofeng_storage.h"

#define BAOFENG_APP_FOLDER ANY_PATH("baofeng")
#define BAOFENG_APP_EXTENSION ".img"

const uint8_t CHIRP_IMG_MAGIC[] = {
    0x00, 0xFF, 'c', 'h', 'i', 'r', 'p', 0xEE, 'i', 'm', 'g', 0x00, 0x01
};
const char* CHIRP_IMG_METADATA = "eyJtZW1fZXh0cmEiOiB7fSwgInJjbGFzcyI6ICJINzc3UmFkaW8iLCAidmVuZG9yIjogIkJhb2ZlbmciLCAibW9kZWwiOiAiQkYtODg4IiwgInZhcmlhbnQiOiAiIiwgImNoaXJwX3ZlcnNpb24iOiAiZmxpcHBlci16ZXJvIn0=";

bool baofeng_load_dump(BaofengApp* app) {
    FuriString* file_path = furi_string_alloc();
    furi_string_set(file_path, BAOFENG_APP_FOLDER);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, "*", NULL);
    browser_options.base_path = BAOFENG_APP_FOLDER;

    bool res = dialog_file_browser_show(
        app->dialogs,
        file_path,
        file_path,
        &browser_options);

    if(res) {
        File* file = storage_file_alloc(app->storage);
        if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t file_size = storage_file_size(file);
            if (file_size >= 992) {
                // Full EEPROM dump
                uint16_t read_count = storage_file_read(file, app->full_eeprom, 992);
                if(read_count == 992) {
                    memcpy(app->channels, &app->full_eeprom[0x0010], sizeof(app->channels));
                } else {
                    res = false;
                }
            } else {
                // Old 256 byte dump
                uint16_t read_count = storage_file_read(file, app->channels, sizeof(app->channels));
                if(read_count == sizeof(app->channels)) {
                    memcpy(&app->full_eeprom[0x0010], app->channels, sizeof(app->channels));
                } else {
                    res = false;
                }
            }
        } else {
            res = false;
        }
        storage_file_close(file);
        storage_file_free(file);
    }
    furi_string_free(file_path);
    return res;
}

bool baofeng_save_dump(BaofengApp* app, const char* filename) {
    storage_common_mkdir(app->storage, BAOFENG_APP_FOLDER);
    
    FuriString* file_path = furi_string_alloc_printf("%s/%s", BAOFENG_APP_FOLDER, filename);
    
    File* file = storage_file_alloc(app->storage);
    bool res = false;
    
    // Sync channels to full_eeprom before saving
    memcpy(&app->full_eeprom[0x0010], app->channels, sizeof(app->channels));
    
    if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint16_t write_count = storage_file_write(file, app->full_eeprom, 992);
        if(write_count == 992) {
            // Write CHIRP tail
            uint16_t magic_count = storage_file_write(file, CHIRP_IMG_MAGIC, sizeof(CHIRP_IMG_MAGIC));
            uint16_t meta_count = storage_file_write(file, CHIRP_IMG_METADATA, strlen(CHIRP_IMG_METADATA));
            if (magic_count == sizeof(CHIRP_IMG_MAGIC) && meta_count == strlen(CHIRP_IMG_METADATA)) {
                res = true;
            }
        }
    }
    
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(file_path);
    
    return res;
}
