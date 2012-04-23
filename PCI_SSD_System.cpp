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

#include "PCI_SSD_System.h"

using namespace std;

namespace PCISSD
{
	PCI_SSD_System::PCI_SSD_System(uint id)
	{
		systemID = id;
		hybridsim = HybridSim::getMemorySystemInstance(0);

		currentClockCycle = 0;

		// Set up clock domain crosser.
		ClockDomain::ClockUpdateCB *cd_callback = new ClockDomain::Callback<PCI_SSD_System, void>(this, &PCI_SSD_System::update_internal);
		clockdomain = new ClockDomain::ClockDomainCrosser(INTERNAL_CLOCK, EXTERNAL_CLOCK, cd_callback);

		// Register callbacks to HybridSim.
		typedef HybridSim::Callback <PCI_SSD_System, void, uint, uint64_t, uint64_t> hybridsim_callback_t;
        HybridSim::TransactionCompleteCB *read_cb = new hybridsim_callback_t(this, &PCI_SSD_System::HybridSim_Read_Callback);
        HybridSim::TransactionCompleteCB *write_cb = new hybridsim_callback_t(this, &PCI_SSD_System::HybridSim_Write_Callback);
		hybridsim->RegisterCallbacks(read_cb, write_cb);

		// Set up HybridSim's clock domain.
		ClockDomain::ClockUpdateCB *hybridsim_cd_callback = new ClockDomain::Callback<PCI_SSD_System, void>(this, &PCI_SSD_System::hybridsim_update_internal);
		hybridsim_clockdomain = new ClockDomain::ClockDomainCrosser(HYBRIDSIM_CLOCK_1, HYBRIDSIM_CLOCK_2, hybridsim_cd_callback);

		// Set up layers.
		layer1 = new Layer(this, LAYER1_DATA_DELAY, LAYER1_COMMAND_DELAY, LAYER1_LANES, LAYER1_SEND_EVENT, LAYER1_RETURN_EVENT, "Layer 1");
		layer2 = new Layer(this, LAYER2_DATA_DELAY, LAYER2_COMMAND_DELAY, LAYER2_LANES, LAYER2_SEND_EVENT, LAYER2_RETURN_EVENT, "Layer 2");
	}

	PCI_SSD_System::~PCI_SSD_System()
	{
		delete clockdomain;
		delete hybridsim_clockdomain;
		delete layer1;
		delete layer2;
		delete hybridsim;
	}


	bool PCI_SSD_System::addTransaction(bool isWrite, uint64_t addr)
	{
		// Make sure this sector isn't already being processed.
		// This is just going to fail an assert for now. I can fix it later if we ever have a user doing this.
		uint64_t aligned_sector_addr = SECTOR_ALIGN(addr);
		assert(pending_sectors.count(aligned_sector_addr) == 0);

		if (DEBUG)
		{
			if (aligned_sector_addr != addr)
			{
				cout << currentClockCycle << ": Unaligned sector arrived (orig: " << addr 
						<< ", aligned: " << aligned_sector_addr << ")\n";
			}
		}

		// Insert this sector into the pending_sectors set.
		pending_sectors.insert(aligned_sector_addr);
		assert(pending_sectors.count(aligned_sector_addr) == 1);

		// Create the transaction and place it in the Layer 1 Send Queue.
		Transaction t(isWrite, aligned_sector_addr, addr);
		layer1->Add_Send_Transaction(t);

		return true;
	}

	
	bool PCI_SSD_System::WillAcceptTransaction()
	{
		return true;
	}


	void PCI_SSD_System::RegisterCallbacks(TransactionCompleteCB *readDone, TransactionCompleteCB *writeDone)
	{
		ReadDone = readDone;
		WriteDone = writeDone;
	}


	void PCI_SSD_System::printLogfile()
	{
		hybridsim->printLogfile();
	}

	void PCI_SSD_System::HybridSim_Read_Callback(uint id, uint64_t addr, uint64_t cycle)
	{
		handle_hybridsim_callback(false, addr);
	}


