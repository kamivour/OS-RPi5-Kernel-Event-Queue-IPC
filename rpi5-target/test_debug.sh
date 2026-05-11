#!/bin/bash
# Test script với debug output để thấy internal processing

echo "=== DEBUG MODE TEST ==="
echo

# Load debug version
echo "[1] Loading DEBUG module..."
sudo insmod event_driver_debug.ko

# Show device created
echo "[2] Device node:"
ls -l /dev/pi5_event
echo

# Clear kernel log
echo "[3] Clearing dmesg..."
sudo dmesg -c > /dev/null
echo

# Test write
echo "[4] Writing 'TEST_MSG' to device..."
echo "TEST_MSG" > /dev/pi5_event
echo

# Show kernel log (internal processing)
echo "[5] Kernel log (internal processing):"
sudo dmesg | grep pi5_event
echo

# Test read
echo "[6] Reading from device..."
cat /dev/pi5_event | od -c
echo

# Show kernel log after read
echo "[7] Kernel log after read:"
sudo dmesg | grep pi5_event
echo

# Test with timer
echo "[8] Testing with timer..."
sudo rmmod event_driver_debug
sudo insmod event_driver_debug.ko timer_enabled=1
echo "Timer enabled, watching logs for 10 seconds..."
sleep 10
sudo dmesg | grep pi5_event | tail -20
echo

# Cleanup
echo "[9] Unloading module..."
sudo rmmod event_driver_debug
sudo dmesg | grep pi5_event | tail -5
echo

echo "=== TEST COMPLETE ==="
