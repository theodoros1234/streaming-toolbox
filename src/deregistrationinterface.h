#ifndef DEREGISTRATIONINTERFACE_H
#define DEREGISTRATIONINTERFACE_H

template <class Identifier> class DeregistrationInterface {
protected:
    virtual ~DeregistrationInterface() = default;
public:
    virtual void deregister(Identifier object) = 0;
};

#endif // DEREGISTRATIONINTERFACE_H
