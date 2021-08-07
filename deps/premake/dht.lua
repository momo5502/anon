dht = {
	source = path.join(dependencies.basePath, "dht"),
}

function dht.import()
	links { "dht" }
	dht.includes()
end

function dht.includes()
	includedirs {
		dht.source
	}
end

function dht.project()
	project "dht"
		language "C"

		dht.includes()

		files {
			path.join(dht.source, "dht.h"),
			path.join(dht.source, "dht.c"),
		}

		warnings "Off"
		kind "StaticLib"
end

table.insert(dependencies, dht)
