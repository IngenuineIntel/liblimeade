#!/usr/bin/bash
# this script facilitates the tests in `test_client.c` and `test_host.c`.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <https://unlicense.org/>

clear
echo ""
echo "    LIBLIMEADE v0.1 TEST SCRIPT"
echo ""
echo "How it works:"
echo ""
echo ""
echo "1: The C programs are compiled."
echo "2: A dummy user (limeade_test) is created."
echo "3: SSHd is started and configured to accomodate a limeade system."
echo "   This includes keys and subsystem configs."
echo "4: The C programs are executed and exit after a certain point."
echo "5: The dummy user is deleted."
echo ""
echo "Note: sudo will be required and you will have to put in your password for it"
echo ""
echo ""
echo ""
echo "press ENTER to continue"
read > /dev/null

# STEP 1: compile C programs
make so -C ..
gcc -o liblimeade_test_client test_client.c -L.. -lliblimeade -I../include
gcc -o liblimeade_tesT_host test_host.c -L.. -lliblimeade -I../include

# STEP 2: create a dummy user
# in theory this would be user `limeade` or something adjacent, so here we'll put:
sudo useradd limeade_test
echo "limeade_test_password" | sudo passwd --stdin limeade_test

# STEP 3: prepare SSH
