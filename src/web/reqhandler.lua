--[[
LuaDB :: reqhandler.lua

Generic/example HTTP request handler.

Author:  Chris Rink <chrisrink10@gmail.com>

License: MIT (see LICENSE document at source tree root)
]]--

-- Display a quick debug report of the HTTP Request
local function http_debug()
    -- Response headers
    http.response.headers["Content-Type"] = "text/html"

    local body = "<!DOCTYPE html><html><head><title>LuaDB HTTP Debug</title></head><body>"

    -- Request variables from the webserver
    body = body .. "<h3>Request Variables</h3><ul>"
    for k,v in pairs(http.request.vars) do
        local item = "<li>" .. k .. " = " .. v .. "</li>"
        body = body .. item
    end
    body = body .. "</ul>"

    -- HTTP headers from the webserver
    body = body .. "<h3>HTTP Headers</h3><ul>"
    for k,v in pairs(http.request.headers) do
        local item = "<li>" .. k .. ": " .. v .. "</li>"
        body = body .. item
    end
    body = body .. "</ul>"

    body = body .. "<h3>Request Body</h3>"
    body = body .. "<div><pre>" .. http.request.body .. "</pre></div>"
    body = body .. "</body></html>"

    -- Set the body in the response
    http.response.body = body
end

-- Display a 500 Internal Server Error page
local function server_error()
    http.response.headers["Status"] = "500 LuaDB Server Error"
    http.response.headers["Content-Type"] = "text/html"
    http.response.body = ("<!DOCTYPE html><html><head>" ..
            "<title>500 Internal Server Error</title>" ..
            "</head><body><h1>500 Internal Server Error</h1>" ..
            "</body></html"
    )
end

-- Display a 404 Page Not Found error page
local function not_found()
    http.response.headers["Status"] = "404 Page Not Found"
    http.response.headers["Content-Type"] = "text/html"
    http.response.body = ("<!DOCTYPE html><html><head>" ..
            "<title>404 Page Not Found</title>" ..
            "</head><body><h1>404 Page Not Found</h1>" ..
            "</body></html"
    )
end

-- Route HTTP requests
local function route()
    -- Define a routing table
    local routes = {
        ['/debug'] = http_debug,
    }
    setmetatable(routes, { ["__index"] = not_found })

    -- Execute the route endpoint
    local endpoint = routes[http.request.vars.request_uri]
    if endpoint ~= nil then
        if not pcall(endpoint) then
            server_error()
        end
    end
end

-- Route requests
pcall(route)

return