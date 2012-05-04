#include "Layer.h"
#include "PCI_SSD_System.h"

namespace PCISSD
{

	Layer::Layer(PCI_SSD_System *parent, uint64_t data_delay, uint64_t command_delay, uint64_t num_lanes, bool full_duplex,
			TransactionEventType send_event_type, TransactionEventType return_event_type, string layer_name)
	{
		assert(parent != NULL);

		this->parent = parent;
		this->data_delay = data_delay;
		this->command_delay = command_delay;
		this->num_lanes = num_lanes;
		this->full_duplex = full_duplex;
		this->send_event_type = send_event_type;
		this->return_event_type = return_event_type;
		this->layer_name = layer_name;

		send_busy = false;
		return_busy = false;
	}

	void Layer::update()
	{
		// If we are in half_duplex mode, then we are busy for both send and return
		// if either path is busy.
		bool half_duplex_busy = (!full_duplex) && (send_busy || return_busy);

		// Check return queue
		// Return queue has strict priority over send queue in half duplex mode, so we check it first.
		if (!(half_duplex_busy || return_busy))
		{
			if (!return_queue.empty())
			{
				// Extract the transaction at the front of the queue.
				Transaction t = return_queue.front();
				return_queue.pop_front();

				// Put this transaction in the event queue with appropriate delay as timer.
				Return_Event_Start(t);
			}
		}

		// Recompute half_deplux_busy in case a return event was triggered.
		half_duplex_busy = (!full_duplex) && (send_busy || return_busy);

		// Check send queue
		if (!(half_duplex_busy || send_busy))
		{
			if (!send_queue.empty())
			{
				// Extract the transaction at the front of the queue.
				Transaction t = send_queue.front();
				send_queue.pop_front();

				// Put this transaction in the event queue with appropriate delay as timer.
				Send_Event_Start(t);
			}
		}
	}

	void Layer::Add_Send_Transaction(Transaction t)
	{
		send_queue.push_back(t);

		if (DEBUG)
		{
			(parent->debug_file) << parent->currentClockCycle << " : Added transaction to " << layer_name << 
					" send queue: (" << t.isWrite << ", " << t.addr << ")\n";
			parent->debug_file.flush();
		}
	}

	void Layer::Add_Return_Transaction(Transaction t)
	{
		return_queue.push_back(t);

		if (DEBUG)
		{
			(parent->debug_file) << parent->currentClockCycle << " : Added transaction to " << layer_name << 
					" return queue: (" << t.isWrite << ", " << t.addr << ")\n";
			parent->debug_file.flush();
		}
	}

	void Layer::Send_Event_Start(Transaction t)
	{
		Event_Start(t, data_delay, command_delay, send_event_type, "SEND");
	}

	void Layer::Return_Event_Start(Transaction t)
	{
		Event_Start(t, command_delay, data_delay, return_event_type, "RETURN");
	}

	void Layer::Event_Start(Transaction t, uint64_t write_delay, uint64_t read_delay, TransactionEventType event_type, string type)
	{
		// Add event to the event queue.
		uint64_t delay = t.isWrite ? write_delay : read_delay;
		delay = delay * t.num_sectors; 
		delay = delay / num_lanes;
		TransactionEvent e (event_type, t, parent->currentClockCycle + delay);
		parent->Add_Event(e);

		// Set the appropriate busy flag.
		if (type == "SEND")
			send_busy = true;
		else
			return_busy = true;

		if (DEBUG)
		{
			(parent->debug_file) << parent->currentClockCycle << " : Starting " << layer_name << " " << type << 
					" for transaction: (" << t.isWrite << ", " << t.addr << ")\n";
			parent->debug_file.flush();
		}

	}

	void Layer::Send_Event_Done(Transaction t)
	{
		Event_Done(t, "SEND");
	}


	void Layer::Return_Event_Done(Transaction t)
	{
		Event_Done(t, "RETURN");
	}

	void Layer::Event_Done(Transaction t, string type)
	{
		// Unset the appropriate busy flag.
		if (type == "SEND")
			send_busy = false;
		else
			return_busy = false;

		if (DEBUG)
		{
			(parent->debug_file) << parent->currentClockCycle << " : Finished " << layer_name << " " << type << 
					" for transaction: (" << t.isWrite << ", " << t.addr << ")\n";
			parent->debug_file.flush();
		}
	}
}

