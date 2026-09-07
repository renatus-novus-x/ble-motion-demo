import os
import sys
import time
import zipfile


class ShiftJISZipInfo(zipfile.ZipInfo):
    def _encodeFilenameFlags(self):
        return self.filename.encode("shift_jis"), self.flag_bits & ~0x800


def add_file(archive, source, name):
    status = os.stat(source)
    info = ShiftJISZipInfo(name)
    info.date_time = time.localtime(status.st_mtime)[:6]
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = (status.st_mode & 0xFFFF) << 16
    with open(source, "rb") as input_file:
        archive.writestr(info, input_file.read())


output_file, xdf_file, license_file = sys.argv[1:4]
with zipfile.ZipFile(output_file, "w") as archive:
    add_file(archive, xdf_file, os.path.basename(xdf_file))
    add_file(archive, license_file, "許諾条件.txt")