	void PCI_SSD_System::HybridSim_Write_Callback(uint id, uint64_t addr, uint64_t cycle)
	{
		handle_hybridsim_callback(true, addr);
	}


	void PCI_SSD_System::update()
	{
		clockdomain->update();
	}

	void PCI_SSD_System::update_internal()
	{
		// Do Processing for layer 2
		layer2->update();

		// Do processing for layer 1
		layer1->update();

		// Do processing for event queue
		Process_Event_Queue();

		// Call update for HybridSim.
		// This uses a clock domain crosser due to the different clock rates.
		// The callback is hybridsim_update_internal.
		hybridsim_clockdomain->update();

		// Increment clock cycle counter.
		currentClockCycle++;

		if (DEBUG)
		{
			if (currentClockCycle % 10000 == 0)
			{
				cout << currentClockCycle << " : length(event_queue)=" << event_queue.size() 
						<< " length(layer1.send_queue)=" << layer1->send_queue.size() 
						<< " length(layer1.return_queue)=" << layer1->return_queue.size()
						<< " length(layer2.send_queue)=" << layer2->send_queue.size()
						<< " length(layer2.return_queue)=" << layer2->return_queue.size() << "\n";
			}
		}
	}


	void PCI_SSD_System::hybridsim_update_internal()
	{
		// Call HybridSim at the appropriate clock rate.
		// Ratio of call is HYBRIDSIM_CLOCK_1 : HYBRIDSIM_CLOCK_2
		hybridsim->update();
	}



	// Event Queue Implementation
	void PCI_SSD_System::Process_Event_Queue()
	{
		while ((!event_queue.empty()) && (event_queue.front().expire_time <= currentClockCycle))
		{
			TransactionEvent e = event_queue.front();
			event_queue.pop_front();

			if (e.type == LAYER1_SEND_EVENT)
			{
				Layer1_Send_Event_Done(e);
			}
			else if (e.type == LAYER1_RETURN_EVENT)
			{
				Layer1_Return_Event_Done(e);
			}
			else if (e.type == LAYER2_SEND_EVENT)
			{
				Layer2_Send_Event_Done(e);
			}
			else if (e.type == LAYER2_RETURN_EVENT)
			{
				Layer2_Return_Event_Done(e);
			}
			else
				assert(0);
		}

	}

	void PCI_SSD_System::Add_Event(TransactionEvent e)
	{
		event_queue.push_back(e);
		event_queue.sort();

	}


	void PCI_SSD_System::Retry_Event(TransactionEvent e)
	{
		// Try again in RETRY_DELAY cycles.
		e.expire_time += RETRY_DELAY;
		Add_Event(e);
	}


	void PCI_SSD_System::Layer1_Send_Event_Done(TransactionEvent e)
	{
		assert(e.type == LAYER1_SEND_EVENT);

		layer1->Send_Event_Done(e.trans);

		layer2->Add_Send_Transaction(e.trans);
	}

	void PCI_SSD_System::Layer1_Return_Event_Done(TransactionEvent e)
	{
		assert(e.type == LAYER1_RETURN_EVENT);

		layer1->Return_Event_Done(e.trans);

		// Remove from the pending_sectors set.
		uint64_t aligned_sector_addr = SECTOR_ALIGN(e.trans.addr);
		assert(pending_sectors.count(aligned_sector_addr) == 1);
		pending_sectors.erase(aligned_sector_addr);
		assert(pending_sectors.count(aligned_sector_addr) == 0);

		// Issue the external callback.
		// Use the orig_addr since this is the unaligned original address that the caller expects.
		issue_external_callback(e.trans.isWrite, e.trans.orig_addr);

	}


	void PCI_SSD_System::Layer2_Send_Event_Done(TransactionEvent e)
	{
		assert(e.type == LAYER2_SEND_EVENT);

		layer2->Send_Event_Done(e.trans);

		handle_hybridsim_add_transaction(e.trans);
	}


	void PCI_SSD_System::Layer2_Return_Event_Done(TransactionEvent e)
	{
		assert(e.type == LAYER2_RETURN_EVENT);

		layer2->Return_Event_Done(e.trans);

		layer1->Add_Return_Transaction(e.trans);
	}


