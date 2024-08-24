#ifndef COMMON_DEREGISTRATION_INTERFACE_H
#define COMMON_DEREGISTRATION_INTERFACE_H

namespace strtb::common {

template <class identifier> class deregistration_interface {
protected:
    virtual ~deregistration_interface() = default;
public:
    virtual void deregister(identifier object) = 0;
};

}

#endif // COMMON_DEREGISTRATION_INTERFACE_H
