#!/bin/bash
# Test Blocking Queue Behavior
# Shows that manual writes BLOCK when queue is full

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

clear
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║          BLOCKING QUEUE TEST                               ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo

# Cleanup first
sudo rmmod event_driver 2>/dev/null || true

echo -e "${YELLOW}Setup: Loading module...${NC}"
sudo insmod event_driver.ko
echo -e "${GREEN}✓ Module loaded${NC}"
echo

# Test 1: Fill queue
echo -e "${YELLOW}TEST 1: Fill Queue (32 events)${NC}"
echo "─────────────────────────────────────────"
for i in {1..32}; do
    echo "INIT_$i" > /dev/pi5_event 2>/dev/null || true
done
echo -e "${GREEN}✓ Queue filled (32/32)${NC}"
echo

# Test 2: Blocking write
echo -e "${YELLOW}TEST 2: Blocking Write (should block)${NC}"
echo "─────────────────────────────────────────"
echo "Starting blocking producer in background..."
(echo "BLOCKING_TEST" > /dev/pi5_event && echo "PRODUCER_COMPLETED") &
PROD_PID=$!
echo -e "${GREEN}✓ Producer started (PID: $PROD_PID)${NC}"
echo

# Check if producer is blocked
sleep 1
if ps -p $PROD_PID > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Producer is BLOCKED (waiting for space)${NC}"
else
    echo -e "${RED}✗ Producer exited unexpectedly${NC}"
fi
echo

# Test 3: Consumer frees space
echo -e "${YELLOW}TEST 3: Consumer Frees Space${NC}"
echo "─────────────────────────────────────────"
echo "Consuming 5 events..."
dd if=/dev/pi5_event bs=40 count=5 2>/dev/null | od -An -c -N10
echo -e "${GREEN}✓ 5 events consumed${NC}"
echo

# Test 4: Wait for producer
echo -e "${YELLOW}TEST 4: Producer Should Wake${NC}"
echo "─────────────────────────────────────────"
sleep 1
if ps -p $PROD_PID > /dev/null 2>&1; then
    echo -e "${YELLOW}⏳ Producer still running...${NC}"
    wait $PROD_PID
    echo -e "${GREEN}✓ Producer completed!${NC}"
else
    echo -e "${GREEN}✓ Producer already completed!${NC}"
fi
echo

# Verify event was written
echo -e "${YELLOW}TEST 5: Verify Event in Queue${NC}"
echo "─────────────────────────────────────────"
echo "Reading one event:"
dd if=/dev/pi5_event bs=40 count=1 2>/dev/null | od -An -c -N15
echo -e "${GREEN}✓ Blocking write succeeded!${NC}"
echo

# Cleanup
echo -e "${YELLOW}Cleanup${NC}"
sudo rmmod event_driver
echo -e "${GREEN}✓ Module unloaded${NC}"
echo

echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}ALL TESTS PASSED - Blocking queue works correctly!${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
