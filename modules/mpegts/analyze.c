/*
 * Astra MPEG-TS Analyze Module
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 * Licensed under the MIT license.
 */

/*
 * Module Name:
 *      analyze
 *
 * Module Options:
 *      upstream    - object, stream instance returned by module_instance:stream()
 *      name        - string, analyzer name
 */

#include <astra.h>

struct module_data_t
{
    MODULE_STREAM_DATA();

    const char *name;

    uint16_t tsid;

    mpegts_psi_t *stream[MAX_PID];
};

#define MSG(_msg) "[analyze %s] " _msg, mod->name

/*
 * oooooooooo   o   ooooooooooo
 *  888    888 888  88  888  88
 *  888oooo88 8  88     888
 *  888      8oooo88    888
 * o888o   o88o  o888o o888o
 *
 */

static void on_pat(void *arg, mpegts_psi_t *psi)
{
    module_data_t *mod = arg;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
        return;

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        asc_log_error(MSG("PAT checksum error"));
        return;
    }

    psi->crc32 = crc32;

    mod->tsid = PAT_GET_TSID(psi);

    asc_log_info(MSG("PAT: tsid:%d"), mod->tsid);
    const uint8_t *pointer = PAT_ITEMS_FIRST(psi);
    while(!PAT_ITEMS_EOL(psi, pointer))
    {
        const uint16_t pnr = PAT_ITEMS_GET_PNR(psi, pointer);
        const uint16_t pid = PAT_ITEMS_GET_PID(psi, pointer);
        if(!pnr)
            asc_log_info(MSG("PAT: pid:%4d NIT"), pid);
        else
        {
            asc_log_info(MSG("PAT: pid:%4d PMT pnr:%4d"), pid, pnr);
            mod->stream[pid] = mpegts_psi_init(MPEGTS_PACKET_PMT, pid);
        }

        PAT_ITEMS_NEXT(psi, pointer);
    }

    asc_log_info(MSG("PAT: crc32:0x%08X"), crc32);
}

/*
 *   oooooooo8     o   ooooooooooo
 * o888     88    888  88  888  88
 * 888           8  88     888
 * 888o     oo  8oooo88    888
 *  888oooo88 o88o  o888o o888o
 *
 */

static void on_cat(void *arg, mpegts_psi_t *psi)
{
    module_data_t *mod = arg;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
        return;

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        asc_log_error(MSG("CAT checksum error"));
        return;
    }
    psi->crc32 = crc32;

    char desc_dump[256];
    const uint8_t *desc_pointer = CAT_DESC_FIRST(psi);
    while(!CAT_DESC_EOL(psi, desc_pointer))
    {
        mpegts_desc_to_string(desc_dump, sizeof(desc_dump), desc_pointer);
        asc_log_info(MSG("CAT: %s"), desc_dump);
        CAT_DESC_NEXT(psi, desc_pointer);
    }

    asc_log_info(MSG("CAT: crc32:0x%08X"), crc32);
}

/*
 * oooooooooo oooo     oooo ooooooooooo
 *  888    888 8888o   888  88  888  88
 *  888oooo88  88 888o8 88      888
 *  888        88  888  88      888
 * o888o      o88o  8  o88o    o888o
 *
 */

static void on_pmt(void *arg, mpegts_psi_t *psi)
{
    module_data_t *mod = arg;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
        return;

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        asc_log_error(MSG("PMT checksum error"));
        return;
    }
    psi->crc32 = crc32;

    const uint16_t pnr = PMT_GET_PNR(psi);
    asc_log_info(MSG("PMT: pnr:%d"), pnr);

    char desc_dump[256];
    const uint8_t *desc_pointer = PMT_DESC_FIRST(psi);
    while(!PMT_DESC_EOL(psi, desc_pointer))
    {
        mpegts_desc_to_string(desc_dump, sizeof(desc_dump), desc_pointer);
        asc_log_info(MSG("PMT:     %s"), desc_dump);
        PMT_DESC_NEXT(psi, desc_pointer);
    }

    asc_log_info(MSG("PMT: pid:%4d PCR"), PMT_GET_PCR(psi));

    const uint8_t *pointer = PMT_ITEMS_FIRST(psi);
    while(!PMT_ITEMS_EOL(psi, pointer))
    {
        const uint16_t pid = PMT_ITEM_GET_PID(psi, pointer);
        const uint8_t type = PMT_ITEM_GET_TYPE(psi, pointer);
        asc_log_info(MSG("PMT: pid:%4d %s:0x%02X")
                     , pid, mpegts_type_name(mpegts_pes_type(type)), type);

        const uint8_t *desc_pointer = PMT_ITEM_DESC_FIRST(pointer);
        while(!PMT_ITEM_DESC_EOL(pointer, desc_pointer))
        {
            mpegts_desc_to_string(desc_dump, sizeof(desc_dump), desc_pointer);
            asc_log_info(MSG("PMT:     %s"), desc_dump);
            PMT_ITEM_DESC_NEXT(pointer, desc_pointer);
        }

        PMT_ITEMS_NEXT(psi, pointer);
    }

    asc_log_info(MSG("PMT: crc32:0x%08X"), crc32);
}

