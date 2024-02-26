Import('env')
import os
import shutil
import gzip

def esp32_create_bin_gzip(source, target, env):
    # create string with location and file names
    bin_file = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    bin_factory_file = env.subst("$BUILD_DIR/${PROGNAME}.factory.bin")
    gzip_file = env.subst("$BUILD_DIR/${PROGNAME}.bin.gz")
    gzip_factory_file = env.subst("$BUILD_DIR/${PROGNAME}.factory.bin.gz")

    # check if new target files exist and remove if necessary
    if os.path.isfile(gzip_file): os.remove(gzip_file)
    if os.path.isfile(gzip_factory_file): os.remove(gzip_factory_file)

    # write gzip firmware file
    print("Compressing firmware.bin for upload...")
    with open(bin_file,"rb") as fp:
        with gzip.open(gzip_file, "wb", compresslevel = 9) as f:
            shutil.copyfileobj(fp, f)

    ORG_FIRMWARE_SIZE = os.stat(bin_file).st_size
    GZ_FIRMWARE_SIZE = os.stat(gzip_file).st_size

    print("Compression reduced the firmware.bin size by {:.0f}% (was {} bytes, now {} bytes)".format((GZ_FIRMWARE_SIZE / ORG_FIRMWARE_SIZE) * 100, ORG_FIRMWARE_SIZE, GZ_FIRMWARE_SIZE))

    print("Compressing firmware.factory.bin for upload...")
    with open(bin_factory_file,"rb") as fp:
        with gzip.open(gzip_factory_file, "wb", compresslevel = 9) as f:
            shutil.copyfileobj(fp, f)


    ORG_FIRMWARE_SIZE = os.stat(bin_factory_file).st_size
    GZ_FIRMWARE_SIZE = os.stat(gzip_factory_file).st_size

    print("Compression reduced the combined fimrware.factory.bin size by {:.0f}% (was {} bytes, now {} bytes)".format((GZ_FIRMWARE_SIZE / ORG_FIRMWARE_SIZE) * 100, ORG_FIRMWARE_SIZE, GZ_FIRMWARE_SIZE))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_bin_gzip)