# Drives msp430_run --gdb from msp430-elf-gdb (see run_tests.sh); each echo marks a result to check.
set pagination off
set confirm off
target remote :2331
echo connected-pc:\n
print/x $pc
break add3
continue
echo bp1-x:\n
print $r12
continue
echo bp2-x:\n
print $r12
delete
watch last
continue
echo watch-last:\n
print last
delete
stepi
echo stepped\n
print/x $pc
set var counter = 100
break add3
continue
echo after-set-x:\n
print $r12
monitor cycles
detach
