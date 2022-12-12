-- for test , dump all table filed
local function print_r ( t )
    local print_r_cache={}
    local function sub_print_r(t,indent)
        if (print_r_cache[tostring(t)]) then
            print(indent.."*"..tostring(t))
        else
            print_r_cache[tostring(t)]=true
            if (type(t)=="table") then
                for pos,val in pairs(t) do
                    if (type(val)=="table") then
                        print(indent.."["..pos.."] => "..tostring(t).." {")
                        sub_print_r(val,indent..string.rep(" ",string.len(pos)+8))
                        print(indent..string.rep(" ",string.len(pos)+6).."}")
                    elseif (type(val)=="string") then
                        print(indent.."["..pos..'] => "'..val..'"')
                    else
                        print(indent.."["..pos.."] => "..tostring(val))
                    end
                end
            else
                print(indent..tostring(t))
            end
        end
    end
    if (type(t)=="table") then
        print(tostring(t).." {")
        sub_print_r(t,"  ")
        print("}")
    else
        sub_print_r(t,"  ")
    end
    print()
end

local function _table2str(lua_table, raw_table, table_map, n, fold, indent)
    indent = indent or 1
    for k, v in pairs(lua_table) do
        if type(k) == 'string' then
            k = string.format('%q', k)
        else
            k = tostring(k)
        end
        n = n + 1; raw_table[n] = string.rep('    ', indent)
        n = n + 1; raw_table[n] = '['
        n = n + 1; raw_table[n] = k
        n = n + 1; raw_table[n] = ']'
        n = n + 1; raw_table[n] = ' = '
        if type(v) == 'table' then
            if fold and table_map[tostring(v)] then
                n = n + 1; raw_table[n] = tostring(v)
                n = n + 1; raw_table[n] = ',\n'
            else
                table_map[tostring(v)] = true
                n = n + 1; raw_table[n] = '{\n'
                n = _table2str(v, raw_table, table_map, n, fold, indent + 1)
                n = n + 1; raw_table[n] = string.rep('    ', indent)
                n = n + 1; raw_table[n] = '},\n'
            end
        else
            if type(v) == 'string' then
                v = string.format('%q', v)
            else
                v = tostring(v)
            end
            n = n + 1; raw_table[n] = v
            n = n + 1; raw_table[n] = ',\n'
        end
    end
    return n
end

local function _serializer(lua_table, table_name, fold)
    -- print_r(lua_table)
    local raw_table = {}
    local table_map = {}
    table_map[tostring(lua_table)] = true
    local n = 0
    n = n + 1; raw_table[n] = table_name..' = {\n'
    n = _table2str(lua_table, raw_table, table_map, n, fold)
    n = n + 1; raw_table[n] = '}\n\n'
    return table.concat(raw_table, '')
end

local function _write_to_file(file, raw_table, table_name)
    local file = io.open(file, "w+")
    assert(file)
    if raw_table == nil then
        file:write(_serializer(system, "system", true))
        file:write(_serializer(media, "media", true))
    elseif table_name ~= nil then
        file:write(_serializer(raw_table, table_name, true))
    end
    file:close()
end

return {
    serializer = _serializer,
    tofile = _write_to_file
}
