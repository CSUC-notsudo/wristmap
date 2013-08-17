#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "http.h"

// 0fba6c1016ac40939abd8a1731c0d85a
#define MY_UUID { 0x0F, 0xBA, 0x6C, 0x10, 0x16, 0xAC, 0x40, 0x93, 0x9A, 0xBD, 0x8A, 0x17, 0x31, 0xC0, 0xD8, 0x5A }
PBL_APP_INFO(HTTP_UUID,
             "Wristmap", "natevw",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_STANDARD_APP);

enum {
  MAP_KEY_ULAT,
  MAP_KEY_ULON,
  MAP_KEY_ZOOM,
  MAP_KEY_ROW
};


Window window;
BitmapLayer map;
GBitmap img;
uint8_t imgData[3360] = {0};        // 144x168 with rows padded to 32-bit word, 20*168 = 3360 bytes


int32_t ulat, ulon;
uint8_t zoom = 12;
int16_t rowN = 0;

void next_rows() {
    DictionaryIterator* req;
    http_out_get("http://192.168.1.112:8000/row", 0, &req);
    dict_write_int32(req, MAP_KEY_ULAT, ulat);
	dict_write_int32(req, MAP_KEY_ULON, ulon);
    dict_write_int32(req, MAP_KEY_ZOOM, zoom);
    dict_write_int32(req, MAP_KEY_ROW, rowN);
    http_out_send();
}

void rcv_location(float lat, float lon, float alt, float acc, void* ctx) {
    ulat = lat * 1e6;
    ulon = lon * 1e6;
    APP_LOG(APP_LOG_LEVEL_INFO, "Got location %i, %i +/- %i, malt=%i", ulat, ulon, acc, alt*1e3);
    rowN = 0;
    next_rows();
}

void rcv_resp(int32_t tok, int code, DictionaryIterator* res, void* ctx) {
    Tuple* row = dict_find(res, MAP_KEY_ROW);
    if (row) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Received %i bytes for row %i (%i)", row->length, row, code);
        memcpy(imgData+20*rowN, row->value->data, row->length);
        rowN += 1;
        if (rowN <= 168) next_rows();
        else layer_mark_dirty((Layer*)&map.layer);
    }
}

void rcv_fail(int32_t tok, int code, void* ctx) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "HTTP request failure (%i)", code);
}


void handle_init(AppContextRef ctx) {
    //resource_init_current_app(&APP_RESOURCES);
    http_set_app_id(0x0fba6c10);
    http_register_callbacks((HTTPCallbacks){
        .success = rcv_resp,
        .failure = rcv_fail,
        .location = rcv_location,
    }, NULL);
    http_location_request();
    
    window_init(&window, "Window Name");
    window_stack_push(&window, true /* Animated */);
    
    int16_t w = 144;    //window.layer.frame.size.w;
    int16_t h = 168;    //window.layer.frame.size.h;
    img = (GBitmap) {
        .addr = imgData,
        .bounds = GRect(0,0,w,h),
        .info_flags = 1,
        .row_size_bytes = 20,
    };
    bitmap_layer_init(&map, GRect(0,0,w,h));
    bitmap_layer_set_bitmap(&map, &img);
    layer_add_child(&window.layer, (Layer*)&map.layer);
}

void pbl_main(void *params) {
    PebbleAppHandlers handlers = {
        .init_handler = &handle_init,
        .messaging_info = {
            .buffer_sizes = {
                .inbound = 124,
                .outbound = 636,
            },
        },
    };
    app_event_loop(params, &handlers);
}
