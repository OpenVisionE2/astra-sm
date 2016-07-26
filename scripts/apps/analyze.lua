-- Astra Applications (TS analyzer)
-- https://cesbo.com/astra/
--
-- Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.

log.set({ color = true })

arg_n = nil

--      o      oooo   oooo     o      ooooo    ooooo  oooo ooooooooooo ooooooooooo
--     888      8888o  88     888      888       888  88   88    888    888    88
--    8  88     88 888o88    8  88     888         888         888      888ooo8
--   8oooo88    88   8888   8oooo88    888      o  888       888    oo  888    oo
-- o88o  o888o o88o    88 o88o  o888o o888ooooo88 o888o    o888oooo888 o888ooo8888

function on_analyze(instance, data)
    if data.error then
        log.error(data.error)
    elseif data.psi then
        if dump_psi_info[data.psi] then
            dump_psi_info[data.psi]("", data)
        else
            log.error("Unknown PSI: " .. data.psi)
        end
    elseif data.analyze then
        local bitrate = 0
        local cc_error = ""
        local pes_error = ""
        local sc_error = ""
        for _,item in pairs(data.analyze) do
            bitrate = bitrate + item.bitrate

            if item.cc_error > 0 then
                cc_error = cc_error .. "PID:" .. tostring(item.pid) .. "=" .. tostring(item.cc_error) .. " "
            end

            if item.pes_error > 0 then
                pes_error = pes_error .. tostring(item.pid) .. "=" .. tostring(item.pes_error) .. " "
            end

            if item.sc_error > 0 then
                sc_error = sc_error .. tostring(item.pid) .. "=" .. tostring(item.sc_error) .. " "
            end
        end
        log.info("Bitrate: " .. tostring(bitrate) .. " Kbit/s")
        if #cc_error > 0 then
            log.error("CC: " .. cc_error)
        end
        if #sc_error > 0 then
            log.error("Scrambled: " .. sc_error)
        else
            if #pes_error > 0 then
                log.error("PES: " .. pes_error)
            end
        end
        if arg_n then
            arg_n = arg_n - 1
            if arg_n == 0 then astra.shutdown() end
        end
    end
end

-- oooo     oooo      o      ooooo oooo   oooo
--  8888o   888      888      888   8888o  88
--  88 888o8 88     8  88     888   88 888o88
--  88  888  88    8oooo88    888   88   8888
-- o88o  8  o88o o88o  o888o o888o o88o    88

function start_analyze(instance, addr)
    local conf = parse_url(addr)
    if not conf then
        log.error("[analyze] wrong url format")
        astra.exit(1)
    end

    conf.name = "analyze"
    if conf.pnr == 0 then conf.pnr = nil end

    if conf.format == "file" then
        local stat, stat_err = utils.stat(conf.filename)
        if not stat or stat.type ~= "file" then
            if stat_err ~= nil then
                log.error("[analyze] " .. stat_err)
            end
            log.error("[analyze] couldn't open requested file")
            astra.exit(1)
        end

        conf.on_error = function()
            astra.exit(1)
        end

    elseif conf.format == "http" then
        conf.on_error = function(code, message)
            astra.exit(1)
        end

    elseif conf.format == "dvb" then
        if conf.pnr == nil then conf.budget = true end

    end

    instance.input = init_input(conf)
    instance.analyze = analyze({
        upstream = instance.input.tail:stream(),
        name = conf.name,
        callback = function(data)
            on_analyze(instance, data)
        end,
    })
end

function stop_analyze(instance)
    kill_input(instance.input)
    instance.analyze = nil
    collectgarbage()
end

options_usage = [[
    -n S                stop analyzer and exit after S seconds
    ADDRESS             source address. Available formats:

UDP:
    Template: udp://[localaddr@]ip[:port]
              localaddr - IP address of the local interface
              port      - default: 1234

    Examples: udp://239.255.2.1
              udp://192.168.1.1@239.255.1.1:1234

RTP:
    Template: rtp://[localaddr@]ip[:port]

File:
    Template: file:///path/to/file.ts

HTTP:
    Template: http://[login:password@]host[:port][/uri]
              login:password
                        - Basic authentication
              host      - Server hostname
              port      - default: 80
              /uri      - resource identifier. default: '/'
]]

input_url = nil

options = {
    ["-n"] = function(idx)
        arg_n = tonumber(argv[idx + 1])
        return 1
    end,
    ["*"] = function(idx)
        input_url = argv[idx]
        return 0
    end,
}

function main()
    log.info("Starting " .. astra.fullname)

    if input_url then
        _G.instance = {}
        start_analyze(_G.instance, input_url)
        return
    end

    astra_usage()
end
