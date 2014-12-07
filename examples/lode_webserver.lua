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
	local tcp = require("tcp", "1.0")
	local http = require("http", "1.0")
	local visitor_count = 0
	local clients = tcp.create_acceptor(8080)
	sync_for_each(clients, function (client)
		--coroutine.resume(coroutine.create(function ()
		--	local request = http.parse_request(client:receiver())
		--	if request == nil then
		--		-- respond with bad request or sth
		--		return
		--	end
			local sender = client
			local response = http.make_response_generator(sender)
			response:status_line("200", "OK", "HTTP/1.0")
			response:header("Content-Type", "text/html")
			response:header("Connection", "close")
			visitor_count = visitor_count + 1
			response:content("Hello, world!<br>Visitor number: " .. tostring(visitor_count))
			sender:flush()
		--end))
	end)
end