	void PCI_SSD_System::handle_hybridsim_callback(bool isWrite, uint64_t addr)
	{
		// Compute the base address.
		uint64_t base_address = SECTOR_ALIGN(addr);

		// Check that this address is in the pending state for HybridSim.
		assert(hybridsim_transactions.count(base_address) == 1);
		assert(hybridsim_accesses.count(base_address) == 1);

		// Get the transaction
		Transaction old_t = hybridsim_transactions[base_address];
		assert(isWrite == old_t.isWrite);
		assert(base_address == old_t.addr);

		// Remove current address from the access set for the base address.
		assert(hybridsim_accesses[base_address].count(addr) == 1);
		hybridsim_accesses[base_address].erase(addr);
		assert(hybridsim_accesses[base_address].count(addr) == 0);

		if (DEBUG)
		{
			cout << currentClockCycle << " : Received callback from HybridSim (" << isWrite << ", " << addr << ")\n";
		}
		
		// If the whole sector transaction is done, then send it back up.
		if (hybridsim_accesses[base_address].empty())
		{
			// Create a Transaction for the return path.
			// Not reusing old_t since it should be deleted by the pending state cleanup.
			// The only thing I need from old_t is the orig_addr (which is the address before SECTOR_ALIGN()).
			Transaction t(isWrite, base_address, old_t.orig_addr);

			// Remove the pending state.
			hybridsim_transactions.erase(base_address);
			hybridsim_accesses.erase(base_address);

			// Check that the pending state is cleared.
			assert(hybridsim_transactions.count(base_address) == 0);
			assert(hybridsim_accesses.count(base_address) == 0);

			if (DEBUG)
			{
				cout << currentClockCycle << " : Finished HybridSim transactions for base address " << base_address << "\n";
			}

			// Put transaction in appropriate return queue.
			layer2->Add_Return_Transaction(t);
		}
	}


	void PCI_SSD_System::handle_hybridsim_add_transaction(Transaction t)
	{
		// This code sends out HYBRIDSIM_TRANSACTIONS transactions for each sector,
		// since HybridSim transactions are at a smaller granularity than a sector.
		// The base address for all transactions is the aligned sector address.
		uint64_t base_address = t.addr;

		// Assert that there is not an outstanding access to this sector.
		assert(hybridsim_transactions.count(base_address) == 0);
		assert(hybridsim_accesses.count(base_address) == 0);

		// Create entries for this sector access.
		hybridsim_transactions[base_address] = t; // Save the transaction object for use by the callback.
		hybridsim_accesses[base_address] = set<uint64_t>(); // Empty set for the outstanding HybridSim accesses.

		// I like assertions. They prevent migraines.
		assert(hybridsim_transactions.count(base_address) == 1);
		assert(hybridsim_accesses.count(base_address) == 1);


		// Add HYBRIDSIM_TRANSACTIONS transactions to HybridSim.
		for (uint64_t i = 0; i < HYBRIDSIM_TRANSACTIONS; i++)
		{
			uint64_t cur_address = base_address + i * HYBRIDSIM_TRANSACTION_SIZE;
			hybridsim_accesses[base_address].insert(cur_address);
			bool success = hybridsim->addTransaction(t.isWrite, cur_address);
			assert(success); // HybridSim::addTransaction should never fail since it just returns true. :)
		}


		if (DEBUG)
		{
			cout << currentClockCycle << " : Added " << HYBRIDSIM_TRANSACTIONS << " to HybridSim for base address " << base_address << "\n";
		}
	}


	void PCI_SSD_System::issue_external_callback(bool isWrite, uint64_t orig_addr)
	{
		// Select the appropriate callback method pointer.
		TransactionCompleteCB *cb = isWrite ? WriteDone : ReadDone;

		// Call the callback if it is not null.
		if (cb != NULL)
			(*cb)(systemID, orig_addr, currentClockCycle);
	}

	// static allocator for the library interface
	PCI_SSD_System *getInstance(uint id)
	{
		return new PCI_SSD_System(id);
	}
}


