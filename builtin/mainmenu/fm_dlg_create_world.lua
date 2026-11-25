mg_preset = {
    math = {{
        name = "xenodreambuie1",
        mg_math = '{"generator":"xenodreambuie", "N":12, "scale":0.003, "power":2, "julia_mode":1, "julia_a":-1.13}',
        mg_float_islands = 0,
    }, {
        name = "aexion",
        mg_math = '{"generator" : "aexion", "N" : 11,"cadd" : -1.0002,"size" : 30000 }',
        water_level = -30000,
        mg_float_islands = 0,
        static_spawnpoint = '0, 2, 0',
    }, {
        name = "hypercomplex",
        mg_math = '{"generator":"hypercomplex", "scale":0.0001, "N":12, "interior_mode": 1, "center":{"x":20000, "y":1, "z":0}}',
        mg_float_islands = 0,
        static_spawnpoint = '0, 2, 0',
    }, {
        name = "xenodreambuie2",
        mg_math = '{"generator":"xenodreambuie", "N":20, "scale":0.0000001, "power":2, "julia_mode":1, "julia_a": 0.999, "julia_b": 0.1, "julia_c": 1.01, "center":{"x":6021817,"z":-11938439,"y":-3601800}}',
        mg_float_islands = 0,
    }, {
        name = "menger sponge",
        mg_math = '{"generator":"mb2:menger_sponge", "N":13, "scale":0.001}',
        water_level = -30000,
        mg_float_islands = 0,
    }, {
        name = "rooms",
        mg_math = '{"generator":"rooms"}',
        light_ambient = '1',
        mg_float_islands = 0,
    }},
    earth = {{
        name = "zero",
        mg_earth = '{"center": {"z": 0, "x": 0, "y":0} }',
    }, {
        name = "big_canyon",
        mg_earth = '{"center": {"z": 34.2139435, "x": -111.1582085, "y":0} }',
    }, {
        name = "dolomites",
        mg_earth = '{"center": {"z": 46.4084595, "x": 11.8393018    } }',
    }, {
        name = "dubai",
        mg_earth = '{"center": {"z": 25.19715,   "x": 55.27288      } }',
    }, {
        name = "everest",
        mg_earth = '{"center": {"z": 27.9878279, "x": 86.923833,    "y":0} }',
    }, {
        name = "hong_kong",
        mg_earth = '{"center": {"z": 22.28422,   "x": 114.15996,    "y":0} }',
    }, {
        name = "istanbul",
        mg_earth = '{"center": {"z": 41.008200,  "x": 28.978400,    "y":0} }',
    }, {
        name = "london",
        mg_earth = '{"center": {"z": 51.507400,  "x": -0.127800,    "y":0} }',
    }, {
        name = "matterhorn",
        mg_earth = '{"center": {"z": 45.976400,  "x": 7.658600      } }',
    }, {
        name = "new_york",
        mg_earth = '{"center": {"z": 40.781137,  "x": -73.966487,   "y":0} }',
    }, {
        name = "paris",
        mg_earth = '{"center": {"z": 48.856600,  "x": 2.352200,     "y":0} }',
    }, {
        name = "san_francisco",
        mg_earth = '{"center": {"z": 37.774900,  "x": -122.419400,  "y":0} }',
    }, {
        name = "rome",
        mg_earth = '{"center": {"z": 41.902800,  "x": 12.496400,    "y":0} }',
    }, {
        name = "tokyo",
        mg_earth = '{"center": {"z": 35.6835103, "x": 139.7538318   } }',
    }, {
        name = "los angeles",
        mg_earth = '{"center": {"z": 34.010366, "x": -118.190405 } }',
    }, {
        name = "sf brigde",
        mg_earth = '{"center": {"z": 37.810965, "x": -122.477366 } }',
    }, {
        name = "sphere",
        mg_earth = '{"scale":{"x":1,"y":1,"z":1},"projection":{"type":"sphere","radius":6378137,"origin":{"x":0,"y":0,"z":0}},"center":{"x":0,"y":0,"z":0}}',
        mg_earth_world_elevation_step = 0,
        gravity_background = 0,
    }, {
        name = "inverted_sphere",
        mg_earth = '{"scale":{"x":1,"y":1,"z":1},"projection":{"type":"inverted_sphere","radius":6378137,"origin":{"x":0,"y":0,"z":0}},"center":{"x":0,"y":0,"z":0}}',
        mg_earth_world_elevation_step = 0,
        gravity_background = 0,
    }, {
        name = "cube",
        mg_earth = '{"scale":{"x":1,"y":1,"z":1},"projection":{"type":"cube","radius":6378137,"origin":{"x":0,"y":0,"z":0}},"center":{"x":0,"y":0,"z":0}}',
        mg_earth_world_elevation_step = 0,
        gravity_background = 0,
    }, {
        name = "torus",
        mg_earth = '{"scale":{"x":1,"y":1,"z":1},"projection":{"type":"torus","radius":6378137,"major_radius":12756274,"origin":{"x":0,"y":0,"z":0}},"center":{"x":0,"y":0,"z":0}}',
        mg_earth_world_elevation_step = 0,
        gravity_background = 0,
    }},
}

