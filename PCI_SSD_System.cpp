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
		cerr << "PCI_SSD id is " << id << "\n";

		debug_file.open(DEBUG_FILE, ios_base::out | ios_base::trunc);
		if (!debug_file.is_open())
		{
			cerr << "ERROR: Debug file " << DEBUG_FILE << " failed to open.\n";
			abort();
		}

		systemID = id;
		hybridsim = HybridSim::getMemorySystemInstance(0, HYBRIDSIM_INI);

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
		layer1 = new Layer(this, LAYER1_DATA_DELAY, LAYER1_COMMAND_DELAY, LAYER1_LANES, LAYER1_FULL_DUPLEX, LAYER1_SEND_EVENT, LAYER1_RETURN_EVENT, "Layer 1");
		layer2 = new Layer(this, LAYER2_DATA_DELAY, LAYER2_COMMAND_DELAY, LAYER2_LANES, LAYER2_FULL_DUPLEX, LAYER2_SEND_EVENT, LAYER2_RETURN_EVENT, "Layer 2");
		if (DEBUG)
		{
			debug_file << "Layer 1 delays are (data: " << LAYER1_DATA_DELAY << ", command: " << LAYER1_COMMAND_DELAY << ")\n";
			debug_file << "Layer 2 delays are (data: " << LAYER2_DATA_DELAY << ", command: " << LAYER2_COMMAND_DELAY << ")\n";
			debug_file.flush();
		}

		// Make sure the add_dma callback is NULL before it is registered.
		add_dma = NULL;
	}

	PCI_SSD_System::~PCI_SSD_System()
	{
		if (DEBUG)
		{
			debug_file.flush();
			debug_file.close();
		}

		delete clockdomain;
		delete hybridsim_clockdomain;
		delete layer1;
		delete layer2;
		delete hybridsim;
	}


	bool PCI_SSD_System::addTransaction(bool isWrite, uint64_t addr, int num_sectors)
	{
		// Make sure I know the range of the number of sectors being transferred.
		assert(num_sectors >= MIN_SECTORS);	
		assert(num_sectors <= MAX_SECTORS);

		// Make sure any sectors in this transaction aren't already being processed.
		// This is just going to fail an assert for now. I can fix it later if we ever have a user doing this.
		uint64_t aligned_sector_addr = SECTOR_ALIGN(addr); 
		for (uint64_t i=0; i < (uint64_t)num_sectors; i++)
		{
			// Insert each sector in this transaction into the pending_sectors set.
			uint64_t cur_sector = aligned_sector_addr + i * SECTOR_SIZE;
			assert(pending_sectors.count(cur_sector) == 0);
			pending_sectors.insert(cur_sector);
			assert(pending_sectors.count(cur_sector) == 1);
		}


		if (DEBUG)
		{
			debug_file << currentClockCycle << ": Sector addTransaction() arrived (isWrite: " << isWrite 
					<< ", addr: " << addr << ", num_sectors: " << num_sectors << ")\n";
			debug_file.flush();

			if (aligned_sector_addr != addr)
			{
				debug_file << currentClockCycle << ": Unaligned sector (orig: " << addr 
						<< ", aligned: " << aligned_sector_addr << ")\n";
				debug_file.flush();
			}
		}


		// Create the transaction and place it in the Layer 1 Send Queue.
		Transaction t(isWrite, aligned_sector_addr, addr, num_sectors, dma_sg_base, dma_sg_len);
		//Transaction t(isWrite, aligned_sector_addr, addr, num_sectors);

		// Clear the scatter gather list for the next transaction.
		dma_sg_base.clear();
		dma_sg_len.clear();
		dma_sg_all.clear();

		// Check for DMA for write transaction.
		if ((ENABLE_DMA) && (isWrite))
		{
			PerformDMA(t);
		}
		else
		{
			layer1->Add_Send_Transaction(t);
		}

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

	// DMA functions
	void PCI_SSD_System::RegisterDMACallback(DMATransactionCB *add_dma, uint64_t mem_size)
	{
		this->add_dma = add_dma;
		this->dma_memory_size = mem_size;

		if (DEBUG)
		{
			debug_file << currentClockCycle << " : RegisterDMACallback called. Memory size is " << this->dma_memory_size << "\n";
			debug_file.flush();
		}
	}


	bool PCI_SSD_System::isDMATransaction(uint64_t addr)
	{
		assert((dma_base_address.count(addr) == 0) || (dma_base_address.count(addr) == 1));
		return (dma_base_address.count(addr) == 1);
	}

	void PCI_SSD_System::CompleteDMATransaction(bool isWrite, uint64_t addr)
	{
		if (DEBUG)
		{
			debug_file << currentClockCycle << ": Completed DRAMSim2 DMA transaction for (" << isWrite << ", " << addr << ")\n";
			debug_file.flush();
		}

		// Get the base address for this DRAMSim2 transaction.
		assert(dma_base_address.count(addr) == 1);
		uint64_t base_address = dma_base_address[addr];

		// Remove the base address entry.
		dma_base_address.erase(addr);
		assert(dma_base_address.count(addr) == 0);

		// Remove the access from dma_accesses.
		assert(dma_accesses[base_address].count(addr) == 1);
		dma_accesses[base_address].erase(addr);

		if (dma_accesses[base_address].empty())
		{
			// Get the transaction
			Transaction old_t = dma_transactions[base_address];
			assert(isWrite == !old_t.isWrite); // DMA type should be the opposite of the SSD access type.
			assert(base_address == old_t.addr);

			// Create a Transaction for the finishDMA call.
			// Not reusing old_t since it should be deleted by the pending state cleanup.
			//Transaction t(old_t.isWrite, base_address, old_t.orig_addr, old_t.num_sectors, old_t.dma_sg_base, old_t.dma_sg_len);
			//Transaction t(isWrite, base_address, old_t.orig_addr, old_t.num_sectors);
			Transaction t = old_t; // Make a deep copy.

			// Remove the pending state.
			dma_transactions.erase(base_address);
			dma_accesses.erase(base_address);
			
			// Check that the pending state is cleared.
			assert(dma_transactions.count(base_address) == 0);
			assert(dma_accesses.count(base_address) == 0);

			// Call finishDMA to complete this DMA transaction.
			FinishDMA(t);
		}
	}


	void PCI_SSD_System::AddDMAScatterGatherEntry(uint64_t addr, uint64_t length)
	{
		if (DEBUG)
		{
			debug_file << currentClockCycle << " : Received new scatter gather entry (" << addr << ", " << length << ")\n";
			debug_file.flush();
		}

		// Check rules for addr and length before putting them in the sg state.
		string fail_reason;
		if (addr >= dma_memory_size)
		{
			fail_reason = "addr is out of memory bounds";
			goto sg_error;
		}
		else if (length > MAX_SECTORS * SECTOR_SIZE)
		{
			fail_reason = "length exceeds MAX_SECTORS * SECTOR_SIZE";
			goto sg_error;
		}
		else if (addr != DRAMSIM_ALIGN(addr))
		{
			fail_reason = "addr is not aligned to DRAMSIM_TRANSACTION_SIZE";
			goto sg_error;
		}
		else if (length != DRAMSIM_ALIGN(length))
		{
			fail_reason = "length is not aligned to DRAMSIM_TRANSACTION_SIZE";
			goto sg_error;
		}
		else
		{
			// Generate all DRAMSim transaction addresses for this SG entry.
			for (uint64_t i=0; i<length/DRAMSIM_TRANSACTION_SIZE; i++)
			{
				uint64_t cur_addr = addr + i * DRAMSIM_TRANSACTION_SIZE;

				// Check to see if this address is already in this particular DMA transaction.
				if (dma_sg_all.count(cur_addr) != 0)
				{
					fail_reason = "Duplicate DRAMSim address found in dma_sg_all";
					goto sg_error;
				}

				dma_sg_all.insert(cur_addr);
			}

			// Passed all rule checks.
			dma_sg_base.push_back(addr);
			dma_sg_len.push_back(length);
			return;
		}

		sg_error:
		if (DEBUG)
		{
			debug_file << currentClockCycle << " : Dropping invalid SG entry. Reason: " << fail_reason << "\n";
			debug_file.flush();
		}
	}


	// Internal functions
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
				debug_file << currentClockCycle << " : length(event_queue)=" << event_queue.size() 
						<< " length(layer1.send_queue)=" << layer1->send_queue.size() 
						<< " length(layer1.return_queue)=" << layer1->return_queue.size()
						<< " length(layer2.send_queue)=" << layer2->send_queue.size()
						<< " length(layer2.return_queue)=" << layer2->return_queue.size() << "\n";
				debug_file.flush();
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
		assert(aligned_sector_addr == e.trans.addr); // Just confirm that the address was aligned.
		for (uint64_t i=0; i < (uint64_t)e.trans.num_sectors; i++)
		{
			// Remove from the pending_sectors set.
			uint64_t cur_sector = aligned_sector_addr + i * SECTOR_SIZE;
			assert(pending_sectors.count(cur_sector) == 1);
			pending_sectors.erase(cur_sector);
			assert(pending_sectors.count(cur_sector) == 0);
		}

		// Check for DMA Write for SSD reads.
		if ((ENABLE_DMA) && (!e.trans.isWrite))
		{
			PerformDMA(e.trans);
		}
		else
		{
			// Issue the external callback.
			// Use the orig_addr since this is the unaligned original address that the caller expects.
			issue_external_callback(e.trans.isWrite, e.trans.orig_addr);
		}
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
		assert(hybridsim_base_address.count(addr) == 1);
		uint64_t base_address = hybridsim_base_address[addr];
		hybridsim_base_address.erase(addr);
		assert(hybridsim_base_address.count(addr) == 0);

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
			debug_file << currentClockCycle << " : Received callback from HybridSim (" << isWrite << ", " << addr << ")\n";
			debug_file.flush();
		}
		
		// If the whole sector transaction is done, then send it back up.
		if (hybridsim_accesses[base_address].empty())
		{
			// Create a Transaction for the return path.
			// Not reusing old_t since it should be deleted by the pending state cleanup.
			// The only thing I need from old_t is the orig_addr (which is the address before SECTOR_ALIGN()).
			//Transaction t(isWrite, base_address, old_t.orig_addr, old_t.num_sectors, old_t.dma_sg_base, old_t.dma_sg_len);
			//Transaction t(isWrite, base_address, old_t.orig_addr, old_t.num_sectors);
			Transaction t = old_t; // Make a deep copy.

			// Remove the pending state.
			hybridsim_transactions.erase(base_address);
			hybridsim_accesses.erase(base_address);

			// Check that the pending state is cleared.
			assert(hybridsim_transactions.count(base_address) == 0);
			assert(hybridsim_accesses.count(base_address) == 0);

			if (DEBUG)
			{
				debug_file << currentClockCycle << " : Finished HybridSim transactions for base address " << base_address << "\n";
				debug_file.flush();
			}

			// Put transaction in appropriate return queue.
			layer2->Add_Return_Transaction(t);
		}
	}


	void PCI_SSD_System::handle_hybridsim_add_transaction(Transaction t)
	{
		// This code sends out HYBRIDSIM_TRANSACTIONS(num_sectors) transactions for each sector,
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


		// Add HYBRIDSIM_TRANSACTIONS(num_sectors) transactions to HybridSim.
		for (uint64_t i = 0; i < (uint64_t)HYBRIDSIM_TRANSACTIONS(t.num_sectors); i++)
		{
			uint64_t cur_address = base_address + i * HYBRIDSIM_TRANSACTION_SIZE;

			// Save this access in the access list for this base address.
			hybridsim_accesses[base_address].insert(cur_address);

			// Save the mapping from this access to the current base address.
			// This is necessary because of the variable sized transactions do not allow for an easy
			// way to compute the base address from the access address.
			assert(hybridsim_base_address.count(cur_address) == 0);
			hybridsim_base_address[cur_address] = base_address;
			assert(hybridsim_base_address.count(cur_address) == 1);

			// Send this transaction to HybridSim.
			bool success = hybridsim->addTransaction(t.isWrite, cur_address);
			assert(success); // HybridSim::addTransaction should never fail since it just returns true. :)
		}


		if (DEBUG)
		{
			debug_file << currentClockCycle << " : Added " << HYBRIDSIM_TRANSACTIONS(t.num_sectors) << " to HybridSim for base address " << base_address << "\n";
			debug_file.flush();
		}
	}


	void PCI_SSD_System::issue_external_callback(bool isWrite, uint64_t orig_addr)
	{
		if (DEBUG)
		{
			debug_file << currentClockCycle << " : Issuing external callback for transaction (" << isWrite << ", " << orig_addr << ")\n";
			debug_file.flush();
		}

		// Select the appropriate callback method pointer.
		TransactionCompleteCB *cb = isWrite ? WriteDone : ReadDone;

		// Call the callback if it is not null.
		if (cb != NULL)
			(*cb)(systemID, orig_addr, currentClockCycle);
	}

	void PCI_SSD_System::PerformDMA(Transaction t)
	{
		assert(add_dma != NULL);

		if (DEBUG)
		{
			debug_file << currentClockCycle << ": Starting DMA transaction for " << t.addr << "\n";
			debug_file << "DMA type is " << t.isWrite << " (1 => SSD Write/DMA Read and 0 => SSD Read/DMA Write)\n";
			debug_file.flush();
		}

		// Make sure there is a DMA to perform.
		// If not, then just go straight to FinishDMA.
		if (t.dma_sg_base.size() == 0)
		{
			if (DEBUG)
			{
				debug_file << currentClockCycle << ": No SG entries to process so skipping directly to FinishDMA().\n";
				debug_file.flush();
			}
			FinishDMA(t);
		}

		// Save this transaction in the dma_transactions map
		assert(dma_transactions.count(t.addr) == 0);
		dma_transactions[t.addr] = t;
		assert(dma_transactions.count(t.addr) == 1);

		// Create the pending set for dma_accesses for this base address.
		assert(dma_accesses.count(t.addr) == 0);
		dma_accesses[t.addr] = set<uint64_t>();
		assert(dma_accesses.count(t.addr) == 1);

		// Generate all of the necessary DRAMSim transactions.
		list<uint64_t>::iterator cur_base = t.dma_sg_base.begin();
		list<uint64_t>::iterator cur_len = t.dma_sg_len.begin();
		while (cur_base != t.dma_sg_base.end())
		{
			for (uint64_t offset=0; offset < (*cur_len); offset += DRAMSIM_TRANSACTION_SIZE)
			{
				uint64_t cur_addr = (*cur_base) + offset;
				bool isWrite = !t.isWrite; // DMA read for SSD write and DMA write for SSD read.
				(*add_dma)(isWrite, cur_addr, 0); // Send the transaction to DRAMSim via the add_dma callback.

				// Save this access in the dma_accesses pending map.
				assert(dma_accesses[t.addr].count(cur_addr) == 0);
				dma_accesses[t.addr].insert(cur_addr);
				assert(dma_accesses[t.addr].count(cur_addr) == 1);

				// Save the base address for this access.
				assert(dma_base_address.count(cur_addr) == 0);
				dma_base_address[cur_addr] = t.addr;
				assert(dma_base_address.count(cur_addr) == 1);
			}

			if (DEBUG)
			{
				debug_file << currentClockCycle << ": Sent " << ((*cur_len) / DRAMSIM_TRANSACTION_SIZE)
						<< " SG entry transactions to DRAMSim2 (base: " << (*cur_base) << ", " << (*cur_len) << ")\n";
				debug_file.flush();
			}

			// Increment the iterators.
			cur_base++;
			cur_len++;
		}
	}

	void PCI_SSD_System::FinishDMA(Transaction t)
	{
		if (DEBUG)
		{
			debug_file << currentClockCycle << ": Finishing DMA transaction for " << t.addr << "\n";
			debug_file << "DMA type is " << t.isWrite << " (1 => SSD Write/DMA Read and 0 => SSD Read/DMA Write)\n";
			debug_file.flush();
		}

		if (t.isWrite)
		{
			// For an SSD write, we perform a DMA read.
			// After the DMA read completes, it is time to send the transaction across the host
			// interface and down to the disk.
			layer1->Add_Send_Transaction(t);
		}
		else
		{
			// For an SSD read, we perform a DMA write.
			// After the DMA write completes, the whole SSD transaction is finished.

			// Issue the external callback.
			// Use the orig_addr since this is the unaligned original address that the caller expects.
			issue_external_callback(t.isWrite, t.orig_addr);
		}
	}


	// static allocator for the library interface
	PCI_SSD_System *getInstance(uint id)
	{
		return new PCI_SSD_System(id);
	}

	// Helper function to compute the delay in nanoseconds of a layer interface.
	uint64_t compute_interface_delay(uint64_t num_bytes, uint64_t bytes_per_second, uint64_t efficiency)
	{
		if (bytes_per_second == 0)
		{
			// For the NONE case.
			return 0;
		}
		else
		{
			double efficiency_percentage = double(efficiency) / 100.0;
			double total_bytes = (double)(num_bytes) / efficiency_percentage;
			double in_seconds = total_bytes / (double)(bytes_per_second);
			double in_nanoseconds = in_seconds * 1000000000;
			return (uint64_t)ceil(in_nanoseconds);
		}
	}

}


