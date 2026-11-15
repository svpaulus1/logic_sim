#include "Circuit.h"

void Circuit::run() 
{
    while (!event_queue_.empty()) 
    {
        Event current_event = event_queue_.top();
        event_queue_.pop();

        current_time_ = current_event.getTimestamp();

        Net* target_net = current_event.getTarget();
        LogicValue new_value = current_event.getValue();

        if (target_net->getValue() == new_value)
        {
            continue; 
        }

        target_net->setValue(new_value);

        for (Component* gate : target_net->getConsumers())
        {
            gate->evaluate(event_queue_, current_time_); 
        }
    }
}
