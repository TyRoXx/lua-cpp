return function (require)
	local web = require("web", "1.0")
	local visitor_count = 0
	local server = web.create_server(8080, function (request, response)
		response:add_header("Content-Type", "text/html")
		response:add_header("Connection", "close")
		visitor_count = visitor_count + 1
		response:set_content("Hello, world!<br>Visitor number: " .. tostring(visitor_count))
	end)
	server:wait()
end
