from pathlib import Path
import subprocess

def run_executable(executable_path, *args):
    try:
        result = subprocess.run([executable_path, *args], check=True, text=True, capture_output=True)
        print("Standard Output:", result.stdout)
        print("Standard Error:", result.stderr)
    except subprocess.CalledProcessError as e:
        print(f"Error running executable: {e}")

BITE_PATH = "cmake-build-debug/bite.exe"

def traverse_directory(path):
    for file_path in Path(path).rglob('*'):
        if file_path.is_file():
            run_executable(BITE_PATH, f"..\\{file_path}")

# Example usage
traverse_directory('tests')

print("hello");