-- Geographic teleportation and GeoIP spawning for Earth mapgens.
-- Do a GeoIP lookup for a player when they join and send the result to that player.
-- Configurable via core.conf:
--   geoip_on_join.enable = true               (default true) -- enable automatic lookup on join
--   geoip_on_join.api_url = <url_with_%s>     (default ip-api.com URL)
--   geoip_on_join.api_key = <key>             (optional; if provider needs a key; use %k in api_url to substitute)
--   geoip_on_join.cache_ttl = <seconds>       (default 3600)
-- Example default api_url: http://ip-api.com/json/%s?fields=status,message,country,regionName,city,lat,lon,timezone,isp,query
local http = core.request_http_api()
if not http then
    core.log("warning", "[geoip_on_join] HTTP API not available; geo lookups disabled")
end

local function strip_port(ip)
    if not ip then
        return ""
    end
    -- IPv6 in brackets: "[::1]:12345" -> "::1"
    local v6 = ip:match("^%[(.-)%]")
    if v6 then
        return v6
    end
    -- IPv4 or plain host with optional :port -> "1.2.3.4:1234" -> "1.2.3.4"
    return ip:match("^(.-):%d+$") or ip
end

local function is_private_ip(ip)
    if not ip or ip == "" then
        return false
    end
    -- IPv4 private/reserved blocks
    if ip:match("^127%.") or ip:match("^10%.") or ip:match("^192%.168%.") or ip:match("^172%.(1[6-9]|2[0-9]|3[0-1])%.") then
        return true
    end
    -- IPv6 special cases
    if ip == "::1" then
        return true
    end
    local lc = ip:lower()
    if lc:match("^fc") or lc:match("^fd") or lc:match("^fe80") then
        return true
    end
    return false
end

local function get_api_url_for_ip(ip)
    local base = core.settings:get("geoip.api_ip_url") or
                     "http://ip-api.com/json/%s?fields=status,message,country,regionName,city,lat,lon,timezone,isp,query"
    local key = core.settings:get("geoip.api_key") or ""
    if key ~= "" then
        if base:find("%%k") then
            base = base:gsub("%%k", key)
        else
            if base:find("%?") then
                base = base .. "&key=" .. key
            else
                base = base .. "?key=" .. key
            end
        end
    end
    return base:format(ip)
end

local function urlencode(str)
    if str then
        str = string.gsub(str, "\n", "\r\n")
        str = string.gsub(str, "([^%w%-_%.~])", function(c)
            return string.format("%%%02X", string.byte(c))
        end)
    end
    return str
end

local EQUATOR_LEN = 40075696.0
local center = {
    X = 0,
    Z = 0,
}
local scale = {
    X = 1,
    Z = 1,
}
function pos_to_ll(x, z)
    local lon = (x * scale.X) / (EQUATOR_LEN / 360) + center.X
    local lat = (z * scale.Z) / (EQUATOR_LEN / 360) + center.Z
    if lat < 90 and lat > -90 and lon < 180 and lon > -180 then
        return {
            lat = lat,
            lon = lon,
        } -- return as a table with named fields
    else
        return {
            lat = 89.9999,
            lon = 0,
        }
    end
end

function ll_to_pos(l)
    local deg2m = EQUATOR_LEN / 360
    local x = math.floor((l.lon / scale.X - center.X) * deg2m)
    local z = math.floor((l.lat / scale.Z - center.Z) * deg2m)
    return {
        x = x,
        z = z,
    }
end

-- simple cache: ip -> { data = table or nil, fetched = time() }
local cache = {}
local cache_ttl = tonumber(core.settings:get("geoip.cache_ttl")) or 365 * 24 * 3600

local function cache_get(ip)
    local e = cache[ip]
    if not e then
        return nil
    end
    if os.time() - (e.fetched or 0) > cache_ttl then
        cache[ip] = nil
        return nil
    end
    return e.data
end

local function cache_set(ip, data)
    cache[ip] = {
        data = data,
        fetched = os.time(),
    }
end

local mg_earth = core.settings:get("mg_earth")
local mg_earth_ok, mg_earth_data = pcall(core.parse_json, mg_earth)

local smooth_move_serial = 0
local smooth_move_active = {}

