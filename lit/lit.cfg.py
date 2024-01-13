import lit.formats
import os

config.name = 'thorin regression'
config.test_format = lit.formats.ShTest(True)

config.suffixes = ['.thorin']

config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.my_obj_root, 'test')

config.substitutions.append(('%thorin', config.thorin))

# inhert env vars
config.environment = os.environ

config.available_features.add("always")
