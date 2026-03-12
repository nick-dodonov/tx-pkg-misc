"""Build rules for embedding files into C++ targets."""

EmbeddedFilesInfo = provider(
    "Provides information about embedded files and their target paths.",
    fields = {
        "files_to_dir": "A dictionary mapping file labels to their corresponding paths in the embedded assets.",
    }
)

_LOG_ENABLED = True

def _log(message):
    """Logs a warning message during the build."""
    if not _LOG_ENABLED:
        return
    _BLUE = "\033[1;34m"
    _RESET = "\033[0m"
    print(_BLUE + message + _RESET)  # buildifier: disable=print


def _embedded_files_impl(ctx):
    _log("=== embedded_files ({})".format(ctx.label))

    # runfiles = []
    # for source, target_dir in ctx.attr.files_to_dir.items():
    #     _log("entry: {} -> {}".format(source, target_dir))
    #     for file in source.files.to_list():
    #         _log("  processing file: {}".format(file))
    #         target_path = "{}/{}".format(target_dir, file.basename)  # e.g., "data/fonts/Roboto-Regular.ttf"
    #         _log("    target_path: {}".format(target_path))
    #         output_file = ctx.actions.declare_file(target_path)
    #         _log("    symlink: {}".format(output_file))
    #         ctx.actions.symlink(
    #             output = output_file,
    #             target_file = file,
    #         )
    #         runfiles.append(output_file)

    return [
        #DefaultInfo(runfiles = ctx.runfiles(files = runfiles)),
        EmbeddedFilesInfo(files_to_dir=ctx.attr.files_to_dir),
    ]

embedded_files = rule(
    implementation = _embedded_files_impl,
    attrs = {
        "files_to_dir": attr.label_keyed_string_dict(allow_files=True),
    },
)


def _droid_embedded_assets_impl(ctx):
    _log("=== droid_embedded_assets ({}) processing ===".format(ctx.label))

    # Create files in the Android assets directory structure
    # android_library with assets_dir="assets" will strip the "assets/" prefix when packaging

    outputs = []
    files_to_dir = ctx.attr.embedded[EmbeddedFilesInfo].files_to_dir
    for source, target_dir in files_to_dir.items():
        # value is the relative path within assets (e.g., "data/fonts")
        # We need to create files at "assets/data/fonts/..." for android_library
        _log("entry: {} -> {}".format(source, target_dir))
        output_dir = "assets/{}".format(target_dir)  # e.g., "assets/data/fonts"
        _log("  output_dir: {}".format(output_dir))
        _log("  source files count: {}".format(len(source.files.to_list())))

        for file in source.files.to_list():
            _log("  processing file: {}".format(file))
            _log("    file.path: {} is_directory={} is_source={} is_symlink={}".format(file.path, file.is_directory, file.is_source, file.is_symlink))
            _log("    file.short_path: {}".format(file.short_path))
            # _log("    file.basename: {}".format(file.basename))
            # _log("    file.extension: {}".format(file.extension))
            # _log("    file.dirname: {}".format(file.dirname))

            # Declare output file in the assets structure
            output_path = "{}/{}".format(output_dir, file.basename)
            output_file = ctx.actions.declare_file(output_path)
            _log("    output_file.path: {}".format(output_file.path))
            _log("    output_file.short_path: {}".format(output_file.short_path))

            # Create symlink according to Bazel docs: output first, then target_file
            ctx.actions.symlink(
                output = output_file,
                target_file = file,
            )
            _log("    symlink: {} -> {}".format(output_file.short_path, file.short_path))
            outputs.append(output_file)
    
    _log("=== droid_embedded_assets ({}) summary ===".format(ctx.label))
    _log("Total output files count: {}".format(len(outputs)))
    for out in outputs:
        _log("  {} (short: {})".format(out.path, out.short_path))
    
    return [DefaultInfo(files=depset(outputs))]


droid_embedded_assets = rule(
    implementation = _droid_embedded_assets_impl,
    attrs = {
        "embedded": attr.label(
            providers = [EmbeddedFilesInfo],
        ),
    },
)

def _wasm_embedded_linkopts_params_impl(ctx):
    _log("=== wasm_embedded_linkopts_params ({}) processing ===".format(ctx.label))
    param_file = ctx.actions.declare_file(ctx.label.name + ".txt")

    files_to_dir = ctx.attr.assets[EmbeddedFilesInfo].files_to_dir
    linkopts = []
    for source, target_dir in files_to_dir.items():
        _log("entry: {} -> {}".format(source, target_dir))
        for file in source.files.to_list():
            target_path = "{}/{}".format(target_dir, file.basename)  # e.g., "data/fonts/Roboto-Regular.ttf"
            _log("  adding --preload-file option {} -> {}".format(file.path, target_path))
            # Format: --preload-file physical_path@path_in_wasm
            linkopts.append("--preload-file %s@/%s\n" % (file.path, target_path))

    ctx.actions.write(param_file, "".join(linkopts))
    return [
        DefaultInfo(files = depset([param_file])),
        # Don't need to pass files itself as dependency build tree should be available to linker..
        # runfiles = ctx.runfiles(files = ctx.files.assets)
    ]

wasm_embedded_linkopts_params = rule(
    implementation = _wasm_embedded_linkopts_params_impl,
    attrs = {
        "assets": attr.label(
            providers = [EmbeddedFilesInfo],
        ),
    },
)