-- Smoothly move a player to a target position using a parabolic trajectory.
-- @param player   Player object to move.
-- @param target   Table with x, y, z fields indicating the destination position.
-- @param max_h    Maximum extra arc height (in nodes), also capped by horizontal distance.
-- @param duration Total time (in seconds) for the movement.
local function smooth_move_player(player, target, max_h, duration)
    if not player or not target then
        return
    end
    local start = player:get_pos()
    if not start then
        return
    end

    target = {
        x = target.x,
        y = target.y,
        z = target.z,
    }
    duration = math.max(0.001, tonumber(duration) or 0.001)
    max_h = math.max(0, tonumber(max_h) or 0)

    local move_key = player:get_player_name()
    smooth_move_serial = smooth_move_serial + 1
    smooth_move_active[move_key] = smooth_move_serial
    local move_id = smooth_move_serial

    -- Keep the step interval short enough to look smooth, and divide the
    -- requested duration evenly so the last step lands exactly on time.
    local step_interval = 0.05
    local steps = math.max(1, math.ceil(duration / step_interval))
    local actual_interval = duration / steps

    local dx = target.x - start.x
    local dz = target.z - start.z
    local horizontal_distance = math.sqrt(dx * dx + dz * dz)

    -- Limit the arc height so long jumps rise visibly without becoming a
    -- near-vertical launch. For straight vertical moves, do not add an arc.
    local limited_max_h = 0
    if horizontal_distance > 0 then
        limited_max_h = math.min(max_h, horizontal_distance * 0.06)
    end

    local function lerp(a, b, t)
        return a + (b - a) * t
    end

    local function smooth_progress(t)
        return t * t * t * (10 + t * (-15 + 6 * t))
    end

    local function arc_y(t)
        return lerp(start.y, target.y, t) + 4 * limited_max_h * t * (1 - t)
    end

    local function path_pos(t)
        return {
            x = lerp(start.x, target.x, t),
            y = arc_y(t),
            z = lerp(start.z, target.z, t),
        }
    end

    local function move_step(i)
        if smooth_move_active[move_key] ~= move_id then
            return
        end

        if not player:get_pos() then
            smooth_move_active[move_key] = nil
            return
        end

        if i >= steps then
            player:set_pos(target)
            player:set_velocity({
                x = 0,
                y = 0,
                z = 0,
            })
            smooth_move_active[move_key] = nil
            return
        end

        local t = smooth_progress(i / steps)
        local next_t = smooth_progress((i + 1) / steps)
        local new_pos = path_pos(t)
        local next_pos = path_pos(next_t)
        player:set_pos(new_pos)
        player:set_velocity({
            x = (next_pos.x - new_pos.x) / actual_interval * 0.25,
            y = (next_pos.y - new_pos.y) / actual_interval * 0.25,
            z = (next_pos.z - new_pos.z) / actual_interval * 0.25,
        })

        minetest.after(actual_interval, move_step, i + 1)
    end

    move_step(0)
end

local function simple_deepcopy(tbl)
    if type(tbl) ~= 'table' then
        return tbl
    end
    local res = {}
    for k, v in pairs(tbl) do
        res[simple_deepcopy(k)] = simple_deepcopy(v)
    end
    return res
end
local function move_player_to_geo(player, data, smooth)
    if data.lat and data.lon then
        -- Make a copy of the data to avoid modifying cached data in-place
        local geo_data = simple_deepcopy(data)

        local center_y = 0
        if mg_earth_ok and mg_earth_data and mg_earth_data.center then
            geo_data.lon = geo_data.lon - mg_earth_data.center.x
            geo_data.lat = geo_data.lat - mg_earth_data.center.z
            center_y = mg_earth_data.center.y
        end

        local pos = ll_to_pos(geo_data)

        pos.y = core.get_spawn_level(pos.x, pos.z) - center_y
        local message = "Earth: Moving to " .. (data.display_name or "") .. (data.country or "") .. " " ..
                            (data.city or "") .. " : " .. pos.x .. "," .. pos.y .. "," .. pos.z
        print(message)
        core.chat_send_player(player:get_player_name(), message)

        local proto_ver = core.get_player_information(player:get_player_name()).protocol_version
        if proto_ver < 140 then
            core.chat_send_player(player:get_player_name(),
                "Your client does not support 32bit worlds, Use freeminer.org")
            return false
        end

        if smooth then
            smooth_move_player(player, pos, 100000, 5)
        else
            player:set_pos(pos)
        end

        return true
    end
    return false
end

