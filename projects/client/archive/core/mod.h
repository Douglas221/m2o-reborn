// =======================================================================//
// !
// ! Generic game events
// !
// =======================================================================//

/* some interfaces */
void module_ped_callback_create();
void module_car_callback_create();
void mod_connect_requested(librg_event_t *);
void mod_connected(librg_event_t *);
void mod_disconnected(librg_event_t *);
void mod_entity_create(librg_event_t *);
void mod_entity_update(librg_event_t *);
void mod_entity_remove(librg_event_t *);
void mod_entity_client_add(librg_event_t *);
void mod_entity_client_update(librg_event_t *);
void mod_entity_client_remove(librg_event_t *);
void mod_entity_interpolate(librg_ctx_t *, librg_entity_t *);
void mod_user_name_set(librg_message_t *);
void mod_on_user_message(librg_message_t *);

CefRefPtr<CefMinimal> cm;

/**
 * Game initialization event
 * World loaded, and we are ready to be telerpoted
 */
void mod_game_init() {
    mod_log("[info] initializing game thread [%x] ...\n", GetCurrentThreadId());

    tools::gamehooks_install_late();
    M2::C_GameGuiModule::Get()->FaderFadeIn(1); // therotically we shouldn't call it here but because it's a sync object it's fine itll work but the local player isn't created just yet.

    // setup callbacks
    librg_event_add(ctx, LIBRG_CONNECTION_REQUEST, mod_connect_requested);
    librg_event_add(ctx, LIBRG_CONNECTION_ACCEPT, mod_connected);
    librg_event_add(ctx, LIBRG_CONNECTION_REFUSE, mod_disconnected);
    librg_event_add(ctx, LIBRG_CONNECTION_DISCONNECT, mod_disconnected);
    librg_event_add(ctx, LIBRG_ENTITY_CREATE, mod_entity_create);
    librg_event_add(ctx, LIBRG_ENTITY_UPDATE, mod_entity_update);
    librg_event_add(ctx, LIBRG_ENTITY_REMOVE, mod_entity_remove);
    librg_event_add(ctx, LIBRG_CLIENT_STREAMER_ADD, mod_entity_client_add);
    librg_event_add(ctx, LIBRG_CLIENT_STREAMER_UPDATE, mod_entity_client_update);
    librg_event_add(ctx, LIBRG_CLIENT_STREAMER_REMOVE, mod_entity_client_remove);

    librg_network_add(ctx, MOD_USER_SET_NAME, mod_user_name_set);
    librg_network_add(ctx, MOD_USER_MESSAGE, mod_on_user_message);

    // call inits for modules
    module_ped_init();
    module_car_init();

    discord_init();

    int x, y;
    graphics_dimensions(&x, &y);

    cm = new CefMinimal();
    cm->init(0, nullptr);
    // cm->navigate("about:blank");
    cm->navigate("https://www.youtube.com/embed/1MGNw-Y2QPk?autoplay=1");

}

void mod_game_stop() {
    discord_free();
    cm->shutdown();
}

static std::vector<M2::C_Entity*> swag;

/**
 * Game tick event
 * takes about ~16 ms per tick
 */
void mod_game_tick() {
    if (cm) {
        cm->update();
    }

    mod.last_delta  = (zpl_time_now() - mod.last_update);
    mod.last_update = zpl_time_now();

    librg_tick(ctx);
    librg_entity_iterate(ctx, (LIBRG_ENTITY_ALIVE | MOD_ENTITY_INTERPOLATED), mod_entity_interpolate);

    discord_update_presence();

    /* show/hide mouse */
    if (GetAsyncKeyState(VK_F1) & 0x1) {
        input_block_set(!input_block_get());
    }

    if (GetAsyncKeyState(VK_F2) & 0x1) {
        cm->clickCenter();
        return;
    }

    if (GetAsyncKeyState(VK_F12) & 0x1) {
        mod_exit("pressed f12");
        return;
    }

    /* connect to the server */
    if (GetAsyncKeyState(VK_F5) & 0x1 && !mod.spawned) {
        librg_network_start(ctx, { 27010, "localhost" });
        mod.spawned = true;
    }

    static M2::C_Entity *ent;
    if (GetAsyncKeyState(VK_F3) & 0x1)
    {
        ent = M2::Wrappers::CreateEntity(M2::eEntityType::MOD_ENTITY_CAR, 10);

        auto pos = reinterpret_cast<M2::C_Human2*>(M2::C_Game::Get()->GetLocalPed())->GetPos();

        ent->SetPosition(pos);

        mod_log("Ped created\n");
    }


    if (GetAsyncKeyState(VK_F4) & 0x1 && mod.spawned && ent) {
        vec3_t dir = reinterpret_cast<M2::C_Human2*>(M2::C_Game::Get()->GetLocalPed())->GetDir();
        M2::S_HumanCommandMoveDir *moveCMD = new M2::S_HumanCommandMoveDir;
        moveCMD->x = dir.x;
        moveCMD->y = dir.y;
        moveCMD->z = dir.z;
        reinterpret_cast<M2::C_Human2*>(ent)->AddCommand(M2::E_Command::COMMAND_MOVEDIR, moveCMD);

        mod_log("Command added\n");
    }

    if (GetAsyncKeyState(VK_F7) & 0x1)
    {
        reinterpret_cast<M2::C_Human2*>(M2::C_Game::Get()->GetLocalPed())->m_pCurrentCar->SetVehicleDirty(100.0);
    }

    if (GetAsyncKeyState(VK_F6) & 0x1) {
        M2::C_Car *car = reinterpret_cast<M2::C_Human2*>(M2::C_Game::Get()->GetLocalPed())->m_pCurrentCar;
        if (!car) {
            mod_log("null ptr\n");
            return;
        }

        float level = *(float*)(car + 0xE8C);
        mod_log("level : %d\n", level);
    }
}

// =======================================================================//
// !
// ! Generic network events
// !
// =======================================================================//

void mod_connect_requested(librg_event_t *event) {
    // TODO: password sending
}

void mod_user_name_set(librg_message_t *msg) {
    auto entity = librg_entity_fetch(msg->ctx, librg_data_ru32(msg->data));
    mod_assert(entity);

    u8 strsize = librg_data_ru8(msg->data);
    auto ped   = get_ped(entity);
    librg_data_rptr(msg->data, ped->name, strsize);

    // client-specific
    zpl_utf8_to_ucs2((u16 *)ped->cached_name, strsize, (const u8 *)ped->name);

    mod_log("set new name for client %u: %s\n", entity->id, ped->name);
}

void mod_request_username_change(u32 entity_id, const char *username) {
    mod_assert(username);
    mod_message_send(ctx, MOD_USER_SET_NAME, [&](librg_data_t *data) {
        librg_data_wu8(data, zpl_strlen(username));
        librg_data_wptr(data, (void *)username, zpl_strlen(username));
    });
}

void mod_on_user_message(librg_message_t *msg) {
    char message_buffer[632];
    u32 strsize = librg_data_ru32(msg->data);
    librg_data_rptr(msg->data, message_buffer, strsize);
    message_buffer[strsize] = '\0';

    for (int i = 0; i < strsize; i++) message_buffer[i] = message_buffer[i] == '%' ? '\045' : message_buffer[i];

    mod_log("[chat] %s\n", message_buffer);
}