-- Freeminer transport selection for the online menu.
local version = core.get_version()
local protocols = {}
local aliases = { mt_ws = "ws", wss = "ws", mt_ws_sctp = "ws_sctp",
	sctp_wss = "ws_sctp", mt_enet = "enet", mt_sctp = "sctp" }
local function normalize(proto)
	return aliases[proto] or proto or "mt"
end
for _, proto in ipairs({ "sctp", "enet", "mt", "ws", "ws_sctp" }) do
	if version.transports and version.transports[proto] then
		protocols[#protocols + 1] = proto
	end
end
local function port_for(server, proto)
	for name, port in pairs(server.proto_multi or {}) do
		if normalize(name) == proto then
			return tonumber(port)
		end
	end
	if normalize(server.proto) == proto then
		return tonumber(server.port)
	end
end
local transport = {}
function transport.matches(server, address, port)
	if server.address ~= address then return false end
	if tonumber(server.port) == port then return true end
	for _, alternate in pairs(server.proto_multi or {}) do
		if tonumber(alternate) == port then return true end
	end
	return false
end
function transport.select(server)
	local preferred = normalize(core.settings:get("remote_proto"))
	for _, proto in ipairs(protocols) do
		if proto == preferred and port_for(server, proto) then
			return proto, port_for(server, proto)
		end
	end
	for _, proto in ipairs(protocols) do
		if port_for(server, proto) then return proto, port_for(server, proto) end
	end
	return normalize(server.proto), server.port
end
local function available_protocols(server)
	local available = {}
	if server then
		for _, proto in ipairs(protocols) do
			if port_for(server, proto) then
				available[#available + 1] = proto
			end
		end
	end
	return available
end
function transport.formspec(server)
	local available = available_protocols(server)
	if not version.transport_multi or #available < 2 then return "" end
	local selected = 1
	for i, proto in ipairs(available) do
		if proto == normalize(core.settings:get("remote_proto")) then selected = i end
	end
	return "label[0.25,1.55;" .. fgettext("Protocol") .. "]" ..
		"dropdown[1.4,1.3;1.9,0.5;fm_transport;" ..
		table.concat(available, ",") .. ";" .. selected .. ";true]"
end
function transport.from_index(index, server)
	local available = available_protocols(server)
	if version.transport_multi and #available > 1 then
		return available[tonumber(index)]
	end
end
return transport
