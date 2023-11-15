# elroy_common_msg Python Generator

Currently, the python generator makes use of the python [`construct`](https://construct.readthedocs.io/en/latest/intro.html) package. It creates `construct` containers for each elroy_common_msg enum, struct, and bus object message. These constructs can be used to easily serialize and deserialize elroy_common_msg types. Here's an example:

```python
>>> # Serialize a CoolingSystemStatus message into a byte stream
>>> CoolingSystemStatus.build({'cooling_temp_deg_c': 52.535, 'cooling_pressure_psi': 32.35, 'cooling_flowrate_lpm': 12.345 })
b'\xd7#RBff\x01B\x1f\x85EA'
```

```python
>>> # Deserialize a CoolingSystemStatus message from a byte stream into a dict
>>> CoolingSystemStatus.parse(b'\xd7#RBff\x01B\x1f\x85EA')
Container(cooling_temp_deg_c=52.53499984741211, cooling_pressure_psi=32.349998474121094, cooling_flowrate_lpm=12.345000267028809)
```

## Building `BusObject` messages

See [here](/scripts/python_generator/serialize_test.py) for an example of building up a `BusObject` in python.

## Parsing elroy_common_msg messages from the wire

The `BusObject` construct type is special in that it can parse any elroy_common_msg message as it came across the wire. It's smart enough to check the message ID and use the proper constructs to deserialize any message type. Here's an example:

```python
>>> raw_bytes = received_data_from_udp_port()
>>> BusObject.parse(raw_bytes)
Container:
        id = 7
        write_timestamp = 0
        msg = Container:
            component = Container:
                component = (enum) Lidar 11
                instance = (enum) Two 1
                location = (enum) Port 0
            report = Container:
                health_level = (enum) Advisory 1
                results = ListContainer:
                    Advisory
                    Advisory
                    Advisory
                    Advisory
                    Advisory
                    Advisory
                    Advisory
```
