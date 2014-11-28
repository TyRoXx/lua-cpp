return function (require)
	local web = require("web", "1.0")
	local server = web.create_server(8080, function (request, response)
		response:header("Content-Type", "text/html")
		response:set_content("Hello, world!")
	end)
	server:wait()
end