mg_preset.voxel_earth = mg_preset.earth

function fm_create_world_flags(flags, cb_caverns)
    local additions = {
        indev = {{"trees", fgettext("Trees and jungle grass")}, {"flat", fgettext("Flat terrain")},
                 {"mudflow", fgettext("Mud flow"), fgettext("Terrain surface erosion")},
                 {"floatlands", fgettext("Floatlands (experimental)"), fgettext("Floating landmasses in the sky")} -- Biome settings are in mgv6_biomes below
        },
        math = {cb_caverns, {"ridges", fgettext("Rivers"), fgettext("Sea level rivers")},
                {"mountains", fgettext("Mountains")},
                {"floatlands", fgettext("Floatlands (experimental)"), fgettext("Floating landmasses in the sky")}},
        earth = {cb_caverns -- { "floatlands", fgettext("Floatlands (experimental)"), fgettext("Floating landmasses in the sky") },
        },

    }
    for name, value in pairs(additions) do
        flags[name] = value
    end
end

function fm_create_world_presets(mapgen, y, dialogdata)
    local form = ""
    if mg_preset[mapgen] then
        local selected = 1
        for index, preset in ipairs(mg_preset[mapgen]) do
            if preset.name == dialogdata.mg_preset_name and dialogdata.mg_preset_mapgen == mapgen then
                selected = index
                break
            end
        end
        if mapgen == "math" and dialogdata.mg_math == nil then
            dialogdata.mg_math = mg_preset.math[selected].mg_math or ""
        elseif (mapgen == "earth" or mapgen == "voxel_earth") and dialogdata.mg_earth == nil then
            dialogdata.mg_earth = mg_preset[mapgen][selected].mg_earth or ""
        end
        if dialogdata.mg_preset_mapgen ~= mapgen then
            dialogdata.mg_preset_mapgen = mapgen
            dialogdata.mg_preset_name = mg_preset[mapgen][selected].name
        end
        y = y + 0.2
        form = form .. "label[0," .. (y + 0.1) .. ";" .. fgettext("Preset") .. "]"
        y = y + 0.5
        form = form .. "dropdown[0," .. y .. ";6.3;mg_preset;"
        for _, preset in ipairs(mg_preset[mapgen]) do
            form = form .. core.formspec_escape(preset.name) .. ","
        end
        form = form .. ";" .. selected .. "]"
    end

    return form, y
end

function fm_create_world_fields(current_mg, dialogdata)
    local voxel_earth_fields = ""
    -- fm: Allow JSON mapgen settings in the world creation dialog.
    local math_fields = ""
    if current_mg == "math" then
        math_fields = "textarea[0.3,4.1;5.8,1.5;te_mg_math;" .. fgettext("Math mapgen JSON or Mandelbulber settings") ..
                          ";" .. core.formspec_escape(dialogdata.mg_math or "") .. "]"
    elseif current_mg == "earth" or current_mg == "voxel_earth" then
        local earth_json_y = current_mg == "voxel_earth" and 4.9 or 4.1
        math_fields =
            "textarea[0.3," .. earth_json_y .. ";5.8,1.5;te_mg_earth;" .. fgettext("Earth mapgen JSON") .. ";" ..
                core.formspec_escape(dialogdata.mg_earth or "") .. "]"
    end
    -- ===
    local devtest_y = 3.5
    if current_mg == "voxel_earth" then
        voxel_earth_fields = "field[0.3,4.1;6,0.5;te_voxel_earth_api_key;" .. fgettext("Voxel Earth API key") .. ";" ..
                                 core.formspec_escape(dialogdata.voxel_earth_api_key or "") .. "]"
        devtest_y = 5.0
    end

    return voxel_earth_fields .. math_fields, devtest_y
