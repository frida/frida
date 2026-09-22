import os
import shutil
import sys

src = sys.argv[1]
install_subdir = sys.argv[2]
install_name = sys.argv[3]

destdir_prefix = os.environ["MESON_INSTALL_DESTDIR_PREFIX"]
dst_dir = os.path.join(destdir_prefix, install_subdir)
dst = os.path.join(dst_dir, install_name)

os.makedirs(dst_dir, exist_ok=True)
shutil.copy2(src, dst)
print(f"Installing {os.path.basename(src)} to {dst_dir}/{install_name}")
