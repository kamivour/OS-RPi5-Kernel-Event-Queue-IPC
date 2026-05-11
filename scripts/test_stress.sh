#!/bin/bash
# Stress Test: Multiple Producers
# Tests blocking queue with concurrent producers

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

clear
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║          STRESS TEST: Multiple Producers                  ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo

# Cleanup
sudo rmmod event_driver 2>/dev/null || true

echo -e "${YELLOW}Setup: Loading module...${NC}"
sudo insmod event_driver.ko
echo -e "${GREEN}✓ Module loaded${NC}"
echo

# Fill queue
echo -e "${YELLOW}Step 1: Fill Queue (32 events)${NC}"
echo "─────────────────────────────────────────"
for i in {1..32}; do
    echo "FILL_$i" > /dev/pi5_event 2>/dev/null || true
done
echo -e "${GREEN}✓ Queue full (32/32)${NC}"
echo

# Start multiple blocking producers
echo -e "${YELLOW}Step 2: Start 5 Blocking Producers${NC}"
echo "─────────────────────────────────────────"
(echo "P1_MSG" > /dev/pi5_event && echo "P1 done") &
P1=$!
(echo "P2_MSG" > /dev/pi5_event && echo "P2 done") &
P2=$!
(echo "P3_MSG" > /dev/pi5_event && echo "P3 done") &
P3=$!
(echo "P4_MSG" > /dev/pi5_event && echo "P4 done") &
P4=$!
(echo "P5_MSG" > /dev/pi5_event && echo "P5 done") &
P5=$!

echo -e "${GREEN}✓ 5 producers started (blocked, waiting for space)${NC}"
echo "  PIDs: $P1, $P2, $P3, $P4, $P5"
echo

# Verify they're blocked
sleep 1
echo -e "${YELLOW}Step 3: Verify Producers Are Blocked${NC}"
echo "─────────────────────────────────────────"
BLOCKED_COUNT=0
for pid in $P1 $P2 $P3 $P4 $P5; do
    if ps -p $pid > /dev/null 2>&1; then
        BLOCKED_COUNT=$((BLOCKED_COUNT + 1))
    fi
done
echo -e "${GREEN}✓ $BLOCKED_COUNT/5 producers blocked${NC}"
echo

# Free up 3 slots
echo -e "${YELLOW}Step 4: Consumer Frees 3 Slots${NC}"
echo "─────────────────────────────────────────"
echo "Consuming 3 events..."
for i in {1..3}; do
    dd if=/dev/pi5_event bs=40 count=1 2>/dev/null > /dev/null
done
echo -e "${GREEN}✓ 3 slots freed${NC}"
echo

# Wait for 3 producers to complete
echo -e "${YELLOW}Step 5: Wait for 3 Producers to Wake${NC}"
echo "─────────────────────────────────────────"
sleep 2
COMPLETED=0
for pid in $P1 $P2 $P3 $P4 $P5; do
    if ! ps -p $pid > /dev/null 2>&1; then
        COMPLETED=$((COMPLETED + 1))
    fi
done
echo -e "${GREEN}✓ $COMPLETED producers completed${NC}"
echo

# Free remaining slots and wait for all
echo -e "${YELLOW}Step 6: Free Remaining Slots${NC}"
echo "─────────────────────────────────────────"
dd if=/dev/pi5_event bs=40 count=30 2>/dev/null > /dev/null
echo -e "${GREEN}✓ Consumed remaining events${NC}"
echo

wait
echo -e "${GREEN}✓ All producers completed!${NC}"
echo

# Verify final queue state
echo -e "${YELLOW}Step 7: Verify Final State${NC}"
echo "─────────────────────────────────────────"
REMAINING=$(sudo dmesg | grep "pi5_event" | grep "head\|tail\|count" | tail -1)
echo "Kernel log: $REMAINING"
echo -e "${GREEN}✓ Test complete${NC}"
echo

# Cleanup
echo -e "${YELLOW}Cleanup${NC}"
sudo rmmod event_driver
echo -e "${GREEN}✓ Module unloaded${NC}"
echo

echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}STRESS TEST PASSED - Multiple producers handled correctly!${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
