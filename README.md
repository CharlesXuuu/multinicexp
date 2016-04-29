# multinicexp

Step 1: in domu compile the source code by <br>
$make clean <br>
$make <br>

Step 2: in domu insert the kernel module xen-eg.ko by <br>
$insmod xen-eg.ko <br>

Step 3: in domu check the status by <br>
$dmesg <br>

Step 4: in domu find the printed message "gref=XXX" <br>

<br><br>

Step 5: in dom0 compile the source code by <br>
$make clean <br>
$make <br>

Step 6: in dom0 insert the kernel module dom-.ko by <br>
$insmod dom-.ko gref=XXX <br>

Step 7: in dom0 check the status by <br>
$dmesg <br>

Step 8: The message in written by domu "chix:1234567890ABCDEF" is read and shown by dom0 <br>

Step 9: Clean up by <br>
$rmmod dom-.ko  (in dom0) <br>
$rmmod xen-eg.ko (in domu) <br>
