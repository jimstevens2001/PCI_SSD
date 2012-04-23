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

// Specify delays for Layer 1 (PCI 2.0, PCI 3.0, DMI 2.0, or none)
// All delays assume a command packet of 16 bytes and a data packet of 528 bytes.
// PCI 2.0 => PCI2, PCI 3.0 => PCI3, DMI 2.0 => DMI2, none => NONEL1
#define PCI3 1

// Specify the number of lanes in Layer 1.
// This only applies to PCI buses. Otherwise it should be 1.
// Valid lane counts are 1, 2, 4, 8, or 16.
#define LAYER1_LANES 16

// Specify whether layer 1 should use half duplex (0) or full duplex (1).
// Note: All of the current layer 1 interfaces (PCI and DMI) are full duplex.
#define LAYER1_FULL_DUPLEX 1


// Specify delays for Layer 1 (SATA 2, SATA 3, or none)
// All delays assume a command packet of 16 bytes and a data packet of 528 bytes.
// SATA 2.0 => SATA2, SATA 3.0 => SATA3, none => NONEL2
#define NONEL2 1

// Specify the number of lanes in Layer 2.
// This applies to PCIe SSDs that have multple SATA links.
// If you are not simulating this situation, then set it to 1.
// Valid lane counts are 1, 2, 4, 8, or 16.
#define LAYER2_LANES 1

// Specify whether layer 2 should use half duplex (0) or full duplex (1).
// Note: All of the current layer2 interfaces are half duplex.
// SAS might be supported in the future and it is full duplex.
#define LAYER2_FULL_DUPLEX 0


////////////////////////////////////////////////////////////////////
// Parameters below this point should never change.

// PCIe 2.0 (500 MB/s per lane)
#ifdef PCI2
#define LAYER1_DATA_DELAY 1056
#define LAYER1_COMMAND_DELAY 32
#endif

// PCIe 3.0 (1 GB/s per lane)
#ifdef PCI3
#define LAYER1_DATA_DELAY 528
#define LAYER1_COMMAND_DELAY 16
#endif

// DMI 2.0 (2.5 GB/s)
#ifdef DMI2
#define LAYER1_DATA_DELAY 212
#define LAYER1_COMMAND_DELAY 7
#endif

// No delay for Layer 1
#ifdef NONEL1
#define LAYER1_DATA_DELAY 0
#define LAYER1_COMMAND_DELAY 0
#endif

// SATA 2.0 (375 MB/s)
#ifdef SATA2
#define LAYER2_DATA_DELAY 1408
#define LAYER2_COMMAND_DELAY 43
#endif

// SATA 3.0 (750 MB/s)
#ifdef SATA3
#define LAYER2_DATA_DELAY 704
#define LAYER2_COMMAND_DELAY 22
#endif

// No delay for Layer 2
#ifdef NONEL2
#define LAYER2_DATA_DELAY 0
#define LAYER2_COMMAND_DELAY 0
#endif


#define RETRY_DELAY 10

// Specify transaction sizes.
// Sector size is what the user of this module expects to be the transaction size.
// HYBRIDSIM_TRANSACTION_SIZE is what HybridSim uses.
// These should never change since QEMU's IDE interface assumes 512 and HybridSim assumes 64.
#define SECTOR_SIZE 512
#define HYBRIDSIM_TRANSACTION_SIZE 64


// Derived Parameters
#define HYBRIDSIM_TRANSACTIONS (SECTOR_SIZE / HYBRIDSIM_TRANSACTION_SIZE)
#define SECTOR_ALIGN(addr) ((addr / SECTOR_SIZE) * SECTOR_SIZE)


#endif
