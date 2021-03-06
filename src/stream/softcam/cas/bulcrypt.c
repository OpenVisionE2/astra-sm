/*
 * Astra Module: SoftCAM
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2013, Andrey Dyldin <and@cesbo.com>
 *               2012, Georgi Chorbadzhiyski <gf@unixsol.org>
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

#include "../module_cam.h"

struct module_data_t
{
    CAS_MODULE_DATA();
};

static bool cas_check_em(module_data_t *mod, ts_psi_t *em)
{
    static const uint8_t z[] = { 0x00, 0x00, 0x00 };

    const uint8_t em_type = em->buffer[0];
    switch(em_type)
    {
        // ECM
        case 0x80:
        case 0x81:
        {
            return true;
        }
        // EMM
        case 0x82:
        { // unique
            if(!memcmp(&em->buffer[3], z, 3))
            {
                memcpy(&em->buffer[3], &mod->__cas.decrypt->cam->ua[4], 3);
                em->buffer[6] = (em->buffer[6] & 0xF0)| (mod->__cas.decrypt->cam->ua[7] & 0xF0);
                return true;
            }

            return (!memcmp(&em->buffer[3], &mod->__cas.decrypt->cam->ua[4], 3)
                    && ((em->buffer[6] & 0xF0) == (mod->__cas.decrypt->cam->ua[7] & 0xF0)));
        }
        case 0x85:
        { // unique
            if(!memcmp(&em->buffer[3], z, 3))
            {
                memcpy(&em->buffer[3], &mod->__cas.decrypt->cam->ua[4], 3);
                if(em_type == 0x82)
                {
                    em->buffer[6] = (em->buffer[6] & 0xF0)
                                  | (mod->__cas.decrypt->cam->ua[7] & 0xF0);
                }
                return true;
            }

            return (!memcmp(&em->buffer[3], &mod->__cas.decrypt->cam->ua[4], 3));
        }
        case 0x84:
        { // shared
            if(!memcmp(&em->buffer[3], z, 2))
            {
                memcpy(&em->buffer[3], &mod->__cas.decrypt->cam->ua[4], 2);
                return true;
            }

            return (!memcmp(&em->buffer[3], &mod->__cas.decrypt->cam->ua[4], 2));
        }
#if 0
        case 0x8b:
        { // shared
            if(!memcmp(&em->buffer[4], z, 2))
            {
                memcpy(&em->buffer[4], &mod->__cas.decrypt->cam->ua[4], 2);
                return true;
            }

            return (!memcmp(&em->buffer[4], &mod->__cas.decrypt->cam->ua[4], 2));
        }
        case 0x8a:
        { // global
            if(!em->buffer[4])
            {
                em->buffer[4] = mod->__cas.decrypt->cam->ua[4];
                return true;
            }

            return (em->buffer[4] == mod->__cas.decrypt->cam->ua[4]);
        }
#endif
        default:
            break;
    }

    return false;
}

static bool cas_check_keys(module_data_t *mod, const uint8_t *keys)
{
    ASC_UNUSED(mod);
    ASC_UNUSED(keys);
    return true;
}

/*
 * CA descriptor (iso13818-1):
 * tag      :8 (must be 0x09)
 * length   :8
 * caid     :16
 * reserved :3
 * pid      :13
 * data     :length-4
 */

static bool cas_check_descriptor(module_data_t *mod, const uint8_t *desc)
{
    ASC_UNUSED(mod);
    ASC_UNUSED(desc);
    return true;
}

static bool cas_check_caid(uint16_t caid)
{
    return (caid == 0x4AEE || caid == 0x5581);
}

MODULE_CAS(bulcrypt)
