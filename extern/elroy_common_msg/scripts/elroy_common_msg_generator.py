import abc
import argparse
import copy
import os
import re
import sys
from os.path import abspath, dirname, join
from pathlib import Path
from shutil import copyfile
from typing import List, Type

import jinja2
import schema_validator
import yaml
from beeprint import pp
from logger import Logger
from python_generator.generate_constructs import ElroyCommonMsgGenerator

"""
bcolors is helpful colors for pretty terminal output
see:
  https://stackoverflow.com/questions/287871/how-to-print-colored-text-to-the-terminal
"""


class bcolors:
    HEADER = "\033[95m"
    OKBLUE = "\033[94m"
    OKCYAN = "\033[96m"
    OKGREEN = "\033[92m"
    WARNING = "\033[93m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"


class MessageGeneratorHook:
    @abc.abstractmethod
    def handle_enums(self, general_config, enum_model, jinja_env: jinja2.Environment):
        pass

    @abc.abstractmethod
    def handle_structs(
        self, general_config, struct_model, jinja_env: jinja2.Environment
    ):
        pass

    @abc.abstractmethod
    def handle_messages(
        self, general_config, message_model, jinja_env: jinja2.Environment
    ):
        pass

    @abc.abstractmethod
    def auxiliary(
        self,
        general_config,
        enum_model,
        struct_model,
        message_model,
        jinja_env: jinja2.Environment,
    ):
        pass


class FlightSoftwareGeneratorHook(MessageGeneratorHook):
    """Generate safe messages for flight software and other safety critical areas."""

    def __init__(self) -> None:
        super().__init__()
        Logger.config(None, True, "DEBUG")
        self.log = Logger.get_logger("FlightSoftwareGeneratorHook")

    def handle_enums(self, general_config, enum_model, jinja_env: jinja2.Environment):
        """
        @brief generates enumerates
        @note output path defaults from `general_config` for cpp + the enum's metadata file path
        """
        enum_template = jinja_env.get_template(general_config["templates"]["enums"])
        for i, enum in enumerate(enum_model):
            enum_output = enum_template.render(enum)

            enum_file_path = (
                general_config["output"]["cpp"]
                + enum["enum"]["metadata"]["file"]["path"]
                + ".h"
            )

            # create directories if needed
            if not Path(os.path.dirname(enum_file_path)).exists():
                Path(os.path.dirname(enum_file_path)).mkdir(parents=True)

            self.log.info(
                "   - "
                + enum["enum"]["name"]
                + " enum cpp header to: "
                + enum_file_path
            )

            enum_file = open(enum_file_path, "w+")
            enum_file.write(enum_output)
            enum_file.close()

    def handle_structs(
        self, general_config, struct_model, jinja_env: jinja2.Environment
    ):
        """
        @brief generates structs
        @note output path defaults from `general_config` for cpp + the struct's metadata file path
        """
        struct_template = jinja_env.get_template(general_config["templates"]["structs"])

        # Add metadata to help detect structures within structures.
        struct_types = [struct["struct"]["name"] for struct in struct_model]

        for i, struct in enumerate(struct_model):
            struct["struct_types"] = struct_types
            struct_output = struct_template.render(struct)

            struct_file_path = (
                general_config["output"]["cpp"]
                + struct["struct"]["metadata"]["file"]["path"]
                + ".h"
            )

            # create directories if needed
            if not Path(os.path.dirname(struct_file_path)).exists():
                Path(os.path.dirname(struct_file_path)).mkdir(parents=True)

            self.log.info(
                "   - "
                + struct["struct"]["name"]
                + " struct cpp header to: "
                + struct_file_path
            )

            struct_file = open(struct_file_path, "w+")
            struct_file.write(struct_output)
            struct_file.close()

    def handle_messages(self, general_config, msg_model, jinja_env: jinja2.Environment):
        """
        @brief generates messages
        @note output path defaults
        """
        header_template = jinja_env.get_template(gen_config["templates"]["msgs"])

        crc_output_path = (
            general_config["output"]["cpp"] + general_config["output"]["crc"]
        )
        msgs_output_path = (
            general_config["output"]["cpp"] + general_config["output"]["msgs"]
        )

        # create generated folders if necessary
        if not Path(os.path.dirname(msgs_output_path)).exists():
            Path(os.path.dirname(msgs_output_path)).mkdir(parents=True)

        # copy base class into gen folder
        copyfile(
            general_config["templates"]["bus_object_message"],
            msgs_output_path + "bus_object_message.h",
        )

        # create crc directory
        if not Path(os.path.dirname(crc_output_path)).exists():
            Path(os.path.dirname(crc_output_path)).mkdir(parents=True)
        # copy crc file
        copyfile(
            general_config["templates"]["crc_calculator"],
            crc_output_path + "c_crc_calculator.h",
        )

        for i, msg in enumerate(msg_model):
            # skip BusObject
            if "BusObject" in msg["class"]["name"]:
                continue

            # create header and source
            header_output = header_template.render(msg)

            header_file_path = (
                general_config["output"]["cpp"]
                + msg["class"]["metadata"]["file"]["path"]
                + ".h"
            )

            # create directories if needed
            if not Path(os.path.dirname(header_file_path)).exists():
                Path(os.path.dirname(header_file_path)).mkdir(parents=True)

            self.log.info(
                "   - "
                + msg["class"]["name"]
                + "Message cpp header to: "
                + header_file_path
            )

            header_file = open(header_file_path, "w+")
            header_file.write(header_output)
            header_file.close()

    def auxiliary(
        self,
        general_config,
        enum_model,
        struct_model,
        message_model,
        jinja_env: jinja2.Environment,
    ):
        self.log.info("Processing Msg Handling...")
        msg_handling = process_msg_handling(message_model, enum_model)
        self.log.info(f"{bcolors.OKGREEN}done{bcolors.ENDC}")

        # load templates
        msg_handler_template = jinja_env.get_template(
            gen_config["templates"]["msg_handler"]
        )
        msg_decoder_template = jinja_env.get_template(
            gen_config["templates"]["msg_decoder"]
        )
        msg_handling_ut_template = jinja_env.get_template(
            gen_config["templates"]["msg_handling_ut"]
        )

        self.gen_msg_handling(
            general_config,
            msg_handling,
            msg_handler_template,
            msg_decoder_template,
            msg_handling_ut_template,
        )

        self.log.info("\nGenerating Python...")
        gen_py_msg(struct_model, enum_model, message_model)
        self.log.info(f"{bcolors.OKGREEN}done{bcolors.ENDC}")

        self.log.info("\nSaving Schema Manifests...")
        save_manifest(message_model, struct_model, enum_model, msg_handling)
        self.log.info(f"{bcolors.OKGREEN}done{bcolors.ENDC}")

    def gen_msg_handling(
        self,
        general_config,
        msg_handling,
        handler_template,
        decoder_template,
        handler_ut_template,
    ):
        """
        @brief generates msg handler interface and decoder and handling ut
        """
        msg_handling_path = (
            general_config["output"]["cpp"] + general_config["output"]["msg_handling"]
        )
        handling_ut_path = general_config["output"]["cpp_ut"]

        if not Path(os.path.dirname(msg_handling_path)).exists():
            Path(os.path.dirname(msg_handling_path)).mkdir(parents=True)

        if not Path(os.path.dirname(handling_ut_path)).exists():
            Path(os.path.dirname(handling_ut_path)).mkdir(parents=True)

        handler_f = open(
            msg_handling_path + general_config["output"]["msg_handler"], "w+"
        )
        decoder_f = open(
            msg_handling_path + general_config["output"]["msg_decoder"], "w+"
        )
        handling_ut_f = open(
            handling_ut_path + general_config["output"]["msg_handling_ut"], "w+"
        )

        handler_output = handler_template.render(msg_handling)
        decoder_output = decoder_template.render(msg_handling)
        handling_ut_output = handler_ut_template.render(msg_handling)

        handler_f.write(handler_output)
        decoder_f.write(decoder_output)
        handling_ut_f.write(handling_ut_output)

        handler_f.close()
        decoder_f.close()
        handling_ut_f.close()