local function do_geo_lookup_for_player(player, instant)
    if not http then
        print("[geoip] HTTP API not available on server; cannot perform GeoIP lookup.")
        return
    end

    local pname = player:get_player_name()
    local raw_ip = (core.get_player_ip and core.get_player_ip(pname)) or ""
    local ip = raw_ip

    if ip == "" then
        -- print(pname, "[geoip] Could not retrieve your IP from the server.")
        return
    end

    if is_private_ip(ip) then
        return
    end

    -- Check cache
    local cached = cache_get(ip)
    if cached then
        return move_player_to_geo(player, cached, not instant)
    end

    local url = get_api_url_for_ip(ip)
    http.fetch({
        url = url,
        timeout = 8,
    }, function(result)
        if not result or not result.succeeded then
            local err = (result and result.error) and result.error or "unknown error"
            core.chat_send_player(pname, "[geoip] GeoIP request failed: " .. tostring(err))
            print("geo fail", tostring(err))
            return
        end
        print("Player geo: ", ip, result.data)
        local ok, data = pcall(core.parse_json, result.data)
        if not ok or not data then
            -- core.chat_send_player(pname, "[geoip] Failed to parse GeoIP response.")
            print("geo fail", data)
            return
        end

        -- Store in cache (even errors are stored to avoid hammering API; store raw response table)
        cache_set(ip, data)

        return move_player_to_geo(player, data, not instant)
    end)
    return true
end

local function do_geo_lookup_for_player_instant(player)
    return do_geo_lookup_for_player(player, 1)
end

