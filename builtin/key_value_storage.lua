--
-- Key-value storage stuff
--

function freeminer.kv_put(key, data)
    local json = freeminer.write_json(data)
    if not json then
        core.log("error", "kv_put: Error in json serialize key=".. key .. " luaized_data=" .. core.serialize(data))
        return
    end
    return freeminer.kv_put_string(key, json)
end

function freeminer.kv_get(key)
    local data = freeminer.kv_get_string(key)
    if data ~= nil then
        data = freeminer.parse_json(data)
    end
    return data
end

function freeminer.kv_rename(key1, key2)
    local data = freeminer.kv_get_string(key1)
    freeminer.kv_delete(key1)
    freeminer.kv_put_string(key2, data)
end