end

-- Convert Mandelbulber's single-fractal settings format into math mapgen JSON.
function fm_parse_mandelbulber_settings(text)
    local result = {
        scale = 0.001,
        mandelbulber = {},
    }
    local section, formula_id, saw_header = nil, nil, false
    for line in (text .. "\n"):gmatch("([^\r\n]*)[\r\n]+") do
        line = line:match("^%s*(.-)%s*$")
        if line:match("^# Mandelbulber settings file") then
            saw_header = true
        elseif line ~= "" and not line:match("^#") then
            local next_section = line:match("^%[([%w_]+)%]$")
            if next_section then
                section = next_section
                if section:match("^fractal_%d+$") and section ~= "fractal_1" then
                    return nil, "Multiple fractal sections are not supported"
                end
            else
                local key, raw = line:match("^([%w_]+)%s+(.+);%s*$")
                if not key or not section then
                    return nil, "Invalid Mandelbulber settings line"
                end
                if section == "main_parameters" then
                    local slot = key:match("^formula_(%d+)$")
                    if slot then
                        local id = tonumber(raw)
                        if not id or id < 0 or id % 1 ~= 0 then
                            return nil, "Invalid Mandelbulber formula ID"
                        end
                        if slot == "1" then
                            formula_id = id
                        elseif id ~= 0 then
                            return nil, "Multiple formulas are not supported"
                        end
                    elseif key == "N" then
                        result.N = tonumber(raw)
                        if not result.N or result.N < 1 or result.N % 1 ~= 0 then
                            return nil, "Invalid iteration count"
                        end
                    elseif key == "julia_mode" then
                        if raw ~= "true" and raw ~= "false" then
                            return nil, "Invalid Julia mode"
                        end
                        result.julia_mode = raw == "true"
                    elseif key == "julia_c" then
                        local values = {}
                        for v in raw:gmatch("%S+") do
                            local number = tonumber(v)
                            if not number or number ~= number or math.abs(number) == math.huge then
                                return nil, "Invalid Julia constant"
                            end
                            values[#values + 1] = number
                        end
                        if #values ~= 3 then
                            return nil, "Invalid Julia constant"
                        end
                        result.julia_a, result.julia_b, result.julia_c = values[1], values[2], values[3]
                    end
                elseif section == "fractal_1" then
                    if raw == "true" or raw == "false" then
                        result.mandelbulber[key] = raw == "true"
                    else
                        local values = {}
                        for v in raw:gmatch("%S+") do
                            local number = tonumber(v)
                            if not number or number ~= number or math.abs(number) == math.huge then
                                return nil, "Unsupported fractal parameter value: " .. key
                            end
                            values[#values + 1] = number
                        end
                        if #values == 1 then
                            result.mandelbulber[key] = values[1]
                        elseif #values == 3 or #values == 4 then
                            result.mandelbulber[key] = values
                        else
                            return nil, "Invalid fractal parameter value: " .. key
                        end
                    end
                end
            end
        end
    end
    if not saw_header or not formula_id or formula_id == 0 then
        return nil, "Mandelbulber settings need a nonzero formula_1"
    end
    result.generator = "mb2:#" .. formula_id
    return result
end

function fm_create_world_validate(this, fields)
    local math_json = fields["te_mg_math"] or this.data.mg_math or ""
    local earth_json = fields["te_mg_earth"] or this.data.mg_earth or ""
    if fields["dd_mapgen"] == "math" and math_json:match("%S") then
        local parsed, error_message
        if math_json:match("^%s*#%s*Mandelbulber settings file") then
            parsed, error_message = fm_parse_mandelbulber_settings(math_json)
        else
            parsed = core.parse_json(math_json)
            if not math_json:match("^%s*{") then
                parsed = nil
            end
        end
        if type(parsed) ~= "table" then
            this.data.mg_math = math_json
            gamedata.errormessage = error_message or fgettext_ne("Math mapgen settings must be a JSON object")
            return false
        end
    end
    if (fields["dd_mapgen"] == "earth" or fields["dd_mapgen"] == "voxel_earth") and earth_json:match("%S") then
        local parsed = core.parse_json(earth_json)
        if type(parsed) ~= "table" or not earth_json:match("^%s*{") then
            this.data.mg_earth = earth_json
            gamedata.errormessage = fgettext_ne("Earth mapgen settings must be a JSON object")
            return false
        end
    end

    return true
end

function fm_create_world_settings(this, fields, settings, table_to_flags)
    settings.mgindev_spflags = table_to_flags(this.data.flags.indev)
    settings.mgmath_spflags = table_to_flags(this.data.flags.math)
    this.data.voxel_earth_api_key = fields.te_voxel_earth_api_key or this.data.voxel_earth_api_key

    if settings.mg_name == "voxel_earth" and this.data.voxel_earth_api_key ~= "" then
        settings.voxel_earth_api_key = this.data.voxel_earth_api_key
    end

    -- fm:
    if fields["mg_preset"] and mg_preset[settings.mg_name] then
        for _, preset in ipairs(mg_preset[settings.mg_name]) do
            if preset.name == fields["mg_preset"] then
                for opt, val in pairs(preset) do
                    if opt ~= "name" then
                        settings[opt] = val;
                    end
                end
                break
            end
        end
    end
    -- ===
    -- fm: Explicit JSON takes precedence over the selected preset.
    if settings.mg_name == "math" and (fields["te_mg_math"] or this.data.mg_math or ""):match("%S") then
        settings.mg_math = (fields["te_mg_math"] or this.data.mg_math or "")
        if settings.mg_math:match("^%s*#%s*Mandelbulber settings file") then
            settings.mg_math = core.write_json(fm_parse_mandelbulber_settings(settings.mg_math))
        end
    elseif (settings.mg_name == "earth" or settings.mg_name == "voxel_earth") and
        (fields["te_mg_earth"] or this.data.mg_earth or ""):match("%S") then
        settings.mg_earth = (fields["te_mg_earth"] or this.data.mg_earth or "")
    end
    -- ===

end

function fm_create_world_event(this, fields)
    -- fm: Preserve the JSON across form updates.
    if fields["te_mg_math"] ~= nil then
        this.data.mg_math = fields["te_mg_math"]
    end
    if fields["te_mg_earth"] ~= nil then
        this.data.mg_earth = fields["te_mg_earth"]
    end
    -- ===
    if fields["te_voxel_earth_api_key"] ~= nil then
        this.data.voxel_earth_api_key = fields["te_voxel_earth_api_key"]
    end
    -- fm: Copy a selected preset into the editable JSON field.
    if fields["mg_preset"] and fields["mg_preset"] ~= this.data.mg_preset_name then
        local mapgen = fields["dd_mapgen"] or this.data.mg or this.data.mg_preset_mapgen
        for _, preset in ipairs(mg_preset[mapgen] or {}) do
            if preset.name == fields["mg_preset"] then
                this.data.mg_preset_name = preset.name
                this.data.mg_preset_mapgen = mapgen
                if mapgen == "math" then
                    this.data.mg_math = preset.mg_math or ""
                elseif mapgen == "earth" or mapgen == "voxel_earth" then
                    this.data.mg_earth = preset.mg_earth or ""
                end
                return true
            end
        end
    end
    -- ===

    return false
end

function fm_create_world_init(data)
    data.voxel_earth_api_key = core.settings:get("voxel_earth_api_key") or ""
    for _, name in ipairs({"indev", "math", "earth"}) do
        data.flags[name] = core.settings:get_flags("mg" .. name .. "_spflags")
    end
end
