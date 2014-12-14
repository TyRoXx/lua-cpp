
return function (require)
	local tcp = require("tcp", "1.0")
	local http = require("http", "1.0")
	local gc = require("gc", "1.0")
	local time = require("time", "1.0")
	local async = require("async", "1.0")

	local await = async.await_one
	local sync_for_each = function (observable, handler)
		while true do
			local element = await(observable)
			if element == nil then
				break
			end
			handler(element)
		end
	end

	local visitor_count = 0
	local clients = tcp.create_acceptor(8080)
	local current_client_count = 0
	sync_for_each(clients, function (client)
		coroutine.resume(coroutine.create(function ()
			current_client_count = current_client_count + 1
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
			response:content(
				"<li>Hello, world!" ..
				"<li>Visitor number: " .. tostring(visitor_count) ..
				"<li>GC allocated bytes: " .. tostring(gc.get_allocated_bytes()) ..
				"<li>Current clients: " .. tostring(current_client_count)
			)

			time.sleep(0.02)

			local timer = time.create_timer(async.constant(0.02))
			await(timer)

			sender:flush()
			current_client_count = current_client_count - 1
		end))
	end)
end