/*
 *  oooooooo8 ooooooooo   ooooooooooo
 * 888         888    88o 88  888  88
 *  888oooooo  888    888     888
 *         888 888    888     888
 * o88oooo888 o888ooo88      o888o
 *
 */

static void on_sdt(void *arg, mpegts_psi_t *psi)
{
    module_data_t *mod = arg;

    if(psi->buffer[0] != 0x42)
        return;

    if(mod->tsid != SDT_GET_TSID(psi))
        return;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
        return;

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
    {
        asc_log_error(MSG("SDT checksum error"));
        return;
    }
    psi->crc32 = crc32;

    asc_log_info(MSG("SDT: tsid:%d"), mod->tsid);
    char desc_dump[256];
    const uint8_t *pointer = SDT_ITEMS_FIRST(psi);
    while(!SDT_ITEMS_EOL(psi, pointer))
    {
        const uint16_t sid = SDT_ITEM_GET_SID(psi, pointer);
        asc_log_info(MSG("SDT: service_id:%d"), sid);
        const uint8_t *desc_pointer = SDT_ITEM_DESC_FIRST(pointer);
        while(!SDT_ITEM_DESC_EOL(pointer, desc_pointer))
        {
            mpegts_desc_to_string(desc_dump, sizeof(desc_dump), desc_pointer);
            asc_log_info(MSG("SDT:     %s"), desc_dump);
            SDT_ITEM_DESC_NEXT(pointer, desc_pointer);
        }
        SDT_ITEMS_NEXT(psi, pointer);
    }

    asc_log_info(MSG("SDT: crc32:0x%08X"), crc32);
}

/*
 * ooooooooooo  oooooooo8
 * 88  888  88 888
 *     888      888oooooo
 *     888             888
 *    o888o    o88oooo888
 *
 */

static void on_ts(module_data_t *mod, const uint8_t *ts)
{
    const uint16_t pid = TS_PID(ts);
    mpegts_psi_t *psi = mod->stream[pid];
    if(psi)
    {
        switch(psi->type)
        {
            case MPEGTS_PACKET_PAT:
                mpegts_psi_mux(psi, ts, on_pat, mod);
                break;
            case MPEGTS_PACKET_CAT:
                mpegts_psi_mux(psi, ts, on_cat, mod);
                break;
            case MPEGTS_PACKET_PMT:
                mpegts_psi_mux(psi, ts, on_pmt, mod);
                break;
            case MPEGTS_PACKET_SDT:
                mpegts_psi_mux(psi, ts, on_sdt, mod);
                break;
            default:
                break;
        }
    }
}

/*
 * oooo     oooo  ooooooo  ooooooooo  ooooo  oooo ooooo       ooooooooooo
 *  8888o   888 o888   888o 888    88o 888    88   888         888    88
 *  88 888o8 88 888     888 888    888 888    88   888         888ooo8
 *  88  888  88 888o   o888 888    888 888    88   888      o  888    oo
 * o88o  8  o88o  88ooo88  o888ooo88    888oo88   o888ooooo88 o888ooo8888
 *
 */

static void module_init(module_data_t *mod)
{
    module_option_string("name", &mod->name);
    if(!mod->name)
    {
        asc_log_error("[analyze] option 'name' is required");
        astra_abort();
    }

    module_stream_init(mod, on_ts);

    mod->stream[0x00] = mpegts_psi_init(MPEGTS_PACKET_PAT, 0x00);
    mod->stream[0x01] = mpegts_psi_init(MPEGTS_PACKET_CAT, 0x01);
    mod->stream[0x11] = mpegts_psi_init(MPEGTS_PACKET_SDT, 0x11);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    for(int i = 0; i < MAX_PID; ++i)
    {
        if(mod->stream[i])
            mpegts_psi_destroy(mod->stream[i]);
    }
}

MODULE_STREAM_METHODS()
MODULE_LUA_METHODS()
{
    MODULE_STREAM_METHODS_REF()
};
MODULE_LUA_REGISTER(analyze)