elroy_common_msg_dir = dirname(dirname(abspath(__file__)))
schema_model_path = join(
    elroy_common_msg_dir, "scripts/templates/cpp/schema_model.yaml"
)

schema_model_f = open(schema_model_path)
schema_model_ = yaml.load(schema_model_f, Loader=yaml.FullLoader)

gen_config = {
    # use add_hooks to add hooks. In the main function area, FlightSoftwareGeneratorHook is added.
    "hooks": [],
    "input": {
        "msgs": "../src/bus_object_schema.yaml",
        "enums": "../src/enumerates_schema.yaml",
        "structs": "../src/structs_schema.yaml",
    },
    "templates": {
        # dictionary of template info: names of files, indexes within schema model
        "root_dir": "./templates",
        "schema_model": {"msg_index": 0, "struct_index": 1, "enum_index": 2},
        "msgs": "./cpp/message_header.jinja2",
        "enums": "./cpp/enumerates.jinja2",
        "structs": "./cpp/structs_header.jinja2",
        "bus_object_message": "./templates/cpp/bus_object_message.h",
        "crc_calculator": "./templates/cpp/c_crc_calculator.h",
        "msg_handler": "./cpp/msg_handler.jinja2",
        "msg_decoder": "./cpp/msg_decoder.jinja2",
        "msg_handling_ut": "./cpp/msg_handling_ut.jinja2",
        "enum_utility": "./templates/cpp/c_enum_utility.h",
    },
    "output": {
        # generated output config, relative to the scripts location
        "root": "../generated/",
        "manifest": "../generated/manifest/",
        "cpp": "../generated/cpp/include/",
        "cpp_ut": "../generated/cpp/unit_tests/",
        "py": "../generated/py/",
        "msgs": "elroy_common_msg/",
        "structs": "elroy_common_msg/structs/",
        "enums": "elroy_common_msg/enums/",
        "utility": "elroy_common_msg/utility/",
        "crc": "elroy_common_msg/crc/",
        "msg_handling": "elroy_common_msg/msg_handling/",
        "msg_handler": "msg_handler.h",
        "msg_decoder": "msg_decoder.h",
        "msg_handling_ut": "msg_handling_ut.cc",
        "utility_yaml": "../generated/utility/",
    },
}


