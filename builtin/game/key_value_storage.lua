--
-- Key-value storage stuff
--

function core.kv_put(key, data, db)
    local json = core.write_json(data)
    if not json then
        core.log("error", "kv_put: Error in json serialize key=".. key .. " luaized_data=" .. core.serialize(data))
        return
    end
    --core.log("action", "core.kv_put: key=".. key .. " json=" .. json .. " db=" .. db)
    return core.kv_put_string(key, json, db)
end

function core.kv_get(key, db)
    local data = core.kv_get_string(key, db)
    --core.log("action", "core.kv_get: key=".. key .. " json=" .. (data or '') .. " db=" .. db)
    if data ~= nil then
        data = core.parse_json(data)
    end
    return data
end

function core.kv_rename(key1, key2, db)
    local data = core.kv_get_string(key1, db)
    core.kv_delete(key1, db)
    core.kv_put_string(key2, data, db)
end
