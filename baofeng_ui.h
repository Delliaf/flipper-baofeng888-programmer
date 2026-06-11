#pragma once
#include "baofeng_app.h"

void baofeng_scene_main_menu_on_enter(void* context);
bool baofeng_scene_main_menu_on_event(void* context, SceneManagerEvent event);
void baofeng_scene_main_menu_on_exit(void* context);

void baofeng_wiring_view_draw_callback(Canvas* canvas, void* context);
void baofeng_scene_wiring_on_enter(void* context);
bool baofeng_scene_wiring_on_event(void* context, SceneManagerEvent event);
void baofeng_scene_wiring_on_exit(void* context);

void baofeng_scene_channel_list_on_enter(void* context);
bool baofeng_scene_channel_list_on_event(void* context, SceneManagerEvent event);
void baofeng_scene_channel_list_on_exit(void* context);

void baofeng_scene_channel_edit_on_enter(void* context);
bool baofeng_scene_channel_edit_on_event(void* context, SceneManagerEvent event);
void baofeng_scene_channel_edit_on_exit(void* context);

void baofeng_scene_popup_on_enter(void* context);
bool baofeng_scene_popup_on_event(void* context, SceneManagerEvent event);
void baofeng_scene_popup_on_exit(void* context);

extern const SceneManagerHandlers baofeng_scene_handlers;
