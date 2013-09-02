echo insmod fat.ko
insmod fat.ko
echo insmod msdos.ko
insmod msdos.ko
echo mount -t msdos /dev/loop0 ./testbed
mount -t msdos /dev/loop0 ./testbed

