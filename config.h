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

// Set options here

#define EXTERNAL_CLOCK 1
#define INTERNAL_CLOCK 1

#define LAYER1_COMMAND_DELAY 50
#define LAYER1_DATA_DELAY 200

#define LAYER2_COMMAND_DELAY 100
#define LAYER2_DATA_DELAY 400

#define RETRY_DELAY 10

// Specify transaction sizes.
// Sector size is what the user of this module expects to be the transaction size.
// HYBRIDSIM_TRANSACTION_SIZE is what HybridSim uses.
#define SECTOR_SIZE 512
#define HYBRIDSIM_TRANSACTION_SIZE 64

// Enable debugging output.
#define DEBUG 1

// Derived Parameters
#define HYBRIDSIM_TRANSACTIONS (SECTOR_SIZE / HYBRIDSIM_TRANSACTION_SIZE)
#define SECTOR_ALIGN(addr) ((addr / SECTOR_SIZE) * SECTOR_SIZE)



#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <set>
#include <unordered_map>
#include <sstream>
#include <stdint.h>
#include <assert.h>

using namespace std;

// External Interface for HybridSim
#include <HybridSim.h>

#include "Transaction.h"
#include "util.h"

#endif
