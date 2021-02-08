#!/usr/bin/python

import os
import re
import glob

root_path = os.path.join(os.getenv("HOME"), "Projects")
grab_paths = [
    "host_workspace/stream_utils/**/*.h",
    "host_workspace/stream_utils/**/*.cc",
    "host_workspace/bazel-bin/stream_utils/control_message.pb.*",
    "host_workspace/nanopb/pb_*.h",
    "host_workspace/nanopb/pb_*.c",
    "host_workspace/nanopb/pb.h",
]

file_rename_rules = [('\.cc$', '.cpp')]

exclude_rules = [".*_test\.cc"]


def ShouldExclude(fname):
    for exclude_rule in exclude_rules:
        if re.search(exclude_rule, fname):
            return True
    return False


def MakeOutputName(fname):
    output_name = os.path.basename(fname)
    for regex, replacement in file_rename_rules:
        if not re.search(regex, output_name):
            continue
        output_name = re.sub(regex, replacement, output_name)
    return output_name


if __name__ == "__main__":
    files_to_grab = []
    for path in grab_paths:
        glob_files = glob.glob(os.path.join(root_path, path), recursive=True)
        files_to_grab.extend(glob_files)

    for fname in files_to_grab:
        if ShouldExclude(fname):
            continue

        out_fname = MakeOutputName(fname)
        print("Copying", fname, "to", out_fname)
        with open(out_fname, 'w') as out_file:
            out_file.write(open(fname).read())
