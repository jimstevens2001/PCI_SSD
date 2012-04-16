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

		// Register callbacks to HybridSim.
		typedef HybridSim::Callback <PCI_SSD_System, void, uint, uint64_t, uint64_t> hybridsim_callback_t;
        HybridSim::TransactionCompleteCB *read_cb = new hybridsim_callback_t(this, &PCI_SSD_System::HybridSim_Read_Callback);
        HybridSim::TransactionCompleteCB *write_cb = new hybridsim_callback_t(this, &PCI_SSD_System::HybridSim_Write_Callback);
		hybridsim->RegisterCallbacks(read_cb, write_cb);
	}


	bool PCI_SSD_System::addTransaction(bool isWrite, uint64_t addr)
	{
		Transaction t(isWrite, addr);
		layer1_send_queue.push_back(t);

		if (DEBUG)
		{
			cout << currentClockCycle << " : Added transaction to layer1_send_queue: (" << t.isWrite << ", " << t.addr << ")\n";
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

	void PCI_SSD_System::HybridSim_Read_Callback(uint id, uint64_t addr, uint64_t cycle)
	{
		if (ReadDone != NULL)
			(*ReadDone)(systemID, addr, currentClockCycle);
	}


	void PCI_SSD_System::HybridSim_Write_Callback(uint id, uint64_t addr, uint64_t cycle)
	{
		if (WriteDone != NULL)
			(*WriteDone)(systemID, addr, currentClockCycle);
	}


	void PCI_SSD_System::update()
	{
		// Do processing for layer 1
		Process_Layer1();

		// Do processing for event queue
		Process_Event_Queue();

		// Tell HybridSim to update
		hybridsim->update();

		// Increment clock cycle counter.
		currentClockCycle++;

		if (DEBUG)
		{
			if (currentClockCycle % 10000 == 0)
			{
				cout << currentClockCycle << " : length(event_queue)=" << event_queue.size() << " length(layer1_send_queue)=" << layer1_send_queue.size() << "\n";
			}
		}
	}




	void PCI_SSD_System::Process_Event_Queue()
	{
		while ((!event_queue.empty()) && (event_queue.front().expire_time <= currentClockCycle))
		{
			TransactionEvent e = event_queue.front();
			event_queue.pop_front();

			if (e.type == LAYER1_SEND_EVENT)
			{
				Process_Layer1_Send_Event(e);
			}
		}

	}

	void PCI_SSD_System::Add_Event(TransactionEvent e)
	{
		event_queue.push_back(e);
		event_queue.sort();

	}


	void PCI_SSD_System::Process_Layer1()
	{
		if (!layer1_busy)
		{
			// TODO: Check return queue

			// Check send queue
			if (!layer1_send_queue.empty())
			{
				// Put this transaction in the event queue with LAYER1_SEND_DELAY as timer.
				// Sort the event queue to make sure the event queue stays sorted by expire time.
				Transaction t = layer1_send_queue.front();
				layer1_send_queue.pop_front();
				TransactionEvent e (LAYER1_SEND_EVENT, t, currentClockCycle + LAYER1_SEND_DELAY);
				Add_Event(e);
				layer1_busy = true;

				if (DEBUG)
				{
					cout << currentClockCycle << " : Starting LAYER1 SEND for transaction: (" << t.isWrite << ", " << t.addr << ")\n";
				}
			}
		}
	}



	void PCI_SSD_System::Process_Layer1_Send_Event(TransactionEvent e)
	{
		assert(e.type == LAYER1_SEND_EVENT);

		bool success = hybridsim->addTransaction(e.trans.isWrite, e.trans.addr);
		if (success)
		{
			layer1_busy = false;

			if (DEBUG)
			{
				cout << currentClockCycle << " : Finished LAYER1 SEND for transaction: (" << e.trans.isWrite << ", " << e.trans.addr << ")\n";
			}
		}
		else
		{
			// Try again in RETRY_DELAY cycles.
			e.expire_time += RETRY_DELAY;
			Add_Event(e);
			layer1_busy = true;
		}
	}


}
