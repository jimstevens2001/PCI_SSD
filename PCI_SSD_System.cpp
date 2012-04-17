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
		// Create a new transaction for the return.
		Transaction t(false, addr);

		// Put transaction in appropriate return queue.
		layer2_return_queue.push_back(t);

		if (DEBUG)
		{
			cout << currentClockCycle << " : Added transaction to layer2_return_queue: (" << t.isWrite << ", " << t.addr << ")\n";
		}
	}


	void PCI_SSD_System::HybridSim_Write_Callback(uint id, uint64_t addr, uint64_t cycle)
	{
		// Create a new transaction for the return.
		Transaction t(true, addr);

		// Put transaction in appropriate return queue.
		layer2_return_queue.push_back(t);

		if (DEBUG)
		{
			cout << currentClockCycle << " : Added transaction to layer2_return_queue: (" << t.isWrite << ", " << t.addr << ")\n";
		}
	}


	void PCI_SSD_System::update()
	{
		// Do Processing for layer 2
		Process_Layer2();

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
				cout << currentClockCycle << " : length(event_queue)=" << event_queue.size() 
						<< " length(layer1_send_queue)=" << layer1_send_queue.size() 
						<< " length(layer1_return_queue)=" << layer1_return_queue.size() << "\n";
			}
		}
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
				Process_Layer1_Send_Event(e);
			}
			else if (e.type == LAYER1_RETURN_EVENT)
			{
				Process_Layer1_Return_Event(e);
			}
			else if (e.type == LAYER2_SEND_EVENT)
			{
				Process_Layer2_Send_Event(e);
			}
			else if (e.type == LAYER2_RETURN_EVENT)
			{
				Process_Layer2_Return_Event(e);
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


	// Layer 1 implementation
	void PCI_SSD_System::Process_Layer1()
	{
		if (!layer1_busy)
		{
			// Check return queue
			// Return queue has strict priority over send queue
			if (!layer1_return_queue.empty())
			{
				// Put this transaction in the event queue with appropriate delay as timer.

				// Extract the transaction at the front of the queue.
				Transaction t = layer1_return_queue.front();
				layer1_return_queue.pop_front();

				// Add event to the event queue.
				uint64_t delay = t.isWrite ? LAYER1_COMMAND_DELAY : LAYER1_DATA_DELAY;
				TransactionEvent e (LAYER1_RETURN_EVENT, t, currentClockCycle + delay);
				Add_Event(e);

				// Set the busy indicator for layer 1.
				layer1_busy = true;

				if (DEBUG)
				{
					cout << currentClockCycle << " : Starting LAYER1 RETURN for transaction: (" << t.isWrite << ", " << t.addr << ")\n";
				}
			}

			// Check send queue
			else if (!layer1_send_queue.empty())
			{
				// Put this transaction in the event queue with appropriate delay as timer.

				// Extract the transaction at the front of the queue.
				Transaction t = layer1_send_queue.front();
				layer1_send_queue.pop_front();

				// Add event to the event queue.
				uint64_t delay = t.isWrite ? LAYER1_DATA_DELAY : LAYER1_COMMAND_DELAY;
				TransactionEvent e (LAYER1_SEND_EVENT, t, currentClockCycle + delay);
				Add_Event(e);

				// Set the busy indicator for layer 1.
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

		// Send transaction to layer 2
		layer2_send_queue.push_back(e.trans);
		layer1_busy = false;

		if (DEBUG)
		{
			cout << currentClockCycle << " : Finished LAYER1 SEND for transaction: (" << e.trans.isWrite << ", " << e.trans.addr << ")\n";
			cout << currentClockCycle << " : Added transaction to layer2_send_queue: (" << e.trans.isWrite << ", " << e.trans.addr << ")\n";
		}

/*
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
			Retry_Event(e);
			layer1_busy = true;
		}
*/
	}

	void PCI_SSD_System::Process_Layer1_Return_Event(TransactionEvent e)
	{
		assert(e.type == LAYER1_RETURN_EVENT);

		if (DEBUG)
		{
			cout << currentClockCycle << " : Finished LAYER1 RETURN for transaction: (" << e.trans.isWrite << ", " << e.trans.addr << ")\n";
		}

		layer1_busy = false;

		// Call the appropriate callback.
		if (e.trans.isWrite)
		{
			if (WriteDone != NULL)
				(*WriteDone)(systemID, e.trans.addr, currentClockCycle);
		}
		else
		{
			if (ReadDone != NULL)
				(*ReadDone)(systemID, e.trans.addr, currentClockCycle);
		}
	}

	void PCI_SSD_System::Process_Layer2()
	{
		if (!layer2_busy)
		{
			// Check return queue
			// Return queue has strict priority over send queue
			if (!layer2_return_queue.empty())
			{
				// Put this transaction in the event queue with appropriate delay as timer.

				// Extract the transaction at the front of the queue.
				Transaction t = layer2_return_queue.front();
				layer2_return_queue.pop_front();

				// Add event to the event queue.
				uint64_t delay = t.isWrite ? LAYER2_COMMAND_DELAY : LAYER2_DATA_DELAY;
				TransactionEvent e (LAYER2_RETURN_EVENT, t, currentClockCycle + delay);
				Add_Event(e);

				// Set the busy indicator for layer 1.
				layer2_busy = true;

				if (DEBUG)
				{
					cout << currentClockCycle << " : Starting LAYER2 RETURN for transaction: (" << t.isWrite << ", " << t.addr << ")\n";
				}
			}

			// Check send queue
			else if (!layer2_send_queue.empty())
			{
				// Put this transaction in the event queue with appropriate delay as timer.

				// Extract the transaction at the front of the queue.
				Transaction t = layer2_send_queue.front();
				layer2_send_queue.pop_front();

				// Add event to the event queue.
				uint64_t delay = t.isWrite ? LAYER2_DATA_DELAY : LAYER2_COMMAND_DELAY;
				TransactionEvent e (LAYER2_SEND_EVENT, t, currentClockCycle + delay);
				Add_Event(e);

				// Set the busy indicator for layer 1.
				layer2_busy = true;

				if (DEBUG)
				{
					cout << currentClockCycle << " : Starting LAYER2 SEND for transaction: (" << t.isWrite << ", " << t.addr << ")\n";
				}
			}
		}

	}


	void PCI_SSD_System::Process_Layer2_Send_Event(TransactionEvent e)
	{
		assert(e.type == LAYER2_SEND_EVENT);

		bool success = hybridsim->addTransaction(e.trans.isWrite, e.trans.addr);
		if (success)
		{
			layer2_busy = false;

			if (DEBUG)
			{
				cout << currentClockCycle << " : Finished LAYER2 SEND for transaction: (" << e.trans.isWrite << ", " << e.trans.addr << ")\n";
			}
		}
		else
		{
			Retry_Event(e);
			layer2_busy = true;
		}

	}


	void PCI_SSD_System::Process_Layer2_Return_Event(TransactionEvent e)
	{
		assert(e.type == LAYER2_RETURN_EVENT);

		// Send transaction back to layer 1
		layer1_return_queue.push_back(e.trans);
		layer2_busy = false;

		if (DEBUG)
		{
			cout << currentClockCycle << " : Finished LAYER2 RETURN for transaction: (" << e.trans.isWrite << ", " << e.trans.addr << ")\n";
			cout << currentClockCycle << " : Added transaction to layer1_return_queue: (" << e.trans.isWrite << ", " << e.trans.addr << ")\n";
		}
	}


}
