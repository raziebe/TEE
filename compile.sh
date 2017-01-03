make -j4 CROSS_COMPILE=/opt/truly/gcc-linaro-4.9-2015.02-3-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu- ARCH=arm64  Image.gz
#make -j4 CROSS_COMPILE=/opt/truly/gcc-linaro-4.9-2015.02-3-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu- ARCH=arm64  O=/opt/truly/OBJ-truly/ Image.gz

scp arch/arm64/boot/Image.gz linaro@192.168.2.126:/boot/vmlinuz-4.4.11-trly+
ssh  linaro@192.168.2.126