def split_camel_case(string) -> list:
    """
    @brief helper function to split camel case into a list
    @note i.e. SplitCamelCase -> ['Split', 'Camel', 'Case']
    @param[in] string - some string that has camel case
    @return list of string split by camel case
    """
    matches = re.finditer(
        ".+?(?:(?<=[a-z])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])|$)", string
    )
    return [m.group(0) for m in matches]


def get_file_name(camel_case_name) -> str:
    """
    @brief takes in list of string, spits out a lowercase
           underscore-sperated string
    @note i.e. ['Split', 'Camel', 'Case'] -> split_camel_case
    @param[in] camel_case_name - list of strings
    @return lowercase underscore-seperated string
    """
    file_name_list = split_camel_case(camel_case_name)
    file_name = ""
    for i, var in enumerate(file_name_list):
        if i + 1 == len(file_name_list):
            file_name += var.lower()
        else:
            file_name += var.lower() + "_"
    return file_name


def get_header_guard(file_path) -> str:
    """
    @brief creates the include guard for this file
    @param[in] file_path - the file path (underscore)
    @return the header guard string
    """
    files = file_path.split("/")
    guard = "INCLUDE"
    for f in files:
        guard = guard + "_" + f.upper()
    guard += "_H_"
    return guard


def calc_enum_type(enum_length) -> str:
    """
    @brief calcualtes the enum type based on the length
    @param[in] enum_length - the length of the enumerate
    @return string of it's fixed-width type
    """
    # print("Enum length: " + str(enum_length))
    if enum_length < 0xFF:
        return "uint8_t"
    elif enum_length < 0xFFFF:
        return "uint16_t"
    elif enum_length < 0xFFFFFFFF:
        return "uint32_t"


def compare_enum_type_size(type_a, type_b):
    """
    @brief Ensures type_a fits within type_b (type_a <= type_b)
    @param[in] type_a a fixed-width integer type (string)
    @param[in] type_b a fixed-width integer type (string)
    @return True if A fits in B
    """
    tmp = {
        "uint8_t": 0xFF,
        "uint16_t": 0xFFFF,
        "uint32_t": 0xFFFFFFFF,
        "uint64_t": 0xFFFFFFFFFFFFFFFF,
    }
    return tmp[type_a] <= tmp[type_b]


def validate_fixed_width_types(data_type) -> bool:
    """
    @brief validates if the enum data type is a fixed-width integer
    @param[in] data_type string of the data type
    @return true if it's a fixed-width integer
    """
    if data_type not in (
        "uint8_t",
        "uint16_t",
        "uint32_t",
        "uint64_t",
        "int8_t",
        "int16_t",
        "int32_t",
        "int64_t",
    ):
        return False
    return True


def validate_number_types(data_type) -> bool:
    """
    @brief validates if the data type is a fixed-width integer, float,
           or double
    @param[in] data_type string of the data type
    @return true if it's fixed-width integer, float, double, or bool;
            else false
    """
    # first check fixed-width integers
    if False is validate_fixed_width_types(data_type):
        # now try float, double, bool
        if data_type not in ("float", "double", "bool"):
            return False
    return True


def validate_schema(schema_file, schema_type, schema):
    """
    @brief This function validates the schema for a message (i.e. the
           user's schema is well-formed). It does NOT check the integrity
           of what the user has entered
    @param[in] schema_file - which file originates the schema (for printout)
    @param[in] schema_type - 'class', 'struct', or 'enum'
    @param[in] schema - list of schema dictionaries to validate
    @note Script will exit on error
    @return None
    """
    got_errors = False
    for i, uut in enumerate(schema):
        validity, errors = schema_validator.validate_schema(schema_type, uut)
        if validity is False:
            got_errors = True
            print("\n\nError in entry #" + str(i) + " ", end="")

            if "class" in uut:
                if "name" in uut["class"]:
                    print(uut["class"]["name"])
            pp(errors)

    print("   " + schema_file + ": ", end="")
    if got_errors:
        print(f"{bcolors.FAIL}invalid{bcolors.ENDC}")
    else:
        print(f"{bcolors.OKGREEN}valid{bcolors.ENDC}")


def check_for_duplicate_entries(schema_file, schema_type, schema):
    """
    @brief This function checks to see if the schema file contains any duplicate
           entries and asserts if so
    @param[in] schema_file - which file originates the schema (for printout)
    @param[in] schema_type - 'class', 'struct', or 'enum'
    @param[in] schema - list of schema dictionaries to validate
    @return None
    """
    names = [entry[schema_type]["name"] for entry in schema]
    duplicate_names = {name for name in names if names.count(name) > 1}
    assert (
        not duplicate_names
    ), f"{schema_file} contains duplicate entries: {duplicate_names}"


