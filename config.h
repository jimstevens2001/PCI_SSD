/*********************************************************************************
* Copyright (c) 2010-2011, 
* Jim Stevens, Paul Tschirhart, Ishwar Singh Bhati, Mu-Tien Chang, Peter Enns, 
* Elliott Cooper-Balis, Paul Rosenfeld, Bruce Jacob
* University of Maryland
* Contact: jims [at] cs [dot] umd [dot] edu
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/


#ifndef PCISSD_CONFIG_H
#define PCISSD_CONFIG_H

////////////////////////////////////////////////////////////////////
// Set options here

// Enable debugging output.
#define DEBUG 1
#define DEBUG_FILE "debug_pci_ssd.txt"
//#define DEBUG_FILE "/dev/stdout"

// Define clock ratio.
// This means the update_internal will be called INTERNAL_CLOCK times
// for every EXTERNAL_CLOCK calls to update.
// For example, if this module is to run at 1 GHz while the cpu is 
// running at 2 Ghz, then INTERNAL_CLOCK should be 1 and EXTERNAL_CLOCK should
// be 2.
// Note: Keep the internal clock running at 1 GHz, since the delays below
// are specified in terms of a 1 ns clock cycle time.
#define INTERNAL_CLOCK 1
#define EXTERNAL_CLOCK 2

// Define a clock ratio for HybridSim.
// HybridSim needs to be called 2 times for every 3 times this module is updated
// since this module runs at 1 GHz and HybridSim runs at 667 MHz.
#define HYBRIDSIM_CLOCK_1 2
#define HYBRIDSIM_CLOCK_2 3

// Define HybridSim ini file.
#define HYBRIDSIM_INI "../HybridSim/ini/hybridsim.ini"
//#define HYBRIDSIM_INI "hybridsim.ini"


// Define interface speeds in bytes per second
#define SATA2 300000000 // SATA 2.0 (300 MB/s, 375 MB/s without PHY overhead) (Also can use this for SAS)
#define SATA3 600000000 // SATA 3.0 (600 MB/s, 750 MB/s without PHY overhead) (Also can use this for SAS)
#define PCI2 500000000  // PCIe 2.0 (500 MB/s per lane)
#define PCI3 1000000000 // PCIe 3.0 (1 GB/s per lane)
#define DMI2 2500000000 // Intel Direct Media Interface 2.0 (2.5 GB/s)
#define NONE 0

// Specify interface speeds for Layer 1 and Layer 2.
#define LAYER1_TYPE PCI3
#define LAYER2_TYPE NONE

// Specify the number of lanes in each layer.
// This only applies to PCI buses (or if you want to simulate multiple SATA buses). Otherwise it should be 1.
// Valid lane counts are 1, 2, 4, 8, or 16.
#define LAYER1_LANES 16
#define LAYER2_LANES 1

// Specify whether layers should use half duplex (0) or full duplex (1).
// Layer 1 Note: All of the current layer 1 interfaces (PCI and DMI) are full duplex.
// Layer 2 Note: SATA interfaces are half duplex. If you want to simulate SAS, 
// set SATA as the speed and turn on full duplex mode.
#define LAYER1_FULL_DUPLEX 1
#define LAYER2_FULL_DUPLEX 0


// Specify whether direct memory access should be simulated.
// If this is 0, the direct memory access parts will simply be skipped.
#define ENABLE_DMA 1


////////////////////////////////////////////////////////////////////
// Parameters below this point should never change.


#define RETRY_DELAY 10

// Specify transaction sizes.
// Sector size is what the user of this module expects to be the transaction size.
// HYBRIDSIM_TRANSACTION_SIZE is what HybridSim uses.
// These should never change since QEMU's IDE interface assumes 512 and HybridSim assumes 64.
#define SECTOR_SIZE 512
#define HYBRIDSIM_TRANSACTION_SIZE 64
#define DRAMSIM_TRANSACTION_SIZE 64

// Specify the range on the number of sectors allowed in one transaction.
#define MIN_SECTORS 1 	
#define MAX_SECTORS 256

// Specify command size for layers.
#define COMMAND_SIZE 16

// Specify protocol efficiency percentage.
#define PROTOCOL_EFFICIENCY 90

// Compute layer 1 delays.
#define LAYER1_COMMAND_DELAY compute_interface_delay(COMMAND_SIZE, LAYER1_TYPE, PROTOCOL_EFFICIENCY)
#define LAYER1_DATA_DELAY compute_interface_delay(COMMAND_SIZE + SECTOR_SIZE, LAYER1_TYPE, PROTOCOL_EFFICIENCY)

// Compute layer 2 delays.
#define LAYER2_COMMAND_DELAY compute_interface_delay(COMMAND_SIZE, LAYER2_TYPE, PROTOCOL_EFFICIENCY)
#define LAYER2_DATA_DELAY compute_interface_delay(COMMAND_SIZE + SECTOR_SIZE, LAYER2_TYPE, PROTOCOL_EFFICIENCY)

// Other Derived Parameters
#define HYBRIDSIM_TRANSACTIONS(ns) ((ns*SECTOR_SIZE) / HYBRIDSIM_TRANSACTION_SIZE)
#define SECTOR_ALIGN(addr) ((addr / SECTOR_SIZE) * SECTOR_SIZE)
#define DRAMSIM_ALIGN(addr) ((addr / DRAMSIM_TRANSACTION_SIZE) * DRAMSIM_TRANSACTION_SIZE)


#endif
