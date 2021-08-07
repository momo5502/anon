sha256 = {
	source = path.join(dependencies.basePath, "sha256"),
}

function sha256.import()
	links { "sha256" }
	sha256.includes()
end

function sha256.includes()
	includedirs {
		path.join(sha256.source, "include"),
	}
end

function sha256.project()
	project "sha256"
		language "C++"

		sha256.includes()

		files {
			path.join(sha256.source, "src/SHA256.cpp"),
		}

		warnings "Off"
		kind "StaticLib"
end

table.insert(dependencies, sha256)
