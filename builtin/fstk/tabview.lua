--Minetest
--Copyright (C) 2014 sapier
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


--------------------------------------------------------------------------------
-- A tabview implementation                                                   --
-- Usage:                                                                     --
-- tabview.create: returns initialized tabview raw element                    --
-- element.add(tab): add a tab declaration                                    --
-- element.handle_buttons()                                                   --
-- element.handle_events()                                                    --
-- element.getFormspec() returns formspec of tabview                          --
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
local function add_tab(self,tab)
	assert(tab.size == nil or (type(tab.size) == table and
			tab.size.x ~= nil and tab.size.y ~= nil))
	assert(tab.cbf_formspec ~= nil and type(tab.cbf_formspec) == "function")
	assert(tab.cbf_button_handler == nil or
			type(tab.cbf_button_handler) == "function")
	assert(tab.cbf_events == nil or type(tab.cbf_events) == "function")

	local newtab = {
		name = tab.name,
		caption = tab.caption,
		button_handler = tab.cbf_button_handler,
		event_handler = tab.cbf_events,
		get_formspec = tab.cbf_formspec,
		tabsize = tab.tabsize,
		on_change = tab.on_change,
		tabdata = {},
	}

	self.tablist[#self.tablist + 1] = newtab

	if self.last_tab_index == #self.tablist then
		self.current_tab = tab.name
		if tab.on_activate ~= nil then
			tab.on_activate(nil,tab.name)
		end
	end
end

--------------------------------------------------------------------------------
local function get_formspec(self)
	local formspec = ""

	if not self.hidden and (self.parent == nil or not self.parent.hidden) then

		if self.parent == nil then
			local tsize = self.tablist[self.last_tab_index].tabsize or
					{width=self.width, height=self.height}
			formspec = formspec ..
					string.format("size[%f,%f,%s]",tsize.width,tsize.height,
						dump(self.fixed_size))
		end
		formspec = formspec .. self:tab_header()
		formspec = formspec ..
				self.tablist[self.last_tab_index].get_formspec(
					self,
					self.tablist[self.last_tab_index].name,
					self.tablist[self.last_tab_index].tabdata,
					self.tablist[self.last_tab_index].tabsize
					)
	end
	return formspec
end

--------------------------------------------------------------------------------
local function handle_buttons(self,fields)

	if self.hidden then
		return false
	end

	if self:handle_tab_buttons(fields) then
		return true
	end

	if self.glb_btn_handler ~= nil and
		self.glb_btn_handler(self,fields) then
		return true
	end

	if self.tablist[self.last_tab_index].button_handler ~= nil then
		return
			self.tablist[self.last_tab_index].button_handler(
					self,
					fields,
					self.tablist[self.last_tab_index].name,
					self.tablist[self.last_tab_index].tabdata
					)
	end

	return false
end

--------------------------------------------------------------------------------
local function handle_events(self,event)

	if self.hidden then
		return false
	end

	if self.glb_evt_handler ~= nil and
		self.glb_evt_handler(self,event) then
		return true
	end

	if self.tablist[self.last_tab_index].evt_handler ~= nil then
		return
			self.tablist[self.last_tab_index].evt_handler(
					self,
					event,
					self.tablist[self.last_tab_index].name,
					self.tablist[self.last_tab_index].tabdata
					)
	end

	return false
end


--------------------------------------------------------------------------------
local function tab_header(self)

	local toadd = ""

	for i=1,#self.tablist,1 do

		if toadd ~= "" then
			toadd = toadd .. ","
		end

		toadd = toadd .. self.tablist[i].caption
	end
	return string.format("tabheader[%f,%f;%s;%s;%i;true;false]",
			self.header_x, self.header_y, self.name, toadd, self.last_tab_index);
end

--------------------------------------------------------------------------------
local function switch_to_tab(self, index)
	--first call on_change for tab to leave
	if self.tablist[self.last_tab_index].on_change ~= nil then
		self.tablist[self.last_tab_index].on_change("LEAVE",
				self.current_tab, self.tablist[index].name)
	end

	--update tabview data
	self.last_tab_index = index
	local old_tab = self.current_tab
	self.current_tab = self.tablist[index].name

	if (self.autosave_tab) then
		core.setting_set(self.name .. "_LAST",self.current_tab)
	end

	-- call for tab to enter
	if self.tablist[index].on_change ~= nil then
		self.tablist[index].on_change("ENTER",
				old_tab,self.current_tab)
	end
end

--------------------------------------------------------------------------------
local function handle_tab_buttons(self,fields)
	--save tab selection to config file
	if fields[self.name] then
		local index = tonumber(fields[self.name])
		switch_to_tab(self, index)
		return true
	end

	return false
end

--------------------------------------------------------------------------------
local function set_tab_by_name(self, name)
	for i=1,#self.tablist,1 do
		if self.tablist[i].name == name then
			switch_to_tab(self, i)
			return true
		end
	end

	return false
end

--------------------------------------------------------------------------------
local function hide_tabview(self)
	self.hidden=true

	--call on_change as we're not gonna show self tab any longer
	if self.tablist[self.last_tab_index].on_change ~= nil then
		self.tablist[self.last_tab_index].on_change("LEAVE",
				self.current_tab, nil)
	end
end

--------------------------------------------------------------------------------
local function show_tabview(self)
	self.hidden=false

	-- call for tab to enter
	if self.tablist[self.last_tab_index].on_change ~= nil then
		self.tablist[self.last_tab_index].on_change("ENTER",
				nil,self.current_tab)
	end
end

local tabview_metatable = {
	add                       = add_tab,
	handle_buttons            = handle_buttons,
	handle_events             = handle_events,
	get_formspec              = get_formspec,
	show                      = show_tabview,
	hide                      = hide_tabview,
	delete                    = function(self) ui.delete(self) end,
	set_parent                = function(self,parent) self.parent = parent end,
	set_autosave_tab          =
			function(self,value) self.autosave_tab = value end,
	set_tab                   = set_tab_by_name,
	set_global_button_handler =
			function(self,handler) self.glb_btn_handler = handler end,
	set_global_event_handler =
			function(self,handler) self.glb_evt_handler = handler end,
	set_fixed_size =
			function(self,state) self.fixed_size = state end,
	tab_header = tab_header,
	handle_tab_buttons = handle_tab_buttons
}

tabview_metatable.__index = tabview_metatable

--------------------------------------------------------------------------------
function tabview_create(name, size, tabheaderpos)
	local self = {}

	self.name     = name
	self.type     = "toplevel"
	self.width    = size.x
	self.height   = size.y
	self.header_x = tabheaderpos.x
	self.header_y = tabheaderpos.y

	setmetatable(self, tabview_metatable)

	self.fixed_size     = true
	self.hidden         = true
	self.current_tab    = nil
	self.last_tab_index = 1
	self.tablist        = {}

	self.autosave_tab   = false

	ui.add(self)
	return self
end
