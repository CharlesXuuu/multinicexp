# multinicexp

# Step 1: in domu compile the source code by 
# $make clean
# $make
# Step 2: in domu insert the kernel module xen-eg.ko by 
# $insmod xen-eg.ko
# Step 3: in domu check the status by
# $dmesg
# Step 4: in domu find the printed message "gref=XXX"

# Step 5: in dom0 compile the source code by 
# $make clean
# $make
# Step 6: in dom0 insert the kernel module dom-.ko by
# $insmode dom-.ko gref=XXX
# Step 7: in dom0 check the status by
# $dmesg
# Step 8: The message in written by domu "chix:1234567890ABCDEF" is read and shown by dom0

# Step 9: Clean up by 
# $rmmod dom-.ko  (in dom0)
# $rmmod xen-eg.ko (in domu)
