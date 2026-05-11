# Demo Scripts

## Scripts

### demo_proof.sh
**Full presentation demo** - Proves this is real kernel code

```bash
./scripts/demo_proof.sh
```

**What it shows:**
1. Before/after comparison
2. Module loaded in kernel (lsmod)
3. Device node created (/dev/pi5_event)
4. Kernel log messages (dmesg)
5. Module information (modinfo)
6. Live read/write demo
7. Killer proof: crash = system freeze

**Time:** ~2 minutes

---

### quick_proof.sh
**Quick Q&A proof** - One-liner demonstration

```bash
./scripts/quick_proof.sh
```

**What it shows:**
- Module appears in lsmod
- Device node appears
- dmesg shows kernel messages

**Time:** ~15 seconds

---

## Usage Tips

### For Presentation
```bash
# Run on RPi5
cd ~/os-project
sudo ../scripts/demo_proof.sh
```

### For Q&A
If someone asks "Is this real kernel code?":
```bash
sudo ../scripts/quick_proof.sh
```

### Key Talking Points During Demo

| Script Point | Say This |
|--------------|----------|
| lsmod shows module | "Our code is running in kernel space" |
| Device node created | "Only kernel can create /dev nodes" |
| dmesg shows messages | "Only kernel code writes to dmesg" |
| System panic on crash | "Userspace crash kills app, kernel crash kills system" |

## Visual Aids

### What to Project
1. Terminal running demo_proof.sh
2. Separate terminal with `htop` (show CPU at 0% when waiting)
3. Diagram from docs/PROJECT_DOCUMENTATION.md

### Backup Commands
If script fails, manual commands:
```bash
# Load
sudo insmod event_driver.ko

# Show proof
lsmod | grep event
ls -l /dev/pi5_event
sudo dmesg | grep pi5_event

# Test
echo "TEST" > /dev/pi5_event
cat /dev/pi5_event

# Cleanup
sudo rmmod event_driver
```
