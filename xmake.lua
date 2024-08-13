add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_requires("glslang", {configs = {binaryonly = true}})
set_languages("c++20")
-- set_toolset("clang")

rule("vklib.glsl2spv_opt")
    set_extensions(".vert", ".tesc", ".tese", ".geom", ".comp", ".frag", ".comp", ".mesh", ".task", ".rgen", ".rint", ".rahit", ".rchit", ".rmiss", ".rcall", ".glsl")
    on_load(function (target)
        local is_bin2c = target:extraconf("rules", "vklib.glsl2spv_opt", "bin2c")
        if is_bin2c then
            local headerdir = path.join(target:autogendir(), "rules", "utils", "glsl2spv")
            if not os.isdir(headerdir) then
                os.mkdir(headerdir)
            end
            target:add("includedirs", headerdir)
        end
    end)
    before_buildcmd_file(function (target, batchcmds, sourcefile_glsl, opt)
        import("lib.detect.find_tool")

        -- get glslangValidator
        local glslc
        local glslangValidator = find_tool("glslangValidator")
        if not glslangValidator then
            glslc = find_tool("glslc")
        end
        assert(glslangValidator or glslc, "glslangValidator or glslc not found!")

        -- glsl to spv
        local targetenv = target:extraconf("rules", "vklib.glsl2spv_opt", "targetenv") or "vulkan1.0"
        local outputdir = target:extraconf("rules", "vklib.glsl2spv_opt", "outputdir") or path.join(target:autogendir(), "rules", "utils", "glsl2spv")

        local spvfilepath = path.join(outputdir, path.filename(sourcefile_glsl) .. ".spv.unopt")
        local optimized_spvfilepath = path.join(outputdir, path.filename(sourcefile_glsl) .. ".spv")

        batchcmds:show_progress(opt.progress, "${color.build.object}generating.glsl2spv %s", sourcefile_glsl)
        batchcmds:mkdir(outputdir)

        if glslangValidator then
            batchcmds:vrunv(glslangValidator.program, {"--target-env", targetenv, "-o", path(spvfilepath), path(sourcefile_glsl)})
        else
            batchcmds:vrunv(glslc.program, {"--target-env", targetenv, "-o", path(spvfilepath), path(sourcefile_glsl)})
        end

		batchcmds:vrunv("spirv-opt", {"--target-env=" .. targetenv, "-O", path(spvfilepath), "-o", path(optimized_spvfilepath), })

        -- do bin2c
        local outputfile = optimized_spvfilepath
        local is_bin2c = target:extraconf("rules", "vklib.glsl2spv_opt", "bin2c")
        if is_bin2c then
            -- get header file
            local headerdir = outputdir
            local headerfile = path.join(headerdir, path.filename(optimized_spvfilepath) .. ".h")
            target:add("includedirs", headerdir)
            outputfile = headerfile

            -- add commands
            local argv = {"lua", "private.utils.bin2c", "--nozeroend", "-i", path(optimized_spvfilepath), "-o", path(headerfile)}
            batchcmds:vrunv(os.programfile(), argv, {envs = {XMAKE_SKIP_HISTORY = "y"}})
        end

        -- add deps
        batchcmds:add_depfiles(sourcefile_glsl)
        batchcmds:set_depmtime(os.mtime(outputfile))
        batchcmds:set_depcache(target:dependfile(outputfile))
    end)

includes("vklib-hpp")
includes("projects")