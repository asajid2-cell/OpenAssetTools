function useSourceTemplating(projectName)
	local projectFolder = path.join(ProjectFolder(), projectName)
	local templateFiles = os.matchfiles(path.join(projectFolder, "**.template"))
	local projectOutputFolder = path.translate(path.getabsolute(path.join(BuildFolder(), "src", projectName)), "\\")
	local rawTemplaterExecutable = path.translate(
		path.getabsolute(path.join(BuildFolder(), "buildtools", "%{cfg.buildcfg}_%{cfg.platform}", ExecutableByOs('RawTemplater'))),
		"\\")

	local createdFiles = {}

	for i = 1, #templateFiles do
		local templateFile = templateFiles[i]
		local absoluteTemplateFile = path.translate(path.getabsolute(templateFile), "\\")
		local relativeTemplatePath = path.getrelative(projectFolder, templateFile)
		local relativeResultPath = path.replaceextension(relativeTemplatePath, "")
		local relativeLogFilePath = path.replaceextension(relativeTemplatePath, ".log")
		local absoluteLogFilePath = path.translate(path.join(projectOutputFolder, relativeLogFilePath), "\\")
		local resultExtension = path.getextension(relativeResultPath)

		local data = io.readfile(templateFile)
		local gameOptionsStart, gameOptionsEnd = string.find(data, "#options%s+GAME%s*%(")

		if gameOptionsStart == nil then
			error("Source template " .. relativeTemplatePath .. " must define an option called GAME")
		end

		local gameOptionsArgsStart, gameOptionsArgsEnd = string.find(data, "[%a%d%s,]+%)", gameOptionsEnd + 1)

		if gameOptionsArgsStart ~= gameOptionsEnd + 1 then
			error("Source template " .. relativeTemplatePath .. " must define an option called GAME")
		end

		local gameOptions = string.sub(data, gameOptionsArgsStart, gameOptionsArgsEnd - 1)
		local games = string.explode(gameOptions, ",%s*")

		files {
			templateFile
		}

		filter("files:" .. templateFile)
			buildmessage("Templating source file " .. relativeTemplatePath)
			buildinputs {
				rawTemplaterExecutable,
				absoluteTemplateFile
			}
			buildcommands {
				'"' .. rawTemplaterExecutable .. '"'
				.. ' -o "' .. projectOutputFolder .. '"'
				.. ' --build-log "' .. absoluteLogFilePath .. '"'
				.. ' "' .. absoluteTemplateFile .. '"'
			}
			buildoutputs {
				absoluteLogFilePath
			}
			for i = 1, #games do
				local gameName = games[i]
				local outputFileName = path.replaceextension(path.replaceextension(relativeResultPath, "") .. gameName, resultExtension)
				local outputFile = path.translate(path.join(projectOutputFolder, "Game", gameName, outputFileName), "\\")

				table.insert(createdFiles, outputFile)

				buildoutputs {
					outputFile
				}
			end
		filter {}

		includedirs {
			"%{prj.location}"
		}

		files {
			createdFiles
		}
		
		RawTemplater:use()
	end
end