-- Predefined cities map (add or change as needed)
local cities = {
    berlin = {
        lat = 52.5200,
        lon = 13.4050,
    },
    london = {
        lat = 51.5074,
        lon = -0.1278,
    },
    paris = {
        lat = 48.8566,
        lon = 2.3522,
    },
    berlin = {
        lat = 52.5200,
        lon = 13.4050,
    },
    new_york = {
        lat = 40.7128,
        lon = -74.0060,
    },
    tokyo = {
        lat = 35.6895,
        lon = 139.6917,
    },
    sydney = {
        lat = -33.8688,
        lon = 151.2093,
    },
    rome = {
        lat = 41.9028,
        lon = 12.4964,
    },
    moscow = {
        lat = 55.7558,
        lon = 37.6176,
    },
    beijing = {
        lat = 39.9042,
        lon = 116.4074,
    },

    -- North America (USA & Canada)
    new_york = {
        lat = 40.7128,
        lon = -74.0060,
    },
    nyc = {
        lat = 40.7128,
        lon = -74.0060,
    },
    los_angeles = {
        lat = 34.0522,
        lon = -118.2437,
    },
    la = {
        lat = 34.0522,
        lon = -118.2437,
    },
    chicago = {
        lat = 41.8781,
        lon = -87.6298,
    },
    houston = {
        lat = 29.7604,
        lon = -95.3698,
    },
    phoenix = {
        lat = 33.4484,
        lon = -112.0740,
    },
    philadelphia = {
        lat = 39.9526,
        lon = -75.1652,
    },
    san_antonio = {
        lat = 29.4241,
        lon = -98.4936,
    },
    san_diego = {
        lat = 32.7157,
        lon = -117.1611,
    },
    dallas = {
        lat = 32.7767,
        lon = -96.7970,
    },
    san_jose = {
        lat = 37.3382,
        lon = -121.8863,
    },
    austin = {
        lat = 30.2672,
        lon = -97.7431,
    },
    toronto = {
        lat = 43.6532,
        lon = -79.3832,
    },
    montreal = {
        lat = 45.5017,
        lon = -73.5673,
    },
    vancouver = {
        lat = 49.2827,
        lon = -123.1207,
    },
    calgary = {
        lat = 51.0447,
        lon = -114.0719,
    },

    -- Central & South America
    mexico_city = {
        lat = 19.4326,
        lon = -99.1332,
    },
    guadalajara = {
        lat = 20.6597,
        lon = -103.3496,
    },
    monterrey = {
        lat = 25.6866,
        lon = -100.3161,
    },
    bogota = {
        lat = 4.7110,
        lon = -74.0721,
    },
    medellin = {
        lat = 6.2442,
        lon = -75.5812,
    },
    cali = {
        lat = 3.4516,
        lon = -76.5320,
    },
    lima = {
        lat = -12.0464,
        lon = -77.0428,
    },
    santiago = {
        lat = -33.4489,
        lon = -70.6693,
    },
    buenos_aires = {
        lat = -34.6037,
        lon = -58.3816,
    },
    sao_paulo = {
        lat = -23.5505,
        lon = -46.6333,
    },
    rio_de_janeiro = {
        lat = -22.9068,
        lon = -43.1729,
    },
    brasilia = {
        lat = -15.8267,
        lon = -47.9218,
    },
    montevideo = {
        lat = -34.9011,
        lon = -56.1645,
    },

    -- Europe
    london = {
        lat = 51.5074,
        lon = -0.1278,
    },
    manchester = {
        lat = 53.4808,
        lon = -2.2426,
    },
    birmingham = {
        lat = 52.4862,
        lon = -1.8904,
    },
    edinburgh = {
        lat = 55.9533,
        lon = -3.1883,
    },
    dublin = {
        lat = 53.3498,
        lon = -6.2603,
    },

    paris = {
        lat = 48.8566,
        lon = 2.3522,
    },
    lyon = {
        lat = 45.7640,
        lon = 4.8357,
    },
    marseille = {
        lat = 43.2965,
        lon = 5.3698,
    },

    berlin = {
        lat = 52.5200,
        lon = 13.4050,
    },
    hamburg = {
        lat = 53.5511,
        lon = 9.9937,
    },
    munich = {
        lat = 48.1351,
        lon = 11.5820,
    },
    frankfurt = {
        lat = 50.1109,
        lon = 8.6821,
    },

    madrid = {
        lat = 40.4168,
        lon = -3.7038,
    },
    barcelona = {
        lat = 41.3851,
        lon = 2.1734,
    },
    valencia = {
        lat = 39.4699,
        lon = -0.3763,
    },
    lisbon = {
        lat = 38.7223,
        lon = -9.1393,
    },

    rome = {
        lat = 41.9028,
        lon = 12.4964,
    },
    milan = {
        lat = 45.4642,
        lon = 9.1900,
    },
    naples = {
        lat = 40.8518,
        lon = 14.2681,
    },

    brussels = {
        lat = 50.8503,
        lon = 4.3517,
    },
    amsterdam = {
        lat = 52.3676,
        lon = 4.9041,
    },
    vienna = {
        lat = 48.2082,
        lon = 16.3738,
    },
    zurich = {
        lat = 47.3769,
        lon = 8.5417,
    },
    geneva = {
        lat = 46.2044,
        lon = 6.1432,
    },
    prague = {
        lat = 50.0755,
        lon = 14.4378,
    },
    budapest = {
        lat = 47.4979,
        lon = 19.0402,
    },
    warsaw = {
        lat = 52.2297,
        lon = 21.0122,
    },
    bucharest = {
        lat = 44.4268,
        lon = 26.1025,
    },
    sofia = {
        lat = 42.6977,
        lon = 23.3219,
    },

    moscow = {
        lat = 55.7558,
        lon = 37.6176,
    },
    saint_petersburg = {
        lat = 59.9343,
        lon = 30.3351,
    },

    stockholm = {
        lat = 59.3293,
        lon = 18.0686,
    },
    gothenburg = {
        lat = 57.7089,
        lon = 11.9746,
    },
    oslo = {
        lat = 59.9139,
        lon = 10.7522,
    },
    copenhagen = {
        lat = 55.6761,
        lon = 12.5683,
    },
    helsinki = {
        lat = 60.1699,
        lon = 24.9384,
    },
    reykjavik = {
        lat = 64.1466,
        lon = -21.9426,
    },

    -- Middle East
    istanbul = {
        lat = 41.0082,
        lon = 28.9784,
    },
    ankara = {
        lat = 39.9208,
        lon = 32.8541,
    },
    izmir = {
        lat = 38.4237,
        lon = 27.1428,
    },
    riyadh = {
        lat = 24.7136,
        lon = 46.6753,
    },
    jeddah = {
        lat = 21.4858,
        lon = 39.1925,
    },
    dubai = {
        lat = 25.2048,
        lon = 55.2708,
    },
    abu_dhabi = {
        lat = 24.4539,
        lon = 54.3773,
    },
    doha = {
        lat = 25.2854,
        lon = 51.5310,
    },
    muscat = {
        lat = 23.5859,
        lon = 58.4059,
    },

    -- Africa
    cairo = {
        lat = 30.0444,
        lon = 31.2357,
    },
    alexandria = {
        lat = 31.2001,
        lon = 29.9187,
    },
    casablanca = {
        lat = 33.5731,
        lon = -7.5898,
    },
    algiers = {
        lat = 36.7538,
        lon = 3.0588,
    },
    tunis = {
        lat = 36.8065,
        lon = 10.1815,
    },
    tripoli = {
        lat = 32.8872,
        lon = 13.1913,
    },
    lagos = {
        lat = 6.5244,
        lon = 3.3792,
    },
    johannesburg = {
        lat = -26.2041,
        lon = 28.0473,
    },
    cape_town = {
        lat = -33.9249,
        lon = 18.4241,
    },
    durban = {
        lat = -29.8587,
        lon = 31.0218,
    },
    nairobi = {
        lat = -1.2921,
        lon = 36.8219,
    },
    addis_ababa = {
        lat = 9.0300,
        lon = 38.7400,
    },
    accra = {
        lat = 5.6037,
        lon = -0.1870,
    },
    abidjan = {
        lat = 5.359951,
        lon = -4.008256,
    },

    -- Central & South Asia
    mumbai = {
        lat = 19.0760,
        lon = 72.8777,
    },
    delhi = {
        lat = 28.6139,
        lon = 77.2090,
    },
    kolkata = {
        lat = 22.5726,
        lon = 88.3639,
    },
    chennai = {
        lat = 13.0827,
        lon = 80.2707,
    },
    bangalore = {
        lat = 12.9716,
        lon = 77.5946,
    },
    hyderabad = {
        lat = 17.3850,
        lon = 78.4867,
    },
    ahmedabad = {
        lat = 23.0225,
        lon = 72.5714,
    },
    pune = {
        lat = 18.5204,
        lon = 73.8567,
    },
    karachi = {
        lat = 24.8607,
        lon = 67.0011,
    },
    lahore = {
        lat = 31.5204,
        lon = 74.3587,
    },

    -- East & Southeast Asia
    tokyo = {
        lat = 35.6895,
        lon = 139.6917,
    },
    yokohama = {
        lat = 35.4437,
        lon = 139.6380,
    },
    osaka = {
        lat = 34.6937,
        lon = 135.5023,
    },
    nagoya = {
        lat = 35.1815,
        lon = 136.9066,
    },
    sapporo = {
        lat = 43.0621,
        lon = 141.3544,
    },

    seoul = {
        lat = 37.5665,
        lon = 126.9780,
    },
    busan = {
        lat = 35.1796,
        lon = 129.0756,
    },

    beijing = {
        lat = 39.9042,
        lon = 116.4074,
    },
    shanghai = {
        lat = 31.2304,
        lon = 121.4737,
    },
    guangzhou = {
        lat = 23.1291,
        lon = 113.2644,
    },
    shenzhen = {
        lat = 22.5431,
        lon = 114.0579,
    },
    chengdu = {
        lat = 30.5728,
        lon = 104.0668,
    },
    chongqing = {
        lat = 29.4316,
        lon = 106.9123,
    },
    tianjin = {
        lat = 39.3434,
        lon = 117.3616,
    },
    hangzhou = {
        lat = 30.2741,
        lon = 120.1551,
    },
    wuhan = {
        lat = 30.5928,
        lon = 114.3055,
    },
    hong_kong = {
        lat = 22.3193,
        lon = 114.1694,
    },
    taipei = {
        lat = 25.0330,
        lon = 121.5654,
    },

    manila = {
        lat = 14.5995,
        lon = 120.9842,
    },
    quezon_city = {
        lat = 14.6760,
        lon = 121.0437,
    },
    jakarta = {
        lat = -6.2088,
        lon = 106.8456,
    },
    surabaya = {
        lat = -7.2575,
        lon = 112.7521,
    },
    bandung = {
        lat = -6.9175,
        lon = 107.6191,
    },
    kuala_lumpur = {
        lat = 3.1390,
        lon = 101.6869,
    },
    george_town = {
        lat = 5.4141,
        lon = 100.3288,
    }, -- Penang
    singapore = {
        lat = 1.3521,
        lon = 103.8198,
    },
    bangkok = {
        lat = 13.7563,
        lon = 100.5018,
    },
    ho_chi_minh = {
        lat = 10.8231,
        lon = 106.6297,
    },
    hanoi = {
        lat = 21.0278,
        lon = 105.8342,
    },
    phnom_penh = {
        lat = 11.5564,
        lon = 104.9282,
    },
    vientiane = {
        lat = 17.9757,
        lon = 102.6331,
    },
    yangon = {
        lat = 16.8409,
        lon = 96.1735,
    },

    -- Oceania
    sydney = {
        lat = -33.8688,
        lon = 151.2093,
    },
    melbourne = {
        lat = -37.8136,
        lon = 144.9631,
    },
    brisbane = {
        lat = -27.4698,
        lon = 153.0251,
    },
    perth = {
        lat = -31.9505,
        lon = 115.8605,
    },
    adelaide = {
        lat = -34.9285,
        lon = 138.6007,
    },
    auckland = {
        lat = -36.8485,
        lon = 174.7633,
    },
    wellington = {
        lat = -41.2865,
        lon = 174.7762,
    },
    christchurch = {
        lat = -43.5321,
        lon = 172.6362,
    },

    -- Misc / additional cities & aliases
    zurich = {
        lat = 47.3769,
        lon = 8.5417,
    },
    geneva = {
        lat = 46.2044,
        lon = 6.1432,
    },
    porto = {
        lat = 41.1579,
        lon = -8.6291,
    },
    valencia = {
        lat = 39.4699,
        lon = -0.3763,
    },
    palma_de_mallorca = {
        lat = 39.5696,
        lon = 2.6502,
    },

    -- add more if desired...

    mariana = {
        lat = 11.35,
        lon = 142.2,
    },
    everest = {
        lat = 7.988333,
        lon = 86.925278,
    },
}

