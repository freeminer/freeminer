--Minetest
--Copyright (C) 2013 sapier
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
-- Generic implementation of a filter/sortable list                           --
-- Usage:                                                                     --
-- Filterlist needs to be initialized on creation. To achieve this you need to --
-- pass following functions:                                                  --
-- raw_fct() (mandatory):                                                     --
--     function returning a table containing the elements to be filtered      --
-- compare_fct(element1,element2) (mandatory):                                --
--     function returning true/false if element1 is same element as element2  --
-- uid_match_fct(element1,uid) (optional)                                     --
--     function telling if uid is attached to element1                        --
-- filter_fct(element,filtercriteria) (optional)                              --
--     function returning true/false if filtercriteria met to element         --
-- fetch_param (optional)                                                     --
--     parameter passed to raw_fct to aquire correct raw data                 --
--                                                                            --
--------------------------------------------------------------------------------
filterlist = {}

--------------------------------------------------------------------------------
function filterlist.refresh(this)
	this.m_raw_list = this.m_raw_list_fct(this.m_fetch_param)
	filterlist.process(this)
end

--------------------------------------------------------------------------------
function filterlist.create(raw_fct,compare_fct,uid_match_fct,filter_fct,fetch_param)

	assert((raw_fct ~= nil) and (type(raw_fct) == "function"))
	assert((compare_fct ~= nil) and (type(compare_fct) == "function"))
	
	local this = {}
	
	this.m_raw_list_fct  = raw_fct
	this.m_compare_fct   = compare_fct
	this.m_filter_fct    = filter_fct
	this.m_uid_match_fct = uid_match_fct
	
	this.m_filtercriteria = nil
	this.m_fetch_param = fetch_param
	
	this.m_sortmode = "none"
	this.m_sort_list = {}

	this.m_processed_list = nil
	this.m_raw_list = this.m_raw_list_fct(this.m_fetch_param)

	filterlist.process(this)
	
	return this
end

--------------------------------------------------------------------------------
function filterlist.add_sort_mechanism(this,name,fct)
	this.m_sort_list[name] = fct
end

--------------------------------------------------------------------------------
function filterlist.set_filtercriteria(this,criteria)
	if criteria == this.m_filtercriteria and
		type(criteria) ~= "table" then
		return
	end
	this.m_filtercriteria = criteria
	filterlist.process(this)
end

--------------------------------------------------------------------------------
function filterlist.get_filtercriteria(this)
	return this.m_filtercriteria
end

--------------------------------------------------------------------------------
--supported sort mode "alphabetic|none"
function filterlist.set_sortmode(this,mode)
	if (mode == this.m_sortmode) then
		return
	end
	this.m_sortmode = mode
	filterlist.process(this)
end

--------------------------------------------------------------------------------
function filterlist.get_list(this)
	return this.m_processed_list
end

--------------------------------------------------------------------------------
function filterlist.get_raw_list(this)
	return this.m_raw_list
end

--------------------------------------------------------------------------------
function filterlist.get_raw_element(this,idx)
	if type(idx) ~= "number" then
		idx = tonumber(idx)
	end
	
	if idx ~= nil and idx > 0 and idx < #this.m_raw_list then
		return this.m_raw_list[idx]
	end
	
	return nil
end

--------------------------------------------------------------------------------
function filterlist.get_raw_index(this,listindex)
	assert(this.m_processed_list ~= nil)
	
	if listindex ~= nil and listindex > 0 and
		listindex <= #this.m_processed_list then
		local entry = this.m_processed_list[listindex]
		
		for i,v in ipairs(this.m_raw_list) do
		
			if this.m_compare_fct(v,entry) then
				return i
			end
		end
	end
	
	return 0
end

--------------------------------------------------------------------------------
function filterlist.get_current_index(this,listindex)
	assert(this.m_processed_list ~= nil)
	
	if listindex ~= nil and listindex > 0 and
		listindex <= #this.m_raw_list then
		local entry = this.m_raw_list[listindex]
		
		for i,v in ipairs(this.m_processed_list) do
		
			if this.m_compare_fct(v,entry) then
				return i
			end
		end
	end
	
	return 0
end

--------------------------------------------------------------------------------
function filterlist.process(this)
	assert(this.m_raw_list ~= nil)

	if this.m_sortmode == "none" and
		this.m_filtercriteria == nil then
		this.m_processed_list = this.m_raw_list
		return
	end
	
	this.m_processed_list = {}
	
	for k,v in pairs(this.m_raw_list) do
		if this.m_filtercriteria == nil or
			this.m_filter_fct(v,this.m_filtercriteria) then
			table.insert(this.m_processed_list,v)
		end
	end
	
	if this.m_sortmode == "none" then
		return
	end
	
	if this.m_sort_list[this.m_sortmode] ~= nil and
		type(this.m_sort_list[this.m_sortmode]) == "function" then
		
		this.m_sort_list[this.m_sortmode](this)
	end
end

--------------------------------------------------------------------------------
function filterlist.size(this)
	if this.m_processed_list == nil then
		return 0
	end
	
	return #this.m_processed_list
end

--------------------------------------------------------------------------------
function filterlist.uid_exists_raw(this,uid)
	for i,v in ipairs(this.m_raw_list) do
		if this.m_uid_match_fct(v,uid) then
			return true
		end
	end
	return false
end

--------------------------------------------------------------------------------
function filterlist.raw_index_by_uid(this, uid)
	local elementcount = 0
	local elementidx = 0
	for i,v in ipairs(this.m_raw_list) do
		if this.m_uid_match_fct(v,uid) then
			elementcount = elementcount +1
			elementidx = i
		end
	end
	
	
	-- If there are more elements than one with same name uid can't decide which
	-- one is meant. This shouldn't be possible but just for sure.
	if elementcount > 1 then
		elementidx=0
	end

	return elementidx
end

--------------------------------------------------------------------------------
-- COMMON helper functions                                                    --
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
function compare_worlds(world1,world2)

	if world1.path ~= world2.path then
		return false
	end
	
	if world1.name ~= world2.name then
		return false
	end
	
	if world1.gameid ~= world2.gameid then
		return false
	end

	return true
end

--------------------------------------------------------------------------------
function sort_worlds_alphabetic(this)

	table.sort(this.m_processed_list, function(a, b)
		--fixes issue #857 (crash due to sorting nil in worldlist)
		if a == nil or b == nil then
			if a == nil and b ~= nil then return false end
			if b == nil and a ~= nil then return true end
			return false
		end
		if a.name:lower() == b.name:lower() then
			return a.name < b.name
		end
		return a.name:lower() < b.name:lower()
	end)
end

--------------------------------------------------------------------------------
function sort_mod_list(this)

	table.sort(this.m_processed_list, function(a, b)
		-- Show game mods at bottom
		if a.typ ~= b.typ then
			return b.typ == "game_mod"
		end
		-- If in same or no modpack, sort by name
		if a.modpack == b.modpack then
			if a.name:lower() == b.name:lower() then
				return a.name < b.name
			end
			return a.name:lower() < b.name:lower()
		-- Else compare name to modpack name
		else
			-- Always show modpack pseudo-mod on top of modpack mod list
			if a.name == b.modpack then
				return true
			elseif b.name == a.modpack then
				return false
			end
			
			local name_a = a.modpack or a.name
			local name_b = b.modpack or b.name
			if name_a:lower() == name_b:lower() then
				return  name_a < name_b
			end
			return name_a:lower() < name_b:lower()
		end
	end)
end
