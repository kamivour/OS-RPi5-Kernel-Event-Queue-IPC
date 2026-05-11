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
7. **Blocking queue demonstration**
8. Killer proof: crash = system freeze

**Time:** ~2 minutes

---

### test_blocking.sh
**Blocking queue test** - Demonstrates proper blocking behavior

```bash
./scripts/test_blocking.sh
```

**What it shows:**
1. Queue filled to capacity (32 events)
2. Producer blocks when queue is full
3. Consumer frees space
4. Producer wakes up and completes

**Time:** ~1 minute

---

### test_stress.sh
**Multiple producer stress test** - Tests concurrent blocking writes

```bash
./scripts/test_stress.sh
```

**What it shows:**
1. Queue filled to capacity
2. 5 producers start simultaneously
3. All 5 producers blocked
4. Space freed → producers wake up
5. All producers complete

**Time:** ~1 minute

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

## Test Results

### test_results.log
**Complete test output** - Contains full test execution results

```bash
cat scripts/test_results.log
```

**Contains:**
- TEST 1: Basic Blocking Queue - PASSED ✓
- TEST 2: Multiple Producer Stress - PASSED ✓
- Platform info (RPi5, ARM64, kernel version)
- Detailed step-by-step output

**Last run:** See timestamp in log file

**Run on RPi5 to generate new results:**
```bash
cd ~/os-project
./run_tests.sh > test_results.log 2>&1
```

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