local function get_api_url_for_name(name)
    local base = core.settings:get("geoip.api_geocode_url") or
                     "https://nominatim.openstreetmap.org/search?format=jsonv2&limit=1&q=%s"
    return base:format(urlencode(name))
end

local function move_to_city(player, name)
    if not name then
        return nil
    end

    local pname = player:get_player_name()

    -- Check cache
    local cached = cache_get(name)
    if cached and cached[1] then
        move_player_to_geo(player, cached[1], 1)
        return nil
    end

    local url = get_api_url_for_name(name)

    if not http then
        print("[geoip] HTTP API not available on server; cannot perform GeoIP lookup.")
        local key = string.lower(name:gsub("[^%w]+", "_"))
        return cities[key]
    end

    http.fetch({
        url = url,
        timeout = 8,
    }, function(result)
        if not result or not result.succeeded then
            local err = (result and result.error) and result.error or "unknown error"
            core.chat_send_player(pname, "[geoip] nominatim request failed: " .. tostring(err))
            print("nominatim fail", tostring(err))
            return
        end
        print("Player geo: ", name, result.data)
        local ok, data = pcall(core.parse_json, result.data)
        if not ok or not data or not data[1] then
            -- core.chat_send_player(pname, "[geoip] Failed to parse GeoIP response.")
            print("nominatim fail", data)
            return
        end

        -- Store in cache (even errors are stored to avoid hammering API; store raw response table)
        cache_set(name, data)

        return move_player_to_geo(player, data[1], 1)
    end)
    return nil
