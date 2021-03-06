/*
 * Astra Module: BISS Encrypt
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Module Name:
 *      biss_encrypt
 *
 * Module Role:
 *      Output stage, no demux
 */

#include <astra/astra.h>
#include <astra/utils/strhex.h>
#include <astra/luaapi/stream.h>
#include <astra/mpegts/psi.h>

#include <dvbcsa/dvbcsa.h>

struct module_data_t
{
    STREAM_MODULE_DATA();

    ts_type_t stream[TS_MAX_PIDS];

    ts_psi_t *pat;
    ts_psi_t *pmt;

    struct dvbcsa_bs_key_s *key;

    size_t storage_size;
    size_t storage_skip;

    int batch_skip;
    uint8_t *batch_storage_recv;
    uint8_t *batch_storage_send;
    struct dvbcsa_bs_batch_s *batch;
};

static void process_ts(module_data_t *mod, const uint8_t *ts, uint8_t hdr_size)
{
    uint8_t *dst = &mod->batch_storage_recv[mod->storage_skip];
    memcpy(dst, ts, TS_PACKET_SIZE);

    if(hdr_size)
    {
        TS_SET_SC(dst, TS_SC_EVEN);
        mod->batch[mod->batch_skip].data = &dst[hdr_size];
        mod->batch[mod->batch_skip].len = TS_PACKET_SIZE - hdr_size;
        ++mod->batch_skip;
    }

    if(mod->batch_storage_send)
        module_stream_send(mod, &mod->batch_storage_send[mod->storage_skip]);

    mod->storage_skip += TS_PACKET_SIZE;

    if(mod->storage_skip >= mod->storage_size)
    {
        mod->batch[mod->batch_skip].data = NULL;
        dvbcsa_bs_encrypt(mod->key, mod->batch, 184);
        uint8_t *storage_tmp = mod->batch_storage_send;
        mod->batch_storage_send = mod->batch_storage_recv;
        if(!storage_tmp)
            storage_tmp = ASC_ALLOC(mod->storage_size, uint8_t);
        mod->batch_storage_recv = storage_tmp;
        mod->batch_skip = 0;
        mod->storage_skip = 0;
    }
}

static void on_pat(void *arg, ts_psi_t *psi)
{
    module_data_t *mod = (module_data_t *)arg;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
        return;

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
        return; // PAT checksum error

    psi->crc32 = crc32;

    memset(mod->stream, 0, sizeof(mod->stream));
    mod->stream[0] = TS_TYPE_PAT;
    mod->pmt->crc32 = 0;

    const uint8_t *pointer = PAT_ITEMS_FIRST(psi);
    while(!PAT_ITEMS_EOL(psi, pointer))
    {
        const uint16_t pnr = PAT_ITEM_GET_PNR(psi, pointer);
        const uint16_t pid = PAT_ITEM_GET_PID(psi, pointer);
        mod->stream[pid] = (pnr) ? TS_TYPE_PMT : TS_TYPE_NIT;
        PAT_ITEMS_NEXT(psi, pointer);
    }
}

static void on_pmt(void *arg, ts_psi_t *psi)
{
    module_data_t *mod = (module_data_t *)arg;

    if(psi->buffer[0] != 0x02)
        return;

    // check changes
    const uint32_t crc32 = PSI_GET_CRC32(psi);
    if(crc32 == psi->crc32)
        return;

    // check crc
    if(crc32 != PSI_CALC_CRC32(psi))
        return; // PAT checksum error

    psi->crc32 = crc32;

    const uint8_t *pointer = PMT_ITEMS_FIRST(psi);
    while(!PMT_ITEMS_EOL(psi, pointer))
    {
        const uint16_t pid = PMT_ITEM_GET_PID(psi, pointer);
        mod->stream[pid] = TS_TYPE_PES;
        PMT_ITEMS_NEXT(psi, pointer);
    }
}

static void on_ts(module_data_t *mod, const uint8_t *ts)
{
    const uint16_t pid = TS_GET_PID(ts);
    switch(mod->stream[pid])
    {
        case TS_TYPE_PES:
            break;
        case TS_TYPE_PAT:
            ts_psi_mux(mod->pat, ts, on_pat, mod);
            process_ts(mod, ts, 0);
            return;
        case TS_TYPE_PMT:
            ts_psi_mux(mod->pmt, ts, on_pmt, mod);
            process_ts(mod, ts, 0);
            return;
        default:
            process_ts(mod, ts, 0);
            return;
    }

    const uint8_t *payload = TS_GET_PAYLOAD(ts);
    process_ts(mod, ts, (payload != NULL) ? (payload - ts) : (0));
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_stream_init(L, mod, on_ts);
    module_demux_set(mod, NULL, NULL);

    size_t biss_length = 0;
    const char *key_value = NULL;
    module_option_string(L, "key", &key_value, &biss_length);
    if(key_value == NULL)
        luaL_error(L, "[biss_encrypt] option 'key' is required");

    if(biss_length != 16)
        luaL_error(L, "[biss_encrypt] key must be 16 char length");

    uint8_t key[8];
    au_str2hex(key_value, key, 16);
    key[3] = (key[0] + key[1] + key[2]) & 0xFF;
    key[7] = (key[4] + key[5] + key[6]) & 0xFF;

    const int batch_size = dvbcsa_bs_batch_size();
    mod->batch = ASC_ALLOC(batch_size + 1, struct dvbcsa_bs_batch_s);
    mod->storage_size = batch_size * TS_PACKET_SIZE;
    mod->batch_storage_recv = ASC_ALLOC(mod->storage_size, uint8_t);

    mod->key = dvbcsa_bs_key_alloc();
    dvbcsa_bs_key_set(key, mod->key);

    mod->stream[0x00] = TS_TYPE_PAT;
    mod->pat = ts_psi_init(TS_TYPE_PAT, 0);
    mod->pmt = ts_psi_init(TS_TYPE_PMT, 0);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    ASC_FREE(mod->batch, free);
    ASC_FREE(mod->batch_storage_recv, free);
    ASC_FREE(mod->key, dvbcsa_bs_key_free);

    ASC_FREE(mod->pat, ts_psi_destroy);
    ASC_FREE(mod->pmt, ts_psi_destroy);
}

STREAM_MODULE_REGISTER(biss_encrypt)
{
    .init = module_init,
    .destroy = module_destroy,
};
