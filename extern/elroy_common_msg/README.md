<!-- markdownlint-disable ol-prefix -->

# Elroy Common Message (ECM)

Messages (serializable data blobs), structs (simple, C-like), and enumerates are auto-generated utilizing `yaml` to define the schema and `jinja2` template engine.

## resources

1. [YAML website](https://yaml.org/)
2. [YAML Syntax](https://docs.ansible.com/ansible/latest/reference_appendices/YAMLSyntax.html)
3. [Jinja2](https://jinja.palletsprojects.com/en/3.0.x/)

## usage

Currently, no CLI arguments are accepted to `./scripts/elroy_common_msg_generator.py`. To generate:

1. If not using the [Elroy software docker container](https://github.com/elroyair/build_env#docker-container-for-development):

```shell
pip install -r ./scripts/requirements.txt
```

2. Run `python3 elroy_common_msg_generator.py --help` to see current usage:

```text
   usage: ./scripts/elroy_common_msg_generator.py [-h] --input-dir INPUT_DIR [--output-dir [OUTPUT_DIR]]

   Elroy Common Msg Code Generation Script

   optional arguments:
     -h, --help            show this help message and exit
     --input-dir INPUT_DIR
                           points to the directory that houses source yaml schema files. Relative to elroy_common_msg/
     --output-dir [OUTPUT_DIR]
                           points to where output generated code. Defaults to ./generated
```

## generated directory

The generated directory is **not** checked into version control, users must be sure to call `./scripts/elroy_common_msg_generator.py` as a pre-build step. The directory structure is `CMake` style as follows:

```text
generated
├── <lang>
│   ├── include
│   │   └── elroy_common_msg
│   │       ├── c1sn1
│   │       ├── common
│   │       ├── enums
│   │       ├── msg_handling
│   │       └── structs
│   └── unit_tests
└── manifest
```

- `<lang>` - output language. The above snippet is for `cpp`
  - `include/` - location of all generated _messages_ for easy include directives `#include <elroy_common_msg/my_message.h>`
    - `enums/` - common enumerate header files
    - `msg_handling` - houses `MsgHandler` interface and `MsgDecoder`
    - `structs/` - common struct files (not messages)
  - `unit_tests/` - auto-generated unit-tests
- `manifest/` - final copy of internal schema (dictionaries) used for generation, saved as `yaml` files

## schemas

Schemas are trivial usage of `yaml`; mostly glorified `dictionary` of `dictionaries` or `lists` of `dictionaries`.

### messages

Messages are serializable blobs of data, as defined in `messages_schema.yaml`, with a unique `message id`. The following features are supported

- data types:
  - fixed-width integers (u/int[8,16,32,64]\_t)
  - float
  - double
  - bool
  - enumerates (from enum_schema)
  - structs (from enum_schema)
  - arrays (of any of the above types)
- Struct-version generated:

  - An associated struct is generated in `structs/` that matches the data member set and inheritance chain (omitting `BusObjectMessage` base)
  - This feature was added as FSW historically utilizes `structs` a lot. Gives flexibility to continue this practice
    <details>
    <summary> More info of a message's simple struct</summary>

    Given the schema

    ```yaml
    - class:
        - name: Taco
        - namespace: mexican_food
        - members:
            - name: carnitas
              type: uint8_t
            - name: salsa
              type: double[2]
    ```

    A `TacoMessage` will be created in `elroy_common_msg/` and a simple struct,
    `Taco` will live in `elroy_common_msg/structs`.

    The inclusion of the simple struct is in the spirit of how `FSW` has utilized structs for 1) data processing and 2) within the busRegistry.

    The simple-struct can be used to:

    1. Construct the message:

       ```cpp
       TacoMessage(const Taco& src)
       : carnitas_{src.carnitas}
       , salsa {} {
         for(int i = 0; i < 2; i++) {
           salsa[i] = src.salsa[i]
         }
       }
       ```

    2. Read/Write into/out of the message (mirroring the original `BusObject`)

       ```cpp
       /// reading
       Taco myTaco;
       myTacoMessage.Read(myTaco);

       /// writing
       Taco myTaco;
       myTacoMessage.Write(myTaco)
       ```

    </details>

- Inheritance:
  - access mode is `public` by default
- Constructors:
  - Default (public)
  - Copy constructor (public)
  - Constructor with struct argument (public)
  - Constructor w/ Message ID argument (protected: for inheritance)
  - Constructor w/ Message ID + struct src (protected: for inheritance support)
- Serialization: allowing structs to know how to serialize themselves makes life easier
  - GetPackedSize() - returns the length of the packed array
  - Pack() - packs the struct bottom-up (i.e. base class first)
  - Unpack() - unpacks the struct bottom-up (i.e. base class first)
  - Data
- data types:
  - fixed-width integers (u/int8_t, u/int16_t, u/int32_t, u/int64_t)
  - float, double
  - bool
  - auto-generated struct or enum, per `structs_schema.yaml` or `enumerates_schema.yaml` respectively
  - arrays of any of the above (arrays length must be > 0)

#### message schema description

The messages schema is a list of dictionaries as follows:

```yaml
- class: # class dictionary - messages.yaml defines a list of classes
    name: None # name of class, generated name = "name + Message", struct = "name"
    namespace: None # namespace for the class (optional), if not specified defaults to `elroy_common_msg`
    inherit: # optional - multiple inheritance allowed
      name: None # name of parent class
    members: # list of member variables
      - name: None # name of data member, generated name = "name_"
        type:
          None # data type: fixed-width integers: u/int[8,16,32,64]_t,
          #            float, double,
          #            enumerate as defined in `enumerates.yaml`
          #            or struct as defined in `struct.yaml`
        default: None # default initialization
```

- `- class:`
  - `-` denotes a list entry.
  - "class" is the root key
- `name : None`
  - Required Key-value pair. "name" is expected key, user entered value
- `namespace: None`
  - Optional key-value pair. Defaults to `elroy_common_msg` if not specified
- `inherit :`
  - optional key, value (dict) pair. "name" is expected key, value is user-entered dictionary
  - `name : None`
    - key, value pair. "name" is expected key, user entered value
  - `access_mode : None`
    - key, value pair. "access_mode" is expected key, user entered value (public, protected, private)
    - **currently not supported**
- `members :`
  - optional key, value pair. "members" is expected key, value is user-entered list of dictionaries
  - `- name : None`
    - `-` denotes a list entry of a dictionary. "name" is expected key, user entered value
    - `type : None`
      - Required key, value pair. "type" is expected key, user entered value
      - Supported data types: fixed-width integers, float, double, bool, generated structs/enums
    - `default : None`
      - Optional key, value pair. "default" is expected key, user entered value

#### BusObjectMessage common base class

All generated messages inherit from `BusObjectMessage`, atm the user must enter this in the schema (i.e. not defaulted in generation). The common base-class and interface allows use of polymorphism, keeping code general and reusable where it can be done.

A few notes:

- `GetPackedSize()`, `Pack` and `Unpack` are virtual but not `pure-virtual`, as the base class implements the functions

### structs

Structs are serializable (no message ID) C++ objects with public data member access. Supported features are as follows:

- data types:
  - fixed-width integers (u/int[8,16,32,64]\_t)
  - float
  - double
  - bool
  - enumerates (from enum_schema)
  - structs (from enum_schema)
  - arrays (of any of the above types)
- Inheritance:
  - access mode is `public` per `struct` data access usage
  - `busObject` base class is _omitted_ from inheritance chain
- Constructors:
  - Default
  - Copy constructor
- Serialization: allowing structs to know how to serialize themselves makes life easier
  - GetPackedSize() - returns the length of the packed array
  - Pack() - packs the struct bottom-up (i.e. base class first)
  - Unpack() - unpacks the struct bottom-up (i.e. base class first)

#### struct schema description

The struct schema is very similar messages, as follows:

```yaml
- struct:
    name: None # name of struct
    namespace: None # namespace for the class (optional), if not specified defaults to `elroy_common_msg`
    inherit: # optional - multiple inheritance allowed
      name: None # name of parent class
    members: # list of struct data members - required, but the list can be empty
      - name: None # name of data member
        type: None # type, see "type" in `class` above
        default: None # default initialization
```

- `- struct:`
  - `-` denotes a list entry.
  - "struct" is the root key
- `name : None`
  - Required Key-value pair. "name" is expected key, user entered value
- `namespace: None`
  - Optional key-value pair. Defaults to `elroy_common_msg` if not specified
- `inherit :`
  - optional key, value (dict) pair. "name" is expected key, value is user-entered dictionary
  - `name : None`
    - key, value pair. "name" is expected key, user entered value
  - `access_mode : None`
    - key, value pair. "access_mode" is expected key, user entered value (public, protected, private)
    - **currently not supported**
- `members :`
  - optional key, value pair. "members" is expected key, value is user-entered list of dictionaries
  - `- name : None`
    - `-` denotes a list entry of a dictionary. "name" is expected key, user entered value
    - `type : None`
      - Required key, value pair. "type" is expected key, user entered value
      - Supported data types: fixed-width integers, float, double, bool, generated structs/enums
    - `default : None`
      - Optional key, value pair. "default" is expected key, user entered value

### enums

Enumerate definition is defined per `enumerates_schema.yaml`. Supported features:

- Naming struct
- enumerating the enum list
- type (fixed-width integer size) is optional

#### enum schema description

The enum schema is very simple, as follows:

```yaml
- enum:
    name: None
    namespace: None # namespace for the class (optional), if not specified defaults to `elroy_common_msg`
    type: None
    list:
      - "On"
      - "Off"
    brief_list:
      - None
      - None
    elaboration_list:
      - None
      - None
```

- `- enum:`
  - `-` denotes a list entry.
  - "enum" is the root key
- `name : None`
  - Required Key-value pair. "name" is expected key, user entered value
- `namespace: None`
  - Optional key-value pair. Defaults to `elroy_common_msg` if not specified
- `type : None`
  - Optional key, value pair. "type" is expected key, user entered value
  - type is a fixed-width integer type (i.e `enum class MyEnum: type {}`)
  - if omitted, the type is calculated based on the number of `list` entries
  - if `type` is provided, the size of `type` is checked against the size of `list` entries
- `list:`
  - Required key, value pair. "list" is expected key, value is user-entered list of strings
  - `- MyEnum`
    - `-` denotes a list entry
- `brief_list:`
  - Optional key, value pair. "brief_list" is expected key, value is user-entered list of strings used for string conversions. Length should match list length.
    - Intended for cases where abbreviations can be used or parts of the name can be removed based on context due to string length limitations; e.g. RadarAltimeter could be "Radalt" and FlightControlsEmergency could be "Emerg" or "FC Emerg".
    - Brief names should be limited to 16 characters for brevity in a UI context (enough for 3 complete, average-length words).
    - EnumToBriefString() will only be generated if this is defined.
  - `- "Brief Enum"`
    - `-` denotes a list entry
- `elaboration_list:`
  - Optional key, value pair. "elaboration_list" is expected key, value is user-entered list of strings used for string conversions. Length should match list length.
    - Intended for cases where spacing and clarity of concepts are desired; e.g. StbdFlightComputer_B could be "Starboard Flight Computer B".
    - Elaborations could be verbose acronym/abbreviation expansions or alternative meanings.
    - Elaboration names should be limited to 60 characters for UI contexts (the maximum recommended line length per [Material guidelines](https://m2.material.io/design/typography/understanding-typography.html#readability)).
    - EnumToElaboratedString() will only be generated if this is defined.
  - `- "My Elaborated Enum"`
    - `-` denotes a list entry

##### Enumerate Keywords

The following words must be put in quotes for proper code generation: - "On", "Off" - if not in quotes these will be generated as "True", "False" (respectively)

## Message Handling

Message handling is supported via `IMsgHandler` and `MsgDecoder`, auto-generated into `elroy_common_msg/msg_handling`, both which implement the `visitor` pattern.

### IMsgHandler

The message handler interface provides a pseudo-pure virtual interface to pop into a concrete message's context and handle a message, example below:

```cpp
  /// @brief default handler for root parent message
  /// @note assert is to signal developer that they are missing
  ///       handler implementation. This assert will need to replace
  ///       with an error log at a future date
  /// @param[in] msg const pointer to a const msg
  virtual void Handle(const elroy_common_msg::BusObjectMessage* msg) noexcept
    { (void)msg; assert(false); }

  /// @brief Handle functions for all defined messages
  /// @note all handlers default cast msgs to base class, call base handler
  /// @param[in] msg const pointer to const msg
  virtual void Handle(const elroy_common_msg::HeartBeatMessage* msg) noexcept {
    Handle(static_cast<const BusObjectMessage*>(msg));
  }

  virtual void Handle(const elroy_common_msg::AirDataStateMessage* msg) noexcept {
    Handle(static_cast<const BusObjectMessage*>(msg));
  }

  virtual void Handle(const elroy_common_msg::FogInsStateMessage* msg) noexcept {
    Handle(static_cast<const BusObjectMessage*>(msg));
  }
```

The pseudo-pure virtual pattern is utilized here to "implement" default functionality of every concrete message: (atm) assert within the base `BusObjectMessage` Handle function, in the future log an error. This allows flexibility for the user to implement desired message handling on per-handler basis. During development, if a user forgets to override a necessary `Handle` function, they will quickly know during testing.

### MsgDecoder

The message decoder is a common class that knows how to crack open and decode a `uint8_t` array that _may_ hold a message. The Message Decoder algorithm is currently trivial, relying on communication-protocol CRC (i.e. `YAP` or `UDP`):

```cpp
 // decoding can begin when buf_len is at least
    if (sizeof(MessageId) <= buf_len) {
      bytes_processed = sizeof(MessageId);
      elroy_common_msg::MsgPackingUtil::Unpack(id, buf);

      switch (id) {
        case MessageId::HeartBeat:
        {
          static elroy_common_msg::HeartBeatMessage msg;
          if (msg.GetPackedSize() <= buf_len) {
            bytes_processed = msg.GetPackedSize();
            msg.Unpack(buf, buf_len);
            handler.Handle(&msg);
            ret = true;
          }
          break;
        }
        case <The rest of the messages...>
```

Aside from code reuse, the benefit of utilizing the common message decoder is the implementation of the visitor pattern via `IMsgHandler` interface: once the message is cracked open, we can immediately handle the message within the context of the concrete class--no additional `switch` statements required.

A few notes:

- Msg Packing/Unpacking does not utilize a CRC. In the future, should we decide to add a crc to individual messages, the change will be reflected here.
- Currently, the `Decode` function returns a bool - true if it was handled, else false. We loose fidelity as to exactly why an error failed to decode (bad length? crc? etc.). When error handling gets flushed out this will be added.

## Versioning

Semantic version numbering (major.minor.patch) will be used for `elroy_common_msg` versioning, being rolled on every merge to `main`. In general, versioning rules are as follows:

- major: increment when a non-backward compatible change occurs (change/removal of old msg), update in msg API
- minor: increment when a backwards compatible change occurs (added new msg)
- patch: bug-fix or bby correction that doesn't result in a change in compatibility

## Serialization

Serialization happens from bottom-up little-endian; that is, `BusObjectMessage` is serialized first, then all subsequent derived classes, and (lastly) the crc. The result is a serialized packet with the following structure:

| Byte: | [0-1]      | [1-4]     | 5              | 6              | ... | N - 4          | N -3        | N - 2       | N - 1       | N           |
| ----- | ---------- | --------- | -------------- | -------------- | --- | -------------- | ----------- | ----------- | ----------- | ----------- |
| Data: | Message ID | Timestamp | Derived Data 1 | Derived Data 2 | ... | Derived Data N | CRC LSW LSB | CRC LSW MSB | CRC MSW LSB | CRC MSW MSB |

where:

- `LSW` is least significant word (16-bits)
- `MSW` is most significant word (16-bits)
- `LSB` is least significant byte
- `MSB` is most significant byte

### CRC

A 32-bit CRC was selected to achieve a good [hamming distance](http://users.ece.cmu.edu/~koopman/crc/index.html) for larger message lengths. From [here](https://users.ece.cmu.edu/~koopman/crc/) we've selected the polynomial `0x1F1922815`, which achieves a HD=8 for messages up to 124 bytes. This high hamming distance provides adequate protection for one of our most safety critical messages, ActuatorCommandMessage, which changes the dynamics of the vehicle, and is (currently) 98 bytes big.

It's noted that the 32-bit CRC becomes inefficient for messages with small lengths; like the HeartBeatMessage. This is deemed an acceptable inefficiency his as all communication lanes--actuator feedback, command distro, data logger, and GCS--can all afford it. It's more than likely that `elroy_common_msg`'s predecessor will have a different CRC scheme.

Additionally:

- `BusObjectMessage` includes
  - `static constexpr size_t kSizeOfCrcInBytes_ = sizeof(uint32_t)`
  - `GetLatestedUnpackedCrc()` that returns the most recently unpacked crc
- CRC is calculated on call to `Pack`
  - _note:_ That `Pack` is not-virtual and implemented in `BusObjectMessage` and calls the derived class's `PackWithoutCrc`
- The CRC is saved into `latest_unpacked_crc_` on call to `Unpack`
  - _note:_ That `Unpack` is not-virtual and implemented in `BusObjectMessage` and calls the derived class's `UnpackWithoutCrc`

For additional CRC reading please refer to: [Selection of Cyclic Redundancy Code and Checksum Algorithms to Ensure Critical Data Integrity](https://users.ece.cmu.edu/~koopman/pubs/faa15_tc-14-49.pdf)

#### Polynomial & CRC parameters

- Polynomial: `0x1F1922815`
- Initial CRC: `0xFFFFFFFF`
- XOR on output: `0xFFFFFFFF`
- Reverse bits: `False`

The selected polynomial, `0x1F1922815`, is the _explicit + 1_ notation; CRC polynomials also have an _implicit + 1_ notation, the equivalent of our selected polynomial would be `0xF8C9140A`; refer to [here](https://users.ece.cmu.edu/~koopman/crc/crc32.html).Most CRC users (especially those of libraries) err toward the explicit + 1 notation, which is actually + 1 bit larger than the CRC-size (i.e. the leading 1). This leading 1 is typically removed because: 1) when executing the CRC algorithm the leading 1 of the data-byte is aligned with the leading 1 of the CRC (shifted until there is a 1 in MSbit or LSbit) and 2) when these 1s are XORd they go to zero. Thus, an explicit + 1 polynomial, 0x1f1922815, will in practice be 0xf1922815.

#### CRCMOD

Note that the CRCMOD [implementation](https://github.com/gsutil-mirrors/crcmod) is slightly nuanced. Copied here is the implementation of [crcmod.\_mkCrcFun](https://github.com/gsutil-mirrors/crcmod/blob/master/python3/crcmod/crcmod.py#L418), for `python3`:

```python
def _mkCrcFun(poly, sizeBits, initCrc, rev, xorOut):
    if rev:
        tableList = _mkTable_r(poly, sizeBits)
        _fun = _sizeMap[sizeBits][1]
    else:
        tableList = _mkTable(poly, sizeBits)
        _fun = _sizeMap[sizeBits][0]

    _table = tableList
    if _usingExtension:
        _table = struct.pack(_sizeToTypeCode[sizeBits], *tableList)

    if xorOut == 0:
        def crcfun(data, crc=initCrc, table=_table, fun=_fun):
            return fun(data, crc, table)
    else:
        def crcfun(data, crc=initCrc, table=_table, fun=_fun):
            return xorOut ^ fun(data, xorOut ^ crc, table)

    return crcfun, tableList
```

Note that for `xorOut != 0` the `crc`, which is initialized to `crc=initCrc`, is actually also `XOR`d with `xorOut`. Thus, for implementations that initialize/seed the CRC with `0xFFFFFFFF` _and_ `xorOut=0xFFFFFFFF` would have to configured `crcmod` to `initCrc=0`.

#### CCrcCalculator

The CRC calculation is done via a static-class `CCrcCalculator` in `scripts/templates/cpp/c_crc_calculator.h`. The CRC-calculator uses a look-up table byte-wise algorithm, instead of the slower bit-wise algorithm typically employed.

The byte-wise algorithm was referenced from [here](https://create.stephan-brumme.com/crc32/#sarwate) (also in `CalculateCrc` function comment)

##### Updating a new polynomial lookup table

Unfortunately, updating a new polynomial lookup table is a bit manual: using `./scripts/crc_util.py` to generate a new table per some polynomial and copying that table into `scripts/templates/cpp/c_crc_calculator.h`. This should ideally be done within the generation tools... the generation python script calls `crc_util.py` to extract the table, then shove it into the `jinja2` template. Alas, for now the steps are:

- `cd ./scripts`
- `python crc_util.py --poly 0x1f1922815 --gen-table`
- copy everything from
  - `----------------------[ COPY BELOW ]---------------------------` </br> `----------------------[ COPY ABOVE ]---------------------------`
- paste to line 68 of `c_crc_calculator.h`

#### scripts/crc_util.py

```bash
usage: crc_utility.py [-h] --poly POLY [--gen-table] [--test] [--crcmod-test]

CRC Utility

options:
  -h, --help     show this help message and exit
  --poly POLY    The 32-bit polynomial in hexadecimal explicit + 1 notation (i.e. most-significant 1)!
  --gen-table    Generate 32-bit lookup table for --poly and updates `templates/cpp/c_crc_calculator.h`. Defaults to use crcmod
                 generated table
  --test         Execute test that compares various CRC methods
  --crcmod-test  Calculate crc for identical array defined in ./scripts/templates/cpp/msg_handling_ut.jinja2 using crcmod
```

A pretty basic python utility script that largely wraps crcmod in a `CrcUtil` class, as well as define a handful of different crc calculations based on various references. The reason for the additional crc calculations was to compare their CRC output and generated tables, ensuring we are using the CRC algorithm(s) correctly.

A `--test` flag that will run through all the different types of CRC calculations and validate the CRCs and tables are correct.

A `--crcmod-test` flag exists that will use the crcmod generated crc function and calculate/compare the CRC for a statically defined buffer; this buffer is identical to the one defined in `msg_handling_ut.jinja2`

The flag `--gen-table` will generate the C++ look-up table, to be copied over to `templates/cpp/c_crc_calculator.h::58`, an example output is:

```text
root@bumpkin:/home/codez/flight_software/extern/elroy_common_msg/scripts# python crc_utility.py --poly 0x1F1922815 --gen-table
Generating table for poly 0x1F1922815

----------------------[COPY BELOW]----------------------
        /// @brief this table was generated with elroy_common_msg/scripts/crc_util.py via:
        ///        crc_util.py --poly 0x1F1922815 --gen-table
        /// @note crc_util defaults to use crcmod lookup table generation
        /// @note crc32 lookup table for polynomial 0x1F1922815
        static constexpr uint32_t kCrc32LookupTable_[256] = {
                0x00000000, 0xF1922815, 0x12B6783F, 0xE324502A, 0x256CF07E, 0xD4FED86B, 0x37DA8841, 0xC648A054,
                0x4AD9E0FC, 0xBB4BC8E9, 0x586F98C3, 0xA9FDB0D6, 0x6FB51082, 0x9E273897, 0x7D0368BD, 0x8C9140A8,
                0x95B3C1F8, 0x6421E9ED, 0x8705B9C7, 0x769791D2, 0xB0DF3186, 0x414D1993, 0xA26949B9, 0x53FB61AC,
                0xDF6A2104, 0x2EF80911, 0xCDDC593B, 0x3C4E712E, 0xFA06D17A, 0x0B94F96F, 0xE8B0A945, 0x19228150,
                0xDAF5ABE5, 0x2B6783F0, 0xC843D3DA, 0x39D1FBCF, 0xFF995B9B, 0x0E0B738E, 0xED2F23A4, 0x1CBD0BB1,
                0x902C4B19, 0x61BE630C, 0x829A3326, 0x73081B33, 0xB540BB67, 0x44D29372, 0xA7F6C358, 0x5664EB4D,
                0x4F466A1D, 0xBED44208, 0x5DF01222, 0xAC623A37, 0x6A2A9A63, 0x9BB8B276, 0x789CE25C, 0x890ECA49,
                0x059F8AE1, 0xF40DA2F4, 0x1729F2DE, 0xE6BBDACB, 0x20F37A9F, 0xD161528A, 0x324502A0, 0xC3D72AB5,
                0x44797FDF, 0xB5EB57CA, 0x56CF07E0, 0xA75D2FF5, 0x61158FA1, 0x9087A7B4, 0x73A3F79E, 0x8231DF8B,
                0x0EA09F23, 0xFF32B736, 0x1C16E71C, 0xED84CF09, 0x2BCC6F5D, 0xDA5E4748, 0x397A1762, 0xC8E83F77,
                0xD1CABE27, 0x20589632, 0xC37CC618, 0x32EEEE0D, 0xF4A64E59, 0x0534664C, 0xE6103666, 0x17821E73,
                0x9B135EDB, 0x6A8176CE, 0x89A526E4, 0x78370EF1, 0xBE7FAEA5, 0x4FED86B0, 0xACC9D69A, 0x5D5BFE8F,
                0x9E8CD43A, 0x6F1EFC2F, 0x8C3AAC05, 0x7DA88410, 0xBBE02444, 0x4A720C51, 0xA9565C7B, 0x58C4746E,
                0xD45534C6, 0x25C71CD3, 0xC6E34CF9, 0x377164EC, 0xF139C4B8, 0x00ABECAD, 0xE38FBC87, 0x121D9492,
                0x0B3F15C2, 0xFAAD3DD7, 0x19896DFD, 0xE81B45E8, 0x2E53E5BC, 0xDFC1CDA9, 0x3CE59D83, 0xCD77B596,
                0x41E6F53E, 0xB074DD2B, 0x53508D01, 0xA2C2A514, 0x648A0540, 0x95182D55, 0x763C7D7F, 0x87AE556A,
                0x88F2FFBE, 0x7960D7AB, 0x9A448781, 0x6BD6AF94, 0xAD9E0FC0, 0x5C0C27D5, 0xBF2877FF, 0x4EBA5FEA,
                0xC22B1F42, 0x33B93757, 0xD09D677D, 0x210F4F68, 0xE747EF3C, 0x16D5C729, 0xF5F19703, 0x0463BF16,
                0x1D413E46, 0xECD31653, 0x0FF74679, 0xFE656E6C, 0x382DCE38, 0xC9BFE62D, 0x2A9BB607, 0xDB099E12,
                0x5798DEBA, 0xA60AF6AF, 0x452EA685, 0xB4BC8E90, 0x72F42EC4, 0x836606D1, 0x604256FB, 0x91D07EEE,
                0x5207545B, 0xA3957C4E, 0x40B12C64, 0xB1230471, 0x776BA425, 0x86F98C30, 0x65DDDC1A, 0x944FF40F,
                0x18DEB4A7, 0xE94C9CB2, 0x0A68CC98, 0xFBFAE48D, 0x3DB244D9, 0xCC206CCC, 0x2F043CE6, 0xDE9614F3,
                0xC7B495A3, 0x3626BDB6, 0xD502ED9C, 0x2490C589, 0xE2D865DD, 0x134A4DC8, 0xF06E1DE2, 0x01FC35F7,
                0x8D6D755F, 0x7CFF5D4A, 0x9FDB0D60, 0x6E492575, 0xA8018521, 0x5993AD34, 0xBAB7FD1E, 0x4B25D50B,
                0xCC8B8061, 0x3D19A874, 0xDE3DF85E, 0x2FAFD04B, 0xE9E7701F, 0x1875580A, 0xFB510820, 0x0AC32035,
                0x8652609D, 0x77C04888, 0x94E418A2, 0x657630B7, 0xA33E90E3, 0x52ACB8F6, 0xB188E8DC, 0x401AC0C9,
                0x59384199, 0xA8AA698C, 0x4B8E39A6, 0xBA1C11B3, 0x7C54B1E7, 0x8DC699F2, 0x6EE2C9D8, 0x9F70E1CD,
                0x13E1A165, 0xE2738970, 0x0157D95A, 0xF0C5F14F, 0x368D511B, 0xC71F790E, 0x243B2924, 0xD5A90131,
                0x167E2B84, 0xE7EC0391, 0x04C853BB, 0xF55A7BAE, 0x3312DBFA, 0xC280F3EF, 0x21A4A3C5, 0xD0368BD0,
                0x5CA7CB78, 0xAD35E36D, 0x4E11B347, 0xBF839B52, 0x79CB3B06, 0x88591313, 0x6B7D4339, 0x9AEF6B2C,
                0x83CDEA7C, 0x725FC269, 0x917B9243, 0x60E9BA56, 0xA6A11A02, 0x57333217, 0xB417623D, 0x45854A28,
                0xC9140A80, 0x38862295, 0xDBA272BF, 0x2A305AAA, 0xEC78FAFE, 0x1DEAD2EB, 0xFECE82C1, 0x0F5CAAD4};

----------------------[COPY ABOVE]----------------------
```

## Continuous Integration

Basic CI is integrated via `github actions` and is currently very simple, utilizing `flight_software_container`:

- Check out `flight_software`
- cd into `extern/elroy_common_msg` and checkout _this_ current branch
- generate messages
- execute `msg_handling_ut`

Checking out the current branch (within `flight_software`) allows developers to test their changes. Utilizing `flight_software` instead of **this** repo is necessary as `elroy_common_msg` does not have build infrastructure.

## Integration into FSW

We think a relatively painless, strategy to integrate this into `FSW` is as follows:

1. Merge code-gen into `elroy_common_msg/main`
2. Integrate into `flight_software`: pre-build step to generate new messages. Merge.
3. Piece-wise integrate/replace current `busRegistry` objects with equivalent `struct/`. Merge.

   1. i.e. replace `busObject<elroy::sensors::AirDataState>` with `busObject<AirDataState>` and integrate new struct.
   2. When we desire to transmit the message, user can simply

   ```cpp
     AirDataStateMessage msg(air_data_state)
     uint8_t buf[msg::GetPackedSize()]; // or buf[AirDataStateMessage::kPackedSize_];
     size_t packed_size = msg.Pack(buf, size);

     transport.send(buf, packed_size);
   ```

4. Once all busRegistry objects are replaced, we can remove `flatbuffers` use in `FSW`
5. Add python support
6. After 5) we can blast `flatbuffers` altogether

### Integration into a project

At the moment, one just needs to add the following paths to your project

```cmake
INCLUDES+=$(TOP)/extern/elroy_common_msg/generated/cpp/include
INCLUDES+=$(TOP)/extern/elroy_common_msg/private/
```

- `extern/elroy_common_msg/generated/cpp/include` new messages are generated alongside the flatbuffers code-gen (this allows for piece-wise integration)
- `exter/elroy_common_msg/private` is added as no build-system infrastructure is added directly to `elroy_common_msg`

## Contributing

A software engineer well versed in code gen might think my implementation cute and juvenile, it's probably not _how_ code-gen should necessarily be done. Utilized dictionaries in a verbose way for intelligibility, though lots of typing.

### generator repo

```text
scripts
├── elroy_common_msg_generator.py
├── schema_validator.py
└── templates
    ├── cpp
    │   ├── bus_object_message.h
    │   ├── enumerates.jinja2
    │   ├── message_header.jinja2
    │   ├── msg_decoder.jinja2
    │   ├── msg_handler.jinja2
    │   ├── msg_handling_ut.jinja2
    │   ├── schema_model.yaml
    │   └── structs_header.jinja2
    └── py
        ├── message.py
        └── py_msg_definition.jinja2
```

- `elroy_common_msg_generator.py`
  - Code gen is written in python. No classes, just statically defined functions.
  - No CLI arguments (at present)
  - Currently only C++ support
  - Generation process:
    1. Validate shape of schema files (sanity checks required/optional keys)
    2. Process each schema: fill out and add additional metadata for generation
    - Original user-entered schema is left unchanged
    - additional data required for generation gets put into a `metadata` dictionary,
      which itself is appended to the user-entered schema
    3. Link schemas: link structs/enum used as data members of messages/structs
    4. Generate: generate the header file
- `schema_validator.py`
  - Utilizes the module `cerberus` for schema validation
  - This requires the shape of the schema to expressed (see file, pretty easy to follow)
  - Nothing really special here, for the most part it's just validating the keys and dictionary shapes
  - function gets called by `elroy_common_msg_generator.py`
- `templates/cpp`
  - `jinja2` template files live here
    - `bus_object_header.jinja2` the base class busObject header file
    - `enumerates.jinja2` enumerates template
    - `structs_header.jinja2` structs template
    - `msg_decoder.jinja2` common message decoder
    - `msg_handler.jinja2` common message handler interface
    - `msg_handling_ut.jinja2` UT for message decoding/handling
  - `schema_model.yaml` a yaml file that describes each schema, used for testing
- `templates/py` python template files

### schema metadata

Internally, `elroy_common_msg_generator.py` processes input schema and adds additional information into a `metadata` dictionary, appended to the end of a given schema. The following metadata is currently being utilized:

```yaml
metadata:
  inherit:
    children: None # True/False - if this class is a parent class (others inherit it)
  members:
    - name: None # copy of name above
      array_len: None # length of array. [X] removed from `type` above on processing
      type: None
      default: None
  file:
    name: None # name of file, no extension
    path: None # aboslute path of the file, no extension
    header:
      extension: ".h"
      include_guard: # header file include guard
      includes: # list of header include files: parent classes, enums, etc
        - None
        - None
```

Note: Inherit and members is not included for `enumerates` metadata.

#### VSCode

Recommend the extension `Better Jinja` for syntax highlighting, makes reading the template file easier.

## Future Features

Very quickly we'll want to add the following features

- Version Number
  - Semantic version numbering; major, minor, patch
  - integrate `git hooks` to increment version number on merge to `main`
- ~~Stream encoder/decoder (useful for data logging, data reception, etc)~~
- ~~Enumerates string equivalent (useful for human readability, debugging)~~
- Bit-field support:
  - backed by fixed-width integers
  - Getter/setters will take in a bit-mask argument `SetThisBitField(\<type> bit_mask)`, `GetThisBitField(\<type> bit_mask)`
- Unit-testing
  - ~~msg decoding and handling~~
  - packing/unpacking validating w/ data types
- ~~Basic CI setup~~

## Rationale for in-house generation

1. Communication is essential to how we exchange information, owning & unifying our ability to communicate is (opinionated) essential for the success
   1. Code Re-use:
   - Messages, structs, and enumerates become re-usable across all SW
   2. Easy inter- intra- process/system communication:
   - Auto-generated serialization, data structs, and enumerates makes communication simple: generated, `.Pack` and send.
   3. Simple API:
   - Messages and struct API allow for simple serialization: `myMessage.Pack(array, size);`
2. We must define a schema for `flatbuffers` which ends up very-nearly (if not exactly) mirroring data-types used across FSW (which are hand-typed). On top of this duplicated schema, we still have to manually utilize the flatbuffer (bullet 3 below). Why not define a schema that makes serialization right into our regular usage?
3. Utilizing `elroy_common_msg` defined enumerates, structs, and messages will allow the various layers of the system to be de-coupled
   - Drivers can live near the hardware, upper layers need-to-know about them
   - GNC input/output can also be de-coupled; currently, the heavy use of GNC auto-code within the various layers of FSW causes more pain than any efficiencies warrants.
4. Use of `flatbuffers` required (at least) x2 the amount of work to send data off-board and didn't lend itself to scalability; creation of data structures (busRegistry objects) used in code, then manual translation into a flatbuffer.

   1. the recipe book for utilizing a `flatbuffers` would be:

      - create data structure in FSW
      - create `flatbuffer` in `elroy_common_msg` schema
      - For data logging: an extra class and call-back (translation function) is necessary to adequately map a `busRegistry` object to a `flatbuffer`--cumbersome and not scalable
      - hand-write translation from data struct → flatbuffer (everywhere you use it)

        - this translation being tedious, comparing flatbuffers vs in-house autogen
          <details>
          <summary> handwritten flatbuffer serialization example: fogInsStateBus</summary>

          ```text
            KvhGeoFogState state;

            BusRegistry::FogInsStateBus.Read(state);

            /// build base
            auto base_msg_fb = Ecm::CreateBaseMsgTable(builder);

            const Ecm::Vec3fStruct velocity(state.velocity[0],
              state.velocity[1],
              state.velocity[2]);
            const Ecm::Vec3fStruct body_accel(state.body_acceleration[0],
              state.body_acceleration[1],
              state.body_acceleration[2]);
            const Ecm::Vec3fStruct orientation(state.orientation[0],
              state.orientation[1],
              state.orientation[2]);
            const Ecm::Vec3fStruct angular_velocity(state.angular_velocity[0],
              state.angular_velocity[1],
              state.angular_velocity[2]);
            const Ecm::Vec3fStruct std_deviation(state.standard_deviation[0],
              state.standard_deviation[1],
              state.standard_deviation[2]);

            Ecm::KvhGeoFogInsStateStruct ins_state_struct(
                state.system_status.r,
                state.filter_status.r,
                state.unix_time_seconds,
                state.microseconds,
                state.latitude,
                state.longitude,
                state.height,
                velocity,
                body_accel,
                state.g_force,
                orientation,
                angular_velocity,
                std_deviation);

            // create KvhGeoFogInsStateMsgTable
            auto ins_msg_fb = Ecm::CreateKvhGeoFogInsStateMsgTable(builder,
              base_msg_fb, &ins_state_struct);

            /// get offset for KvhGeoFogInsStateMsgTable type
            // create vector of KvhGeoFog Union
            auto ins_msg_fb_union = ins_msg_fb.Union();
            auto ins_msg_vec_fb = builder.CreateVector(&ins_msg_fb_union, 1);

            uint8_t msg_type[] = {
              static_cast<uint8_t>(
                Ecm::C1Sn1::DataLogger::DataLoggerUnion::kvh_geo_fog_ins_state)};
            auto data_type_vec = builder.CreateVector(msg_type, 1);

            // finally create data logger msg
            auto data_log_msg = Ecm::C1Sn1::DataLogger::CreateDataLoggerMsgTable(
              builder, data_type_vec, ins_msg_vec_fb);

            builder.Finish(data_log_msg);

            LogDataBuffer(builder.GetBufferPointer(), builder.GetSize());
          ```

          </details>

          <details>
          <summary> In-house code gen serialization: fogInsState</summary>

          ```cpp
            virtual size_t Pack(uint8_t* dest, size_t dest_size) noexcept override {
              assert(dest_size <= GetPackedSize());
              size_t bytes_packed = 0;
              bytes_packed += BusObjectMessage::Pack(&dest[bytes_wrriten], dest_size);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], system_status_);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], filter_status_);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], unix_time_seconds_);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], microseconds_);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], latitude_);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], longitude_);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], velocity_[0]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], velocity_[1]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], velocity_[2]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], body_acceleration_[0]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], body_acceleration_[1]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], body_acceleration_[2]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], g_force_);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], orientation_[0]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], orientation_[1]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], orientation_[2]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], angular_velocity_[0]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], angular_velocity_[1]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], angular_velocity_[2]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], standard_deviation_[0]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], standard_deviation_[1]);
              bytes_packed += MsgPackingUtil::Pack(&dest[bytes_packed], standard_deviation_[2]);
              return bytes_packed
            }
          ```

          </details>

5. In general, the more we built upon `flatbuffers` the more we will become indebted to it and how it doesn't necessarily satisfy our scaling, development, or maintenance needs.
   - A concrete example is the desire to log data in future flight campaigns. Supporting this will be mucho not fun.
6. `flatbuffers` was written for streaming a lot of data efficiently; our system is very slow and this efficiency is unwarranted given the added inconvenience of usability/scalability.
7. There are a handful of scalability and maintenance issues with the current code-base, further exacerbated by use of `flatbuffers`.