def process_msgs(msgs) -> dict:
    """
    @brief processes the message schema by:
            1. creates equivalent simple struct and puts into `msg_struct` list
            2. creates list of enumerate "message ids" for return; default uint16_t
            3. calculates file metadata: file name, path, include guard
            4. Checks if this class is a base class (i.e. has children) and sets
               metadata "inherit::children" to True
            5. Iterates through all members to check for arrays, if array exists
               the size is extracted and placed in the "members" metadata section
            6. Copies modified message schema (metadata) back to original schema
            7. Returns enumerate and struct list
    @return dict of enumerate ids, to be appended to enum_schema
    """
    class_index = gen_config["templates"]["schema_model"]["msg_index"]
    class_meta = schema_model_[class_index]["class"]["metadata"]

    # create enum dictionary
    msg_id_enum = {"enum": {"name": "MessageId", "type": "uint16_t", "list": []}}

    # create msg structs
    msg_structs = []

    for i, src in enumerate(msgs):
        # create local copy for processing
        msg = src["class"]

        # add simple struct to structs list
        # don't create a busObject struct
        if msg["name"] not in "BusObject":
            implied_struct = {
                "struct": {
                    "name": msg["name"],
                    "inherit": msg["inherit"],
                }
            }

            # inherit the namespace if it exists
            if "namespace" in msg:
                implied_struct["struct"]["namespace"] = msg["namespace"]

            msg_structs.append(implied_struct)

        # create empty meta_data section
        msg["metadata"] = {"file": {}, "inherit": {}, "members": []}

        file_meta = class_meta["file"]
        inherit_meta = class_meta["inherit"]
        member_meta = []

        # save off class name in enum list
        msg_id_enum["enum"]["list"].append(msg["name"])

        # process file stuff
        file_meta["name"] = get_file_name(msg["name"]) + "_message"
        file_meta["path"] = gen_config["output"]["msgs"] + file_meta["name"]
        file_meta["header"]["include_guard"] = get_header_guard(file_meta["path"])

        # clear includes list to ensure empty list (schema model has a few list entries of 'None')
        file_meta["header"]["includes"].clear()

        # loop through and see if this msg has any children
        inherit_meta["children"] = False
        for j, child_src in enumerate(msgs):
            child = child_src["class"]
            if "inherit" in child:
                if msg["name"] == child["inherit"]["name"]:
                    inherit_meta["children"] = True

        # loop through member data to tidy up
        # members is a list of dictionaries
        if "members" in msg:
            # index is i - 1 because busObject is not included
            if msg["name"] != "BusObject":
                msg_structs[i - 1]["struct"]["members"] = msg["members"].copy()
            for j, member in enumerate(msg["members"]):
                # straight copy member data to meta
                member_meta.append(member.copy())
                if "[" in member["type"]:
                    array_len = re.search(
                        r"\[([+-A-Za-z0-9_]+)\]", member["type"]
                    ).group(1)

                    # validate array size is:
                    #   1) an int
                    #   2) > 0
                    if array_len.isdigit() is False:
                        sys.exit(
                            "ERROR! Expecting integer (no sign) for "
                            + msg["name"]
                            + "::"
                            + member["name"]
                            + "[] array size. Got: "
                            + str(array_len)
                        )
                    else:
                        if int(array_len) <= 0:
                            sys.exit(
                                "ERROR! "
                                + msg["name"]
                                + "::"
                                + member["name"]
                                + " array size must be > 0. Got: "
                                + str(len)
                            )

                    # remove array syntax from type
                    member_meta[j]["type"] = member_meta[j]["type"].split("[")[0]
                    # add array_len key, vlaue
                    member_meta[j]["array_len"] = int(array_len)

        # place metadata back in class dictionary
        msg["metadata"]["file"].update(file_meta)
        msg["metadata"]["inherit"].update(inherit_meta)
        msg["metadata"]["members"] = member_meta

        # update original
        msgs[i]["class"] = copy.deepcopy(msg)

    return msg_id_enum, msg_structs


def process_structs(structs):
    """
    @brief iterates through the structs schema and:
            1. sets up file metadata; file name, path, include guard
            2. for all data members:
                a. checks for array, extracts array length
                b. adds member data to metadata section
            3. Copies struct back to original schema
    """
    struct_index = gen_config["templates"]["schema_model"]["struct_index"]
    struct_meta = schema_model_[struct_index]["struct"]["metadata"]

    for i, src in enumerate(structs):
        # create local copy for processing
        struct = src["struct"]

        # create empty meta_data section
        struct["metadata"] = {"file": {}, "members": []}

        # tmp metadata for less array subscript
        file_meta = struct_meta["file"]
        member_meta = []

        # process file stuff
        file_meta["name"] = get_file_name(struct["name"])
        file_meta["path"] = gen_config["output"]["structs"] + file_meta["name"]
        file_meta["header"]["include_guard"] = get_header_guard(file_meta["path"])

        # clear includes list to ensure empty list (schema model has a few list entries of 'None')
        file_meta["header"]["includes"].clear()

        # loop through member data to tidy up
        # members is a list of dictionaries
        if (
            ("members" in struct)
            and (struct["members"])
            and (0 < len(struct["members"]))
        ):
            for j, member in enumerate(struct["members"]):
                # straight copy member data to meta
                member_meta.append(member.copy())
                if "[" in member["type"]:
                    array_len = re.search(
                        r"\[([+-A-Za-z0-9_]+)\]", member["type"]
                    ).group(1)

                    # validate array size is:
                    #   1) an int
                    #   2) > 0
                    if array_len.isdigit() is False:
                        sys.exit(
                            "ERROR! Expecting integer (no sign) for "
                            + struct["name"]
                            + "::"
                            + member["name"]
                            + "[] array size. Got: "
                            + str(array_len)
                        )
                    else:
                        if int(array_len) <= 0:
                            sys.exit(
                                "ERROR! "
                                + struct["name"]
                                + "::"
                                + member["name"]
                                + " array size must be > 0. Got: "
                                + str(len)
                            )

                    # remove array syntax from type
                    member_meta[j]["type"] = member_meta[j]["type"].split("[")[0]
                    # add array_len key, vlaue
                    member_meta[j]["array_len"] = int(array_len)

        # place metadata back in class dictionary
        struct["metadata"]["file"] = file_meta
        struct["metadata"]["members"] = member_meta

        # update original
        structs[i]["struct"] = copy.deepcopy(struct)


