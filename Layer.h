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

#ifndef PCI_SSD_LAYER_H
#define PCI_SSD_LAYER_H

#include "common.h"

namespace PCISSD
{
	// Forward declare.
	class PCI_SSD_System;

	class Layer
	{
		public:
		Layer(PCI_SSD_System *parent, uint64_t data_delay, uint64_t command_delay, uint64_t num_lanes, bool full_duplex,
				TransactionEventType send_event_type, TransactionEventType return_event_type, string layer_name);

		void update();
		void Add_Send_Transaction(Transaction t);
		void Add_Return_Transaction(Transaction t);

		void Send_Event_Done(Transaction t);
		void Return_Event_Done(Transaction t);

		// Internal functions
		void Send_Event_Start(Transaction t);
		void Return_Event_Start(Transaction t);

		void Event_Start(Transaction t, uint64_t write_delay, uint64_t read_delay, TransactionEventType event_type, string type);
		void Event_Done(Transaction t, string type);


		// Parameters
		PCI_SSD_System *parent;
		uint64_t data_delay;
		uint64_t command_delay;
		uint64_t num_lanes;
		bool full_duplex;
		TransactionEventType send_event_type;
		TransactionEventType return_event_type;
		string layer_name;

		// Internal state
		bool send_busy;
		bool return_busy;
		list<Transaction> send_queue;
		list<Transaction> return_queue;
	};
}

#endif
