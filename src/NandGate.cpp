#include "NandGate.h"

void NandGate::evaluate(EventQueue& queue, uint64_t current_time)
{
    if (inputs_.empty() || outputs_.empty())
    {
        return; 
    }

    LogicValue result = LogicValue::LOW;

    for (Net* input_net : inputs_)
    {
        LogicValue input_val = input_net->getValue();

        if (input_val == LogicValue::LOW)
        {
            result = LogicValue::HIGH;
            break;
        } 
        else if (input_val == LogicValue::UNKNOWN ||
                 input_val == LogicValue::HIGHZ)
        {
            result = LogicValue::UNKNOWN;
        }
    }

    Net* output_net = outputs_[0];

    if (output_net->getValue() != result)
    {
        uint64_t future_time = current_time + delay_;
        queue.push(Event(future_time, output_net, result));
    }
}
