#ifndef VCD_BASE_H_
#define VCD_BASE_H_

/**
 * The base class of VCDClient and VCDServer.
 * A pointer to this class is added to each packet.
 * So that upon correct receipt or incorrect receipt, the packet would call the
 * CorrectSend or InCorrectSend function in VCDClient or VCDServer.
 * VCDClient and VCDServer override these two functions in their implementation.
 */
class  VCDBase {
public:
        virtual void correctSend() {};
        virtual void inCorrectSend() {};;
};

#endif
