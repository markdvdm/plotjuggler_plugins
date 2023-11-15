#!/usr/bin/env python3
import os
import git
import yaml
import re


# Map of elroy_common_msg type to corresponding 'construct' type
elroy_type_to_construct_type_map = {
    "int8_t":   "construct.Int8sl",
    "int16_t":  "construct.Int16sl",
    "int32_t":  "construct.Int32sl",
    "int64_t":  "construct.Int64sl",
    "uint8_t":  "construct.Int8ul",
    "uint16_t": "construct.Int16ul",
    "uint32_t": "construct.Int32ul",
    "uint64_t": "construct.Int64ul",
    "float":    "construct.Float32l",
    "double":   "construct.Float64l",
    "bool":     "construct.Flag",
}


# The CRC polynomial to use for message checksumming
crc32_polynomial = 0x1f1922815


class ElroyCommonMsgGenerator:
    '''
    Class which provides utilities to auto-generate python files representing
    elroy_common_msg enums, structs, and messages. The generated files make use
    of the 'construct' python package, which allows easy
    serializing/deserializing elroy_common_msgs to/from raw byte arrays.
    '''

    def __init__(self, schemas):
        '''
        Initialize with a list of schemas, where each schema is a list of schema
        elements, e.g. {'class': {'name': 'BusObject', 'members' = [...]}}
        '''
        self._schema_elems = []
        for schema in schemas:
            [self._schema_elems.append(elem) for elem in schema]

    def write_generated_files(self, output_dir):
        '''
        Write the generated python files into the specified output_dir
        '''

        # Create generated directories and add empty __init__.py files
        for dir in ['enums', 'structs', 'msgs']:
            dir = os.path.join(output_dir, dir)
            os.makedirs(dir, exist_ok=True)
            with open(os.path.join(dir, '__init__.py'), 'w') as f:
                pass

        # Generate the files
        for elem in self._schema_elems:
            if 'enum' in elem:
                file_name = os.path.join(
                    output_dir, 'enums', elem['enum']['name'] + '.py')
            elif 'struct' in elem:
                file_name = os.path.join(
                    output_dir, 'structs', elem['struct']['name'] + '.py')
            elif 'class' in elem:
                file_name = os.path.join(
                    output_dir, 'msgs', elem['class']['name'] + '.py')
            else:
                raise NameError('Unknown schema element type!')

            (construct_str, imports) = self._get_construct_type_definition(elem)
            file_contents = ''
            file_contents += '\n'.join(sorted(list(imports)))
            file_contents += '\n\n'
            file_contents += construct_str
            with open(file_name, 'w') as f:
                f.write(file_contents)

        # Generate the MessageId.py file
        file_name = os.path.join(output_dir, 'enums', 'MessageId.py')
        file_contents = self._get_message_id_file()
        with open(file_name, 'w') as f:
            f.write(file_contents)

    def _get_message_id_file(self):
        '''
        Generate a string representing a file that contains an enumeration of
        message IDs for all messages.
        '''
        s = ''
        s += 'import construct'
        s += '\n\n'
        s += 'MessageId = construct.Enum(construct.Int16ul,\n'
        s += '    Unknown = 0,\n'
        msgs = [elem for elem in self._schema_elems if 'class' in elem]
        for index, msg in enumerate(msgs):
            name = msg['class']['name']
            s += '    {} = {},\n'.format(name, index+1)
        s += ')\n'
        return s

    def _get_construct_type_definition(self, element):
        '''
        For the given schema element, generate a string describing the python
        'construct' type definition. This string can be incorporated directly
        into generated python code.
        '''
        if 'class' in element and element['class']['name'] == 'BusObject':
            return self._get_bus_object_construct(element['class'])
        if 'enum' in element:
            return self._get_enum_construct(element['enum'])
        elif 'struct' in element:
            return self._get_struct_construct(element['struct'])
        elif 'class' in element:
            return self._get_msg_construct(element['class'])

    def _get_bus_object_construct(self, bus_obj):
        '''
        For the special 'BusObject' element, generate a string describing the
        python 'construct' type definition. This is the base construct for any
        elroy_common_msg message as serialized on the wire.
        '''
        members = bus_obj['members']
        assert [m['name'] for m in members] == ['id', 'write_timestamp_ns',
                                                'mutex'], "Unexpected member names in 'BusObject'! Did the definition change?"
        assert [m['type'] for m in members] == ['MessageId', 'int64_t',
                                                'std::mutex'], "Unexpected member types in 'BusObject'! Did the definition change?"
        imports = set()
        imports.add('import construct')
        imports.add('import crcmod')
        imports.add('from ..enums.MessageId import MessageId')
        msgs = [elem for elem in self._schema_elems if 'class' in elem]
        for msg in msgs:
            if 'class' in msg:
                name = msg['class']['name']
                if name == bus_obj['name']:
                    continue
                imports.add('from ..msgs.{} import {}'.format(name, name))
        s = ''
        s += self._get_comment(bus_obj['name'])
        s += '\n'
        s += 'crc_func = crcmod.mkCrcFun({}, initCrc=0, xorOut=0xFFFFFFFF, rev=False)\n'.format(hex(crc32_polynomial))
        s += '\n'
        s += '{} = construct.Struct(\n'.format(bus_obj['name'])
        s += '    "fields" / construct.RawCopy(construct.Struct(\n'
        s += '        "id" / MessageId,\n'
        s += '        "write_timestamp_ns" / construct.Int64sl,\n'
        s += '        "msg" / construct.Switch(construct.this.id, {\n'
        for index, msg in enumerate(msgs):
            name = msg['class']['name']
            if name == bus_obj['name']:
                continue
            s += '            MessageId.{}: {},\n'.format(name, name)
        s += '        })\n'
        s += '    )),\n'
        s += '    "crc" / construct.Checksum(construct.Int32ul,\n'
        s += '        lambda data: crc_func(data),\n'
        s += '        construct.this.fields.data),\n'
        s += ')\n'
        return (s, imports)

    def _get_enum_construct(self, enum):
        '''
        For the given enum schema element, generate a string describing the
        python 'construct' type definition.
        '''
        name = enum['name']
        fields = enum['list']

        fields = ['Unknown'] + fields + ['Count']

        elabs = ["Unknown"] + enum['elaboration_list'] + ["Count"] if 'elaboration_list' in enum else [''] * \
            len(fields)

        imports = set()
        imports.add('import construct')

        s = ''
        s += self._get_comment(name)
        s += '\n'
        s += '{} = construct.Enum(\n "{}" / construct.Byte,\n'.format(name, name)
        for field, elab, i in zip(fields, elabs, range(len(fields))):
            s += '    {}={},'.format(field, i)
            if elab:
                s += ' # ' + elab
            s += '\n'
        s += ')\n'
        return (s, imports)

    def _get_struct_construct(self, struct):
        '''
        For the given struct schema element, generate a string describing the
        python 'construct' type definition.
        '''
        name = struct['name']
        inherit = struct['inherit'] if 'inherit' in struct else None
        members = struct['members'] if 'members' in struct else []

        imports = set()
        imports.add('import construct')
        s = ''
        s += self._get_comment(name)
        s += '\n'
        s += '{} = construct.Struct(\n'.format(name)
        if members:
            if inherit:
                inherit_name = inherit['name']
                if inherit_name != "BusObject":
                    (contruct_type, import_statement) = self._construct_type_from_elroy_type(
                        inherit_name, default_value=None)
                    s += '    "{}" / {}, \n'.format(
                        inherit_name, contruct_type)
                    if import_statement:
                        imports.add(import_statement)
            for member in members:
                member_name = member['name']
                member_type_str = member['type']
                default_value = member['default'] if 'default' in member else None
                is_enumeration = isinstance(default_value, str) and "::" in default_value
                if is_enumeration:
                    # this is an enumeration type, so treat accordingly. Default enumerations are specified as
                    # "EnumerationName::EnumerationValue", e.g. "SystemComponentId::ActuatorFeedbackBus"
                    default_value = default_value.replace("::", ".")
                (contruct_type, import_statement) = self._construct_type_from_elroy_type(
                    member_type_str, default_value)
                s += '    "{}" / {}, \n'.format(member_name, contruct_type)
                if import_statement:
                    imports.add(import_statement)
        s += ')\n'
        return (s, imports)

    def _get_msg_construct(self, msg):
        '''
        For the bus object schema element, generate a string describing the
        python 'construct' type definition.
        '''
        name = msg['name']

        # 'BusObject' is special, so ensure that it's not this one
        assert name != 'BusObject', 'BusObject is a special message and must be generated on its own!'

        inherit = msg['inherit'] if 'inherit' in msg else None
        members = msg['members'] if 'members' in msg else []

        imports = set()
        imports.add('import construct')
        s = ''
        s += self._get_comment(name)
        s += '\n'
        s += '{} = construct.Struct(\n'.format(name)
        if inherit:
            inherit_name = inherit['name']
            if inherit_name != "BusObject":
                (contruct_type, import_statement) = self._construct_type_from_elroy_type(
                    inherit_name, default_value=None)
                s += '    "{}" / {}, \n'.format(
                    inherit_name, contruct_type)
                if import_statement:
                    imports.add(import_statement)
        for member in members:
            member_name = member['name']
            member_type_str = member['type']
            default_value = member['default'] if 'default' in member else None
            is_enumeration = isinstance(default_value, str) and "::" in default_value
            if is_enumeration:
                # this is an enumeration type, so treat accordingly. Default enumerations are specified as
                # "EnumerationName::EnumerationValue", e.g. "SystemComponentId::ActuatorFeedbackBus"
                default_value = default_value.replace("::", ".")
            (contruct_type, import_statement) = self._construct_type_from_elroy_type(
                member_type_str, default_value)
            s += '    "{}" / {}, \n'.format(member_name, contruct_type)
            if import_statement:
                imports.add(import_statement)

        s += ')\n'
        return (s, imports)

    def _get_comment(self, elem_name):
        '''
        Generate a comment string for the generated construct type definition
        corresponding to the element with name 'element_name'
        '''
        s = ''
        s += '# To deserialize from a byte array into a dict:\n'
        s += '# msg_dict = {}.parse(byte_array)\n'.format(elem_name)
        s += '#\n'
        s += '# To serialize a dict into a byte array:\n'
        s += '# byte_array = {}.build(msg_dict)\n'.format(elem_name)
        return s

    def _construct_type_from_elroy_type(self, elroy_member_type, default_value):
        '''
        For the given elroy_common_msg member type (e.g. 'uint16_t' or 'InstanceId'),
        return the appropriate 'construct' type to use. Use default_value if it is
        not None.

        The given member type might be a plain data type, like 'uint32_t' or
        'float', or it might be another elroy_common_msg enum, struct, or bus object
        message. It can also be an array of any of these.

        This function returns a 2-tuple:
        (construct type as a string, any additional 'import' statement required
        to find the resulting type)
        '''
        construct_type = ''
        import_statement = None

        # Parse the elroy_member_type string to determine if it is an array or
        # not, and grab the number of elements if it is. Example: 'float[4]'
        x = re.findall('([^\[]*)(\[(\d*)\])?$', elroy_member_type.strip())
        member_type = x[0][0]
        num_array_elements = int(x[0][2]) if x[0][2] else 0

        # Find out what type of member this is and add the corresponding import
        # statement if necessary
        if member_type in [elem['enum']['name'] for elem in self._schema_elems if 'enum' in elem]:
            import_statement = 'from ..enums.{} import {}'.format(
                member_type, member_type)
        elif member_type in [elem['struct']['name'] for elem in self._schema_elems if 'struct' in elem]:
            import_statement = 'from ..structs.{} import {}'.format(
                member_type, member_type)
        elif member_type in [elem['class']['name'] for elem in self._schema_elems if 'class' in elem]:
            import_statement = 'from ..msgs.{} import {}'.format(
                member_type, member_type)
        elif member_type in elroy_type_to_construct_type_map:
            member_type = elroy_type_to_construct_type_map[member_type]
        else:
            raise NameError("Could not get a python construct type from the elroy type '{}'".format(
                elroy_member_type))

        construct_type = member_type

        if default_value is not None:
            construct_type = 'construct.Default({}, {})'.format(construct_type, default_value)

        if num_array_elements > 0:
            construct_type = 'construct.Array({}, {})'.format(
                num_array_elements, construct_type)

        return (construct_type, import_statement)


def main():
    git_repo = git.Repo(os.path.abspath(__file__),
                        search_parent_directories=True)
    git_root = git_repo.git.rev_parse("--show-toplevel")
    schema_dir = os.path.join(git_root, 'src')

    with open(os.path.join(schema_dir, "enumerates_schema.yaml"), 'r') as f:
        enum_schema = yaml.safe_load(f)
    with open(os.path.join(schema_dir, "structs_schema.yaml"), 'r') as f:
        structs_schema = yaml.safe_load(f)
    with open(os.path.join(schema_dir, "bus_object_schema.yaml"), 'r') as f:
        bus_object_schema = yaml.safe_load(f)

    generator = ElroyCommonMsgGenerator(
        [enum_schema, structs_schema, bus_object_schema])
    generator.write_generated_files(output_dir=os.path.join(
        git_root, 'generated', 'py', 'elroy_common_msg'))


if __name__ == "__main__":
    main()
