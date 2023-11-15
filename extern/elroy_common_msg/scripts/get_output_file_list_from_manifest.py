import yaml
import sys
import os
from os.path import dirname, abspath, join, realpath, isdir
import elroy_common_msg_generator


def filename_from_manifest_entry(yaml_entry) -> str:
    # Given a yaml manifest entry for an auto-generated entity (either a
    # message, struct, or enum), extract the generated output file name.
    basename = yaml_entry['metadata']['file']['path']
    extension = yaml_entry['metadata']['file']['header']['extension']
    return basename + extension


def create_list_of_files_from_manifest(manifest_dir: str, manifest_file_name: str, entry_name: str) -> list:
    # Create a list of all generated output files names given a manifest file
    with open(join(manifest_dir, manifest_file_name)) as f:
        yml = yaml.load(f, Loader=yaml.FullLoader)
        return [filename_from_manifest_entry(entry[entry_name]) for entry in yml]


if __name__ == '__main__':
    # Grab the manifest directory location from the elroy_common_msg_generator script
    this_script_dir = dirname(realpath(__file__))
    manifest_dir = join(
        this_script_dir, elroy_common_msg_generator.gen_config["output"]["manifest"])
    generated_dir = join(
        this_script_dir, elroy_common_msg_generator.gen_config["output"]["root"], "cpp", "include")

    # If the generated directory does not exist, it could be that we just haven't
    # run code generation yet, so simply print the generated folder as the output
    # and exit
    if not isdir(generated_dir):
        print(generated_dir)
        sys.exit(0)

    # Sanity checks - ensure manifest dir exists and that it contains the expected number of files
    if not isdir(manifest_dir):
        raise NameError(
            "Manifest directory does not exist! Try running code generation. (I looked here: {})".format(manifest_dir))

    _, dirs, files = next(os.walk(manifest_dir))
    num_manifest_files = len(files)

    assert num_manifest_files == 4, "Expected to find exactly 4 manifest files, instead found {}!".format(
        num_manifest_files)

    generated_file_list = []

    # For each manifest file, add to the list of expected generated files
    generated_file_list.extend(create_list_of_files_from_manifest(
        manifest_dir, 'bus_object_schema_final.yaml', 'class'))
    generated_file_list.extend(create_list_of_files_from_manifest(
        manifest_dir, 'enumerates_schema_final.yaml', 'enum'))
    generated_file_list.extend(create_list_of_files_from_manifest(
        manifest_dir, 'structs_schema_final.yaml', 'struct'))

    with open(join(manifest_dir, 'msg_handling_schema_final.yaml')) as f:
        yml = yaml.load(f, Loader=yaml.FullLoader)
        generated_file_list.append(filename_from_manifest_entry(
            yml['msg_handling']['enum']))
        for entry in yml['msg_handling']['msgs']:
            generated_file_list.append(filename_from_manifest_entry(
                entry['class']))

    # prepend each entry with the generated path to create a full path,
    # and convert to a set() to remove duplicates
    generated_file_list_full_path = set()
    for file in generated_file_list:
        generated_file_list_full_path.add(
            abspath(join(generated_dir, file)))

    # Print a semicolon-delimited list to the console (for use e.g. by CMake)
    print(';'.join(generated_file_list_full_path))

    sys.exit(0)
