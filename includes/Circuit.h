#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <vector>
#include <memory>

#include "Component.h"
#include "Net.h"
#include "Event.h"

class Circuit
{
private:
    std::vector<std::unique_ptr<Component>> all_components_;
    std::vector<std::unique_ptr<Net>> all_nets_;
public:
    
};

#endif
