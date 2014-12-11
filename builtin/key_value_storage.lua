--
-- Key-value storage stuff
--

function core.kv_put(key, data)
    local json = core.write_json(data)
    if not json then
        core.log("error", "kv_put: Error in json serialize key=".. key .. " luaized_data=" .. core.serialize(data))
        return
    end
    return core.kv_put_string(key, json)
end

function core.kv_get(key)
    local data = core.kv_get_string(key)
    if data ~= nil then
        data = core.parse_json(data)
    end
    return data
end

function core.kv_rename(key1, key2)
    local data = core.kv_get_string(key1)
    core.kv_delete(key1)
    core.kv_put_string(key2, data)
end
