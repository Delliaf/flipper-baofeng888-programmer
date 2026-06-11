#include "baofeng_app.h"
#include "baofeng_ui.h"

static bool baofeng_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BaofengApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool baofeng_back_event_callback(void* context) {
    furi_assert(context);
    BaofengApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static BaofengApp* baofeng_app_alloc() {
    BaofengApp* app = malloc(sizeof(BaofengApp));
    memset(app, 0, sizeof(BaofengApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&baofeng_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, baofeng_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, baofeng_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BaofengViewSubmenu, submenu_get_view(app->submenu));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BaofengViewVariableItemList, variable_item_list_get_view(app->variable_item_list));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BaofengViewPopup, popup_get_view(app->popup));
    
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BaofengViewWidget, widget_get_view(app->widget));

    app->wiring_view = view_alloc();
    view_set_context(app->wiring_view, app);
    view_set_draw_callback(app->wiring_view, baofeng_wiring_view_draw_callback);
    view_dispatcher_add_view(app->view_dispatcher, BaofengViewWiring, app->wiring_view);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BaofengViewTextInput, text_input_get_view(app->text_input));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void baofeng_app_free(BaofengApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, BaofengViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, BaofengViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, BaofengViewPopup);
    popup_free(app->popup);
    
    view_dispatcher_remove_view(app->view_dispatcher, BaofengViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, BaofengViewWiring);
    view_free(app->wiring_view);

    view_dispatcher_remove_view(app->view_dispatcher, BaofengViewTextInput);
    text_input_free(app->text_input);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t baofeng_app_main(void* p) {
    UNUSED(p);
    BaofengApp* app = baofeng_app_alloc();

    scene_manager_next_scene(app->scene_manager, BaofengSceneMainMenu);

    view_dispatcher_run(app->view_dispatcher);

    baofeng_app_free(app);
    return 0;
}
