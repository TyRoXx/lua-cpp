local sync_for_each = function (observable, handler)
	while true do
		local element = observable:sync_get()
		if element == nil then
			break
		end
		handler(element)
	end
end

return function (require)
	local web = require("web", "2.0")
	local visitor_count = 0
	local requests = web.create_server(8080)
	sync_for_each(requests, function (response)
		coroutine.resume(coroutine.create(function ()
			response:add_header("Content-Type", "text/html")
			response:add_header("Connection", "close")
			visitor_count = visitor_count + 1
			response:set_content("Hello, world!<br>Visitor number: " .. tostring(visitor_count))
		end))
	end)
end