def process_enums(enum_schema):
    """
    @brief processes the enum schema by:
            1. Setting file metadata; file name, path, include guard
            2. Calculates enumerate type (size) from list length
            3. If optional "type" is included in schema, compares
               the calculated length with entered
            4. Validates all enumerates in list are unique
            5. Copies enum back to original schema
    """
    enum_index = gen_config["templates"]["schema_model"]["enum_index"]
    enum_meta = schema_model_[enum_index]["enum"]["metadata"]

    for i, src in enumerate(enum_schema):
        # local copy for processing
        enum = src["enum"]

        # create empty meta data section
        enum["metadata"] = {"file": {}}

        file_meta = enum_meta["file"]

        # process file stuff
        file_meta["name"] = get_file_name(enum["name"])
        file_meta["path"] = gen_config["output"]["enums"] + file_meta["name"]
        file_meta["header"]["include_guard"] = get_header_guard(file_meta["path"])

        # clear includes list to ensure empty list (schema model has a few list entries of 'None')
        # note: includes is not necessary for enumerates
        file_meta["header"]["includes"].clear()

        # See if user has entered a type, if they have make sure it's sized correctly
        enum_size = len(enum["list"])
        enum_type = calc_enum_type(enum_size)
        if "type" in enum:
            if False is validate_fixed_width_types(enum["type"]):
                sys.exit(
                    "ERROR! Expecting fixed-width integer enumerate type (u/int<8,16,32,64>_t) but got "
                    + enum["type"]
                )
            if False is compare_enum_type_size(enum_type, enum["type"]):
                sys.exit(
                    "ERROR! Enumerate: "
                    + enum["name"]
                    + " type does not fit list. Calculated: "
                    + str(enum_size)
                    + " but got "
                    + enum["type"]
                )
        else:
            enum["type"] = enum_type

        # validate unique enumerate list
        if False is (len(enum["list"]) == len(set(enum["list"]))):
            sys.exit(
                "Error! " + enum["name"] + " has duplicate entries in enumerate list"
            )

        # validate optional string conversion lists match the enumerate list length
        if False is (
            ("brief_list" not in enum) or (len(enum["list"]) == len(enum["brief_list"]))
        ):
            sys.exit(
                "Error! "
                + enum["name"]
                + " defined brief_list which does not match list length"
            )

        if False is (
            ("elaboration_list" not in enum)
            or (len(enum["list"]) == len(enum["elaboration_list"]))
        ):
            sys.exit(
                "Error! "
                + enum["name"]
                + " defined elaboration_list which does not match list length"
            )

        # update metadata
        enum["metadata"]["file"].update(file_meta)

        # update original
        enum_schema[i]["enum"] = copy.deepcopy(enum)


def process_msg_handling(msgs, enums):
    """
    @brief this function creates a `msg_handling` dictionary with:
            1. a copy of `MessageId` enum from enums
            2. copies all paths to all messages
    @note the shape of this dictionary is expected for
          msg_decoder.jinja2, msg_handler.jinja2
    @return msg_handling dictionary
    """
    msg_handling = {
        "msg_handling": {
            "msgs": [],
            "enum": None,
            "metadata": {"file": {"header": {"includes": []}}},
        }
    }
    # iterate through msgs, copy include path to all
    for msg in msgs:
        if "BusObject" == msg["class"]["name"]:
            continue
        # msg_handling["msg_handling"]["metadata"]["file"]["header"]["includes"].append(msg["class"]["metadata"]["file"]["path"] + ".h")
        msg_handling["msg_handling"]["msgs"].append(msg)

    for enum in enums:
        if "MessageId" == enum["enum"]["name"]:
            msg_handling["msg_handling"]["enum"] = copy.deepcopy(enum["enum"])
            # remove the first entry: BusObject
            msg_handling["msg_handling"]["enum"]["list"].remove("BusObject")

    return msg_handling


