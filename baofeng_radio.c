#include "baofeng_radio.h"

// Frequency Conversion: Little-endian BCD to 10Hz units
// E.g. 462.12500 MHz = 46212500 (in 10Hz) -> 0x00 0x25 0x21 0x46
uint32_t bcd_to_freq_10hz(const uint8_t bcd[4]) {
    uint32_t freq = 0;
    uint32_t multiplier = 1;
    for(int i = 0; i < 4; i++) {
        uint8_t low_nibble = bcd[i] & 0x0F;
        uint8_t high_nibble = (bcd[i] >> 4) & 0x0F;
        
        freq += low_nibble * multiplier;
        multiplier *= 10;
        freq += high_nibble * multiplier;
        multiplier *= 10;
    }
    return freq;
}

void freq_10hz_to_bcd(uint32_t freq, uint8_t bcd[4]) {
    for(int i = 0; i < 4; i++) {
        uint8_t low_nibble = freq % 10;
        freq /= 10;
        uint8_t high_nibble = freq % 10;
        freq /= 10;
        bcd[i] = (high_nibble << 4) | low_nibble;
    }
}

ToneType decode_tone(const uint8_t tone[2], uint16_t* value) {
    if (tone[0] == 0xFF && tone[1] == 0xFF) {
        *value = 0;
        return ToneTypeNone;
    }
    
    uint8_t t1 = tone[1] & 0x3F; // mask out DCS and REV flags
    uint8_t t0 = tone[0];
    *value = (t1 >> 4) * 1000 + (t1 & 0x0F) * 100 + (t0 >> 4) * 10 + (t0 & 0x0F);
    
    if ((tone[1] & 0x80) == 0) {
        return ToneTypeCTCSS;
    } else {
        return ToneTypeDCS;
    }
}

void encode_tone(ToneType type, uint16_t value, uint8_t tone[2]) {
    if (type == ToneTypeNone) {
        tone[0] = 0xFF;
        tone[1] = 0xFF;
    } else {
        tone[1] = ((value / 1000) << 4) | ((value / 100) % 10);
        tone[0] = (((value / 10) % 10) << 4) | (value % 10);
        
        if (type == ToneTypeDCS) {
            tone[1] |= 0x80; // Set DCS bit
        }
    }
}

static FuriStreamBuffer* rx_stream = NULL;

static FuriHalSerialHandle* serial_handle = NULL;

static void uart_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    UNUSED(ctx);
    if(event & FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(rx_stream, &data, 1, 0);
    }
}

static bool wait_uart_rx(uint8_t* byte, uint32_t timeout_ms) {
    size_t received = furi_stream_buffer_receive(rx_stream, byte, 1, timeout_ms);
    return received == 1;
}

static void uart_tx(const uint8_t* data, size_t len) {
    furi_hal_serial_tx(serial_handle, (uint8_t*)data, len);
}

static bool radio_handshake() {
    uint8_t byte;
    uint8_t cmd_prog[] = {'P', 'R', 'O', 'G', 'R', 'A', 'M'};
    uint8_t cmd_02[] = {0x02};
    uint8_t cmd_06[] = {0x06};

    uart_tx(cmd_02, 1);
    furi_delay_ms(100);
    uart_tx(cmd_prog, 7);
    
    if (!wait_uart_rx(&byte, 500) || byte != 0x06) return false;
    
    uart_tx(cmd_02, 1);
    
    for (int i=0; i<8; i++) {
        if (!wait_uart_rx(&byte, 500)) return false;
    }
    
    uart_tx(cmd_06, 1);
    if (!wait_uart_rx(&byte, 500) || byte != 0x06) return false;
    
    return true;
}

