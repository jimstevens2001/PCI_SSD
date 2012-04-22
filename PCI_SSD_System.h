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


#ifndef PCI_SSD_SYSTEM_H
#define PCI_SSD_SYSTEM_H

#include "ClockDomain.h"
#include "CallbackPCI.h"
#include "config.h"

namespace PCISSD
{
	class PCI_SSD_System
	{
		public:
		PCI_SSD_System(uint id);
		bool addTransaction(bool isWrite, uint64_t addr);
		bool WillAcceptTransaction();
		void update();
		void RegisterCallbacks(TransactionCompleteCB *readDone, TransactionCompleteCB *writeDone);
		void printLogfile();

		// Internal functions
		void HybridSim_Read_Callback(uint id, uint64_t addr, uint64_t cycle);
		void HybridSim_Write_Callback(uint id, uint64_t addr, uint64_t cycle);


		void update_internal();
		void hybridsim_update_internal();

		void Process_Event_Queue();
		void Add_Event(TransactionEvent e);
		void Retry_Event(TransactionEvent e);

		void Process_Layer1();
		void Layer1_Send_Event_Start(Transaction t);
		void Layer1_Return_Event_Start(Transaction t);
		void Layer1_Send_Event_Done(TransactionEvent e);
		void Layer1_Return_Event_Done(TransactionEvent e);

		void Process_Layer2();
		void Layer2_Send_Event_Start(Transaction t);
		void Layer2_Return_Event_Start(Transaction t);
		void Layer2_Send_Event_Done(TransactionEvent e);
		void Layer2_Return_Event_Done(TransactionEvent e);

		void handle_hybridsim_callback(bool isWrite, uint64_t addr);

		void issue_external_callback(bool isWrite, uint64_t orig_addr);


		// Internal state
        TransactionCompleteCB *ReadDone;
        TransactionCompleteCB *WriteDone;
		uint systemID;

		uint64_t currentClockCycle;
		ClockDomain::ClockDomainCrosser *clockdomain;

		HybridSim::HybridSystem *hybridsim;
		ClockDomain::ClockDomainCrosser *hybridsim_clockdomain;

		// State to save while HybridSim is doing its thing.
		unordered_map<uint64_t, Transaction> hybridsim_transactions; // Outstanding sector transactions.
		unordered_map<uint64_t, set<uint64_t>> hybridsim_accesses; // Outstanding acceses to HybridSim for each sector.

		set<uint64_t> pending_sectors; // Simple rule: only one instance of each address at a time, otherwise, this is an error.

		list<TransactionEvent> event_queue;

		bool layer1_busy;
		list<Transaction> layer1_send_queue;
		list<Transaction> layer1_return_queue;

		bool layer2_busy;
		list<Transaction> layer2_send_queue;
		list<Transaction> layer2_return_queue;

	};

	PCI_SSD_System *getInstance(uint id);

}

#endif