def link_schemas(msgs, structs, enums):
    for i, msg_src in enumerate(msgs):
        # copy for processing
        msg = msg_src["class"]

        # skip base class
        if msg["name"] in "BusObject":
            continue

        # if this class has a parent, get its include path
        if "inherit" in msg:
            for j, parent in enumerate(msgs):
                # find parent and add include path to list of includes
                if msg["inherit"]["name"] == parent["class"]["name"]:
                    msg["metadata"]["file"]["header"]["includes"].append(
                        parent["class"]["metadata"]["file"]["path"] + ".h"
                    )

        # validate data member types
        if "members" in msg:
            for j, member in enumerate(msg["metadata"]["members"]):
                # First, check if it's a valid number type
                if not validate_number_types(member["type"]):
                    # If not a number-type, check if an enum
                    type_found = False
                    for enum_idx, enum in enumerate(enums):
                        if member["type"] == enum["enum"]["name"]:
                            type_found = True
                            # print(msg["name"] + "data member " + member["name"] + "has enum type " + member["type"])
                            msg["metadata"]["file"]["header"]["includes"].append(
                                enum["enum"]["metadata"]["file"]["path"] + ".h"
                            )
                            msg["metadata"]["members"][j]["is_enum"] = True

                    # not an enum? Check structs
                    if not type_found:
                        for struct_idx, struct in enumerate(structs):
                            if member["type"] == struct["struct"]["name"]:
                                type_found = True
                                # save off struct include info
                                msg["metadata"]["file"]["header"]["includes"].append(
                                    struct["struct"]["metadata"]["file"]["path"] + ".h"
                                )
                                # save that this is a struct (for Pack/Unpacking)
                                msg["metadata"]["members"][j]["is_struct"] = True

                    # not found? Error
                    if not type_found:
                        sys.exit(
                            "ERROR! Invalid data type for "
                            + msg["name"]
                            + "::"
                            + member["name"]
                            + ", received: "
                            + str(member["type"])
                        )

            # add include path to this message's basic struct
            for j, struct_src in enumerate(structs):
                if msg["name"] in struct_src["struct"]["name"]:
                    msg["metadata"]["file"]["header"]["includes"].append(
                        struct_src["struct"]["metadata"]["file"]["path"] + ".h"
                    )

        else:
            # Handle edge case for message which has no elements but still needs include path to its basic struct
            msg["metadata"]["file"]["header"]["includes"].append(
                gen_config["output"]["structs"] + get_file_name(msg["name"]) + ".h"
            )

    # loop through structs and validate types
    for i, struct_src in enumerate(structs):
        # copy for processing
        struct = struct_src["struct"]

        # get the parent include path and ensure a common base class for all structs
        if struct["name"] not in "BusObjectStruct":
            if "inherit" not in struct:
                struct["inherit"] = {}
                struct["inherit"]["name"] = ""

            if (not struct["inherit"]["name"]) or (
                struct["inherit"]["name"] in "BusObject"
            ):
                struct["inherit"][
                    "name"
                ] = "BusObjectStruct"  # matches BusObjectStruct in structs_schema.yaml

            for j, parent in enumerate(structs):
                # find parent and add include path to list of includes
                if struct["inherit"]["name"] == parent["struct"]["name"]:
                    struct["metadata"]["file"]["header"]["includes"].append(
                        parent["struct"]["metadata"]["file"]["path"] + ".h"
                    )
            for j, member in enumerate(struct["metadata"]["members"]):
                # First, check if it's a valid number type
                if not validate_number_types(member["type"]):
                    # If not a number-type, check if an enum
                    type_found = False
                    for enum_idx, enum in enumerate(enums):
                        if member["type"] == enum["enum"]["name"]:
                            type_found = True
                            struct["metadata"]["file"]["header"]["includes"].append(
                                enum["enum"]["metadata"]["file"]["path"] + ".h"
                            )
                            struct["metadata"]["members"][j]["is_enum"] = True

    # validate data member types
    # note: for weird reasons must iterate through this struct again
    for i, struct_src in enumerate(structs):
        # copy for processing
        struct = struct_src["struct"]

        if "members" in struct.keys():
            for j, member in enumerate(struct["metadata"]["members"]):
                # First, check if it's a valid number type
                if not validate_number_types(member["type"]):
                    # If not a number-type, check if an enum
                    type_found = False
                    for enum_idx, enum in enumerate(enums):
                        if member["type"] == enum["enum"]["name"]:
                            type_found = True
                            struct["metadata"]["file"]["header"]["includes"].append(
                                enum["enum"]["metadata"]["file"]["path"] + ".h"
                            )

                    # not an enum? Check structs
                    if not type_found:
                        for struct_idx, struct_src in enumerate(structs):
                            if member["type"] == struct_src["struct"]["name"]:
                                type_found = True
                                struct["metadata"]["file"]["header"]["includes"].append(
                                    struct_src["struct"]["metadata"]["file"]["path"]
                                    + ".h"
                                )
                                # save that this is a struct (for Pack/Unpacking)
                                struct["metadata"]["members"][j]["is_struct"] = True

                    # not found? Error
                    if not type_found:
                        sys.exit(
                            "ERROR! Invalid data type for "
                            + struct["name"]
                            + "::"
                            + member["name"]
                            + ", received: "
                            + str(member["type"])
                        )


def gen_py_msg(structs, enums, msg_schema):
    """
    @brief Constructs the python messages using the ElroyCommonMsgGenerator
    @return None
    """
    generator = ElroyCommonMsgGenerator([structs, enums, msg_schema])
    output_dir = os.path.join(gen_config["output"]["py"], "elroy_common_msg")
    generator.write_generated_files(output_dir=output_dir)


def write_rendered_template_to_file(file_path, file_contents):
    """
    @brief Given a valid file path, it opens the file and writes the file_contents to this
    file.
    @return None
    """
    with open(file_path, "w+") as file_handle:
        file_handle.write(file_contents)
        file_handle.close()


