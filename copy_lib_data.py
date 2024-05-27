import shutil
import configparser
import os

def copy_lib_data(source_dir, target_dir):
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
    for item in os.listdir(source_dir):
        s = os.path.join(source_dir, item)
        d = os.path.join(target_dir, item)
        if os.path.isdir(s):
            copy_lib_data(s, d)
        else:
            shutil.copy2(s, d)
            print(f"Copied file: {s} to {d}")

def read_ini_with_comments(filepath):
    with open(filepath, 'r') as file:
        lines = file.readlines()
    return lines

def write_ini_with_comments(filepath, lines):
    with open(filepath, 'w') as file:
        file.writelines(lines)

def merge_dependencies(source_ini, target_ini):
    # Read both ini files with comments preserved
    source_lines = read_ini_with_comments(source_ini)
    target_lines = read_ini_with_comments(target_ini)

    # Extract lib_deps from source
    source_lib_deps = []
    in_lib_deps = False
    for line in source_lines:
        if 'lib_deps' in line:
            in_lib_deps = True
        elif line.strip() == '' or line.strip().startswith('['):
            in_lib_deps = False
        if in_lib_deps and not line.strip().startswith('lib_deps'):
            source_lib_deps.append(line.strip())

    source_lib_deps = [dep for dep in source_lib_deps if dep]

    # Merge dependencies into all environments in the target ini
    in_lib_deps = False
    updated_lines = []
    for line in target_lines:
        if 'lib_deps' in line:
            in_lib_deps = True
            updated_lines.append(line)
            for dep in source_lib_deps:
                if dep not in updated_lines:
                    updated_lines.append(f"    {dep}\n")
        elif line.strip().startswith('['):
            in_lib_deps = False
            updated_lines.append(line)
        elif in_lib_deps and line.strip():
            if line.strip() not in [dep.strip() for dep in source_lib_deps]:
                updated_lines.append(line)
        else:
            updated_lines.append(line)

    # Write back to target ini with comments preserved
    write_ini_with_comments(target_ini, updated_lines)
    print(f"Merged dependencies from {source_ini} to {target_ini}")

# Paths
lib_dir = os.getcwd()  # Changed from os.path.dirname(os.path.realpath(__file__))
custom_lib_name = "common_lib_wifi_debugger"
source_data_path = os.path.join(lib_dir, "lib", custom_lib_name, 'data')
target_data_path = os.path.abspath(os.path.join(lib_dir, 'data'))

source_ini_path = os.path.join(lib_dir, "lib", custom_lib_name, 'platformio.ini')
target_ini_path = os.path.abspath(os.path.join(lib_dir, 'platformio.ini'))

# Copy data files from custom library to main project's data directory
print(f"Copying data from {source_data_path} to {target_data_path}")
copy_lib_data(source_data_path, target_data_path)

# Merge dependencies
print(f"Merging dependencies from {source_ini_path} to {target_ini_path}")
merge_dependencies(source_ini_path, target_ini_path)