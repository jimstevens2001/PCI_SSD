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


#ifndef CALLBACK_PCI_H
#define CALLBACK_PCI_H

#include <vector>
#include <stdint.h>
#include <sys/types.h>

using std::vector;

namespace PCISSD
{

	template <typename ReturnT, typename Param1T, typename Param2T, typename Param3T>
	class CallbackBase
	{
		public:
		virtual ReturnT operator()(Param1T, Param2T, Param3T) = 0;
	};


	template <typename ConsumerT, typename ReturnT, typename Param1T, typename Param2T, typename Param3T >
	class Callback: public CallbackBase<ReturnT,Param1T,Param2T,Param3T>
	{
		private:
		typedef ReturnT (ConsumerT::*PtrMember)(Param1T,Param2T,Param3T);

		public:
		Callback(ConsumerT* const object, PtrMember member) : object(object), member(member) {}

		Callback(const Callback<ConsumerT,ReturnT,Param1T,Param2T,Param3T> &e) : object(e.object), member(e.member) {}

		ReturnT operator()(Param1T param1, Param2T param2, Param3T param3)
		{
			return (const_cast<ConsumerT*>(object)->*member)(param1,param2,param3);
		}

		private:
		ConsumerT* const object;
		const PtrMember  member;
	};

	typedef CallbackBase <void, uint, uint64_t, uint64_t> TransactionCompleteCB;
	typedef CallbackBase <void, uint, uint64_t, uint64_t> DMATransactionCB;

} 

#endif

