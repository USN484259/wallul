#!/usr/bin/env lua
-- get folder from command line

local interval = 120
local wp = require("lua_gnome_wallpaper")
local cur_cnt = 0
local file_list = {}

local function has_lock()
	local res, val = pcall(wp.has_screenlock, 3000)
	if not res then
		print(val)
		return false
	end
	return val
end

local function quarter_chance(val)
	local res = (val >> 4) ~ (val & 0x0F)
	res = (res >> 2) ~ (res & 0x03)
	if res == 0 then
		return true
	else
		return false
	end
end

local function update()
	cur_cnt = cur_cnt + 1
	local msg = "attempt " .. cur_cnt .. ' ...\t'

	if has_lock() then
		return msg .. "locked"
	end
	local res, rand = pcall(wp.random, 1)
	if not res then
		return msg .. rand
	end
	if not quarter_chance(string.byte(rand)) then
		return msg .. "skipped"
	end

	local res, index = pcall(wp.random)
	if not res then
		return msg .. index
	end
	index = 1 + index % #file_list
	local path = file_list[index]
	local res, err = pcall(wp.set_wallpaper, path)
	cur_cnt = 0

	if not res then
		msg = msg .. "failed " .. err
		file_list[index] = nil
	else
		msg = msg .. "changed to " .. path
	end
	return msg
end

local function build_list(path, list)
	if '/' ~= string.sub(path,-1) then
		path = path .. '/'
	end
	local k,v
	for k, v in pairs(list) do
		if v == "FILE" then
			local ext = string.match(k, ".*%.(.*)")
			if ext then
				ext = string.lower(ext)
			end
			if ext == "jpg" or ext == "jpeg" or ext == "png" or ext == "bmp" then
				local full_name = path .. k
				print(arg[0] .. ": " .. full_name)
				file_list[#file_list + 1] = full_name
			end
		end
	end
end

local function main()
	local i
	for i = 1, #arg, 1 do
		local path = arg[i]
		print(arg[0] .. ": finding pictures in " .. path)
		local res, list = pcall(wp.ls, path)
		if not res then
			print(list)
			return
		end
		build_list(path, list)
	end
	collectgarbage()
	while #file_list > 0 do
		wp.sleep(interval * 1000)
		local msg = update()
		print(arg[0] .. ": " .. msg)
	end
end


if #arg < 1 then
	print(arg[0] .. ' <folder_list>')
else
	main()
end