def flatten_list(list_val):
    """
    @brief Flattens a nested list. For e.g. [[a], [b]] --> [a, b]
    @return list: Flattened list
    """
    return [item for sublist in list_val for item in sublist]


def get_enums(enums, target_name):
    """
    @brief Helper method that returns a list of enum objects where the name matches
    the target_name.
    @return list(dict()) A list containing enum objects
    """
    return [
        enum["enum"]["name"] for enum in enums if enum["enum"]["name"] == target_name
    ]


def get_structs(structs, target_name):
    """
    @brief Helper method that returns a list of struct objects where the name matches
    the target_name.
    @return list(dict()) A list containing struct objects
    """
    structs_list = []
    for elem in structs:
        if elem["struct"]["name"] == target_name:
            structs_list.append(elem["struct"])
            for member in elem["struct"]["metadata"]["members"]:
                if "is_struct" in member.keys():
                    return get_structs(structs, member["type"])
    return structs_list


def get_python_compatible_type(member) -> str:
    """
    @brief Helper method that converts the C++ type associated with member to a Python
    compatible type.
    @return str: If a compatible type is found
    @return None:   If no compatible type is found
    """
    python_type = None
    cpp_to_python_type_map = {
        "int8_t": "Int8sl",
        "uint8_t": "Int8ul",
        "uint16_t": "Int16ul",
        "int16_t": "Int16sl",
        "uint32_t": "Int32ul",
        "int32_t": "Int32sl",
        "uint64_t": "Int64ul",
        "int64_t": "Int64sl",
        "float": "Float32l",
        "double": "Float64l",
        "bool": "Flag",
    }
    type_name = member["type"]
    # handle primitive or enum type
    if "array_len" not in member.keys():
        if type_name in cpp_to_python_type_map:
            python_type = cpp_to_python_type_map[type_name]
    # handle array type
    else:
        if type_name in cpp_to_python_type_map:
            python_type = (
                "Array("
                + str(member["array_len"])
                + ","
                + cpp_to_python_type_map[type_name]
                + ")"
            )

    return python_type


def add_enumerates_conversion_maps(enums):
    """
    @brief creates an enumerates schema as follows:
           adds two additional map nodes to the enumerate manifest:
            1. enum_to_value:
                Enum Name (string):
                  "Enumerator" : Value (int)
            2. enum_to_string:
                Enum Name (string):
                  - "Enumerator"
    """
    if not Path(os.path.dirname(gen_config["output"]["utility_yaml"])).exists():
        Path(os.path.dirname(gen_config["output"]["utility_yaml"])).mkdir(parents=True)

    enum_to_string_out = open(
        gen_config["output"]["utility_yaml"] + "enum_to_string.yaml", "w+"
    )
    enum_to_value_out = open(
        gen_config["output"]["utility_yaml"] + "enum_to_value.yaml", "w+"
    )
    enum_to_value = {}
    enum_to_string = {}

    # iterate through list of "enum" maps
    for enum in enums:
        name = enum["enum"]["name"]

        # add Unknown = 0 standard
        e_map = {"Unknown": 0}
        e_list = ["Unknown"]  # the index is value

        # next value starts at 1
        enum_value = 1

        # iterate over "enum"'s list of define enumerates
        for e in enum["enum"]["list"]:
            # add to name lookup
            e_list.append(str(e))

            # add to value lookup
            e_map[str(e)] = enum_value

            # increment index
            enum_value += 1

        # post-pend standard "Count = N+1"
        e_map["Count"] = enum_value
        e_list.append("Count")

        # add temp dictionary to output library
        enum_to_value[name] = e_map.copy()
        enum_to_string[name] = e_list.copy()

        e_map.clear()
        e_list.clear()

    # dump to manifest
    yaml.dump(enum_to_value, enum_to_value_out)
    yaml.dump(enum_to_string, enum_to_string_out)

    # copy enum utility
    output_path = gen_config["output"]["cpp"] + gen_config["output"]["utility"]
    print("The output path: " + output_path)
    if not Path(os.path.dirname(output_path)).exists():
        Path(os.path.dirname(output_path)).mkdir(parents=True)

    copyfile(gen_config["templates"]["enum_utility"], output_path + "c_enum_utility.h")

    enum_to_string_out.close()
    enum_to_value_out.close()


def save_manifest(msgs, structs, enums, msg_handling):
    """
    @brief saves the final schema as an artifact `generated/manifest`
    @note yaml.dump orders the yaml alphabetically by key (i.e. it won't look
          exactly like the input yaml). This will need to be fixed
    @return none
    """
    # create directories if needed
    if not Path(os.path.dirname(gen_config["output"]["manifest"])).exists():
        Path(os.path.dirname(gen_config["output"]["manifest"])).mkdir(parents=True)

    msg_manifest_f = open(
        gen_config["output"]["manifest"] + "bus_object_schema_final.yaml", "w+"
    )
    structs_manifest_f = open(
        gen_config["output"]["manifest"] + "structs_schema_final.yaml", "w+"
    )
    enum_manifest_f = open(
        gen_config["output"]["manifest"] + "enumerates_schema_final.yaml", "w+"
    )
    msg_handling_f = open(
        gen_config["output"]["manifest"] + "msg_handling_schema_final.yaml", "w+"
    )

    yaml.dump(msgs, msg_manifest_f)
    yaml.dump(enums, enum_manifest_f)
    yaml.dump(structs, structs_manifest_f)
    yaml.dump(msg_handling, msg_handling_f)

    add_enumerates_conversion_maps(enums)

    msg_manifest_f.close()
    structs_manifest_f.close()
    enum_manifest_f.close()
    msg_handling_f.close()


