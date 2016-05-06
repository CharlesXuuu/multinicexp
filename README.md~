# multinicexp

Step 1: in domu compile the source code by <br>
$make clean <br>
$make <br>

Step 2: in domu insert the kernel module mndomu.ko by <br>
$insmod mndomu.ko filename="/path/to/file" ip="target ip" port=[port number]<br>

Step 3: in domu check the status by <br>
$dmesg <br>

Step 4: in domu find the printed message "gref=XXX" <br>

<br><br>

Step 5: in dom0 compile the source code by <br>
$make clean <br>
$make <br>

Step 6: in dom0 insert the kernel module mndom0.ko by <br>
$insmod mndom0.ko gref=XXX domu=[domu id]<br>

Step 7: in dom0 check the status by <br>
$dmesg <br>

Step 8: The message in domu file "/path/to/file" is read and shown by dom0 <br>

Step 9: Clean up by <br>
$rmmod mndom0.ko  (in dom0) <br>
$rmmod mndomu.ko (in domu) <br>
