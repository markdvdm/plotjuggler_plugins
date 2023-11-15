import cerberus
import sys
import yaml
from beeprint import pp

# The schema_models defines the expected shape of the three
# schems (class, struct, enum) per cerberus, a schema validator.
# This information is duplicated in the comment in each of the
# schema yaml files.
schema_models = [
  {
    'class' : {
      'type': 'dict',
      'schema' : {
        'name' : {
          'type' : 'string',
          'required' : True
        },
        'namespace' : {
          'type' : 'string',
          'required' : False
        },
        'inherit' : {
          'type' : 'dict',
          'required' : False,
          'schema' : {
            'name' : {
              'type' : 'string',
              'required' : True
            }
          }
        },
        'members' : {
          'type' : 'list',
          'required' : False,
          'schema' : {
              'type' : 'dict',
              'schema' : {
                'name' : {
                  'type' : 'string',
                  'required' : True
                },
                'type' : { 
                  'type' : 'string',
                  'required' : True
                },
                'default' : {
                  'type' : ['boolean', 'number', 'string'],
                  'required' : False
                }
              }
            }
        }
      }
    }
  },
  {
    'struct' : {
      'type': 'dict',
      'schema' : {
        'name' : {
          'type' : 'string',
          'required' : True
        },
        'namespace' : {
          'type' : 'string',
          'required' : False
        },
        'inherit' : {
          'type' : 'dict',
          'required' : False,
          'schema' : {
            'name' : {
              'type' : 'string',
              'required' : True
            }
          }
        },
        'members' : {
          'type' : 'list',
          'required' : False,
          'schema' : {
              'type' : 'dict',
              'schema' : {
                'name' : {
                  'type' : 'string',
                  'required' : True
                },
                'type' : { 
                  'type' : 'string',
                  'required' : True
                },
                'default' : {
                  'type' : ['boolean', 'number', 'string'],
                  'required' : False
                }
              }
            }
        }
      }
    }
  },
  {
    'enum' : {
    'type' : 'dict',
    'required' : True,
    'schema' : {
      'name' : {
        'type' : 'string',
        'required' : True
      },
      'namespace' : {
        'type' : 'string',
        'required' : False
      },
      'type' : {
        'type' : 'string',
        'required' : False
      },
      'list' : {
        'type' : 'list',
        'required' : True,
        'schema' : {
          'type': 'string'
        }
      },
      'brief_list' : {
        'type' : 'list',
        'required' : False,
        'schema' : {
          'type': 'string'
        }
      },
      'elaboration_list' : {
        'type' : 'list',
        'required' : False,
        'schema' : {
          'type': 'string'
        }
      }
    }
  }
}
]

def validate_schema(schema_type, schema_uut) -> [bool, dict]:
  """
  validates the schema
  @param[in] schema_type - 'class', 'struct', 'enum'
  @param[out] schema_uut - the schema unit under test
  @return true if the schema is well formed, false other-wise
  """

  validator = cerberus.Validator()
  if schema_type in 'class': 
    validity = validator.validate(schema_uut, schema_models[0])
  elif schema_type in 'struct' : 
    validity = validator.validate(schema_uut, schema_models[1])
  elif schema_type in 'enum' :
    validity = validator.validate(schema_uut, schema_models[2])
  else:
    sys.exit("Invalid schema_type got " + str(schema_type) + " but expected one of ['class', 'struct', 'enum']")

  if validity is False:
    return False, validator.errors

  return True, validator.errors

if __name__ == '__main__':
  sys.exit("model_validator not scriptable")