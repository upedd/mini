from pathlib import Path
import subprocess
from datetime import datetime


# TODO: in future this should be written in bite

def run_executable(executable_path, *args):
    try:
        result = subprocess.run([executable_path, *args], check=True, text=True, capture_output=True)
        return result
    except subprocess.CalledProcessError as e:
        print(f"Error running executable: {e}")


BITE_PATH = "cmake-build-debug/bite"
date_string = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
log = open(f"testrunner_{date_string}.log", "w")

success_cnt = 0
error_cnt = 0


def traverse_directory(path):
    global error_cnt
    global success_cnt
    for file_path in Path(path).rglob('*'):
        if file_path.is_file():
            if file_path.suffix == ".bite":
                print(f"Running {file_path.stem} test...")
                result = run_executable(BITE_PATH, f".\\{file_path}")
                f = open(file_path.with_suffix(".out"), "r")
                out = f.read()
                if result.stderr:
                    error_cnt += 1
                    print("Test failed!")
                    log.write(f"Test {file_path.stem} failed for reason: error output.\n")
                    log.write("error:\n")
                    log.write(result.stderr)
                elif out.strip() != result.stdout.strip():
                    error_cnt += 1
                    print("Test failed!")
                    log.write(f"Test {file_path.stem} failed for reason: invalid output.\n")
                    log.write("Expected:\n")
                    log.write(out)
                    log.write('\n')
                    log.write("Got:\n")
                    log.write(result.stdout)
                    log.write('\n\n')
                else:
                    success_cnt += 1
                f.close()


# Example usage
traverse_directory('tests')
print(f"Ran {success_cnt + error_cnt} tests. ")
if error_cnt > 0:
    print(f"{error_cnt} failed. Check logs for more information.")
