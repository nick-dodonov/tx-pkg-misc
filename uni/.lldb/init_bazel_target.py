import os
import shlex
import datetime
import subprocess
# https://lldb.llvm.org/python_api/lldb.SBDebugger.html
# to support code completion setup used lldb package, e.g., for CLion w/ .venv python interpreter:
# ln -s "/Users/nik/Applications/CLion 2025.3 EAP.app/Contents/bin/lldb/mac/x64/LLDB.framework/Resources/Python/lldb" .venv/lib/python3.13/site-packages/lldb
import lldb

print(f"[{datetime.datetime.now().strftime('%H:%M:%S')}] === Load {__file__}")
print(f"lldb/package: {lldb.__path__}")

#TODO: bazel_module_name(): sed -n '/^module(/,/^)/{s/.*name = "\([^"]*\)".*/\1/p;}' MODULE.bazel
def bazel_output_base():
    """Extract bazel output base from MODULE.bazel file."""
    output = subprocess.check_output(shlex.split('bazel info output_base'), text=True)
    return output

def lldb_cmd(command: str):
    print(f'lldb/cmd: {command}')
    lldb.debugger.HandleCommand(command)

def setup_target_source_map():
    '''Setup Bazel source mapping for current target.'''
    target = lldb.debugger.GetSelectedTarget()
    if target:
        print(f"""target: 
    triple: {target.GetTriple()}
    platform: {target.GetPlatform().GetTriple()}
    executable: {target.GetExecutable()}""")

    current_dir = os.getcwd()
    print(f'current_dir: {current_dir}')

    output_base = bazel_output_base().strip()
    print(f'output_base: {output_base}')

    external_dst = f'{output_base}/external'

    source_map = {
        # С/С++ extension debugger (cppdbg) in VSCode doesn't add working dir to source map
        ".": current_dir,
        # Relative path mapping for builds compiled/linked with -ffile-compilation-dir=. and/or --linkopt=-Wl,-oso_prefix,
        "external": external_dst,
        # Absolute path mapping for another build variants (e.g. CLion's Bazel plugin build overrides setup)
        f'{current_dir}/external': external_dst,
        #TODO: adopt to realpaths of specific externals instead of symlinked ones in output_base (local --override_module)
    }

    for src, dst in source_map.items():
        lldb_cmd(f'settings append target.source-map {src} {dst}')
    lldb_cmd('settings show target.source-map')

setup_target_source_map()
