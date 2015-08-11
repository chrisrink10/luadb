--[[
LuaDB :: reqhandler.lua

Generic/example HTTP request handler.

Author:  Chris Rink <chrisrink10@gmail.com>

License: MIT (see LICENSE document at source tree root)
]]--

-- Display a quick debug report of the HTTP Request
local function http_debug(request)
    -- Response headers
    local headers = { ["Content-Type"] = "text/html" }

    local body = {
        "<!DOCTYPE html><html><head><title>",
        "LuaDB HTTP Debug</title></head><body>"
    }

    -- Request variables from the webserver
    table.insert(body, "<h3>Request Variables</h3><ul>")
    for k,v in pairs(request.vars) do
        local item = "<li>" .. k .. " = " .. v .. "</li>"
        table.insert(body, item)
    end
    table.insert(body, "</ul>")

    -- HTTP headers from the webserver
    table.insert(body, "<h3>HTTP Headers</h3><ul>")
    for k,v in pairs(request.headers) do
        local item = "<li>" .. k .. ": " .. v .. "</li>"
        table.insert(body, item)
    end
    table.insert(body, "</ul>")

    table.insert(body, "<h3>Request Body</h3>")
    table.insert(body, "<div><pre>" .. request.body .. "</pre></div>")
    table.insert(body, "</body></html>")

    -- Set the body in the response
    return 200, headers, table.concat(body)
end

-- Display a 500 Internal Server Error page
local function server_error(request)
    local status = "500 LuaDB Server Error"
    local headers = { ["Content-Type"] = "text/html" }
    local body = ("<!DOCTYPE html><html><head>" ..
            "<title>500 Internal Server Error</title>" ..
            "</head><body><h1>500 Internal Server Error</h1>" ..
            "</body></html"
    )

    return status, headers, body
end

-- Display a 404 Page Not Found error page
local function not_found(request)
    local status = "404 Page Not Found"
    local headers = { ["Content-Type"] = "text/html" }
    local body = ("<!DOCTYPE html><html><head>" ..
            "<title>404 Page Not Found</title>" ..
            "</head><body><h1>404 Page Not Found</h1>" ..
            "</body></html"
    )

    return status, headers, body
end

-- Route HTTP requests
local function route(request)
    -- Define a routing table
    local routes = {
        ['/debug'] = http_debug,
    }
    setmetatable(routes, { ["__index"] = function() return not_found end })

    -- Execute the route endpoint
    local endpoint = routes[request.vars.document_uri]
    if endpoint ~= nil then
        local success, status, headers, body = pcall(endpoint, request)
        if not success then
            status, headers, body = server_error(request)
        end
        return {
            ['status'] = status,
            ['headers'] = headers,
            ['body'] = body,
        }
    end

    return { ['status'] = nil, ['headers'] = nil,  ['body'] = nil }
end

-- Route requests
return route