end

local function split_args(s)
    local t = {}
    for token in string.gmatch(s, "%S+") do
        table.insert(t, token)
    end
    return t
end

if core.is_creative_enabled("") then
    core.register_chatcommand("geo", {
        params = "",
        description = "Teleport to geo, city or lat,lon",
        privs = {teleport = true},
        func = function(name, param)
            local player = core.get_player_by_name(name)
            if not player then
                return false, "Player not found."
            end

        print("/geo ", name, param)

        local params = {} -- Table to store split parameters
        -- First, try splitting by comma
        for word in string.gmatch(param, "[^,]+") do -- Split by comma
            table.insert(params, string.trim(word)) -- Use string.trim if available, or assume clean input
        end
        if #params ~= 2 then
            -- If not exactly two after comma split, try splitting by spaces
            params = {}
            for word in string.gmatch(param, "%S+") do -- %S+ matches non-space sequences
                table.insert(params, word)
            end
        end

        local data
        if #params == 2 then
            local lat = tonumber(params[1])
            local lon = tonumber(params[2])
            if lat and lon then
                data = {
                    lat = lat,
                    lon = lon,
                }
            end
        end

        if not data then
            local cityref = move_to_city(player, table.concat(params, " ", 1))
            if cityref then
                data = {
                    lat = cityref.lat,
                    lon = cityref.lon,
                }
            else
                -- maybe moved async
                return true, ""
            end
        end

        if not data then
            return false, "Invalid lat/lon or unknown city"
        end

            move_player_to_geo(player, data, 1)
            return true, ""
        end,
    })
end

if mg_earth_ok and mg_earth_data and mg_earth_data.center and mg_earth_data.center.x and mg_earth_data.center.z then
elseif core.settings:get("earth_geo_spawn") then
    core.register_on_newplayer(do_geo_lookup_for_player)
    core.register_on_respawnplayer(do_geo_lookup_for_player_instant)
end
