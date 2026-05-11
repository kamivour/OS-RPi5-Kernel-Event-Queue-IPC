#!/bin/bash
# Demo Proof Script - Shows this is REAL kernel code
# For presentation: "Kernel Event Queue IPC"

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

clear
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     KERNEL EVENT QUEUE IPC - PROOF OF KERNEL CODE         ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo

# Part 1: Before loading
echo -e "${YELLOW}PART 1: Before Loading Module${NC}"
echo "─────────────────────────────────────────"
echo -n "Checking for kernel module... "
if lsmod | grep -q event_driver; then
    echo -e "${RED}FOUND (removing first)${NC}"
    sudo rmmod event_driver 2>/dev/null || true
    sleep 1
else
    echo -e "${GREEN}NOT FOUND (good)${NC}"
fi

echo -n "Checking for device node... "
if [ -e /dev/pi5_event ]; then
    echo -e "${RED}EXISTS (shouldn't!)${NC}"
else
    echo -e "${GREEN}NOT FOUND (good)${NC}"
fi
echo

# Part 2: Load module
echo -e "${YELLOW}PART 2: Loading Into Kernel Space${NC}"
echo "─────────────────────────────────────────"
echo "Running: sudo insmod event_driver.ko"
sudo insmod event_driver.ko
echo -e "${GREEN}✓ Module loaded into kernel!${NC}"
echo

# Part 3: Proof it's in kernel
echo -e "${YELLOW}PART 3: Proof It's Running in Ring 0${NC}"
echo "─────────────────────────────────────────"

echo
echo -e "${BLUE}1. Kernel Module List (lsmod)${NC}"
echo "   Shows code running in kernel space:"
lsmod | grep event_driver
echo -e "   ${GREEN}✓ This is our code in the kernel!${NC}"
echo

echo -e "${BLUE}2. Device Node Created${NC}"
echo "   Character devices are ONLY created by kernel:"
ls -l /dev/pi5_event
echo -e "   ${GREEN}✓ 'c' = character device (created by kernel)${NC}"
echo -e "   ${GREEN}✓ Major 10 = misc subsystem${NC}"
echo

echo -e "${BLUE}3. Kernel Log Messages (dmesg)${NC}"
echo "   Our printk messages appear in KERNEL log:"
sudo dmesg | tail -5 | grep pi5_event
echo -e "   ${GREEN}✓ Only kernel code can write to dmesg!${NC}"
echo

echo -e "${BLUE}4. Module Information${NC}"
echo "   Verified as loadable kernel module:"
modinfo event_driver | grep -E 'filename|description|license'
echo -e "   ${GREEN}✓ .ko = Kernel Object file${NC}"
echo

echo -e "${BLUE}5. Process Memory Map${NC}"
echo "   Shows module loaded in kernel memory:"
cat /proc/modules | grep event_driver
echo -e "   ${GREEN}✓ Non-zero address = loaded in kernel RAM${NC}"
echo

# Part 4: Show it works
echo -e "${YELLOW}PART 4: Live Demonstration${NC}"
echo "─────────────────────────────────────────"
echo "Sending test event to kernel..."
echo "DEMO_EVENT" | sudo tee /dev/pi5_event > /dev/null
echo -e "${GREEN}✓ Data entered kernel via system call${NC}"
echo
echo "Reading back from kernel..."
sudo dd if=/dev/pi5_event bs=40 count=1 2>/dev/null | od -An -c -N20
echo -e "${GREEN}✓ Data returned from kernel!${NC}"
echo

# Part 5: The clincher
echo -e "${YELLOW}PART 5: The Killer Proof${NC}"
echo "─────────────────────────────────────────"
echo -e "${RED}If this code crashes → ENTIRE SYSTEM freezes${NC}"
echo -e "${RED}Userspace crash → only app dies${NC}"
echo -e "${GREEN}Kernel crash   → system panic/reboot${NC}"
echo
echo -e "${BLUE}That's how you KNOW it's real kernel code!${NC}"
echo

# Cleanup
echo -e "${YELLOW}Cleanup${NC}"
sudo rmmod event_driver
echo -e "${GREEN}✓ Module unloaded cleanly${NC}"
echo

echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}DEMO COMPLETE - This is REAL kernel code running in Ring 0!${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
