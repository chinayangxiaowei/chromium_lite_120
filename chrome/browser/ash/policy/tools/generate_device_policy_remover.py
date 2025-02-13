# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create function which removes the specified device policies
from ChromeDeviceSettingsProto.
This function is primarily intended to be used
for the implementation of the DeviceOffHours policy.
"""

from optparse import OptionParser
import sys

file_header = """//
// DO NOT MODIFY THIS FILE DIRECTLY!
// IT IS GENERATED BY generate_device_policy_remover.py
// FROM chrome_device_policy_pb2.py
//

#include "chrome/browser/ash/policy/core/device_policy_remover.h"

namespace policy {

void RemovePolicies(enterprise_management::ChromeDeviceSettingsProto* policies,
                    const std::vector<int>& policy_proto_tags_to_remove) {
  for (const int tag : policy_proto_tags_to_remove) {
    switch(tag) {
"""

file_footer = """    }
  }
}
} // namespace policy
"""

def main():
  parser = OptionParser(usage=__doc__)
  (opts, args) = parser.parse_args()
  off_hours_cleaner_path = args[0]
  descriptor_pool_path = args[1]
  symbol_database_path = args[2]
  chrome_device_policy_pb2_path = args[3]
  sys.path.insert(0, descriptor_pool_path)
  sys.path.append(symbol_database_path)
  sys.path.append(chrome_device_policy_pb2_path)
  # Make reload google library
  # which might be already loaded due to Google App Engine
  # TODO(crbug.com/764314): find better solution how to import protobuf.
  import google.protobuf
  # Python 3 doesn't expose a global `reload`.
  if sys.version_info.major == 2:
    reload(google)
    reload(google.protobuf)
  else:
    import importlib
    importlib.reload(google)
    importlib.reload(google.protobuf)
  from chrome_device_policy_pb2 import ChromeDeviceSettingsProto
  with open(off_hours_cleaner_path, 'wt') as file:
    file.write(file_header);
    for field in ChromeDeviceSettingsProto.DESCRIPTOR.fields:
      file.write('      case {proto_tag}:\n'
                 '        policies->clear_{name}();\n'
                 '        break;\n'
                 .format(proto_tag=field.number, name=field.name.lower()))
    file.write(file_footer);
  return 0

if __name__ == '__main__':
  sys.exit(main())