bool baofeng_radio_read(BaofengApp* app) {
    if(app->debug_mode) {
        furi_delay_ms(500);
        return true;
    }

    if(!rx_stream) rx_stream = furi_stream_buffer_alloc(2048, 1);
    furi_stream_buffer_reset(rx_stream);
    
    serial_handle = furi_hal_serial_control_acquire(BAOFENG_UART_CH);
    furi_hal_serial_init(serial_handle, BAOFENG_BAUDRATE);
    furi_hal_serial_async_rx_start(serial_handle, uart_irq_cb, NULL, false);
    furi_delay_ms(50);
    
    if (!radio_handshake()) {
        furi_hal_serial_async_rx_stop(serial_handle);
        furi_hal_serial_deinit(serial_handle);
        furi_hal_serial_control_release(serial_handle);
        return false;
    }
    
    uint8_t* eeprom = app->full_eeprom;
    
    // Read 0x0000 to 0x03E0 (992 bytes)
    for (uint16_t addr = 0x0000; addr < 0x03E0; addr += 8) {
        uint8_t cmd[4] = {'R', (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), 0x08};
        uart_tx(cmd, 4);
        
        uint8_t byte;
        if (!wait_uart_rx(&byte, 500) || byte != 'W') goto error;
        
        // Skip Address (2 bytes) and Size (1 byte)
        for (int i=0; i<3; i++) {
            if (!wait_uart_rx(&byte, 500)) goto error;
        }
        
        for (int i=0; i<8; i++) {
            if (!wait_uart_rx(&byte, 500)) goto error;
            eeprom[addr + i] = byte;
        }
        
        uint8_t ack = 0x06;
        uart_tx(&ack, 1);
        if (!wait_uart_rx(&byte, 500) || byte != 0x06) goto error;
    }
    
    // Copy to channels for UI
    memcpy(app->channels, &app->full_eeprom[0x0010], sizeof(app->channels));
    
    uint8_t exit_cmd = 'E';
    uart_tx(&exit_cmd, 1);
    furi_delay_ms(50);
    
    furi_hal_serial_async_rx_stop(serial_handle);
    furi_hal_serial_deinit(serial_handle);
    furi_hal_serial_control_release(serial_handle);
    return true;
    
error:
    exit_cmd = 'E';
    uart_tx(&exit_cmd, 1);
    furi_delay_ms(50);
    
    furi_hal_serial_async_rx_stop(serial_handle);
    furi_hal_serial_deinit(serial_handle);
    furi_hal_serial_control_release(serial_handle);
    return false;
}

bool baofeng_radio_write(BaofengApp* app) {
    if(app->debug_mode) {
        furi_delay_ms(500);
        return true;
    }

    if(!rx_stream) rx_stream = furi_stream_buffer_alloc(2048, 1);
    furi_stream_buffer_reset(rx_stream);
    
    serial_handle = furi_hal_serial_control_acquire(BAOFENG_UART_CH);
    furi_hal_serial_init(serial_handle, BAOFENG_BAUDRATE);
    furi_hal_serial_async_rx_start(serial_handle, uart_irq_cb, NULL, false);
    furi_delay_ms(50);
    
    if (!radio_handshake()) {
        furi_hal_serial_async_rx_stop(serial_handle);
        furi_hal_serial_deinit(serial_handle);
        furi_hal_serial_control_release(serial_handle);
        return false;
    }
    
    uint8_t* eeprom = app->full_eeprom;
    
    // Sync channels from UI back to full_eeprom
    memcpy(&app->full_eeprom[0x0010], app->channels, sizeof(app->channels));
    
    // Write 0x0000 to 0x03E0 (992 bytes)
    for (uint16_t addr = 0x0000; addr < 0x03E0; addr += 8) {
        uint8_t cmd[4] = {'W', (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), 0x08};
        uart_tx(cmd, 4);
        uart_tx(&eeprom[addr], 8);
        
        uint8_t byte;
        if (!wait_uart_rx(&byte, 500) || byte != 0x06) {
            uint8_t exit_cmd = 'E';
            uart_tx(&exit_cmd, 1);
            furi_delay_ms(50);
            
            furi_hal_serial_async_rx_stop(serial_handle);
            furi_hal_serial_deinit(serial_handle);
            furi_hal_serial_control_release(serial_handle);
            return false;
        }
    }
    
    uint8_t exit_cmd = 'E';
    uart_tx(&exit_cmd, 1);
    furi_delay_ms(50);
    
    furi_hal_serial_async_rx_stop(serial_handle);
    furi_hal_serial_deinit(serial_handle);
    furi_hal_serial_control_release(serial_handle);
    return true;
}
