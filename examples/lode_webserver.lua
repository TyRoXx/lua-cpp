return function (require)
	local web = require("web", "2.0")
	local visitor_count = 0
	local requests = web.create_server(8080)
	while true do
		local response = requests:sync_get()
		if response == nil then
			break
		end
		coroutine.resume(coroutine.create(function ()
			response:add_header("Content-Type", "text/html")
			response:add_header("Connection", "close")
			visitor_count = visitor_count + 1
			response:set_content("Hello, world!<br>Visitor number: " .. tostring(visitor_count))
		end))
	end
end
