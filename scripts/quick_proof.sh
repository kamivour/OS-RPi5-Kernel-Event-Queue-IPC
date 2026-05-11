#!/bin/bash
# Quick proof for Q&A - "Is this real kernel code?"

echo "=== PROOF: Real Kernel Code ==="
echo
echo "Before:"
lsmod | grep event || echo "  ✗ No module"
ls /dev/pi5_event 2>/dev/null || echo "  ✗ No device"
echo
echo "Loading kernel module..."
sudo insmod event_driver.ko 2>/dev/null || sudo insmod ~/os-project/event_driver.ko
echo
echo "After:"
echo "  ✓ Kernel module:"
lsmod | grep event
echo "  ✓ Device node:"
ls -l /dev/pi5_event
echo "  ✓ Kernel log:"
sudo dmesg | grep pi5_event | tail -1
echo
echo "💡 Only kernel code creates device nodes & appears in dmesg!"
