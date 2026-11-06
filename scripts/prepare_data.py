import os
import gzip
import shutil

try:
    Import("env")
    PROJECT_DIR = env.get("PROJECT_DIR", os.getcwd())
except Exception:
    PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

def prepare_filesystem(*args, **kwargs):
    src_dir = os.path.join(PROJECT_DIR, "server")
    if not os.path.exists(src_dir):
        src_dir = os.path.join(PROJECT_DIR, "cadena")
    dst_dir = os.path.join(PROJECT_DIR, "data")

    if not os.path.exists(src_dir):
        return

    if os.path.exists(dst_dir):
        shutil.rmtree(dst_dir)
    os.makedirs(dst_dir, exist_ok=True)

    # Limpiar y regenerar data/ con compresión gzip transparente
    for root, dirs, files in os.walk(src_dir):
        rel_root = os.path.relpath(root, src_dir)
        target_dir = os.path.join(dst_dir, rel_root) if rel_root != "." else dst_dir
        os.makedirs(target_dir, exist_ok=True)

        for file_name in files:
            # Omitir el script del servidor python local
            if file_name == "serve.py":
                continue

            src_file = os.path.join(root, file_name)
            with open(src_file, "rb") as f_in:
                content = f_in.read()

            # Comprimir con gzip
            compressed = gzip.compress(content, 9)

            # Si la compresión ahorra espacio, guardar como .gz
            if len(compressed) < len(content):
                target_file = os.path.join(target_dir, file_name + ".gz")
                with open(target_file, "wb") as f_out:
                    f_out.write(compressed)
            else:
                target_file = os.path.join(target_dir, file_name)
                with open(target_file, "wb") as f_out:
                    f_out.write(content)

    src_name = os.path.basename(src_dir)
    print(f"\n[prepare_data] Carpeta 'data' sincronizada y optimizada con éxito desde '{src_name}'")

if __name__ == "__main__":
    prepare_filesystem()

try:
    env.AddPreAction("$BUILD_DIR/littlefs.bin", prepare_filesystem)
    env.AddPreAction("uploadfs", prepare_filesystem)
    env.AddPreAction("buildfs", prepare_filesystem)
except Exception:
    pass
