#include "baofeng_storage.h"

#define BAOFENG_APP_FOLDER ANY_PATH("baofeng")
#define BAOFENG_APP_EXTENSION ".img"

const uint8_t CHIRP_IMG_MAGIC[] = {
    0x00, 0xFF, 'c', 'h', 'i', 'r', 'p', 0xEE, 'i', 'm', 'g', 0x00, 0x01
};

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_idx(char c) {
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= 'a' && c <= 'z') return c - 'a' + 26;
    if(c >= '0' && c <= '9') return c - '0' + 52;
    if(c == '+') return 62;
    if(c == '/') return 63;
    return -1;
}

static void unescape_json(const char* src, size_t src_len, char* dest, size_t max_dest_len) {
    size_t j = 0;
    for(size_t i=0; i<src_len && j < max_dest_len - 1; i++) {
        if(src[i] == '\\' && i + 1 < src_len) {
            if(src[i+1] == 'u' && i + 5 < src_len) {
                uint32_t cp = 0;
                for(int k=0; k<4; k++) {
                    char c = src[i+2+k];
                    cp <<= 4;
                    if(c >= '0' && c <= '9') cp |= c - '0';
                    else if(c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
                    else if(c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
                }
                if(cp <= 0x7F) {
                    dest[j++] = cp;
                } else if(cp <= 0x7FF) {
                    if(j + 2 >= max_dest_len) break;
                    dest[j++] = 0xC0 | (cp >> 6);
                    dest[j++] = 0x80 | (cp & 0x3F);
                } else if(cp <= 0xFFFF) {
                    if(j + 3 >= max_dest_len) break;
                    dest[j++] = 0xE0 | (cp >> 12);
                    dest[j++] = 0x80 | ((cp >> 6) & 0x3F);
                    dest[j++] = 0x80 | (cp & 0x3F);
                }
                i += 5;
            } else {
                if (src[i+1] == '"' || src[i+1] == '\\') {
                    dest[j++] = src[i+1];
                } else {
                    dest[j++] = src[i];
                    dest[j++] = src[i+1];
                }
                i++;
            }
        } else {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

static void json_escape(const char* src, char* dest) {
    while(*src) {
        if(*src == '"' || *src == '\\') {
            *dest++ = '\\';
        }
        *dest++ = *src++;
    }
    *dest = '\0';
}

static void feed_char_to_fsm(char dc, const char* key, size_t key_len, size_t* match, bool* in_value, bool* escaped, char* raw_val, size_t* out_len, size_t max_out, bool* done) {
    if (*done) return;
    if (!*in_value) {
        if (dc == key[*match]) {
            (*match)++;
            if (*match == key_len) *in_value = true;
        } else {
            if (dc == key[0]) *match = 1;
            else *match = 0;
        }
    } else {
        if (*escaped) {
            if(*out_len < max_out - 1) raw_val[(*out_len)++] = '\\';
            if(*out_len < max_out - 1) raw_val[(*out_len)++] = dc;
            *escaped = false;
        } else if (dc == '\\') {
            *escaped = true;
        } else if (dc == '"') {
            raw_val[*out_len] = '\0';
            *done = true;
        } else {
            if(*out_len < max_out - 1) raw_val[(*out_len)++] = dc;
        }
    }
}

bool baofeng_load_dump(BaofengApp* app) {
    FuriString* file_path = furi_string_alloc();
    furi_string_set(file_path, BAOFENG_APP_FOLDER);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, BAOFENG_APP_EXTENSION, NULL);
    browser_options.base_path = BAOFENG_APP_FOLDER;

    bool res = dialog_file_browser_show(
        app->dialogs,
        file_path,
        file_path,
        &browser_options);

    if(res) {
        for(int i=0; i<BAOFENG_CHANNELS_COUNT; i++) memset(app->channel_names[i], 0, 200);
        File* file = storage_file_alloc(app->storage);
        if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t file_size = storage_file_size(file);
            if (file_size >= 992) {
                uint16_t read_count = storage_file_read(file, app->full_eeprom, 992);
                if(read_count == 992) {
                    memcpy(app->channels, &app->full_eeprom[0x0010], sizeof(app->channels));
                    
                    // Stream parse CHIRP JSON if present
                    if (file_size > 992 + sizeof(CHIRP_IMG_MAGIC)) {
                        uint8_t magic[sizeof(CHIRP_IMG_MAGIC)];
                        storage_file_read(file, magic, sizeof(magic));
                        if (memcmp(magic, CHIRP_IMG_MAGIC, sizeof(magic)) == 0) {
                            
                            for(int ch = 0; ch < BAOFENG_CHANNELS_COUNT; ch++) {
                                char key[32];
                                snprintf(key, sizeof(key), "\"%04d_comment\": \"", ch + 1);
                                size_t key_len = strlen(key);
                                size_t match = 0;
                                bool in_value = false;
                                bool escaped = false;
                                bool done = false;
                                size_t out_len = 0;
                                char raw_val[512] = {0};
                                
                                storage_file_seek(file, 992 + sizeof(CHIRP_IMG_MAGIC), true);
                                
                                uint8_t buf[128];
                                uint16_t bytes;
                                int b64_state = 0;
                                uint32_t b64_acc = 0;
                                
                                while(!done && (bytes = storage_file_read(file, buf, sizeof(buf))) > 0) {
                                    for(uint16_t i=0; i<bytes && !done; i++) {
                                        char c = buf[i];
                                        if (c == '\n' || c == '\r' || c == ' ') continue;
                                        if (c == '=') {
                                            if (b64_state == 2) {
                                                b64_acc <<= 12;
                                                feed_char_to_fsm((b64_acc >> 16) & 0xFF, key, key_len, &match, &in_value, &escaped, raw_val, &out_len, sizeof(raw_val), &done);
                                            } else if (b64_state == 3) {
                                                b64_acc <<= 6;
                                                feed_char_to_fsm((b64_acc >> 16) & 0xFF, key, key_len, &match, &in_value, &escaped, raw_val, &out_len, sizeof(raw_val), &done);
                                                feed_char_to_fsm((b64_acc >> 8) & 0xFF, key, key_len, &match, &in_value, &escaped, raw_val, &out_len, sizeof(raw_val), &done);
                                            }
                                            done = true; // stop parsing
                                            break;
                                        }
                                        
                                        int val = b64_idx(c);
                                        if (val == -1) continue;
                                        
                                        b64_acc = (b64_acc << 6) | val;
                                        b64_state++;
                                        
                                        if (b64_state == 4) {
                                            feed_char_to_fsm((b64_acc >> 16) & 0xFF, key, key_len, &match, &in_value, &escaped, raw_val, &out_len, sizeof(raw_val), &done);
                                            feed_char_to_fsm((b64_acc >> 8) & 0xFF, key, key_len, &match, &in_value, &escaped, raw_val, &out_len, sizeof(raw_val), &done);
                                            feed_char_to_fsm(b64_acc & 0xFF, key, key_len, &match, &in_value, &escaped, raw_val, &out_len, sizeof(raw_val), &done);
                                            b64_state = 0;
                                            b64_acc = 0;
                                        }
                                    }
                                }
                                if (out_len > 0) {
                                    unescape_json(raw_val, out_len, app->channel_names[ch], 200);
                                }
                            }
                        }
                    }
                } else {
                    res = false;
                }
            } else {
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

static void write_string_b64(File* file, const char* str, int* b64_state, uint32_t* b64_acc) {
    while(*str) {
        *b64_acc = (*b64_acc << 8) | (uint8_t)(*str++);
        (*b64_state)++;
        if (*b64_state == 3) {
            char out[4];
            out[0] = b64_table[(*b64_acc >> 18) & 0x3F];
            out[1] = b64_table[(*b64_acc >> 12) & 0x3F];
            out[2] = b64_table[(*b64_acc >> 6) & 0x3F];
            out[3] = b64_table[*b64_acc & 0x3F];
            storage_file_write(file, out, 4);
            *b64_state = 0;
            *b64_acc = 0;
        }
    }
}

static void finish_b64(File* file, int b64_state, uint32_t b64_acc) {
    if (b64_state == 1) {
        char out[4];
        b64_acc <<= 16;
        out[0] = b64_table[(b64_acc >> 18) & 0x3F];
        out[1] = b64_table[(b64_acc >> 12) & 0x3F];
        out[2] = '='; out[3] = '=';
        storage_file_write(file, out, 4);
    } else if (b64_state == 2) {
        char out[4];
        b64_acc <<= 8;
        out[0] = b64_table[(b64_acc >> 18) & 0x3F];
        out[1] = b64_table[(b64_acc >> 12) & 0x3F];
        out[2] = b64_table[(b64_acc >> 6) & 0x3F];
        out[3] = '=';
        storage_file_write(file, out, 4);
    }
}

bool baofeng_save_dump(BaofengApp* app, const char* filename) {
    storage_common_mkdir(app->storage, BAOFENG_APP_FOLDER);
    
    FuriString* file_path = furi_string_alloc_printf("%s/%s", BAOFENG_APP_FOLDER, filename);
    
    File* file = storage_file_alloc(app->storage);
    bool res = false;
    
    memcpy(&app->full_eeprom[0x0010], app->channels, sizeof(app->channels));
    
    if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint16_t write_count = storage_file_write(file, app->full_eeprom, 992);
        if(write_count == 992) {
            storage_file_write(file, CHIRP_IMG_MAGIC, sizeof(CHIRP_IMG_MAGIC));
            
            int b64_state = 0;
            uint32_t b64_acc = 0;
            
            write_string_b64(file, "{\"mem_extra\": {", &b64_state, &b64_acc);
            
            bool first_comment = true;
            for(int ch = 0; ch < BAOFENG_CHANNELS_COUNT; ch++) {
                if (strlen(app->channel_names[ch]) > 0) {
                    if (!first_comment) write_string_b64(file, ", ", &b64_state, &b64_acc);
                    
                    char key[32];
                    snprintf(key, sizeof(key), "\"%04d_comment\": \"", ch + 1);
                    write_string_b64(file, key, &b64_state, &b64_acc);
                    
                    char escaped_name[400];
                    json_escape(app->channel_names[ch], escaped_name);
                    write_string_b64(file, escaped_name, &b64_state, &b64_acc);
                    
                    write_string_b64(file, "\"", &b64_state, &b64_acc);
                    first_comment = false;
                }
            }
            write_string_b64(file, "}, \"rclass\": \"H777Radio\", \"vendor\": \"Baofeng\", \"model\": \"BF-888\", \"variant\": \"\", \"chirp_version\": \"flipper-zero\"}", &b64_state, &b64_acc);
            
            finish_b64(file, b64_state, b64_acc);
            res = true;
        }
    }
    
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(file_path);
    
    return res;
}
