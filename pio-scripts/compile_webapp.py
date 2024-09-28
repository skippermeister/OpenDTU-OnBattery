import os
import hashlib
import pickle
import subprocess

def check_files(directories, filepaths, hash_file):
    old_file_hashes = {}
    file_hashes = {}

    for directory in directories:
        for root, dirs, filenames in os.walk(directory):
            for file in filenames:
                filepaths.append(os.path.join(root, file))

    for file_path in filepaths:
        with open(file_path, 'rb') as f:
            file_data = f.read()
        file_hashes[file_path] = hashlib.md5(file_data).hexdigest()

    if os.path.exists(hash_file):
        with open(hash_file, 'rb') as f:
            old_file_hashes = pickle.load(f)

    update = False
    for file_path, file_hash in file_hashes.items():
        if file_path not in old_file_hashes or old_file_hashes[file_path] != file_hash:
            update = True
            break

    if not update:
        print("INFO: webapp artifacts should be up-to-date")
        return

    print("INFO: compiling webapp (hang on, this can take a while and there might be little output)...")

    yarn = "yarn"
    try:
        subprocess.check_output([yarn, "--version"], shell=True)
    except FileNotFoundError:
        yarn = "yarnpkg"
        try:
            subprocess.check_output([yarn, "--version"], shell=True)
        except FileNotFoundError:
            raise Exception("it seems neither 'yarn' nor 'yarnpkg' is installed/available on your system")

    # if these commands fail, an exception will prevent us from
    # persisting the current hashes => commands will be executed again
    subprocess.run([yarn, "--cwd", "webapp", "install", "--frozen-lockfile"], shell=True,
                   check=True)

    subprocess.run([yarn, "--cwd", "webapp", "build"], shell=True, check=True)

    with open(hash_file, 'wb') as f:
        pickle.dump(file_hashes, f)

def main():
    if os.getenv('GITHUB_ACTIONS') == 'true':
        print("INFO: not testing for up-to-date webapp artifacts when running as Github action")
        return 0

    print("INFO: testing for up-to-date webapp artifacts")

    directories = ["webapp/src/", "webapp/public/"]
    files = ["webapp/index.html", "webapp/tsconfig.config.json",
             "webapp/tsconfig.json", "webapp/vite.config.ts",
             "webapp/yarn.lock", "webapp/package.json"]
    hash_file = "webapp_dist/.hashes.pkl"

    check_files(directories, files, hash_file)

main()