def add_hooks(hook_classes: List[Type[MessageGeneratorHook]]) -> None:
    if type(hook_classes) is not list:
        hook_classes = [hook_classes]

    gen_config["hooks"] += hook_classes


def run_generators() -> int:
    # print header
    print("\n------------------[elroy_common_msg_generator.py]---------------------")
    # open files
    msg_file = open(gen_config["input"]["msgs"])
    enum_file = open(gen_config["input"]["enums"])
    struct_file = open(gen_config["input"]["structs"])

    # load yamls
    msg_schema = yaml.load(msg_file, Loader=yaml.FullLoader)
    enum_schema = yaml.load(enum_file, Loader=yaml.FullLoader)
    struct_schema = yaml.load(struct_file, Loader=yaml.FullLoader)

    # create jinja environment
    file_loader = jinja2.FileSystemLoader(
        searchpath=gen_config["templates"]["root_dir"]
    )
    jinja_env = jinja2.Environment(
        loader=file_loader, lstrip_blocks=True, trim_blocks=True
    )

    print("Validating schemas...")  # new line for output readability
    validate_schema(gen_config["input"]["msgs"], "class", msg_schema)
    validate_schema(gen_config["input"]["enums"], "enum", enum_schema)
    validate_schema(gen_config["input"]["structs"], "struct", struct_schema)
    check_for_duplicate_entries(gen_config["input"]["msgs"], "class", msg_schema)
    check_for_duplicate_entries(gen_config["input"]["enums"], "enum", enum_schema)
    check_for_duplicate_entries(gen_config["input"]["structs"], "struct", struct_schema)
    print()  # new line for output readability

    # process all schemas, metadata added to schema dictionaries
    print("processing schemas...", end="")
    msg_id_enums, msg_structs = process_msgs(msg_schema)
    enum_schema.append(msg_id_enums)
    struct_schema.extend(msg_structs)

    process_structs(struct_schema)
    process_enums(enum_schema)
    print(f"{bcolors.OKGREEN}done{bcolors.ENDC}")

    print("\nlinking schemas...", end="")
    link_schemas(msg_schema, struct_schema, enum_schema)
    print(f"{bcolors.OKGREEN}done{bcolors.ENDC}")

    print("\ngenerating...")

    for GeneratorType in gen_config["hooks"]:
        # Make copies so that the generators cannot modify the content of the original.
        general_config = copy.deepcopy(gen_config)
        enum_model = copy.deepcopy(enum_schema)
        struct_model = copy.deepcopy(struct_schema)
        message_model = copy.deepcopy(msg_schema)

        GeneratorType: Type[MessageGeneratorHook]
        generator = GeneratorType()
        generator.handle_enums(general_config, enum_model, jinja_env)
        generator.handle_structs(general_config, struct_model, jinja_env)
        generator.handle_messages(general_config, message_model, jinja_env)
        generator.auxiliary(
            general_config, enum_model, struct_model, message_model, jinja_env
        )

    print("\ngeneration " + f"{bcolors.OKGREEN}done{bcolors.ENDC}")

    print("\n\n------------------[elroy_common_msg_generator.py]---------------------")
    return 0


if __name__ == "__main__":
    # set up argument parser
    parser = argparse.ArgumentParser(
        description="Elroy Common Msg Code Generation Script"
    )
    parser.add_argument(
        "--input-dir",
        type=str,
        required=True,
        dest="input_dir",
        help="points to the directory that houses source yaml schema files. "
        + "Relative to elroy_common_msg/",
    )

    # default value for output-dir is relative to the scripts directory
    parser.add_argument(
        "--output-dir",
        type=str,
        required=False,
        dest="output_dir",
        default="..",
        nargs="?",
        help="points to where output generated code. Defaults to ./generated",
    )

    args = parser.parse_args()

    # script executes within ./scripts/
    # this very dumb replacement points to the correct directory
    gen_config["input"]["msgs"] = gen_config["input"]["msgs"].replace(
        "src", args.input_dir, 1
    )
    gen_config["input"]["enums"] = gen_config["input"]["enums"].replace(
        "src", args.input_dir, 1
    )
    gen_config["input"]["structs"] = gen_config["input"]["structs"].replace(
        "src", args.input_dir, 1
    )

    if args.output_dir != "..":
        gen_config["output"]["root"] = gen_config["output"]["root"].replace(
            "..", "../" + args.output_dir, 1
        )
        gen_config["output"]["manifest"] = gen_config["output"]["manifest"].replace(
            "..", "../" + args.output_dir, 1
        )
        gen_config["output"]["cpp"] = gen_config["output"]["cpp"].replace(
            "..", "../" + args.output_dir, 1
        )

    add_hooks(FlightSoftwareGeneratorHook)
    sys.exit(run_generators())
